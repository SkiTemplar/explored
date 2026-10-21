#include "Items/ExploredIslandStateSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

#include "Boats/ExploredRaft.h"
#include "Explored.h"
#include "Items/ExploredPickup.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Narrative/ExploredSupplyCrate.h"
#include "Save/SaveArchive.h"
#include "Save/SaveIslandSections.h"
#include "UI/ExploredSaveSubsystem.h"
#include "Weather/ExploredRainBarrel.h"
#include "WorldGen/ExploredCoconutPalm.h"

namespace IslandStateDetail
{
	const TCHAR* const Section = ExploredSaveStates::SectionIsland;
}

bool UExploredIslandStateSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UExploredIslandStateSubsystem* UExploredIslandStateSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UExploredIslandStateSubsystem>() : nullptr;
}

void UExploredIslandStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegisterSection();
}

void UExploredIslandStateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RegisterSection();
}

void UExploredIslandStateSubsystem::Deinitialize()
{
	if (bSectionRegistered)
	{
		if (UExploredSaveSubsystem* Save = FindSave())
		{
			Save->UnregisterSection(IslandStateDetail::Section);
		}
	}
	Super::Deinitialize();
}

UExploredSaveSubsystem* UExploredIslandStateSubsystem::FindSave() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
}

void UExploredIslandStateSubsystem::RegisterSection()
{
	UExploredSaveSubsystem* SaveSystem = bSectionRegistered ? nullptr : FindSave();
	if (!SaveSystem)
	{
		return;
	}
	const TWeakObjectPtr<UExploredIslandStateSubsystem> WeakThis(this);
	bSectionRegistered = SaveSystem->RegisterSection(IslandStateDetail::Section,
		[WeakThis](FSaveArchive& Ar)
		{
			if (const UExploredIslandStateSubsystem* Self = WeakThis.Get())
			{
				Self->Save(Ar);
			}
		},
		[WeakThis](const FSaveArchive& Ar)
		{
			if (UExploredIslandStateSubsystem* Self = WeakThis.Get())
			{
				Self->Load(Ar);
			}
		});
}

void UExploredIslandStateSubsystem::MarkCollected(FName PropId)
{
	if (!PropId.IsNone())
	{
		Collected.Add(PropId);
	}
}

int32 UExploredIslandStateSubsystem::GetPalmCoconuts(FName PalmId, int32 Default) const
{
	const int32* Found = PalmCoconuts.Find(PalmId);
	return Found ? *Found : Default;
}

void UExploredIslandStateSubsystem::SetPalmCoconuts(FName PalmId, int32 Coconuts)
{
	PalmCoconuts.Add(PalmId, FMath::Max(0, Coconuts));
}

AExploredPickup* UExploredIslandStateSubsystem::SpawnDropped(FName ItemId, int32 Count, const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World || ItemId.IsNone() || Count <= 0)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AExploredPickup* Pickup = World->SpawnActor<AExploredPickup>(AExploredPickup::StaticClass(), FTransform(Location), Params);
	if (Pickup)
	{
		Pickup->ConfigureLoose(ItemId, Count);
		Dropped.Add({ ItemId, Count, Location });
		DroppedActors.Add(Pickup);
	}
	return Pickup;
}

void UExploredIslandStateSubsystem::ForgetDropped(const AExploredPickup* Pickup)
{
	const int32 Index = DroppedActors.IndexOfByPredicate([Pickup](const TWeakObjectPtr<AExploredPickup>& Weak) { return Weak.Get() == Pickup; });
	if (Index != INDEX_NONE)
	{
		DroppedActors.RemoveAt(Index);
		Dropped.RemoveAt(Index);
	}
}

void UExploredIslandStateSubsystem::ApplyToWorld()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AExploredPickup> It(World); It; ++It)
	{
		It->RefreshFromIslandState();
	}
	for (TActorIterator<AExploredCoconutPalm> It(World); It; ++It)
	{
		It->RefreshFromIslandState();
	}
	for (TActorIterator<AExploredRainBarrel> It(World); It; ++It)
	{
		It->RefreshFromIslandState();
	}
	for (TActorIterator<AExploredSupplyCrate> It(World); It; ++It)
	{
		It->RefreshFromIslandState();
	}
	for (TActorIterator<AExploredRaftSite> It(World); It; ++It)
	{
		It->RefreshFromIslandState();
	}
}

TOptional<FBackpackState> UExploredIslandStateSubsystem::TakePendingBackpack()
{
	TOptional<FBackpackState> Out = MoveTemp(PendingBackpack);
	PendingBackpack.Reset();
	return Out;
}

