#include "Ruins/RuinsSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/HitResult.h"
#include "EngineUtils.h"
#include "Explored.h"
#include "Ruins/ExploredRuinElement.h"
#include "Save/SaveSystemStates.h"
#include "Subsystems/SubsystemCollection.h"
#include "UI/ExploredSaveSubsystem.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainDensity.h"

namespace RuinsSubsystemDetail
{
	/** Sección de la partida (docs/tecnico/guardado.md). */
	const TCHAR* const SaveSection = TEXT("ruins");
	/** Altura desde la que se busca el suelo al asentar un elemento (cm). */
	constexpr double GroundTraceUpCm = 20000.0;
	constexpr double GroundTraceDownCm = 40000.0;

	UExploredSaveSubsystem* FindSave(const UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
	}

	FString DataFilePath(const TCHAR* FileName)
	{
		return FPaths::ProjectContentDir() / TEXT("Data") / FileName;
	}

	bool ParseRoot(const FString& JsonText, TSharedPtr<FJsonObject>& OutRoot, FString& OutError, const TCHAR* FileName)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
		{
			OutError = FString::Printf(TEXT("%s no es un JSON válido"), FileName);
			return false;
		}
		return true;
	}

	/** Lee «id», «nameEs» y «nameEn» de cada entrada de un array y los guarda en los mapas de nombres. */
	void ReadNames(const TSharedPtr<FJsonObject>& Root, const TCHAR* Field, TMap<FName, FString>& OutEs, TMap<FName, FString>& OutEn)
	{
		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		if (!Root->TryGetArrayField(Field, Entries) || !Entries)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& Entry : *Entries)
		{
			const TSharedPtr<FJsonObject> Obj = Entry.IsValid() ? Entry->AsObject() : nullptr;
			FString Id;
			if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
			{
				continue;
			}
			FString Es;
			FString En;
			Obj->TryGetStringField(TEXT("nameEs"), Es);
			Obj->TryGetStringField(TEXT("nameEn"), En);
			OutEs.Add(FName(*Id), Es);
			OutEn.Add(FName(*Id), En.IsEmpty() ? Es : En);
		}
	}
}

bool URuinsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URuinsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UExploredSaveSubsystem* Save = RuinsSubsystemDetail::FindSave(GetWorld()))
	{
		TWeakObjectPtr<URuinsSubsystem> WeakThis(this);
		Save->RegisterSection(RuinsSubsystemDetail::SaveSection,
			[WeakThis](FSaveArchive& Ar)
			{
				if (const URuinsSubsystem* Self = WeakThis.Get())
				{
					ExploredSaveStates::SaveRuins(Ar, Self->GetRuinsState(), Self->GetMuseumState());
				}
			},
			[WeakThis](const FSaveArchive& Ar)
			{
				if (URuinsSubsystem* Self = WeakThis.Get())
				{
					FRuinsState RuinsState;
					FMuseumState MuseumState;
					ExploredSaveStates::LoadRuins(Ar, RuinsState, MuseumState);
					Self->LoadSavedState(RuinsState, MuseumState);
				}
			});
	}
}

void URuinsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	LoadDataFiles();
	BuildForSeed(FArchipelagoLayout::OfficialSeed);
	SpawnMissingElementActors(InWorld);
}

