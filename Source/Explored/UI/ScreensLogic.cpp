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
	TArray<FMuseumEntry> BuildMuseumEntries(const FMuseumModel& Museum)
	{
		TArray<FMuseumEntry> Out;
		const TArray<FArtifactDef>& Artifacts = Museum.GetCatalog().Artifacts;
		Out.Reserve(Artifacts.Num());
		for (const FArtifactDef& Def : Artifacts)
		{
			FMuseumEntry& Entry = Out.AddDefaulted_GetRef();
			Entry.ArtifactId = Def.Id;
			Entry.Provenance = Def.Provenance;
			Entry.Rarity = Def.Rarity;
			Entry.Size = Def.Size;

			const FArtifactRecord* Record = Museum.FindRecord(Def.Id);
			if (Record && Record->bFound)
			{
				Entry.State = Museum.IsExhibited(Def.Id) ? EMuseumEntryState::Exhibited : EMuseumEntryState::Found;
				Entry.FoundAt = Record->FoundAt;
				Entry.FoundIsland = Record->FoundIsland;
			}
			else if (Record && Record->bPhotographed)
			{
				Entry.State = EMuseumEntryState::Photographed;
			}
			Entry.bShowName = Entry.State != EMuseumEntryState::Unknown;
		}
		return Out;
	}

	FMuseumSummary SummarizeMuseum(const FMuseumModel& Museum)
	{
		FMuseumSummary Out;
		Out.Total = Museum.GetCatalog().Artifacts.Num();
		Out.Registered = Museum.CountRegistered();
		Out.Found = Museum.CountFound();
		Out.Exhibited = Museum.CountExhibited();
		Out.CollectorTarget = FMuseumModel::CollectorThreshold;
		Out.CollectorProgress = Out.CollectorTarget > 0
			? FMath::Clamp(static_cast<float>(Out.Exhibited) / static_cast<float>(Out.CollectorTarget), 0.0f, 1.0f)
			: 1.0f;
		Out.bCollectorDone = Museum.HasCollectorAchievement();
		Out.CatalogProgress = FMath::Clamp(Museum.CatalogCompletion(), 0.0f, 1.0f);
		return Out;
	}

	TArray<FAchievementRow> BuildAchievementRows(const FAchievementsModel& Model)
	{
		TArray<FAchievementRow> Out;
		const bool bInRun = !Model.GetState().RunMode.IsNone();
		for (const FAchievementDef& Def : Model.GetAchievements())
		{
			// Los de una fase aún no publicada no salen (biblia 07 §2), salvo que ya estén conseguidos.
			if (!Model.IsReleased(Def) && !Model.IsUnlocked(Def.Id))
			{
				continue;
			}
			FAchievementRow& Row = Out.AddDefaulted_GetRef();
			Row.Id = Def.Id;
			Row.bUnlocked = Model.IsUnlocked(Def.Id);
			Row.bMasked = Def.bHidden && !Row.bUnlocked;
			Row.bAvailableInMode = Row.bUnlocked || !bInRun || Model.IsAvailableInCurrentMode(Def);
			const float Progress = Model.GetProgress(Def.Id);
			Row.Progress = Row.bMasked ? 0.0f : (FMath::IsFinite(Progress) ? FMath::Clamp(Progress, 0.0f, 1.0f) : 0.0f);
		}
		return Out;
	}

	FAchievementsSummary SummarizeAchievements(const FAchievementsModel& Model)
	{
		FAchievementsSummary Out;
		for (const FAchievementDef& Def : Model.GetAchievements())
		{
			if (!Model.IsReleased(Def) && !Model.IsUnlocked(Def.Id))
			{
				continue;
			}
			++Out.Total;
			Out.Unlocked += Model.IsUnlocked(Def.Id) ? 1 : 0;
		}
		Out.Fraction = Out.Total > 0 ? static_cast<float>(Out.Unlocked) / static_cast<float>(Out.Total) : 0.0f;
		return Out;
	}

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
}
