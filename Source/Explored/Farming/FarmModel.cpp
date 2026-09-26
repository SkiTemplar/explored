#include "Farming/FarmModel.h"

#include "Core/ExploredRandom.h"

namespace FarmModelDetail
{
	/** Tolerancia al comparar riegos acumulados en coma flotante. */
	constexpr float WaterEpsilon = 1.0e-3f;
	/** Muestras por día al integrar la lluvia del planificador (una cada media hora). */
	constexpr int32 RainSamplesPerDay = 48;
	/** Sales del hash de cosecha: cantidad y aves. */
	constexpr int32 YieldSalt = 0;
	constexpr int32 BirdSalt = 1;
}

const TCHAR* LexToString(EPlantHealth Health)
{
	switch (Health)
	{
	case EPlantHealth::Healthy: return TEXT("Healthy");
	case EPlantHealth::Thirsty: return TEXT("Thirsty");
	case EPlantHealth::Wilting: return TEXT("Wilting");
	case EPlantHealth::Dead: return TEXT("Dead");
	}
	return TEXT("Unknown");
}

const TCHAR* LexToString(EFarmResult Result)
{
	switch (Result)
	{
	case EFarmResult::Ok: return TEXT("Ok");
	case EFarmResult::UnknownPlot: return TEXT("UnknownPlot");
	case EFarmResult::UnknownPlant: return TEXT("UnknownPlant");
	case EFarmResult::PlotOccupied: return TEXT("PlotOccupied");
	case EFarmResult::PlotEmpty: return TEXT("PlotEmpty");
	case EFarmResult::WrongItem: return TEXT("WrongItem");
	case EFarmResult::MissingPiece: return TEXT("MissingPiece");
	case EFarmResult::OutOfSeason: return TEXT("OutOfSeason");
	case EFarmResult::PlantDead: return TEXT("PlantDead");
	case EFarmResult::NotReady: return TEXT("NotReady");
	case EFarmResult::NeverRemoved: return TEXT("NeverRemoved");
	}
	return TEXT("Unknown");
}

int32 FPlantDef::DaysToFirstHarvest() const
{
	int32 Total = 0;
	for (int32 I = 0; I + 1 < Stages.Num(); ++I)
	{
		Total += FMath::Max(0, Stages[I].Days);
	}
	return Total;
}

FFarmModel::FFarmModel(TArray<FPlantDef> InPlants, uint32 InSeed)
	: Plants(MoveTemp(InPlants))
	, Seed(InSeed)
{
}

const FPlantDef* FFarmModel::FindPlant(FName PlantId) const
{
	return Plants.FindByPredicate([PlantId](const FPlantDef& P) { return P.Id == PlantId; });
}

bool FFarmModel::SeasonFromId(const FString& Id, ESeason& OutSeason)
{
	if (Id == TEXT("seca")) { OutSeason = ESeason::Dry; return true; }
	if (Id == TEXT("primeras_lluvias")) { OutSeason = ESeason::FirstRains; return true; }
	if (Id == TEXT("monzon")) { OutSeason = ESeason::Monsoon; return true; }
	if (Id == TEXT("ciclones")) { OutSeason = ESeason::Cyclones; return true; }
	return false;
}

float FFarmModel::WateringsFromRainHours(float RainHours)
{
	return FMath::Max(0.0f, RainHours) / RainHoursPerWatering;
}

float FFarmModel::RainWateringsForDay(const FWeatherModel& Weather, int32 Day)
{
	using namespace FarmModelDetail;
	const float Step = 1.0f / RainSamplesPerDay;
	float RainHours = 0.0f;
	for (int32 I = 0; I < RainSamplesPerDay; ++I)
	{
		const float T = static_cast<float>(Day) + (I + 0.5f) * Step;
		RainHours += Weather.SampleAt(T).Rain * 24.0f * Step;
	}
	return WateringsFromRainHours(RainHours);
}

int32 FFarmModel::FirstHarvestDay(const FPlantDef& Def, int32 PlantedDay)
{
	// El día de plantar cuenta como primer día de crecimiento: se cierra con EndDay(PlantedDay).
	return PlantedDay + Def.DaysToFirstHarvest();
}

// --- Parcelas ------------------------------------------------------------------------

int32 FFarmModel::AddPlot(const FVector& Location, const TArray<FName>& Pieces)
{
	FFarmPlot& Plot = State.Plots.AddDefaulted_GetRef();
	Plot.Id = State.NextPlotId++;
	Plot.Location = Location;
	Plot.Pieces = Pieces;
	return Plot.Id;
}