void URuinsSubsystem::SpawnMissingElementActors(UWorld& World)
{
	TSet<FName> Placed;
	for (TActorIterator<AExploredRuinElement> It(&World); It; ++It)
	{
		Placed.Add(It->GetElementId());
	}
	int32 Spawned = 0;
	for (const FRuinSite& Site : Ruins.GetLayout().Sites)
	{
		for (const FRuinElement& Element : Site.Elements)
		{
			if (Placed.Contains(Element.Id))
			{
				continue;
			}
			// El modelo trabaja en metros; el suelo real lo da una traza vertical.
			FVector Location = Element.Location * 100.0;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ExploredRuinElementGround), false);
			const FVector Start(Location.X, Location.Y, Location.Z + RuinsSubsystemDetail::GroundTraceUpCm);
			const FVector End(Location.X, Location.Y, Location.Z - RuinsSubsystemDetail::GroundTraceDownCm);
			if (World.LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
			{
				Location = Hit.ImpactPoint;
			}
			const FTransform Transform(FRotator(0.0, Element.Yaw, 0.0), Location);
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.bDeferConstruction = true;
			if (AExploredRuinElement* Actor = World.SpawnActor<AExploredRuinElement>(AExploredRuinElement::StaticClass(), Transform, SpawnParams))
			{
				Actor->Configure(Element.Id, Element.Kind);
				Actor->FinishSpawning(Transform);
				++Spawned;
			}
		}
	}
	UE_LOG(LogExplored, Display, TEXT("Ruinas: %d elementos creados en el mundo"), Spawned);
}

void URuinsSubsystem::Deinitialize()
{
	if (UExploredSaveSubsystem* Save = RuinsSubsystemDetail::FindSave(GetWorld()))
	{
		Save->UnregisterSection(RuinsSubsystemDetail::SaveSection);
	}
	OnRuinDiscovery.Clear();
	OnMuseumChanged.Clear();
	Super::Deinitialize();
}

void URuinsSubsystem::LoadDataFiles()
{
	using namespace RuinsSubsystemDetail;

	NamesEs.Reset();
	NamesEn.Reset();
	FString Text;
	FString Error;
	if (FFileHelper::LoadFileToString(Text, *DataFilePath(TEXT("artifacts.json"))))
	{
		FTreasureCatalog Loaded;
		if (ParseArtifactsJson(Text, Loaded, NamesEs, NamesEn, Error))
		{
			Catalog = MoveTemp(Loaded);
		}
		else
		{
			UE_LOG(LogExplored, Error, TEXT("Ruinas: %s"), *Error);
		}
	}
	else
	{
		UE_LOG(LogExplored, Warning, TEXT("Ruinas: no se encuentra Content/Data/artifacts.json"));
	}
	if (FFileHelper::LoadFileToString(Text, *DataFilePath(TEXT("ruins.json"))))
	{
		if (!ParseRuinsJson(Text, NamesEs, NamesEn, Error))
		{
			UE_LOG(LogExplored, Error, TEXT("Ruinas: %s"), *Error);
		}
	}
}

void URuinsSubsystem::BuildForSeed(uint32 Seed)
{
	const double Start = FPlatformTime::Seconds();
	// Mismo cálculo que el horneado del mundo: la disposición y los puntos de interés son deterministas.
	const FTerrainDensity Density(FArchipelagoLayout::Generate(Seed));
	Pois = FPoiLayout::Generate(Density);
	FRuinsLayout Layout = FRuinsLayout::Generate(Density.GetLayout(), Pois);
	Placements = FTreasurePlacement::Generate(Layout, Pois, Catalog);
	Ruins = FRuinsModel(MoveTemp(Layout));
	Museum = FMuseumModel(Catalog);
	UE_LOG(LogExplored, Display, TEXT("Ruinas: %d ruinas y %d tesoros en %.2f s"), Ruins.GetLayout().Sites.Num(), Placements.Num(),
		FPlatformTime::Seconds() - Start);
	OnMuseumChanged.Broadcast();
}

FRuinDiscovery URuinsSubsystem::NotifyElementDiscovered(FName ElementId)
{
	const FRuinDiscovery Result = Ruins.Discover(ElementId);
	if (Result.bNew)
	{
		OnRuinDiscovery.Broadcast(ElementId, Result);
	}
	return Result;
}

bool URuinsSubsystem::NotifyArtifactFound(FName ArtifactId)
{
	const FArtifactPlacement* Placement = Placements.FindByPredicate([ArtifactId](const FArtifactPlacement& P) { return P.ArtifactId == ArtifactId; });
	const bool bNew = Museum.MarkFound(ArtifactId, Placement ? Placement->PlaceId : FName(), Placement ? Placement->IslandIndex : INDEX_NONE);
	if (bNew)
	{
		OnMuseumChanged.Broadcast();
	}
	return bNew;
}

