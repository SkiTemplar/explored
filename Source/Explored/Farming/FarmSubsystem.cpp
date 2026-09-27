#include "Farming/FarmSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Subsystems/SubsystemCollection.h"

#include "Achievements/AchievementsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Items/ItemRegistrySubsystem.h"
#include "Save/SaveSystemStates.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "UI/ExploredSaveSubsystem.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/ArchipelagoLayout.h"

namespace FarmSubsystemDetail
{
	constexpr uint32 FarmSeed = FArchipelagoLayout::OfficialSeed ^ 0xFA23u;
	/** Como mucho se recuperan estos días de golpe (un salto de reloj enorme no congela el juego). */
	constexpr int32 MaxCatchUpDays = FWeatherModel::DaysPerYear * 2;
	/** Un salto de reloj mayor que esto (SetTime, dormir) no se toma como lluvia vista. */
	constexpr float MaxLiveRainStepDays = 0.25f;

	/** Sección de la partida (docs/tecnico/guardado.md). */
	const TCHAR* const SaveSection = TEXT("farm");

	UExploredSaveSubsystem* FindSave(const UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UExploredSaveSubsystem>() : nullptr;
	}

	bool ParseStage(const TSharedPtr<FJsonValue>& Value, FPlantStageDef& OutStage)
	{
		TSharedPtr<FJsonObject> Obj;
		if (Value.IsValid())
		{
			Obj = Value->AsObject();
		}
		if (!Obj.IsValid())
		{
			return false;
		}
		FString Id;
		if (!Obj->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
		{
			return false;
		}
		OutStage.Id = FName(*Id);
		Obj->TryGetStringField(TEXT("nameEs"), OutStage.NameEs);
		int32 Days = 0;
		Obj->TryGetNumberField(TEXT("days"), Days);
		OutStage.Days = FMath::Max(0, Days);
		// «mesh»: null = marcador pendiente (meshes_pendientes.json); TryGetStringField falla y queda vacía.
		Obj->TryGetStringField(TEXT("mesh"), OutStage.MeshPath);
		return true;
	}

	bool ParsePlant(const TSharedPtr<FJsonObject>& Obj, FPlantDef& OutPlant, FString& OutError)
	{
		if (!Obj.IsValid())
		{
			OutError = TEXT("Entrada de plants.json vacía");
			return false;
		}
		FString Id, PlantedFrom, RequiresPiece;
		if (!Obj->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
		{
			OutError = TEXT("Planta sin \"id\" en plants.json");
			return false;
		}
		OutPlant.Id = FName(*Id);
		Obj->TryGetStringField(TEXT("nameEs"), OutPlant.NameEs);
		if (!Obj->TryGetStringField(TEXT("plantedFrom"), PlantedFrom) || PlantedFrom.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Planta «%s» sin plantedFrom"), *Id);
			return false;
		}
		OutPlant.PlantedFrom = FName(*PlantedFrom);
		Obj->TryGetStringField(TEXT("requiresPiece"), RequiresPiece);
		OutPlant.RequiresPiece = RequiresPiece.IsEmpty() ? NAME_None : FName(*RequiresPiece);

		TArray<FString> Seasons;
		Obj->TryGetStringArrayField(TEXT("seasons"), Seasons);
		for (const FString& SeasonId : Seasons)
		{
			ESeason Season = ESeason::Dry;
			if (!FFarmModel::SeasonFromId(SeasonId, Season))
			{
				OutError = FString::Printf(TEXT("Planta «%s»: estación desconocida «%s»"), *Id, *SeasonId);
				return false;
			}
			OutPlant.Seasons.AddUnique(Season);
		}
		if (OutPlant.Seasons.Num() == 0)
		{
			OutError = FString::Printf(TEXT("Planta «%s» sin estaciones"), *Id);
			return false;
		}

		int32 WaterPerDay = 1;
		Obj->TryGetNumberField(TEXT("waterPerDay"), WaterPerDay);
		OutPlant.WaterPerDay = FMath::Max(0, WaterPerDay);
		bool bNeverRemoved = false;
		Obj->TryGetBoolField(TEXT("neverRemoved"), bNeverRemoved);
		OutPlant.bNeverRemoved = bNeverRemoved;
		bool bBirdsEat = false;
		Obj->TryGetBoolField(TEXT("birdsEat"), bBirdsEat);
		OutPlant.bBirdsEat = bBirdsEat;

		const TArray<TSharedPtr<FJsonValue>>* Stages = nullptr;
		if (Obj->TryGetArrayField(TEXT("stages"), Stages) && Stages)
		{
			for (const TSharedPtr<FJsonValue>& StageValue : *Stages)
			{
				FPlantStageDef Stage;
				if (!ParseStage(StageValue, Stage))
				{
					OutError = FString::Printf(TEXT("Planta «%s»: etapa sin id"), *Id);
					return false;
				}
				OutPlant.Stages.Add(Stage);
			}
		}
		if (OutPlant.Stages.Num() < 2)
		{
			OutError = FString::Printf(TEXT("Planta «%s»: necesita al menos dos etapas"), *Id);
			return false;
		}

		const TSharedPtr<FJsonObject>* Harvest = nullptr;
		if (!Obj->TryGetObjectField(TEXT("harvest"), Harvest) || !Harvest || !Harvest->IsValid())
		{
			OutError = FString::Printf(TEXT("Planta «%s» sin cosecha"), *Id);
			return false;
		}
		FString Item;
		(*Harvest)->TryGetStringField(TEXT("item"), Item);
		OutPlant.Harvest.Item = FName(*Item);
		(*Harvest)->TryGetNumberField(TEXT("min"), OutPlant.Harvest.Min);
		(*Harvest)->TryGetNumberField(TEXT("max"), OutPlant.Harvest.Max);
		(*Harvest)->TryGetNumberField(TEXT("everyDays"), OutPlant.Harvest.EveryDays);
		if (Item.IsEmpty() || OutPlant.Harvest.Min < 0 || OutPlant.Harvest.Max < OutPlant.Harvest.Min || OutPlant.Harvest.EveryDays < 0)
		{
			OutError = FString::Printf(TEXT("Planta «%s»: cosecha inválida"), *Id);
			return false;
		}
		return true;
	}
}

bool UFarmSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFarmSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UTimeOfDaySubsystem* Time = Collection.InitializeDependency<UTimeOfDaySubsystem>();
	Collection.InitializeDependency<UExploredWeatherSubsystem>();

	Model = MakeUnique<FFarmModel>(TArray<FPlantDef>(), FarmSubsystemDetail::FarmSeed);
	ReloadFromDisk();

	TimeOfDay = Time;
	if (Time)
	{
		// AddUObject guarda una referencia débil: si el subsistema muere antes, no se llama.
		NewDayHandle = Time->OnNewDay.AddUObject(this, &UFarmSubsystem::HandleNewDay);
	}

	if (UExploredSaveSubsystem* Save = FarmSubsystemDetail::FindSave(GetWorld()))
	{
		TWeakObjectPtr<UFarmSubsystem> WeakThis(this);
		Save->RegisterSection(FarmSubsystemDetail::SaveSection,
			[WeakThis](FSaveArchive& Ar)
			{
				if (const UFarmSubsystem* Self = WeakThis.Get())
				{
					ExploredSaveStates::SaveFarm(Ar, Self->GetSaveState());
				}
			},
			[WeakThis](const FSaveArchive& Ar)
			{
				if (UFarmSubsystem* Self = WeakThis.Get())
				{
					FFarmState State;
					ExploredSaveStates::LoadFarm(Ar, State);
					// Sin sección (partida anterior al huerto) se conservan las parcelas del mapa.
					if (!Ar.IsEmpty())
					{
						Self->LoadSaveState(State);
					}
				}
			});
	}
}

void UFarmSubsystem::Deinitialize()
{
	if (UExploredSaveSubsystem* Save = FarmSubsystemDetail::FindSave(GetWorld()))
	{
		Save->UnregisterSection(FarmSubsystemDetail::SaveSection);
	}
	if (UTimeOfDaySubsystem* Time = TimeOfDay.Get())
	{
		Time->OnNewDay.Remove(NewDayHandle);
	}
	NewDayHandle.Reset();
	Model.Reset();
	Super::Deinitialize();
}

bool UFarmSubsystem::ReloadFromDisk()
{
	const FString Path = UItemRegistrySubsystem::GetDataFilePath(TEXT("plants.json"));
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		UE_LOG(LogTemp, Error, TEXT("[Explored] No se encontró %s"), *Path);
		return false;
	}
	TArray<FPlantDef> Plants;
	FString Error;
	if (!ParsePlantsJson(Json, Plants, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("[Explored] plants.json inválido: %s"), *Error);
		return false;
	}
	const FFarmState Kept = Model ? Model->GetState() : FFarmState();
	Model = MakeUnique<FFarmModel>(MoveTemp(Plants), FarmSubsystemDetail::FarmSeed);
	Model->SetState(Kept);
	OnPlotChanged.Broadcast(INDEX_NONE);
	return true;
}

