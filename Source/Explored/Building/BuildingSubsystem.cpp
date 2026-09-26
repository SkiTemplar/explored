#include "Building/BuildingSubsystem.h"

#include "CollisionQueryParams.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Achievements/AchievementsSubsystem.h"
#include "Building/ExploredBuildingPiece.h"
#include "Engine/GameInstance.h"
#include "Items/ItemRegistrySubsystem.h"
#include "Save/SaveSystemStates.h"
#include "UI/ExploredSaveSubsystem.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Weather/ExploredWeatherSubsystem.h"

#define LOCTEXT_NAMESPACE "ExploredBuilding"

namespace BuildingSubsystemDetail
{
	/** Tope de horas por paso (saltos de tiempo al dormir o al cargar). */
	constexpr float MaxHoursPerStep = 48.0f;

	/** Sección de la partida (docs/tecnico/guardado.md). */
	const TCHAR* const SaveSection = TEXT("building");

	UExploredSaveSubsystem* FindSave(const UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
	}

	void ReadNameArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, TArray<FName>& Out)
	{
		TArray<FString> Strings;
		if (Obj->TryGetStringArrayField(Field, Strings))
		{
			for (const FString& Value : Strings)
			{
				Out.Add(FName(*Value));
			}
		}
	}

	bool ParseTier(const TSharedPtr<FJsonObject>& Obj, FBuildingTierDef& Out, FString& OutError)
	{
		FString Id;
		if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
		{
			OutError = TEXT("Nivel sin \"id\" en building_pieces.json");
			return false;
		}
		Out.Id = FName(*Id);
		int32 Order = 0;
		Obj->TryGetNumberField(TEXT("order"), Order);
		Out.Order = Order;
		Obj->TryGetStringField(TEXT("nameEs"), Out.NameEs);
		ReadNameArray(Obj, TEXT("requiresTools"), Out.RequiresTools);
		return true;
	}

	bool ParsePiece(const TSharedPtr<FJsonObject>& Obj, FBuildingPieceDef& Out, FString& OutError)
	{
		FString Id;
		if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
		{
			OutError = TEXT("Pieza sin \"id\" en building_pieces.json");
			return false;
		}
		Out.Id = FName(*Id);
		Obj->TryGetStringField(TEXT("nameEs"), Out.NameEs);

		FString Tier, Category, Socket, Mesh;
		Obj->TryGetStringField(TEXT("tier"), Tier);
		Obj->TryGetStringField(TEXT("category"), Category);
		Out.Tier = FName(*Tier);
		Out.Category = FName(*Category);
		if (!Obj->TryGetStringField(TEXT("socket"), Socket) || !ParseBuildSocket(Socket, Out.Socket))
		{
			OutError = FString::Printf(TEXT("Pieza \"%s\" con socket \"%s\" desconocido"), *Id, *Socket);
			return false;
		}
		// "mesh": null en las pendientes: TryGetStringField falla y queda NAME_None.
		if (Obj->TryGetStringField(TEXT("mesh"), Mesh) && !Mesh.IsEmpty())
		{
			Out.Mesh = FName(*Mesh);
		}

		const TArray<TSharedPtr<FJsonValue>>* CostArray = nullptr;
		if (Obj->TryGetArrayField(TEXT("cost"), CostArray) && CostArray)
		{
			for (const TSharedPtr<FJsonValue>& Entry : *CostArray)
			{
				const TSharedPtr<FJsonObject> CostObj = Entry.IsValid() ? Entry->AsObject() : nullptr;
				FString Item;
				int32 Count = 0;
				if (CostObj.IsValid() && CostObj->TryGetStringField(TEXT("item"), Item) && CostObj->TryGetNumberField(TEXT("count"), Count))
				{
					Out.Cost.Add({FName(*Item), Count});
				}
			}
		}
		ReadNameArray(Obj, TEXT("tools"), Out.Tools);
		ReadNameArray(Obj, TEXT("requiresPieces"), Out.RequiresPieces);

		float Minutes = Out.BuildMinutes, Integrity = Out.Integrity;
		int32 Cyclone = 0;
		bool bRespawn = false;
		Obj->TryGetNumberField(TEXT("buildMinutes"), Minutes);
		Obj->TryGetNumberField(TEXT("integrity"), Integrity);
		Obj->TryGetNumberField(TEXT("maxCycloneCategory"), Cyclone);
		Obj->TryGetBoolField(TEXT("respawnPoint"), bRespawn);
		Out.BuildMinutes = Minutes;
		Out.Integrity = Integrity;
		Out.MaxCycloneCategory = Cyclone;
		Out.bRespawnPoint = bRespawn;
		return true;
	}

	/** Las fogatas y el terreno siguen la forma del suelo: se bajan hasta el terreno real. */
	FVector DropToGround(const UWorld* World, const FVector& Location)
	{
		if (!World)
		{
			return Location;
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ExploredBuildingGround), false);
		const FVector Start = Location + FVector(0.0, 0.0, 150.0);
		const FVector End = Location - FVector(0.0, 0.0, 300.0);
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params) &&
			!Cast<AExploredBuildingPiece>(Hit.GetActor()))
		{
			return Hit.ImpactPoint;
		}
		return Location;
	}
}

