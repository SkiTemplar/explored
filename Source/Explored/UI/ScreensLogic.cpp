#include "UI/ScreensLogic.h"

// Espacio de nombres con nombre (no anónimo) para no chocar en el Unity build.
namespace ExploredScreensDetail
{
	ExploredScreens::FInventorySection MakeSection(EInventorySlot Slot, const FInventoryContainer& Container)
	{
		ExploredScreens::FInventorySection Section;
		Section.Slot = Slot;
		Section.Capacity = Container.Spec.MaxSlots;
		Section.UsedWeightKg = Container.GetUsedWeightKg();
		Section.MaxWeightKg = Container.Spec.MaxWeightKg;
		Section.UsedVolumeLiters = Container.GetUsedVolumeLiters();
		Section.MaxVolumeLiters = Container.Spec.MaxVolumeLiters;

		TArray<FInventoryEntry> Sorted = Container.Entries;
		Sorted.Sort([](const FInventoryEntry& A, const FInventoryEntry& B) { return A.SlotIndex < B.SlotIndex; });
		for (const FInventoryEntry& Entry : Sorted)
		{
			Section.ItemIds.Add(Entry.Item.InstanceId);
		}
		return Section;
	}
}

namespace ExploredScreens
{
	TArray<FSaveSlotRow> BuildSaveSlotRows(const TArray<FSaveSlotInfo>& Listed, const TArray<FString>& SlotsWithBackup, ESaveSlotsMode Mode)
	{
		TArray<FSaveSlotRow> Out;
		const TArray<FString> SlotIds = FSaveSlotPolicy::AllSlotIds();
		for (int32 Index = 0; Index < SlotIds.Num(); ++Index)
		{
			FSaveSlotRow& Row = Out.AddDefaulted_GetRef();
			Row.SlotId = SlotIds[Index];
			Row.bIsAuto = Row.SlotId == FSaveSlotPolicy::AutoSlotId();
			for (int32 Manual = 1; Manual <= FSaveSlotPolicy::NumManualSlots; ++Manual)
			{
				if (Row.SlotId == FSaveSlotPolicy::ManualSlotId(Manual))
				{
					Row.ManualIndex = Manual;
				}
			}
			for (const FString& WithBackup : SlotsWithBackup)
			{
				Row.bHasBackup |= FSaveSlotPolicy::NormalizeSlotId(WithBackup) == Row.SlotId;
			}

			const FSaveSlotInfo* Info = Listed.FindByPredicate([&Row](const FSaveSlotInfo& Candidate)
			{
				return FSaveSlotPolicy::NormalizeSlotId(Candidate.SlotId) == Row.SlotId;
			});
			if (!Info || Info->Result == ESaveLoadResult::NotFound)
			{
				Row.State = ESaveSlotRowState::Empty;
			}
			else if (Info->IsLoadable())
			{
				Row.State = Info->bFromBackup ? ESaveSlotRowState::Recovered : ESaveSlotRowState::Ok;
				Row.Header = Info->Header;
			}
			else if (Info->Result == ESaveLoadResult::FutureVersion)
			{
				Row.State = ESaveSlotRowState::FutureVersion;
			}
			else if (Info->Result == ESaveLoadResult::ObsoleteVersion)
			{
				Row.State = ESaveSlotRowState::Obsolete;
			}
			else
			{
				Row.State = ESaveSlotRowState::Damaged;
			}

			const bool bHasFile = Row.State != ESaveSlotRowState::Empty;
			if (Mode == ESaveSlotsMode::Save)
			{
				Row.bSelectable = !Row.bIsAuto;
				Row.bNeedsOverwriteConfirm = Row.bSelectable && bHasFile;
			}
			else
			{
				Row.bSelectable = Row.State == ESaveSlotRowState::Ok || Row.State == ESaveSlotRowState::Recovered;
			}
		}
		return Out;
	}

	FPlayTime SplitPlayTime(double Seconds)
	{
		FPlayTime Out;
		if (!FMath::IsFinite(Seconds) || Seconds <= 0.0)
		{
			return Out;
		}
		// Tope de 10⁶ horas: evita desbordar int32 con una cabecera manipulada.
		const double TotalMinutes = FMath::Min(FMath::FloorToDouble(Seconds / 60.0), 60.0e6);
		Out.Hours = static_cast<int32>(TotalMinutes / 60.0);
		Out.Minutes = static_cast<int32>(TotalMinutes - static_cast<double>(Out.Hours) * 60.0);
		return Out;
	}

	ELoadBand LoadBandFor(float CarriedWeightRatio)
	{
		if (!FMath::IsFinite(CarriedWeightRatio) || CarriedWeightRatio <= 1.0f)
		{
			return ELoadBand::Comfortable;
		}
		return CarriedWeightRatio >= FInventoryModel::MaxLoadRatio ? ELoadBand::Overloaded : ELoadBand::Heavy;
	}

	TArray<FInventorySection> BuildInventorySections(const FInventoryState& State, bool bHasPouch)
	{
		TArray<FInventorySection> Out;
		Out.Add(ExploredScreensDetail::MakeSection(EInventorySlot::Pockets, State.Pockets));
		Out.Add(ExploredScreensDetail::MakeSection(EInventorySlot::Belt, State.Belt));
		if (bHasPouch || !State.Pouch.IsEmpty())
		{
			Out.Add(ExploredScreensDetail::MakeSection(EInventorySlot::Pouch, State.Pouch));
		}
		if (State.bHasBackpack)
		{
			Out.Add(ExploredScreensDetail::MakeSection(EInventorySlot::Backpack, State.Backpack));
		}
		if (State.bHasSledge)
		{
			Out.Add(ExploredScreensDetail::MakeSection(EInventorySlot::Sledge, State.Sledge));
		}
		return Out;
	}

	FNotebookView BuildNotebookView(uint8 PagesMask, int32 Requested)
	{
		FNotebookView View;
		const auto IsRead = [PagesMask](int32 Page) { return Page >= 1 && Page <= NotebookPages && (PagesMask & (1u << (Page - 1))) != 0; };
		for (int32 Page = 1; Page <= NotebookPages; ++Page)
		{
			if (IsRead(Page))
			{
				++View.ReadCount;
				View.Shown = Page;
			}
		}
		if (IsRead(Requested))
		{
			View.Shown = Requested;
		}
		for (int32 Page = 1; Page <= NotebookPages; ++Page)
		{
			View.Rows.Add({ Page, IsRead(Page), Page == View.Shown });
		}
		return View;
	}

	int32 StepNotebookPage(uint8 PagesMask, int32 Current, int32 Direction)
	{
		const int32 Step = Direction >= 0 ? 1 : -1;
		for (int32 Page = Current + Step; Page >= 1 && Page <= NotebookPages; Page += Step)
		{
			if ((PagesMask & (1u << (Page - 1))) != 0)
			{
				return Page;
			}
		}
		return Current;
	}
}