bool UFarmSubsystem::ParsePlantsJson(const FString& JsonText, TArray<FPlantDef>& OutPlants, FString& OutError)
{
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON de plants.json mal formado");
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!Root->TryGetArrayField(TEXT("plants"), Entries) || !Entries)
	{
		OutError = TEXT("plants.json sin lista \"plants\"");
		return false;
	}
	OutPlants.Reset();
	for (const TSharedPtr<FJsonValue>& Entry : *Entries)
	{
		FPlantDef Parsed;
		const TSharedPtr<FJsonObject> Obj = Entry.IsValid() ? Entry->AsObject() : TSharedPtr<FJsonObject>();
		if (!FarmSubsystemDetail::ParsePlant(Obj, Parsed, OutError))
		{
			return false;
		}
		if (OutPlants.ContainsByPredicate([&Parsed](const FPlantDef& P) { return P.Id == Parsed.Id; }))
		{
			OutError = FString::Printf(TEXT("Id de planta duplicado: %s"), *Parsed.Id.ToString());
			return false;
		}
		OutPlants.Add(MoveTemp(Parsed));
	}
	return true;
}

// --- Tiempo ----------------------------------------------------------------------------

void UFarmSubsystem::Tick(float DeltaTime)
{
	UTimeOfDaySubsystem* Time = TimeOfDay.Get();
	if (!Time || !Model)
	{
		return;
	}

	// Lluvia vista en juego: cuenta también la forzada (ForceState) que el planificador no conoce.
	const float TotalDays = Time->GetTotalDays();
	const UWorld* World = GetWorld();
	const UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
	if (Weather && LastTotalDays >= 0.0f)
	{
		const float StepDays = TotalDays - LastTotalDays;
		if (StepDays > 0.0f && StepDays < FarmSubsystemDetail::MaxLiveRainStepDays)
		{
			LiveRainHours += Weather->GetCurrent().Rain * StepDays * 24.0f;
		}
	}
	LastTotalDays = TotalDays;

	CatchUpTo(Time->GetDay());
}