bool UBuildingSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBuildingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UTimeOfDaySubsystem>();
	Collection.InitializeDependency<UExploredWeatherSubsystem>();
	Model = MakeUnique<FBuildingModel>(FBuildingCatalog());
	ReloadFromDisk();

	if (UExploredSaveSubsystem* Save = BuildingSubsystemDetail::FindSave(GetWorld()))
	{
		TWeakObjectPtr<UBuildingSubsystem> WeakThis(this);
		Save->RegisterSection(BuildingSubsystemDetail::SaveSection,
			[WeakThis](FSaveArchive& Ar)
			{
				if (const UBuildingSubsystem* Self = WeakThis.Get())
				{
					ExploredSaveStates::SaveBuilding(Ar, Self->GetSaveState());
				}
			},
			[WeakThis](const FSaveArchive& Ar)
			{
				if (UBuildingSubsystem* Self = WeakThis.Get())
				{
					FBuildingSaveState State;
					ExploredSaveStates::LoadBuilding(Ar, State);
					Self->RestoreSaveState(State);
				}
			});
	}
}

void UBuildingSubsystem::Deinitialize()
{
	if (UExploredSaveSubsystem* Save = BuildingSubsystemDetail::FindSave(GetWorld()))
	{
		Save->UnregisterSection(BuildingSubsystemDetail::SaveSection);
	}
	Actors.Empty();
	Model.Reset();
	Super::Deinitialize();
}

TStatId UBuildingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBuildingSubsystem, STATGROUP_Tickables);
}

bool UBuildingSubsystem::ParseBuildingJson(const FString& JsonText, FBuildingCatalog& OutCatalog, FString& OutError)
{
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON de building_pieces.json mal formado");
		return false;
	}

	FBuildingCatalog Catalog;
	const TArray<TSharedPtr<FJsonValue>>* TierArray = nullptr;
	if (Root->TryGetArrayField(TEXT("tiers"), TierArray) && TierArray)
	{
		for (const TSharedPtr<FJsonValue>& Entry : *TierArray)
		{
			FBuildingTierDef Tier;
			if (!BuildingSubsystemDetail::ParseTier(Entry.IsValid() ? Entry->AsObject() : nullptr, Tier, OutError))
			{
				return false;
			}
			Catalog.Tiers.Add(Tier);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* PieceArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("pieces"), PieceArray) || !PieceArray)
	{
		OutError = TEXT("building_pieces.json sin \"pieces\"");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Entry : *PieceArray)
	{
		FBuildingPieceDef Piece;
		if (!BuildingSubsystemDetail::ParsePiece(Entry.IsValid() ? Entry->AsObject() : nullptr, Piece, OutError))
		{
			return false;
		}
		if (Catalog.FindPiece(Piece.Id))
		{
			OutError = FString::Printf(TEXT("Id de pieza duplicado: %s"), *Piece.Id.ToString());
			return false;
		}
		Catalog.Pieces.Add(Piece);
	}
	OutCatalog = MoveTemp(Catalog);
	return true;
}