EFarmResult FFarmModel::RemovePlot(int32 PlotId)
{
	const int32 Index = State.Plots.IndexOfByPredicate([PlotId](const FFarmPlot& P) { return P.Id == PlotId; });
	if (Index == INDEX_NONE)
	{
		return EFarmResult::UnknownPlot;
	}
	const FPlantDef* Def = FindPlant(State.Plots[Index].Crop.PlantId);
	if (Def && Def->bNeverRemoved)
	{
		return EFarmResult::NeverRemoved;
	}
	State.Plots.RemoveAt(Index);
	return EFarmResult::Ok;
}

const FFarmPlot* FFarmModel::FindPlot(int32 PlotId) const
{
	return State.Plots.FindByPredicate([PlotId](const FFarmPlot& P) { return P.Id == PlotId; });
}

FFarmPlot* FFarmModel::FindPlotMutable(int32 PlotId)
{
	return State.Plots.FindByPredicate([PlotId](const FFarmPlot& P) { return P.Id == PlotId; });
}

int32 FFarmModel::FindPlotNear(const FVector& Location, double MaxDistance) const
{
	int32 Best = INDEX_NONE;
	double BestDistSq = MaxDistance * MaxDistance;
	for (const FFarmPlot& Plot : State.Plots)
	{
		const double DistSq = FVector::DistSquared(Plot.Location, Location);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Plot.Id;
		}
	}
	return Best;
}

void FFarmModel::AddScarecrow(const FVector& Location)
{
	State.Scarecrows.Add(Location);
}

bool FFarmModel::RemoveScarecrow(const FVector& Location, double Tolerance)
{
	const int32 Index = State.Scarecrows.IndexOfByPredicate([&](const FVector& S)
	{
		return FVector::DistSquared(S, Location) <= Tolerance * Tolerance;
	});
	if (Index == INDEX_NONE)
	{
		return false;
	}
	State.Scarecrows.RemoveAt(Index);
	return true;
}

bool FFarmModel::IsProtectedFromBirds(int32 PlotId) const
{
	const FFarmPlot* Plot = FindPlot(PlotId);
	if (!Plot)
	{
		return false;
	}
	return State.Scarecrows.ContainsByPredicate([Plot](const FVector& S)
	{
		return FVector::DistSquared(S, Plot->Location) <= ScarecrowRadius * ScarecrowRadius;
	});
}

// --- Acciones del jugador ------------------------------------------------------------

EFarmResult FFarmModel::CanPlant(int32 PlotId, FName PlantId, FName OfferedItem, int32 Day) const
{
	const FFarmPlot* Plot = FindPlot(PlotId);
	if (!Plot)
	{
		return EFarmResult::UnknownPlot;
	}
	const FPlantDef* Def = FindPlant(PlantId);
	if (!Def)
	{
		return EFarmResult::UnknownPlant;
	}
	if (!Plot->Crop.IsEmpty())
	{
		return EFarmResult::PlotOccupied;
	}
	if (OfferedItem != Def->PlantedFrom)
	{
		return EFarmResult::WrongItem;
	}
	if (!Def->RequiresPiece.IsNone() && !Plot->Pieces.Contains(Def->RequiresPiece))
	{
		return EFarmResult::MissingPiece;
	}
	if (!Def->GrowsIn(FWeatherModel::SeasonForDay(static_cast<float>(Day))))
	{
		return EFarmResult::OutOfSeason;
	}
	return EFarmResult::Ok;
}

EFarmResult FFarmModel::Plant(int32 PlotId, FName PlantId, FName OfferedItem, int32 Day)
{
	const EFarmResult Result = CanPlant(PlotId, PlantId, OfferedItem, Day);
	if (Result != EFarmResult::Ok)
	{
		return Result;
	}
	const FPlantDef* Def = FindPlant(PlantId);
	FFarmPlot* Plot = FindPlotMutable(PlotId);
	Plot->Crop = FFarmCrop();
	Plot->Crop.PlantId = PlantId;
	Plot->Crop.PlantedDay = Day;
	// Un cultivo sin etapas de crecimiento (solo la final) da cosecha desde el primer momento.
	Plot->Crop.bHarvestReady = Def->DaysToFirstHarvest() == 0;
	return EFarmResult::Ok;
}

FName FFarmModel::PlantForItem(int32 PlotId, FName OfferedItem, int32 Day) const
{
	for (const FPlantDef& Def : Plants)
	{
		if (Def.PlantedFrom == OfferedItem && CanPlant(PlotId, Def.Id, OfferedItem, Day) == EFarmResult::Ok)
		{
			return Def.Id;
		}
	}
	return NAME_None;
}

EFarmResult FFarmModel::Water(int32 PlotId, float Waterings)
{
	FFarmPlot* Plot = FindPlotMutable(PlotId);
	if (!Plot)
	{
		return EFarmResult::UnknownPlot;
	}
	if (Plot->Crop.IsEmpty())
	{
		return EFarmResult::PlotEmpty;
	}
	if (Plot->Crop.bDead)
	{
		return EFarmResult::PlantDead;
	}
	Plot->Crop.WaterToday += FMath::Max(0.0f, Waterings);
	return EFarmResult::Ok;
}