TStatId UFarmSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFarmSubsystem, STATGROUP_Tickables);
}

void UFarmSubsystem::HandleNewDay(int32 NewDay)
{
	CatchUpTo(NewDay);
}

void UFarmSubsystem::CatchUpTo(int32 CurrentDay)
{
	if (!Model)
	{
		return;
	}
	const int32 LastEnded = Model->GetState().LastEndedDay;
	int32 FirstDay = LastEnded == INDEX_NONE ? CurrentDay - 1 : LastEnded + 1;
	FirstDay = FMath::Max(FirstDay, CurrentDay - FarmSubsystemDetail::MaxCatchUpDays);

	if (FirstDay < CurrentDay)
	{
		const UWorld* World = GetWorld();
		const UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
		const FWeatherModel* Planner = Weather ? Weather->GetModel() : nullptr;
		for (int32 Day = FMath::Max(0, FirstDay); Day < CurrentDay; ++Day)
		{
			float Rain = Planner ? FFarmModel::RainWateringsForDay(*Planner, Day) : 0.0f;
			if (Day == LiveRainDay)
			{
				// Si solo se jugó parte del día, el planificador completa lo que no se vio.
				Rain = FMath::Max(Rain, FFarmModel::WateringsFromRainHours(LiveRainHours));
			}
			Model->EndDay(Day, Rain);
		}
		OnPlotChanged.Broadcast(INDEX_NONE);
	}

	if (LiveRainDay != CurrentDay)
	{
		LiveRainDay = CurrentDay;
		LiveRainHours = 0.0f;
	}
}

int32 UFarmSubsystem::GetCurrentDay() const
{
	const UTimeOfDaySubsystem* Time = TimeOfDay.Get();
	return Time ? Time->GetDay() : 0;
}

// --- Parcelas y acciones ---------------------------------------------------------------

int32 UFarmSubsystem::RegisterPlot(const FVector& Location, const TArray<FName>& Pieces)
{
	if (!Model)
	{
		return INDEX_NONE;
	}
	const int32 PlotId = Model->AddPlot(Location, Pieces);
	OnPlotChanged.Broadcast(PlotId);
	return PlotId;
}