bool URuinsSubsystem::NotifyArtifactPhotographed(FName ArtifactId)
{
	const bool bNew = Museum.MarkPhotographed(ArtifactId);
	if (bNew)
	{
		OnMuseumChanged.Broadcast();
	}
	return bNew;
}

EMuseumResult URuinsSubsystem::RegisterDisplay(int32 Key, FName DisplayId)
{
	const EMuseumResult Result = Museum.AddDisplay(Key, DisplayId);
	if (Result == EMuseumResult::Ok)
	{
		OnMuseumChanged.Broadcast();
	}
	return Result;
}

EMuseumResult URuinsSubsystem::UnregisterDisplay(int32 Key, TArray<FName>& OutReturned)
{
	const EMuseumResult Result = Museum.RemoveDisplay(Key, OutReturned);
	if (Result == EMuseumResult::Ok)
	{
		OnMuseumChanged.Broadcast();
	}
	return Result;
}

EMuseumResult URuinsSubsystem::PlaceArtifact(FName ArtifactId, int32 Key, int32 Slot)
{
	const EMuseumResult Result = Museum.Place(ArtifactId, Key, Slot);
	if (Result == EMuseumResult::Ok)
	{
		OnMuseumChanged.Broadcast();
	}
	return Result;
}

EMuseumResult URuinsSubsystem::RemoveArtifact(int32 Key, int32 Slot, FName& OutArtifact)
{
	const EMuseumResult Result = Museum.Remove(Key, Slot, OutArtifact);
	if (Result == EMuseumResult::Ok)
	{
		OnMuseumChanged.Broadcast();
	}
	return Result;
}

FString URuinsSubsystem::GetDisplayName(FName Id, bool bEnglish) const
{
	const FString* Name = (bEnglish ? NamesEn : NamesEs).Find(Id);
	return Name ? *Name : Id.ToString();
}

void URuinsSubsystem::LoadSavedState(const FRuinsState& RuinsState, const FMuseumState& MuseumState)
{
	Ruins.LoadState(RuinsState);
	Museum.LoadState(MuseumState);
	OnMuseumChanged.Broadcast();
}

