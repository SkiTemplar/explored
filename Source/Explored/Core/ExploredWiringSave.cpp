// Secciones de guardado de UExploredWiringSubsystem (docs/tecnico/guardado.md): reloj, mochila,
// cuerpo, pesca y fuegos. El resto del subsistema está en ExploredWiringSubsystem.cpp.
#include "Core/ExploredWiringSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#include "Carry/CarryComponent.h"
#include "Cooking/ExploredFire.h"
#include "Explored.h"
#include "Fishing/ExploredFishingSubsystem.h"
#include "Items/ItemTypes.h"
#include "Player/ExploredCharacter.h"
#include "Save/SaveIslandSections.h"
#include "Save/SaveSystemStates.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/IslandBodyComponent.h"
#include "UI/ExploredSaveSubsystem.h"
#include "Weather/ExploredWeatherSubsystem.h"

namespace ExploredWiringSaveDetail
{
	const TCHAR* const SectionInventory = TEXT("inventory");
	const TCHAR* const SectionBody = TEXT("body");
	const TCHAR* const SectionFishing = TEXT("fishing");
	const TCHAR* const SectionCooking = TEXT("cooking");
	const TCHAR* const SectionTime = TEXT("time");

	/** Un fuego guardado se reconoce por su sitio con esta tolerancia (cm). */
	constexpr double FireMatchToleranceCm = 100.0;

	ExploredSaveStates::FSavedItemNode ToNode(const FItemInstance& Item)
	{
		ExploredSaveStates::FSavedItemNode Node;
		Node.DefinitionId = Item.DefinitionId;
		Node.Quality = Item.Quality;
		Node.Durability = Item.Durability;
		Node.Count = Item.Count;
		Node.LiquidLiters = Item.LiquidLiters;
		Node.GeneratedName = Item.GeneratedName.ToString();
		return Node;
	}

	FItemInstance FromNode(const ExploredSaveStates::FSavedItemNode& Node)
	{
		FItemInstance Item;
		Item.DefinitionId = Node.DefinitionId;
		Item.Quality = Node.Quality;
		Item.Durability = Node.Durability;
		Item.Count = Node.Count;
		Item.LiquidLiters = Node.LiquidLiters;
		// Texto ya formateado al fabricar (ver docs/tecnico/localizacion.md, nombres de los datos).
		Item.GeneratedName = Node.GeneratedName.IsEmpty() ? FText::GetEmpty() : FText::FromString(Node.GeneratedName);
		return Item;
	}

	/** Un objeto con sus piezas como lista aplanada. */
	void WriteItem(FSaveArchive& Ar, const TCHAR* Key, const FItemInstance& Item)
	{
		TArray<ExploredSaveStates::FSavedItemNode> Nodes;
		if (Item.IsValid())
		{
			ExploredSaveStates::FlattenItemTree(Item, &ToNode, Nodes);
		}
		ExploredSaveStates::SaveItemNodes(Ar, Key, Nodes);
	}

	FItemInstance ReadItem(const FSaveArchive& Ar, const TCHAR* Key)
	{
		TArray<ExploredSaveStates::FSavedItemNode> Nodes;
		ExploredSaveStates::LoadItemNodes(Ar, Key, Nodes);
		TArray<FItemInstance> Roots = ExploredSaveStates::RebuildItemTrees<FItemInstance>(Nodes, &FromNode);
		return Roots.Num() > 0 ? Roots[0] : FItemInstance();
	}
}

UExploredSaveSubsystem* UExploredWiringSubsystem::FindSaveSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
}

void UExploredWiringSubsystem::RegisterSections(UExploredSaveSubsystem& Save)
{
	using namespace ExploredWiringSaveDetail;
	const TWeakObjectPtr<UExploredWiringSubsystem> WeakThis(this);
	auto Register = [&Save, WeakThis](const TCHAR* Name, void (UExploredWiringSubsystem::*SaveFn)(FSaveArchive&) const,
		void (UExploredWiringSubsystem::*LoadFn)(const FSaveArchive&))
	{
		Save.RegisterSection(Name,
			[WeakThis, SaveFn](FSaveArchive& Ar)
			{
				if (const UExploredWiringSubsystem* Self = WeakThis.Get())
				{
					(Self->*SaveFn)(Ar);
				}
			},
			[WeakThis, LoadFn](const FSaveArchive& Ar)
			{
				if (UExploredWiringSubsystem* Self = WeakThis.Get())
				{
					(Self->*LoadFn)(Ar);
				}
			});
	};
	// El reloj primero: los demás sistemas miden el tiempo desde la hora cargada.
	Register(SectionTime, &UExploredWiringSubsystem::SaveClock, &UExploredWiringSubsystem::LoadClock);
	Register(SectionInventory, &UExploredWiringSubsystem::SaveInventory, &UExploredWiringSubsystem::LoadInventory);
	Register(SectionBody, &UExploredWiringSubsystem::SaveBody, &UExploredWiringSubsystem::LoadBody);
	Register(SectionFishing, &UExploredWiringSubsystem::SaveFishing, &UExploredWiringSubsystem::LoadFishing);
	Register(SectionCooking, &UExploredWiringSubsystem::SaveFires, &UExploredWiringSubsystem::LoadFires);
	bSectionsRegistered = true;
}