void UExploredIslandStateSubsystem::Save(FSaveArchive& Ar) const
{
	using namespace IslandStateDetail;
	ExploredSaveStates::SaveCollectedIds(Ar, Collected);
	TArray<FSaveArchive> Palms;
	for (const TPair<FName, int32>& Pair : PalmCoconuts)
	{
		FSaveArchive& Entry = Palms.AddDefaulted_GetRef();
		Entry.Write(TEXT("id"), Pair.Key);
		Entry.Write(TEXT("coconuts"), Pair.Value);
	}
	Ar.Write(TEXT("palms"), Palms);
	TArray<FSaveArchive> BarrelEntries;
	for (const TPair<FName, FRainBarrelState>& Pair : Barrels)
	{
		FSaveArchive& Entry = BarrelEntries.AddDefaulted_GetRef();
		Entry.Write(TEXT("id"), Pair.Key);
		Entry.Write(TEXT("charges"), Pair.Value.Charges);
		Entry.Write(TEXT("rainSeconds"), Pair.Value.RainSeconds);
	}
	Ar.Write(TEXT("barrels"), BarrelEntries);
	TArray<FString> Delivered;
	for (const TPair<FName, FSupplyCrateState>& Pair : Crates)
	{
		if (Pair.Value.bDelivered)
		{
			Delivered.Add(Pair.Key.ToString());
		}
	}
	Ar.Write(TEXT("cratesDelivered"), Delivered);
	TArray<FSaveArchive> DroppedEntries;
	for (const FIslandDroppedItem& Item : Dropped)
	{
		FSaveArchive& Entry = DroppedEntries.AddDefaulted_GetRef();
		Entry.Write(TEXT("item"), Item.ItemId);
		Entry.Write(TEXT("count"), Item.Count);
		Entry.Write(TEXT("location"), Item.Location);
	}
	Ar.Write(TEXT("dropped"), DroppedEntries);
	FSaveArchive RaftAr;
	ExploredSaveStates::SaveRaft(RaftAr, Raft);
	Ar.Write(TEXT("raft"), RaftAr);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (const UExploredSurvivorComponent* Survivor = Pawn ? Pawn->FindComponentByClass<UExploredSurvivorComponent>() : nullptr)
	{
		FSaveArchive Pack;
		ExploredSaveStates::SaveBackpack(Pack, Survivor->GetBackpack());
		Ar.Write(TEXT("backpack"), Pack);
	}
}

void UExploredIslandStateSubsystem::Load(const FSaveArchive& Ar)
{
	using namespace IslandStateDetail;
	Collected.Reset();
	PalmCoconuts.Reset();
	Barrels.Reset();
	Crates.Reset();
	ExploredSaveStates::LoadCollectedIds(Ar, Collected);
	FSaveArchive RaftAr;
	Raft = ExploredSaveStates::FRaftSaveData();
	if (Ar.Read(TEXT("raft"), RaftAr) && !ExploredSaveStates::LoadRaft(RaftAr, Raft))
	{
		UE_LOG(LogExplored, Warning, TEXT("[Isla] La balsa guardada no es coherente: queda por hacer"));
	}
	TArray<FString> Ids;
	TArray<FSaveArchive> Entries;
	Ar.Read(TEXT("palms"), Entries);
	for (const FSaveArchive& Entry : Entries)
	{
		FName Id;
		int32 Coconuts = 0;
		Entry.Read(TEXT("id"), Id);
		Entry.Read(TEXT("coconuts"), Coconuts);
		SetPalmCoconuts(Id, Coconuts);
	}
	Entries.Reset();
	Ar.Read(TEXT("barrels"), Entries);
	for (const FSaveArchive& Entry : Entries)
	{
		FName Id;
		FRainBarrelState State;
		Entry.Read(TEXT("id"), Id);
		Entry.Read(TEXT("charges"), State.Charges);
		Entry.Read(TEXT("rainSeconds"), State.RainSeconds);
		Barrels.Add(Id, FRainBarrelModel::AddCharges(FRainBarrelState{ 0, FMath::Max(0.0f, State.RainSeconds) }, State.Charges));
	}
	Ids.Reset();
	Ar.Read(TEXT("cratesDelivered"), Ids);
	for (const FString& Id : Ids)
	{
		Crates.Add(FName(*Id), FSupplyCrateState{ true });
	}

	for (const TWeakObjectPtr<AExploredPickup>& Weak : DroppedActors)
	{
		if (AExploredPickup* Pickup = Weak.Get())
		{
			Pickup->Destroy();
		}
	}
	Dropped.Reset();
	DroppedActors.Reset();
	Entries.Reset();
	Ar.Read(TEXT("dropped"), Entries);
	for (const FSaveArchive& Entry : Entries)
	{
		FIslandDroppedItem Item;
		Entry.Read(TEXT("item"), Item.ItemId);
		Entry.Read(TEXT("count"), Item.Count);
		Entry.Read(TEXT("location"), Item.Location);
		SpawnDropped(Item.ItemId, Item.Count, Item.Location);
	}

	FSaveArchive PackArchive;
	FBackpackState Pack;
	if (Ar.Read(TEXT("backpack"), PackArchive) && ExploredSaveStates::LoadBackpack(PackArchive, Pack))
	{
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
		UExploredSurvivorComponent* Survivor = Pawn ? Pawn->FindComponentByClass<UExploredSurvivorComponent>() : nullptr;
		if (Survivor)
		{
			Survivor->RestoreBackpack(Pack);
		}
		else
		{
			PendingBackpack = Pack;
		}
	}
	ApplyToWorld();
}