bool UBuildingSubsystem::ReloadFromDisk()
{
	const FString Path = UItemRegistrySubsystem::GetDataFilePath(TEXT("building_pieces.json"));
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		UE_LOG(LogTemp, Error, TEXT("[Explored] No se encontró %s"), *Path);
		return false;
	}
	FBuildingCatalog Catalog;
	FString Error;
	if (!ParseBuildingJson(Json, Catalog, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("[Explored] building_pieces.json inválido: %s"), *Error);
		return false;
	}

	// Conserva lo construido: se guarda el estado y se vuelve a cargar con el catálogo nuevo.
	const FBuildingSaveState Previous = Model ? Model->SaveState() : FBuildingSaveState();
	Model = MakeUnique<FBuildingModel>(MoveTemp(Catalog));
	RestoreSaveState(Previous);
	return true;
}

FText UBuildingSubsystem::GetReasonText(EBuildFailReason Reason)
{
	switch (Reason)
	{
	case EBuildFailReason::None: return FText::GetEmpty();
	case EBuildFailReason::UnknownPiece: return LOCTEXT("UnknownPiece", "Pieza desconocida");
	case EBuildFailReason::InvalidRotation: return LOCTEXT("InvalidRotation", "Giro no válido");
	case EBuildFailReason::UnknownBase: return LOCTEXT("UnknownBase", "Fuera de la base");
	case EBuildFailReason::Occupied: return LOCTEXT("Occupied", "Ya hay algo ahí");
	case EBuildFailReason::NoSupport: return LOCTEXT("NoSupport", "No tiene dónde apoyarse");
	case EBuildFailReason::Unstable: return LOCTEXT("Unstable", "Demasiado lejos de un apoyo");
	case EBuildFailReason::NeedsGround: return LOCTEXT("NeedsGround", "Tiene que ir sobre el terreno");
	case EBuildFailReason::MissingRequiredPiece: return LOCTEXT("MissingRequiredPiece", "Falta otra pieza en la base");
	case EBuildFailReason::MissingTools: return LOCTEXT("MissingTools", "Falta una herramienta");
	case EBuildFailReason::MissingMaterials: return LOCTEXT("MissingMaterials", "Faltan materiales");
	case EBuildFailReason::NothingToRepair: return LOCTEXT("NothingToRepair", "No hay nada que reparar");
	case EBuildFailReason::UnknownInstance: return LOCTEXT("UnknownInstance", "Esa pieza ya no existe");
	default: return FText::GetEmpty();
	}
}

EBuildFailReason UBuildingSubsystem::PreviewPlacement(FName DefId, const FVector& AimPoint, int32 Rotation, bool bGroundContact,
	FVector& OutLocation, float& OutYawDegrees) const
{
	OutLocation = AimPoint;
	OutYawDegrees = 0.0f;
	if (!Model)
	{
		return EBuildFailReason::UnknownPiece;
	}
	const FBuildingPieceDef* Def = Model->GetCatalog().FindPiece(DefId);
	if (!Def)
	{
		return EBuildFailReason::UnknownPiece;
	}

	const int32 BaseId = Model->FindBaseAt(AimPoint);
	if (BaseId == INDEX_NONE)
	{
		// Primera pieza de una base nueva: celda (0,0) en la rejilla global, en el terreno.
		OutLocation = FVector(
			FMath::RoundToDouble(AimPoint.X / FBuildingModel::CellSizeCm) * FBuildingModel::CellSizeCm,
			FMath::RoundToDouble(AimPoint.Y / FBuildingModel::CellSizeCm) * FBuildingModel::CellSizeCm,
			AimPoint.Z);
		OutYawDegrees = static_cast<float>(((Rotation % 4) + 4) % 4 * 90);
		if (Def->Socket == EBuildSocket::Roof)
		{
			return EBuildFailReason::NoSupport;
		}
		if (!bGroundContact)
		{
			return (Def->Socket == EBuildSocket::Pillar || Def->Socket == EBuildSocket::GroundOnly)
				? EBuildFailReason::NeedsGround : EBuildFailReason::NoSupport;
		}
		return Def->RequiresPieces.Num() > 0 ? EBuildFailReason::MissingRequiredPiece : EBuildFailReason::None;
	}

	FBuildingPlaceRequest Request;
	Request.DefId = DefId;
	Request.Placement = Model->SnapToGrid(BaseId, AimPoint, Def->Socket, Rotation);
	Request.bGroundContact = bGroundContact;
	Model->PlacementToWorld(Request.Placement, Def->Socket, OutLocation, OutYawDegrees);
	return Model->CanPlace(Request);
}

