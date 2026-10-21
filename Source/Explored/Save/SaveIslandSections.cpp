#include "Save/SaveIslandSections.h"

namespace SaveIslandDetail
{
	const TCHAR* const KeyStage = TEXT("stage");
	const TCHAR* const KeyFlags = TEXT("flags");
	const TCHAR* const KeyOilJars = TEXT("oilJars");
	const TCHAR* const KeySlots = TEXT("lanternSlots");
	const TCHAR* const KeyThoughts = TEXT("thoughts");
	const TCHAR* const KeyLastHours = TEXT("lastHours");
	const TCHAR* const KeyLitAt = TEXT("litAtHours");
	const TCHAR* const KeyMistral = TEXT("mistralArrived");
	const TCHAR* const KeyPagesMask = TEXT("mask");
	const TCHAR* const KeyCollected = TEXT("collected");
}

namespace ExploredSaveStates
{
	void SaveBeaconProgress(FSaveArchive& Ar, const FBeaconRunState& Run)
	{
		using namespace SaveIslandDetail;
		const FBeaconProgressRecord Record = FBeaconProgressModel::ToRecord(Run.Progress);
		Ar.Write(KeyStage, static_cast<EBeaconStage>(Record.Stage));
		Ar.Write(KeyFlags, static_cast<int32>(Record.Flags));
		Ar.Write(KeyOilJars, static_cast<int32>(Record.OilJars));
		Ar.Write(KeySlots, static_cast<int32>(Record.LanternSlots));
		Ar.Write(KeyThoughts, static_cast<int32>(Run.ThoughtsShown));
		Ar.Write(KeyLastHours, Run.LastAbsHours);
		Ar.Write(KeyLitAt, Run.LitAtHours);
		Ar.Write(KeyMistral, Run.bMistralArrived);
	}

	bool LoadBeaconProgress(const FSaveArchive& Ar, FBeaconRunState& InOutRun)
	{
		using namespace SaveIslandDetail;
		FBeaconRunState Loaded;
		Loaded.Notebook = InOutRun.Notebook;
		if (Ar.IsEmpty())
		{
			InOutRun = Loaded;
			return true;
		}
		EBeaconStage Stage = EBeaconStage::Prologo;
		int32 Flags = 0;
		int32 OilJars = 0;
		int32 Slots = 0;
		int32 Thoughts = 0;
		const bool bRead = Ar.Read(KeyStage, Stage) && Ar.Read(KeyFlags, Flags) && Ar.Read(KeyOilJars, OilJars) && Ar.Read(KeySlots, Slots);
		const auto FitsByte = [](int32 Value) { return Value >= 0 && Value <= 0xFF; };
		if (!bRead || !FitsByte(Flags) || !FitsByte(OilJars) || !FitsByte(Slots))
		{
			InOutRun = Loaded;
			return false;
		}
		FBeaconProgressRecord Record;
		Record.Stage = static_cast<uint8>(Stage);
		Record.Flags = static_cast<uint8>(Flags);
		Record.OilJars = static_cast<uint8>(OilJars);
		Record.LanternSlots = static_cast<uint8>(Slots);
		const FBeaconProgressResult Progress = FBeaconProgressModel::FromRecord(Record);
		if (!Progress.bOk)
		{
			InOutRun = Loaded;
			return false;
		}
		Loaded.Progress = Progress.State;
		if (Ar.Read(KeyThoughts, Thoughts))
		{
			Loaded.ThoughtsShown = static_cast<uint16>(Thoughts & FBeaconDirectorModel::AllThoughtsMask);
		}
		double LastHours = -1.0;
		double LitAt = -1.0;
		Ar.Read(KeyLastHours, LastHours);
		Ar.Read(KeyLitAt, LitAt);
		Loaded.LastAbsHours = FMath::IsFinite(LastHours) ? LastHours : -1.0;
		// La hora de encendido solo tiene sentido con la baliza encendida.
		Loaded.LitAtHours = Loaded.Progress.bBeaconLit && FMath::IsFinite(LitAt) && LitAt >= 0.0 ? LitAt : -1.0;
		bool bMistral = false;
		Ar.Read(KeyMistral, bMistral);
		Loaded.bMistralArrived = bMistral && Loaded.Progress.bBeaconLit;
		InOutRun = Loaded;
		return true;
	}

	void SavePages(FSaveArchive& Ar, const FNotebookState& Notebook)
	{
		Ar.Write(SaveIslandDetail::KeyPagesMask, static_cast<int32>(Notebook.PagesMask));
	}

	bool LoadPages(const FSaveArchive& Ar, FNotebookState& OutNotebook)
	{
		OutNotebook = FNotebookState();
		int32 Mask = 0;
		if (!Ar.Read(SaveIslandDetail::KeyPagesMask, Mask))
		{
			return Ar.IsEmpty();
		}
		if (Mask < 0 || Mask > FNotebookModel::AllPagesMask)
		{
			return false;
		}
		OutNotebook.PagesMask = static_cast<uint8>(Mask);
		return true;
	}

	void SaveBackpack(FSaveArchive& Ar, const FBackpackState& Pack)
	{
		TArray<FSaveArchive> Stacks;
		for (const FBackpackStack& Stack : Pack.Stacks)
		{
			FSaveArchive& Entry = Stacks.AddDefaulted_GetRef();
			Entry.Write(TEXT("id"), Stack.Id);
			Entry.Write(TEXT("count"), Stack.Count);
		}
		Ar.Write(TEXT("stacks"), Stacks);
		Ar.Write(TEXT("heldHeavy"), Pack.HeldHeavy);
		Ar.Write(TEXT("canteenSips"), Pack.CanteenSips);
	}

	bool LoadBackpack(const FSaveArchive& Ar, FBackpackState& OutPack)
	{
		OutPack = FBackpackState();
		TArray<FSaveArchive> Stacks;
		if (!Ar.Read(TEXT("stacks"), Stacks))
		{
			return false;
		}
		for (const FSaveArchive& Entry : Stacks)
		{
			FBackpackStack Stack;
			Entry.Read(TEXT("id"), Stack.Id);
			Entry.Read(TEXT("count"), Stack.Count);
			OutPack.Stacks.Add(Stack);
		}
		Ar.Read(TEXT("heldHeavy"), OutPack.HeldHeavy);
		Ar.Read(TEXT("canteenSips"), OutPack.CanteenSips);
		return true;
	}

	void SaveCollectedIds(FSaveArchive& Ar, const TSet<FName>& Ids)
	{
		TArray<FString> Sorted;
		for (const FName& Id : Ids)
		{
			Sorted.Add(Id.ToString());
		}
		Sorted.Sort();
		Ar.Write(SaveIslandDetail::KeyCollected, Sorted);
	}

	void LoadCollectedIds(const FSaveArchive& Ar, TSet<FName>& OutIds)
	{
		OutIds.Reset();
		TArray<FString> Ids;
		Ar.Read(SaveIslandDetail::KeyCollected, Ids);
		for (const FString& Id : Ids)
		{
			if (!Id.IsEmpty())
			{
				OutIds.Add(FName(*Id));
			}
		}
	}
}