bool URuinsSubsystem::ParseArtifactsJson(const FString& JsonText, FTreasureCatalog& OutCatalog, TMap<FName, FString>& OutNamesEs,
	TMap<FName, FString>& OutNamesEn, FString& OutError)
{
	using namespace RuinsSubsystemDetail;

	TSharedPtr<FJsonObject> Root;
	if (!ParseRoot(JsonText, Root, OutError, TEXT("artifacts.json")))
	{
		return false;
	}
	OutCatalog = FTreasureCatalog();

	const TArray<TSharedPtr<FJsonValue>>* Artifacts = nullptr;
	if (!Root->TryGetArrayField(TEXT("artifacts"), Artifacts) || !Artifacts)
	{
		OutError = TEXT("artifacts.json sin «artifacts»");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Entry : *Artifacts)
	{
		const TSharedPtr<FJsonObject> Obj = Entry.IsValid() ? Entry->AsObject() : nullptr;
		FString Id;
		if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
		{
			OutError = TEXT("Tesoro sin «id» en artifacts.json");
			return false;
		}
		FArtifactDef Def;
		Def.Id = FName(*Id);
		FString Kind, Provenance, Rarity, Size, Mesh;
		Obj->TryGetStringField(TEXT("kind"), Kind);
		Obj->TryGetStringField(TEXT("nameEs"), Def.NameEs);
		Obj->TryGetStringField(TEXT("nameEn"), Def.NameEn);
		Obj->TryGetStringField(TEXT("provenance"), Provenance);
		Obj->TryGetStringField(TEXT("rarity"), Rarity);
		Obj->TryGetStringField(TEXT("size"), Size);
		Obj->TryGetStringField(TEXT("mesh"), Mesh);
		if (!FTreasureCatalog::ParseProvenance(Provenance, Def.Provenance) || !FTreasureCatalog::ParseRarity(Rarity, Def.Rarity)
			|| !FTreasureCatalog::ParseSize(Size, Def.Size))
		{
			OutError = FString::Printf(TEXT("Tesoro «%s»: procedencia, rareza o tamaño no válidos"), *Id);
			return false;
		}
		Def.Kind = FName(*Kind);
		Def.Mesh = Mesh.IsEmpty() ? FName() : FName(*Mesh);
		OutNamesEs.Add(Def.Id, Def.NameEs);
		OutNamesEn.Add(Def.Id, Def.NameEn.IsEmpty() ? Def.NameEs : Def.NameEn);
		OutCatalog.Artifacts.Add(MoveTemp(Def));
	}

	const TArray<TSharedPtr<FJsonValue>>* Displays = nullptr;
	if (Root->TryGetArrayField(TEXT("displays"), Displays) && Displays)
	{
		for (const TSharedPtr<FJsonValue>& Entry : *Displays)
		{
			const TSharedPtr<FJsonObject> Obj = Entry.IsValid() ? Entry->AsObject() : nullptr;
			FString Id;
			if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
			{
				continue;
			}
			FDisplayDef Def;
			Def.Id = FName(*Id);
			const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
			if (Obj->TryGetArrayField(TEXT("slots"), Slots) && Slots)
			{
				for (const TSharedPtr<FJsonValue>& SlotValue : *Slots)
				{
					const TSharedPtr<FJsonObject> SlotObj = SlotValue.IsValid() ? SlotValue->AsObject() : nullptr;
					if (!SlotObj.IsValid())
					{
						continue;
					}
					FDisplaySlotDef Slot;
					FString MaxSize;
					SlotObj->TryGetStringField(TEXT("maxSize"), MaxSize);
					FTreasureCatalog::ParseSize(MaxSize, Slot.MaxSize);
					const TArray<TSharedPtr<FJsonValue>>* Offset = nullptr;
					if (SlotObj->TryGetArrayField(TEXT("offsetCm"), Offset) && Offset && Offset->Num() == 3)
					{
						Slot.Offset = FVector((*Offset)[0]->AsNumber(), (*Offset)[1]->AsNumber(), (*Offset)[2]->AsNumber());
					}
					Def.Slots.Add(Slot);
				}
			}
			OutCatalog.Displays.Add(MoveTemp(Def));
		}
	}
	ReadNames(Root, TEXT("displays"), OutNamesEs, OutNamesEn);
	ReadNames(Root, TEXT("provenances"), OutNamesEs, OutNamesEn);
	ReadNames(Root, TEXT("rarities"), OutNamesEs, OutNamesEn);
	return true;
}

bool URuinsSubsystem::ParseRuinsJson(const FString& JsonText, TMap<FName, FString>& OutNamesEs, TMap<FName, FString>& OutNamesEn,
	FString& OutError)
{
	using namespace RuinsSubsystemDetail;

	TSharedPtr<FJsonObject> Root;
	if (!ParseRoot(JsonText, Root, OutError, TEXT("ruins.json")))
	{
		return false;
	}
	ReadNames(Root, TEXT("sites"), OutNamesEs, OutNamesEn);
	ReadNames(Root, TEXT("techniques"), OutNamesEs, OutNamesEn);
	ReadNames(Root, TEXT("elements"), OutNamesEs, OutNamesEn);

	int32 RequiredStarPaths = 0;
	if (Root->TryGetNumberField(TEXT("requiredStarPaths"), RequiredStarPaths) && RequiredStarPaths != FRuinsLayout::RequiredStarPaths)
	{
		OutError = FString::Printf(TEXT("ruins.json pide %d caminos de estrellas y el código %d"), RequiredStarPaths, FRuinsLayout::RequiredStarPaths);
		return false;
	}
	return true;
}