void UExploredWiringSubsystem::UnregisterSections(UExploredSaveSubsystem& Save)
{
	using namespace ExploredWiringSaveDetail;
	for (const TCHAR* Name : {SectionInventory, SectionBody, SectionFishing, SectionCooking, SectionTime})
	{
		Save.UnregisterSection(Name);
	}
}

// --- Reloj ----------------------------------------------------------------------------

void UExploredWiringSubsystem::SaveClock(FSaveArchive& Ar) const
{
	const UWorld* World = GetWorld();
	ExploredSaveStates::FSavedClock Clock;
	if (const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr)
	{
		Clock.Day = Time->GetDay();
		Clock.Hours = Time->GetHours();
	}
	if (const UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr)
	{
		Clock.ForcedWeather = Weather->GetForcedState(Clock.ForcedUntilDays);
	}
	ExploredSaveStates::SaveClock(Ar, Clock);
}

void UExploredWiringSubsystem::LoadClock(const FSaveArchive& Ar)
{
	if (Ar.IsEmpty())
	{
		return;
	}
	UWorld* World = GetWorld();
	ExploredSaveStates::FSavedClock Clock;
	ExploredSaveStates::LoadClock(Ar, Clock);
	if (UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr)
	{
		Time->SetTime(Clock.Day, Clock.Hours);
	}
	if (UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr)
	{
		Weather->RestoreForcedState(Clock.ForcedWeather, Clock.ForcedUntilDays);
	}
}

// --- Personaje: inventario y cuerpo ---------------------------------------

void UExploredWiringSubsystem::SaveInventory(FSaveArchive& Ar) const
{
	const AExploredCharacter* Character = GetPlayerCharacter();
	const UCarryComponent* Carry = Character ? Character->GetCarryComponent() : nullptr;
	if (!Carry)
	{
		// Sin personaje se conserva lo que llegó de la partida.
		if (PendingInventory.IsSet())
		{
			Ar = PendingInventory.GetValue();
		}
		return;
	}
	FInventoryState State;
	TMap<int64, FItemInstance> Instances;
	Carry->ExportState(State, Instances);
	FSaveArchive StateAr;
	ExploredSaveStates::SaveInventory(StateAr, State);
	Ar.Write(TEXT("state"), StateAr);
	TArray<FSaveArchive> Payloads;
	for (const TPair<int64, FItemInstance>& Entry : Instances)
	{
		FSaveArchive& Payload = Payloads.AddDefaulted_GetRef();
		Payload.Write(TEXT("id"), Entry.Key);
		ExploredWiringSaveDetail::WriteItem(Payload, TEXT("item"), Entry.Value);
	}
	Ar.Write(TEXT("payloads"), Payloads);
}

void UExploredWiringSubsystem::LoadInventory(const FSaveArchive& Ar)
{
	PendingInventory.Reset();
	if (Ar.IsEmpty())
	{
		return;
	}
	PendingInventory = Ar;
	ApplyPendingPawnSections();
}

void UExploredWiringSubsystem::SaveBody(FSaveArchive& Ar) const
{
	const AExploredCharacter* Character = GetPlayerCharacter();
	const UIslandBodyComponent* Body = Character ? Character->GetIslandBodyComponent() : nullptr;
	if (!Body)
	{
		if (PendingBody.IsSet())
		{
			Ar = PendingBody.GetValue();
		}
		return;
	}
	// Baliza Siete: las cuatro barras del GDD v3 §5.1 y el último refugio (desmayo).
	ExploredSaveStates::SaveBodyBars(Ar, Body->GetBars(), Body->GetLastShelterId());
}

void UExploredWiringSubsystem::LoadBody(const FSaveArchive& Ar)
{
	PendingBody.Reset();
	if (Ar.IsEmpty())
	{
		return;
	}
	PendingBody = Ar;
	ApplyPendingPawnSections();
}

void UExploredWiringSubsystem::ApplyPendingPawnSections()
{
	AExploredCharacter* Character = GetPlayerCharacter();
	if (!Character)
	{
		return;
	}
	if (PendingInventory.IsSet())
	{
		if (UCarryComponent* Carry = Character->GetCarryComponent())
		{
			const FSaveArchive& Ar = PendingInventory.GetValue();
			FInventoryState State;
			FSaveArchive StateAr;
			if (Ar.Read(TEXT("state"), StateAr))
			{
				ExploredSaveStates::LoadInventory(StateAr, State);
			}
			TMap<int64, FItemInstance> Instances;
			TArray<FSaveArchive> Payloads;
			Ar.Read(TEXT("payloads"), Payloads);
			for (const FSaveArchive& Payload : Payloads)
			{
				int64 Id = 0;
				const FItemInstance Item = ExploredWiringSaveDetail::ReadItem(Payload, TEXT("item"));
				if (Payload.Read(TEXT("id"), Id) && Id != 0 && Item.IsValid())
				{
					Instances.Add(Id, Item);
				}
			}
			Carry->ImportState(State, Instances);
		}
		PendingInventory.Reset();
	}
	if (PendingBody.IsSet())
	{
		if (UIslandBodyComponent* Body = Character->GetIslandBodyComponent())
		{
			FBodyBars Bars;
			FName Shelter;
			if (!ExploredSaveStates::LoadBodyBars(PendingBody.GetValue(), Bars, Shelter))
			{
				UE_LOG(LogExplored, Warning, TEXT("[Cuerpo] La sección «body» no se puede leer: cuerpo inicial"));
				Bars = Body->GetTuning().Initial;
			}
			Body->RestoreBars(Bars, Shelter);
		}
		PendingBody.Reset();
	}
}