EFarmResult FFarmModel::ApplyCompost(int32 PlotId)
{
	FFarmPlot* Plot = FindPlotMutable(PlotId);
	if (!Plot)
	{
		return EFarmResult::UnknownPlot;
	}
	// No se acumula: echar más compost solo renueva su duración.
	Plot->CompostDaysLeft = CompostDays;
	return EFarmResult::Ok;
}

EFarmResult FFarmModel::Harvest(int32 PlotId, int32 Day, FFarmHarvest& Out)
{
	using namespace FarmModelDetail;
	Out = FFarmHarvest();
	FFarmPlot* Plot = FindPlotMutable(PlotId);
	if (!Plot)
	{
		return EFarmResult::UnknownPlot;
	}
	FFarmCrop& Crop = Plot->Crop;
	if (Crop.IsEmpty())
	{
		return EFarmResult::PlotEmpty;
	}
	if (Crop.bDead)
	{
		return EFarmResult::PlantDead;
	}
	const FPlantDef* Def = FindPlant(Crop.PlantId);
	if (!Def)
	{
		return EFarmResult::UnknownPlant;
	}
	if (!Crop.bHarvestReady)
	{
		return EFarmResult::NotReady;
	}

	const FPlantHarvestDef& H = Def->Harvest;
	const int32 Min = FMath::Max(0, FMath::Min(H.Min, H.Max));
	const int32 Max = FMath::Max(Min, H.Max);
	const uint32 Span = static_cast<uint32>(Max - Min) + 1u;
	int32 Count = Min + static_cast<int32>(ExploredHash::Hash3D(Seed, PlotId, Day, YieldSalt) % Span);

	if (Def->bBirdsEat && !IsProtectedFromBirds(PlotId)
		&& ExploredHash::ToUnitFloat(ExploredHash::Hash3D(Seed, PlotId, Day, BirdSalt)) < BirdChance)
	{
		Out.LostToBirds = FMath::Min(Count, FMath::Max(1, FMath::CeilToInt32(Count * BirdShare)));
		Count -= Out.LostToBirds;
	}

	Out.Item = H.Item;
	Out.Count = Count;
	++Crop.Harvests;

	if (H.EveryDays > 0 || Def->bNeverRemoved)
	{
		Crop.bHarvestReady = false;
		Crop.HarvestClock = 0.0f;
	}
	else
	{
		Crop = FFarmCrop();
		Out.bPlantRemoved = true;
	}
	return EFarmResult::Ok;
}

EFarmResult FFarmModel::ClearPlot(int32 PlotId)
{
	FFarmPlot* Plot = FindPlotMutable(PlotId);
	if (!Plot)
	{
		return EFarmResult::UnknownPlot;
	}
	if (Plot->Crop.IsEmpty())
	{
		return EFarmResult::PlotEmpty;
	}
	const FPlantDef* Def = FindPlant(Plot->Crop.PlantId);
	if (Def && Def->bNeverRemoved)
	{
		return EFarmResult::NeverRemoved;
	}
	Plot->Crop = FFarmCrop();
	return EFarmResult::Ok;
}

// --- Tiempo ----------------------------------------------------------------------------

void FFarmModel::GrowCrop(const FPlantDef& Def, FFarmCrop& Crop, float Units, bool bInSeason) const
{
	const float Total = static_cast<float>(Def.DaysToFirstHarvest());
	if (Crop.GrowthDays < Total)
	{
		Crop.GrowthDays = FMath::Min(Total, Crop.GrowthDays + Units);
		if (Crop.GrowthDays >= Total)
		{
			Crop.bHarvestReady = true;
			Crop.HarvestClock = 0.0f;
		}
		return;
	}
	// Etapa final: la fruta vuelve a salir tras everyDays días buenos, y solo en estación.
	if (Def.Harvest.EveryDays > 0 && !Crop.bHarvestReady && bInSeason)
	{
		Crop.HarvestClock += Units;
		if (Crop.HarvestClock >= static_cast<float>(Def.Harvest.EveryDays))
		{
			Crop.bHarvestReady = true;
			Crop.HarvestClock = 0.0f;
		}
	}
}