int32 UBuildingSubsystem::TryPlacePiece(FName DefId, const FVector& AimPoint, int32 Rotation, bool bGroundContact,
	TMap<FName, int32>& Inventory, const TSet<FName>& HeldTools, EBuildFailReason& OutReason)
{
	OutReason = EBuildFailReason::UnknownPiece;
	if (!Model)
	{
		return INDEX_NONE;
	}
	const FBuildingPieceDef* Def = Model->GetCatalog().FindPiece(DefId);
	if (!Def)
	{
		return INDEX_NONE;
	}

	// No se funda una base para una pieza que no se puede pagar o que no se sostendría.
	FVector Location;
	float Yaw = 0.0f;
	OutReason = PreviewPlacement(DefId, AimPoint, Rotation, bGroundContact, Location, Yaw);
	if (OutReason == EBuildFailReason::None)
	{
		OutReason = Model->CanAfford(DefId, Inventory, HeldTools);
	}
	if (OutReason != EBuildFailReason::None)
	{
		return INDEX_NONE;
	}

	FBuildingPlaceRequest Request;
	Request.DefId = DefId;
	const int32 BaseId = Model->FindOrCreateBase(AimPoint);
	Request.Placement = Model->SnapToGrid(BaseId, AimPoint, Def->Socket, Rotation);
	Request.bGroundContact = bGroundContact;
	int32 PieceId = INDEX_NONE;
	OutReason = Model->TryPlace(Request, Inventory, HeldTools, PieceId);
	if (OutReason == EBuildFailReason::None)
	{
		if (const FBuildingPieceState* Piece = Model->FindPiece(PieceId))
		{
			SpawnActorFor(*Piece);
		}
		// Estadísticas (docs/tecnico/estadisticas.md): pieza construida y nivel de material.
		if (UAchievementsSubsystem* Achievements = UAchievementsSubsystem::Get(this))
		{
			Achievements->ReportStatItem(TEXT("building_pieces_built"), DefId);
			Achievements->ReportStat(TEXT("building_tier_max"), Model->GetCatalog().TierOrderOf(*Def));
		}
	}
	return PieceId;
}

void UBuildingSubsystem::RemovePiece(int32 PieceId)
{
	if (!Model || !Model->FindPiece(PieceId))
	{
		return;
	}
	FBuildingChangeResult Change;
	Change.Destroyed.Add(PieceId);
	Change.Collapsed = Model->RemovePiece(PieceId);
	HandleChange(Change);
}

bool UBuildingSubsystem::SetFireLit(int32 PieceId, bool bLit)
{
	return Model && Model->SetLit(PieceId, bLit);
}

bool UBuildingSubsystem::FindRespawnPoint(const FVector& From, FVector& OutLocation) const
{
	int32 PieceId = INDEX_NONE;
	if (!Model || !Model->FindNearestRespawnPoint(From, PieceId, OutLocation))
	{
		return false;
	}
	if (const AExploredBuildingPiece* Actor = FindActor(PieceId))
	{
		OutLocation = Actor->GetActorLocation();
	}
	return true;
}

AExploredBuildingPiece* UBuildingSubsystem::FindActor(int32 PieceId) const
{
	const TObjectPtr<AExploredBuildingPiece>* Found = Actors.Find(PieceId);
	return Found ? Found->Get() : nullptr;
}

FBuildingSaveState UBuildingSubsystem::GetSaveState() const
{
	return Model ? Model->SaveState() : FBuildingSaveState();
}

void UBuildingSubsystem::RestoreSaveState(const FBuildingSaveState& State)
{
	if (!Model)
	{
		return;
	}
	DestroyAllActors();
	int32 Dropped = 0;
	if (!Model->LoadState(State, &Dropped))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Explored] Construcción: %d piezas descartadas al cargar"), Dropped);
	}
	for (const FBuildingPieceState& Piece : Model->GetPieces())
	{
		SpawnActorFor(Piece);
	}
	LastSimulatedDays = -1.0f;
}