// --- Pesca y fuegos ------------------------------------------------------------------

void UExploredWiringSubsystem::SaveFishing(FSaveArchive& Ar) const
{
	const UWorld* World = GetWorld();
	if (const UExploredFishingSubsystem* Fishing = World ? World->GetSubsystem<UExploredFishingSubsystem>() : nullptr)
	{
		ExploredSaveStates::SaveFishing(Ar, Fishing->GetState());
	}
}

void UExploredWiringSubsystem::LoadFishing(const FSaveArchive& Ar)
{
	UWorld* World = GetWorld();
	UExploredFishingSubsystem* Fishing = World ? World->GetSubsystem<UExploredFishingSubsystem>() : nullptr;
	if (!Fishing || Ar.IsEmpty())
	{
		return;
	}
	ExploredSaveStates::LoadFishing(Ar, Fishing->GetState());
}

void UExploredWiringSubsystem::SaveFires(FSaveArchive& Ar) const
{
	TArray<FSaveArchive> Fires;
	for (TActorIterator<AExploredFire> It(GetWorld()); It; ++It)
	{
		const FCampfireState& Fire = It->GetCampfireState();
		FSaveArchive& Entry = Fires.AddDefaulted_GetRef();
		Entry.Write(TEXT("location"), It->GetActorLocation());
		Entry.Write(TEXT("lit"), Fire.bLit);
		Entry.Write(TEXT("burnSeconds"), Fire.BurnSeconds);
		Entry.Write(TEXT("jobRecipe"), Fire.Job.RecipeId);
		Entry.Write(TEXT("jobItem"), Fire.Job.OutputItem);
		Entry.Write(TEXT("jobCount"), Fire.Job.OutputCount);
		Entry.Write(TEXT("jobSeconds"), Fire.Job.RemainingSeconds);
	}
	Ar.Write(TEXT("campfires"), Fires);
}

void UExploredWiringSubsystem::LoadFires(const FSaveArchive& Ar)
{
	UWorld* World = GetWorld();
	TArray<FSaveArchive> Fires;
	if (!World || !Ar.Read(TEXT("campfires"), Fires))
	{
		return;
	}
	TArray<AExploredFire*> Available;
	for (TActorIterator<AExploredFire> It(World); It; ++It)
	{
		Available.Add(*It);
	}
	for (const FSaveArchive& Entry : Fires)
	{
		FVector Location = FVector::ZeroVector;
		FCampfireState Fire;
		Entry.Read(TEXT("location"), Location);
		Entry.Read(TEXT("lit"), Fire.bLit);
		Entry.Read(TEXT("burnSeconds"), Fire.BurnSeconds);
		Entry.Read(TEXT("jobRecipe"), Fire.Job.RecipeId);
		Entry.Read(TEXT("jobItem"), Fire.Job.OutputItem);
		Entry.Read(TEXT("jobCount"), Fire.Job.OutputCount);
		Entry.Read(TEXT("jobSeconds"), Fire.Job.RemainingSeconds);
		// Entrada validada: una partida editada a mano no puede dejar un fuego con tiempos
		// negativos o no finitos, ni fuera del mundo.
		Fire.BurnSeconds = FMath::IsFinite(Fire.BurnSeconds) ? FMath::Max(0.0f, Fire.BurnSeconds) : 0.0f;
		Fire.Job.RemainingSeconds = FMath::IsFinite(Fire.Job.RemainingSeconds) ? FMath::Max(0.0f, Fire.Job.RemainingSeconds) : 0.0f;
		if (Location.ContainsNaN() || Location.GetAbsMax() > UE_LARGE_WORLD_MAX)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Guardado] Fuego con posición imposible: se descarta"));
			continue;
		}
		// El fuego del mapa que está en ese sitio; si no hay (se encendió en partida), se crea.
		AExploredFire* Target = nullptr;
		for (int32 I = 0; I < Available.Num(); ++I)
		{
			if (FVector::Dist(Available[I]->GetActorLocation(), Location) <= ExploredWiringSaveDetail::FireMatchToleranceCm)
			{
				Target = Available[I];
				Available.RemoveAt(I);
				break;
			}
		}
		if (!Target)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Target = World->SpawnActor<AExploredFire>(AExploredFire::StaticClass(), Location, FRotator::ZeroRotator, Params);
		}
		if (Target)
		{
			Target->RestoreCampfire(Fire);
		}
	}
}