EFarmResult UFarmSubsystem::UnregisterPlot(int32 PlotId)
{
	if (!Model)
	{
		return EFarmResult::UnknownPlot;
	}
	const EFarmResult Result = Model->RemovePlot(PlotId);
	if (Result == EFarmResult::Ok)
	{
		OnPlotChanged.Broadcast(PlotId);
	}
	return Result;
}

void UFarmSubsystem::AddScarecrow(const FVector& Location)
{
	if (Model)
	{
		Model->AddScarecrow(Location);
	}
}

void UFarmSubsystem::RemoveScarecrow(const FVector& Location)
{
	if (Model)
	{
		Model->RemoveScarecrow(Location);
	}
}

FName UFarmSubsystem::PlantForItem(int32 PlotId, FName Item) const
{
	return Model ? Model->PlantForItem(PlotId, Item, GetCurrentDay()) : NAME_None;
}

EFarmResult UFarmSubsystem::Plant(int32 PlotId, FName Item)
{
	if (!Model)
	{
		return EFarmResult::UnknownPlot;
	}
	const int32 Day = GetCurrentDay();
	const FName PlantId = Model->PlantForItem(PlotId, Item, Day);
	if (PlantId.IsNone())
	{
		return EFarmResult::WrongItem;
	}
	const EFarmResult Result = Model->Plant(PlotId, PlantId, Item, Day);
	if (Result == EFarmResult::Ok)
	{
		OnPlotChanged.Broadcast(PlotId);
	}
	return Result;
}

EFarmResult UFarmSubsystem::Water(int32 PlotId, float Waterings)
{
	if (!Model)
	{
		return EFarmResult::UnknownPlot;
	}
	const EFarmResult Result = Model->Water(PlotId, Waterings);
	if (Result == EFarmResult::Ok)
	{
		OnPlotChanged.Broadcast(PlotId);
	}
	return Result;
}

EFarmResult UFarmSubsystem::ApplyCompost(int32 PlotId)
{
	if (!Model)
	{
		return EFarmResult::UnknownPlot;
	}
	const EFarmResult Result = Model->ApplyCompost(PlotId);
	if (Result == EFarmResult::Ok)
	{
		OnPlotChanged.Broadcast(PlotId);
	}
	return Result;
}

EFarmResult UFarmSubsystem::Harvest(int32 PlotId, FFarmHarvest& Out)
{
	if (!Model)
	{
		return EFarmResult::UnknownPlot;
	}
	// El cultivo se lee antes: una cosecha que arranca la planta deja la parcela vacía.
	const FName PlantId = Model->GetStageView(PlotId).PlantId;
	const EFarmResult Result = Model->Harvest(PlotId, GetCurrentDay(), Out);
	if (Result == EFarmResult::Ok)
	{
		OnPlotChanged.Broadcast(PlotId);
		if (UAchievementsSubsystem* Achievements = UAchievementsSubsystem::Get(this))
		{
			Achievements->ReportStatItem(TEXT("crops_harvested"), PlantId);
		}
	}
	return Result;
}

EFarmResult UFarmSubsystem::ClearPlot(int32 PlotId)
{
	if (!Model)
	{
		return EFarmResult::UnknownPlot;
	}
	const EFarmResult Result = Model->ClearPlot(PlotId);
	if (Result == EFarmResult::Ok)
	{
		OnPlotChanged.Broadcast(PlotId);
	}
	return Result;
}

FFarmStageView UFarmSubsystem::GetStageView(int32 PlotId) const
{
	return Model ? Model->GetStageView(PlotId) : FFarmStageView();
}

const FPlantDef* UFarmSubsystem::FindPlant(FName PlantId) const
{
	return Model ? Model->FindPlant(PlantId) : nullptr;
}

FFarmState UFarmSubsystem::GetSaveState() const
{
	return Model ? Model->GetState() : FFarmState();
}

void UFarmSubsystem::LoadSaveState(const FFarmState& InState)
{
	if (Model)
	{
		Model->SetState(InState);
		// La lluvia medida en la sesión descartada no riega la partida cargada.
		LiveRainHours = 0.0f;
		LiveRainDay = INDEX_NONE;
		LastTotalDays = -1.0f;
		OnPlotChanged.Broadcast(INDEX_NONE);
	}
}