void UBuildingSubsystem::Tick(float DeltaTime)
{
	if (!Model)
	{
		return;
	}
	SimulationTimer -= DeltaTime;
	if (SimulationTimer > 0.0f)
	{
		return;
	}
	SimulationTimer = FMath::Max(SimulationIntervalSeconds, 0.1f);

	const float Days = GetGameDays();
	if (LastSimulatedDays < 0.0f || Days < LastSimulatedDays)
	{
		LastSimulatedDays = Days;
		return;
	}
	const float Hours = FMath::Min((Days - LastSimulatedDays) * 24.0f, BuildingSubsystemDetail::MaxHoursPerStep);
	LastSimulatedDays = Days;
	if (Hours <= 0.0f || Model->GetPieces().Num() == 0)
	{
		return;
	}
	const FBuildingChangeResult Change = Model->Tick(Hours, SampleWeather());
	if (!Change.IsEmpty())
	{
		HandleChange(Change);
	}
}

float UBuildingSubsystem::GetGameDays() const
{
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	return Time ? Time->GetTotalDays() : 0.0f;
}

FBuildingWeather UBuildingSubsystem::SampleWeather() const
{
	FBuildingWeather Weather;
	const UWorld* World = GetWorld();
	const UExploredWeatherSubsystem* WeatherSubsystem = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
	if (!WeatherSubsystem)
	{
		return Weather;
	}
	Weather.Rain = WeatherSubsystem->GetCurrent().Rain;
	switch (WeatherSubsystem->GetState())
	{
	case EWeatherState::Gale:
		Weather.StormCategory = 0.5f;
		break;
	case EWeatherState::Cyclone:
	{
		// La categoría sale del modelo del clima; un ciclón forzado (depuración) usa CycloneCategory.
		const int32 Category = WeatherSubsystem->GetCycloneCategory();
		Weather.StormCategory = FMath::Clamp(Category > 0 ? static_cast<float>(Category) : CycloneCategory, 1.0f, 3.0f);
		break;
	}
	default:
		break;
	}
	return Weather;
}

void UBuildingSubsystem::HandleChange(const FBuildingChangeResult& Change)
{
	DestroyActors(Change.Destroyed);
	DestroyActors(Change.Collapsed);
	OnPiecesRemoved.Broadcast(Change);
}

void UBuildingSubsystem::SpawnActorFor(const FBuildingPieceState& Piece)
{
	UWorld* World = GetWorld();
	const FBuildingPieceDef* Def = Model ? Model->GetCatalog().FindPiece(Piece.DefId) : nullptr;
	if (!World || !Def)
	{
		return;
	}
	FVector Location;
	float Yaw = 0.0f;
	if (!Model->PlacementToWorld(Piece.Placement, Def->Socket, Location, Yaw))
	{
		return;
	}
	const bool bFollowsTerrain = Def->Socket == EBuildSocket::GroundOnly ||
		(Def->Socket == EBuildSocket::Furniture && Piece.bGroundContact);
	if (bFollowsTerrain)
	{
		Location = BuildingSubsystemDetail::DropToGround(World, Location);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AExploredBuildingPiece* Actor = World->SpawnActor<AExploredBuildingPiece>(
		AExploredBuildingPiece::StaticClass(), Location, FRotator(0.0f, Yaw, 0.0f), Params);
	if (!Actor)
	{
		return;
	}
	Actor->InitializePiece(Piece.Id, Piece.DefId, Def->Socket, Model->ResolveMeshPath(*Def));
	Actors.Add(Piece.Id, Actor);
}

void UBuildingSubsystem::DestroyActors(const TArray<int32>& PieceIds)
{
	for (const int32 Id : PieceIds)
	{
		TObjectPtr<AExploredBuildingPiece> Actor;
		if (Actors.RemoveAndCopyValue(Id, Actor) && IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
}

void UBuildingSubsystem::DestroyAllActors()
{
	for (const TPair<int32, TObjectPtr<AExploredBuildingPiece>>& Entry : Actors)
	{
		if (IsValid(Entry.Value))
		{
			Entry.Value->Destroy();
		}
	}
	Actors.Empty();
}

#undef LOCTEXT_NAMESPACE
