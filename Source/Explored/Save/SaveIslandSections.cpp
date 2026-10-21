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
		// Una pila sin id o con cantidad no positiva invalida la mochila entera: mejor avisar que cargar a medias.
		for (const FSaveArchive& Entry : Stacks)
		{
			FBackpackStack Stack;
			if (!Entry.Read(TEXT("id"), Stack.Id) || !Entry.Read(TEXT("count"), Stack.Count) || Stack.Id.IsNone() || Stack.Count <= 0)
			{
				OutPack = FBackpackState();
				return false;
			}
			OutPack.Stacks.Add(Stack);
		}
		Ar.Read(TEXT("heldHeavy"), OutPack.HeldHeavy);
		if (Ar.Read(TEXT("canteenSips"), OutPack.CanteenSips) && OutPack.CanteenSips < 0)
		{
			OutPack = FBackpackState();
			return false;
		}
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

	void SaveBodyBars(FSaveArchive& Ar, const FBodyBars& Bars, FName LastShelter)
	{
		Ar.Write(TEXT("thirst"), Bars.Thirst);
		Ar.Write(TEXT("hunger"), Bars.Hunger);
		Ar.Write(TEXT("energy"), Bars.Energy);
		Ar.Write(TEXT("warmth"), Bars.Warmth);
		Ar.Write(TEXT("lastShelter"), LastShelter);
	}

	bool LoadBodyBars(const FSaveArchive& Ar, FBodyBars& OutBars, FName& OutLastShelter)
	{
		FBodyBars Bars;
		float* const Fields[] = { &Bars.Thirst, &Bars.Hunger, &Bars.Energy, &Bars.Warmth };
		const TCHAR* const Keys[] = { TEXT("thirst"), TEXT("hunger"), TEXT("energy"), TEXT("warmth") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Keys); ++Index)
		{
			if (!Ar.Read(Keys[Index], *Fields[Index]) || !FMath::IsFinite(*Fields[Index]) || *Fields[Index] < 0.0f || *Fields[Index] > 100.0f)
			{
				OutBars = FBodyBars();
				OutLastShelter = NAME_None;
				return false;
			}
		}
		FName Shelter;
		Ar.Read(TEXT("lastShelter"), Shelter);
		OutBars = Bars;
		OutLastShelter = Shelter;
		return true;
	}

	void SaveRaft(FSaveArchive& Ar, const FRaftSaveData& Raft)
	{
		Ar.Write(TEXT("logs"), Raft.Site.LogsPlaced);
		Ar.Write(TEXT("built"), Raft.Site.bBuilt);
		Ar.Write(TEXT("hasPose"), Raft.bHasPose);
		if (Raft.bHasPose)
		{
			Ar.Write(TEXT("location"), Raft.Location);
			Ar.Write(TEXT("yaw"), Raft.YawDeg);
		}
	}

	bool LoadRaft(const FSaveArchive& Ar, FRaftSaveData& OutRaft)
	{
		FRaftSaveData Raft;
		Ar.Read(TEXT("logs"), Raft.Site.LogsPlaced);
		Ar.Read(TEXT("built"), Raft.Site.bBuilt);
		Ar.Read(TEXT("hasPose"), Raft.bHasPose);
		if (Raft.bHasPose && !(Ar.Read(TEXT("location"), Raft.Location) && Ar.Read(TEXT("yaw"), Raft.YawDeg)
			&& FMath::IsFinite(Raft.Location.X) && FMath::IsFinite(Raft.Location.Y) && FMath::IsFinite(Raft.Location.Z) && FMath::IsFinite(Raft.YawDeg)))
		{
			Raft.bHasPose = false;
		}
		if (!FRaftModel::IsValid(Raft.Site))
		{
			OutRaft = FRaftSaveData();
			return false;
		}
		OutRaft = Raft;
		return true;
	}
}