bool FFarmModel::EndDay(int32 Day, float RainWaterings)
{
	using namespace FarmModelDetail;
	if (State.LastEndedDay != INDEX_NONE && Day <= State.LastEndedDay)
	{
		return false;
	}
	State.LastEndedDay = Day;

	const ESeason Season = FWeatherModel::SeasonForDay(static_cast<float>(Day));
	const float Rain = FMath::Max(0.0f, RainWaterings);

	for (FFarmPlot& Plot : State.Plots)
	{
		const bool bCompost = Plot.CompostDaysLeft > 0;
		Plot.CompostDaysLeft = FMath::Max(0, Plot.CompostDaysLeft - 1);

		FFarmCrop& Crop = Plot.Crop;
		const FPlantDef* Def = Crop.IsEmpty() ? nullptr : FindPlant(Crop.PlantId);
		const float WaterToday = Crop.WaterToday;
		Crop.WaterToday = 0.0f;
		if (!Def || Crop.bDead || Day < Crop.PlantedDay)
		{
			continue;
		}

		const bool bWatered = Def->WaterPerDay <= 0
			|| WaterToday + Rain + WaterEpsilon >= static_cast<float>(Def->WaterPerDay);
		if (!bWatered)
		{
			++Crop.DryDays;
			if (Def->bNeverRemoved)
			{
				// El limonero se queda marchito y en pausa: no cuenta más allá de eso.
				Crop.DryDays = FMath::Min(Crop.DryDays, DryDaysToWilt);
			}
			else if (Crop.DryDays >= DryDaysToDie)
			{
				Crop.bDead = true;
				Crop.bHarvestReady = false;
			}
			continue;
		}

		Crop.DryDays = 0;
		const bool bInSeason = Def->GrowsIn(Season);
		const float Units = (bInSeason ? 1.0f : OutOfSeasonGrowth) * (bCompost ? CompostGrowth : 1.0f);
		GrowCrop(*Def, Crop, Units, bInSeason);
	}
	return true;
}

// --- Consultas -------------------------------------------------------------------------

EPlantHealth FFarmModel::GetHealth(int32 PlotId) const
{
	const FFarmPlot* Plot = FindPlot(PlotId);
	if (!Plot || Plot->Crop.IsEmpty())
	{
		return EPlantHealth::Healthy;
	}
	if (Plot->Crop.bDead)
	{
		return EPlantHealth::Dead;
	}
	if (Plot->Crop.DryDays >= DryDaysToWilt)
	{
		return EPlantHealth::Wilting;
	}
	return Plot->Crop.DryDays > 0 ? EPlantHealth::Thirsty : EPlantHealth::Healthy;
}

FFarmStageView FFarmModel::GetStageView(int32 PlotId) const
{
	FFarmStageView View;
	const FFarmPlot* Plot = FindPlot(PlotId);
	if (!Plot || Plot->Crop.IsEmpty())
	{
		return View;
	}
	const FFarmCrop& Crop = Plot->Crop;
	const FPlantDef* Def = FindPlant(Crop.PlantId);
	if (!Def || Def->Stages.Num() == 0)
	{
		return View;
	}

	View.bHasPlant = true;
	View.PlantId = Def->Id;
	View.StageCount = Def->Stages.Num();
	View.Health = GetHealth(PlotId);
	View.bHarvestReady = Crop.bHarvestReady;
	View.bNeedsWater = !Crop.bDead && Def->WaterPerDay > 0
		&& Crop.WaterToday + FarmModelDetail::WaterEpsilon < static_cast<float>(Def->WaterPerDay);

	const float Total = static_cast<float>(Def->DaysToFirstHarvest());
	View.OverallProgress = Total > 0.0f ? FMath::Clamp(Crop.GrowthDays / Total, 0.0f, 1.0f) : 1.0f;

	// Etapa: la primera cuyo tramo acumulado aún no se ha completado; si no, la final.
	View.StageIndex = View.StageCount - 1;
	View.StageProgress = 1.0f;
	float Start = 0.0f;
	for (int32 I = 0; I + 1 < Def->Stages.Num(); ++I)
	{
		const float Days = static_cast<float>(FMath::Max(0, Def->Stages[I].Days));
		if (Crop.GrowthDays < Start + Days)
		{
			View.StageIndex = I;
			View.StageProgress = FMath::Clamp((Crop.GrowthDays - Start) / Days, 0.0f, 1.0f);
			break;
		}
		Start += Days;
	}
	const FPlantStageDef& Stage = Def->Stages[View.StageIndex];
	View.StageId = Stage.Id;
	View.MeshPath = Stage.MeshPath;
	return View;
}

void FFarmModel::SetState(const FFarmState& InState)
{
	State = InState;
	int32 MaxId = 0;
	for (FFarmPlot& Plot : State.Plots)
	{
		MaxId = FMath::Max(MaxId, Plot.Id);
		if (!Plot.Crop.IsEmpty() && !FindPlant(Plot.Crop.PlantId))
		{
			Plot.Crop = FFarmCrop();
		}
	}
	State.NextPlotId = FMath::Max(State.NextPlotId, MaxId + 1);
}
