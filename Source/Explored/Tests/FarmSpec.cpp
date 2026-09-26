#include "Misc/AutomationTest.h"

#include "Farming/FarmModel.h"
#include "Weather/WeatherModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace FarmSpecDetail
{
	FPlantStageDef Stage(const TCHAR* Id, int32 Days)
	{
		FPlantStageDef S;
		S.Id = FName(Id);
		S.Days = Days;
		return S;
	}

	FPlantDef Plant(const TCHAR* Id, const TCHAR* From, const TCHAR* Piece, TArray<ESeason> Seasons, int32 Water,
		TArray<FPlantStageDef> Stages, const TCHAR* Item, int32 Min, int32 Max, int32 EveryDays)
	{
		FPlantDef P;
		P.Id = FName(Id);
		P.PlantedFrom = FName(From);
		P.RequiresPiece = FName(Piece);
		P.Seasons = MoveTemp(Seasons);
		P.WaterPerDay = Water;
		P.Stages = MoveTemp(Stages);
		P.Harvest.Item = FName(Item);
		P.Harvest.Min = Min;
		P.Harvest.Max = Max;
		P.Harvest.EveryDays = EveryDays;
		return P;
	}

	/** Copia de Content/Data/plants.json (versión 1): si cambian los datos, cambia aquí. */
	TArray<FPlantDef> DataPlants()
	{
		using S = ESeason;
		TArray<FPlantDef> Out;
		FPlantDef Lemon = Plant(TEXT("limonero"), TEXT("limon"), TEXT("arriate_limonero"),
			{S::Dry, S::FirstRains, S::Monsoon, S::Cyclones}, 1,
			{Stage(TEXT("esqueje"), 2), Stage(TEXT("brote"), 4), Stage(TEXT("arbol_joven"), 6), Stage(TEXT("arbol_adulto"), 0)},
			TEXT("limon"), 2, 4, 3);
		Lemon.bNeverRemoved = true;
		Out.Add(Lemon);
		FPlantDef Banana = Plant(TEXT("platanera"), TEXT("hijuelo_platano"), TEXT("bancal"), {S::FirstRains, S::Monsoon}, 1,
			{Stage(TEXT("hijuelo"), 3), Stage(TEXT("planta_joven"), 5), Stage(TEXT("planta_con_racimo"), 0)},
			TEXT("platano"), 4, 8, 6);
		Banana.bBirdsEat = true;
		Out.Add(Banana);
		Out.Add(Plant(TEXT("taro"), TEXT("taro"), TEXT("bancal"), {S::FirstRains, S::Monsoon}, 2,
			{Stage(TEXT("brote"), 3), Stage(TEXT("hojas_grandes"), 5), Stage(TEXT("listo"), 0)},
			TEXT("taro"), 2, 3, 0));
		Out.Add(Plant(TEXT("batata"), TEXT("batata"), TEXT("bancal"), {S::Dry, S::FirstRains, S::Monsoon}, 1,
			{Stage(TEXT("brote"), 2), Stage(TEXT("enredadera"), 4), Stage(TEXT("listo"), 0)},
			TEXT("batata"), 2, 4, 0));
		Out.Add(Plant(TEXT("pina"), TEXT("pina"), TEXT("bancal"), {S::Dry, S::FirstRains}, 0,
			{Stage(TEXT("corona"), 4), Stage(TEXT("roseta"), 6), Stage(TEXT("con_fruto"), 0)},
			TEXT("pina"), 1, 1, 0));
		FPlantDef Passion = Plant(TEXT("maracuya"), TEXT("maracuya"), TEXT("espaldera"), {S::FirstRains, S::Monsoon}, 1,
			{Stage(TEXT("brote"), 3), Stage(TEXT("trepadora"), 5), Stage(TEXT("en_flor_y_fruto"), 0)},
			TEXT("maracuya"), 3, 6, 3);
		Passion.bBirdsEat = true;
		Out.Add(Passion);
		return Out;
	}

	/** Primer día de una estación en el primer año. */
	int32 SeasonStart(ESeason Season)
	{
		return static_cast<int32>(Season) * FWeatherModel::DaysPerSeason;
	}

	TArray<FName> PiecesFor(const FPlantDef& Plant)
	{
		if (Plant.RequiresPiece == FName(TEXT("espaldera")))
		{
			return {FName(TEXT("bancal")), FName(TEXT("espaldera"))};
		}
		return {Plant.RequiresPiece};
	}

	/** Riega lo que pide la planta y cierra el día sin lluvia. */
	void WaterAndEnd(FFarmModel& Model, int32 PlotId, int32 Day)
	{
		const FFarmPlot* Plot = Model.FindPlot(PlotId);
		const FPlantDef* Plant = Plot ? Model.FindPlant(Plot->Crop.PlantId) : nullptr;
		if (Plant && !Plot->Crop.bDead)
		{
			Model.Water(PlotId, static_cast<float>(FMath::Max(0, Plant->WaterPerDay)));
		}
		Model.EndDay(Day, 0.0f);
	}

	/** Planta en una parcela nueva adecuada y devuelve su id. */
	int32 PlantNew(FFarmModel& Model, FName PlantId, int32 Day, const FVector& Location = FVector::ZeroVector)
	{
		const FPlantDef* Plant = Model.FindPlant(PlantId);
		check(Plant);
		const int32 PlotId = Model.AddPlot(Location, PiecesFor(*Plant));
		const EFarmResult Result = Model.Plant(PlotId, PlantId, Plant->PlantedFrom, Day);
		check(Result == EFarmResult::Ok);
		return PlotId;
	}
}

BEGIN_DEFINE_SPEC(FFarmSpec, "Explored.Farm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FFarmSpec)

void FFarmSpec::Define()
{
	using namespace FarmSpecDetail;
	const FName Limonero(TEXT("limonero"));

	Describe(TEXT("plantar"), [this, Limonero]()
	{
		It("exige el objeto de plantar, la pieza y la estación", [this, Limonero]()
		{
			FFarmModel Model(DataPlants(), 1);
			const int32 Bancal = Model.AddPlot(FVector::ZeroVector, {FName(TEXT("bancal"))});
			const int32 Arriate = Model.AddPlot(FVector(500.0, 0.0, 0.0), {FName(TEXT("arriate_limonero"))});
			const int32 Seca = SeasonStart(ESeason::Dry) + 2;

			TestEqual(TEXT("Limonero con un plátano"), Model.CanPlant(Arriate, Limonero, FName(TEXT("platano")), Seca), EFarmResult::WrongItem);
			TestEqual(TEXT("Limonero en un bancal"), Model.CanPlant(Bancal, Limonero, FarmIds::Lemon(), Seca), EFarmResult::MissingPiece);
			TestEqual(TEXT("Maracuyá sin espaldera"), Model.CanPlant(Bancal, FName(TEXT("maracuya")), FName(TEXT("maracuya")), SeasonStart(ESeason::Monsoon)), EFarmResult::MissingPiece);
			TestEqual(TEXT("Platanera en la seca"), Model.CanPlant(Bancal, FName(TEXT("platanera")), FName(TEXT("hijuelo_platano")), Seca), EFarmResult::OutOfSeason);
			TestEqual(TEXT("Cultivo desconocido"), Model.CanPlant(Bancal, FName(TEXT("arroz")), FName(TEXT("arroz")), Seca), EFarmResult::UnknownPlant);
			TestEqual(TEXT("Parcela desconocida"), Model.CanPlant(99, Limonero, FarmIds::Lemon(), Seca), EFarmResult::UnknownPlot);

			TestEqual(TEXT("El limón elige el limonero en su arriate"), Model.PlantForItem(Arriate, FarmIds::Lemon(), Seca), Limonero);
			TestTrue(TEXT("El limón no sirve en un bancal"), Model.PlantForItem(Bancal, FarmIds::Lemon(), Seca).IsNone());
			TestEqual(TEXT("Planta"), Model.Plant(Arriate, Limonero, FarmIds::Lemon(), Seca), EFarmResult::Ok);
			TestEqual(TEXT("Ocupada"), Model.CanPlant(Arriate, Limonero, FarmIds::Lemon(), Seca), EFarmResult::PlotOccupied);
		});

		It("lee los ids de estación de plants.json", [this]()
		{
			ESeason Season = ESeason::Count;
			TestTrue(TEXT("seca"), FFarmModel::SeasonFromId(TEXT("seca"), Season) && Season == ESeason::Dry);
			TestTrue(TEXT("primeras_lluvias"), FFarmModel::SeasonFromId(TEXT("primeras_lluvias"), Season) && Season == ESeason::FirstRains);
			TestTrue(TEXT("monzon"), FFarmModel::SeasonFromId(TEXT("monzon"), Season) && Season == ESeason::Monsoon);
			TestTrue(TEXT("ciclones"), FFarmModel::SeasonFromId(TEXT("ciclones"), Season) && Season == ESeason::Cyclones);
			TestFalse(TEXT("desconocida"), FFarmModel::SeasonFromId(TEXT("invierno"), Season));
		});
	});

	Describe(TEXT("crecimiento"), [this, Limonero]()
	{
		It("tarda lo que dicen los datos hasta la primera cosecha", [this]()
		{
			// Tabla del balance (docs/balance/2026-09-26-progresion.md §3).
			const TMap<FName, int32> Expected = []()
			{
				TMap<FName, int32> M;
				M.Add(FName(TEXT("limonero")), 12);
				M.Add(FName(TEXT("batata")), 6);
				M.Add(FName(TEXT("taro")), 8);
				M.Add(FName(TEXT("platanera")), 8);
				M.Add(FName(TEXT("maracuya")), 8);
				M.Add(FName(TEXT("pina")), 10);
				return M;
			}();
			FFarmModel Model(DataPlants(), 1);
			for (const FPlantDef& Plant : Model.GetPlants())
			{
				TestEqual(*FString::Printf(TEXT("Días de %s"), *Plant.Id.ToString()), Plant.DaysToFirstHarvest(), Expected.FindChecked(Plant.Id));

				FFarmModel Garden(DataPlants(), 1);
				const int32 Day0 = SeasonStart(Plant.Seasons[0]);
				const int32 PlotId = PlantNew(Garden, Plant.Id, Day0);
				int32 ReadyDay = INDEX_NONE;
				for (int32 Day = Day0; Day < Day0 + 30 && ReadyDay == INDEX_NONE; ++Day)
				{
					TestFalse(TEXT("No está lista antes de tiempo"), Garden.GetStageView(PlotId).bHarvestReady);
					WaterAndEnd(Garden, PlotId, Day);
					if (Garden.GetStageView(PlotId).bHarvestReady)
					{
						ReadyDay = Day + 1;
					}
				}
				TestEqual(*FString::Printf(TEXT("Primera cosecha de %s"), *Plant.Id.ToString()), ReadyDay, FFarmModel::FirstHarvestDay(Plant, Day0));
			}
		});

		It("el limonero plantado el día 2 da fruto el día 14 pasando por sus cuatro etapas", [this, Limonero]()
		{
			FFarmModel Model(DataPlants(), 1);
			const int32 PlotId = PlantNew(Model, Limonero, 2);
			TestEqual(TEXT("Día de fruto"), FFarmModel::FirstHarvestDay(*Model.FindPlant(Limonero), 2), 14);
			TestEqual(TEXT("Recién plantado"), Model.GetStageView(PlotId).StageId, FName(TEXT("esqueje")));
			TArray<FName> Seen;
			for (int32 Day = 2; Day < 14; ++Day)
			{
				WaterAndEnd(Model, PlotId, Day);
				Seen.AddUnique(Model.GetStageView(PlotId).StageId);
			}
			const TArray<FName> Order = {FName(TEXT("esqueje")), FName(TEXT("brote")), FName(TEXT("arbol_joven")), FName(TEXT("arbol_adulto"))};
			TestTrue(TEXT("Etapas en orden"), Seen == Order);
			TestTrue(TEXT("Fruta el día 14"), Model.GetStageView(PlotId).bHarvestReady);
			TestEqual(TEXT("Da limones"), Model.FindPlant(Limonero)->Harvest.Item, FarmIds::Lemon());
		});

		It("expone la etapa y el progreso normalizado para escalar la malla", [this, Limonero]()
		{
			FFarmModel Model(DataPlants(), 1);
			const int32 PlotId = PlantNew(Model, Limonero, 2);
			for (int32 Day = 2; Day < 5; ++Day)
			{
				WaterAndEnd(Model, PlotId, Day);
			}
			const FFarmStageView View = Model.GetStageView(PlotId);
			TestTrue(TEXT("Tiene planta"), View.bHasPlant);
			TestEqual(TEXT("Etapa brote"), View.StageId, FName(TEXT("brote")));
			TestEqual(TEXT("Índice"), View.StageIndex, 1);
			TestEqual(TEXT("Cuatro etapas"), View.StageCount, 4);
			TestEqual(TEXT("Un día de cuatro en el brote"), View.StageProgress, 0.25f);
			TestEqual(TEXT("Tres de doce días"), View.OverallProgress, 0.25f);
			TestTrue(TEXT("Hoy aún no se ha regado"), View.bNeedsWater);
			Model.Water(PlotId);
			TestFalse(TEXT("Ya regado"), Model.GetStageView(PlotId).bNeedsWater);
			TestFalse(TEXT("Parcela vacía sin planta"), Model.GetStageView(Model.AddPlot(FVector::ZeroVector, {})).bHasPlant);
		});

		It("no crece sin agua suficiente", [this, Limonero]()
		{
			FFarmModel Model(DataPlants(), 1);
			const int32 Lemon = PlantNew(Model, Limonero, 2);
			const int32 Taro = PlantNew(Model, FName(TEXT("taro")), SeasonStart(ESeason::FirstRains), FVector(1000.0, 0.0, 0.0));
			for (int32 Day = 2; Day < SeasonStart(ESeason::FirstRains) + 3; ++Day)
			{
				Model.Water(Taro, 1.0f); // el taro pide dos riegos: uno no basta
				Model.EndDay(Day, 0.0f);
			}
			TestEqual(TEXT("El limonero no ha crecido"), Model.FindPlot(Lemon)->Crop.GrowthDays, 0.0f);
			TestEqual(TEXT("El taro no ha crecido"), Model.FindPlot(Taro)->Crop.GrowthDays, 0.0f);
		});

		It("la lluvia riega igual que la regadera", [this, Limonero]()
		{
			FFarmModel Model(DataPlants(), 1);
			const int32 PlotId = PlantNew(Model, Limonero, 2);
			Model.EndDay(2, FFarmModel::WateringsFromRainHours(FFarmModel::RainHoursPerWatering));
			TestEqual(TEXT("Un día de crecimiento con lluvia"), Model.FindPlot(PlotId)->Crop.GrowthDays, 1.0f);
			Model.Water(PlotId, 0.5f);
			Model.EndDay(3, 0.5f); // media lluvia + medio riego = un riego
			TestEqual(TEXT("Lluvia y riego se suman"), Model.FindPlot(PlotId)->Crop.GrowthDays, 2.0f);
			Model.EndDay(4, 0.4f);
			TestEqual(TEXT("Poca lluvia no basta"), Model.FindPlot(PlotId)->Crop.GrowthDays, 2.0f);
		});

		It("el monzón riega mucho más que la estación seca", [this]()
		{
			const FWeatherModel Weather(7);
			float DryRain = 0.0f;
			float MonsoonRain = 0.0f;
			for (int32 I = 0; I < FWeatherModel::DaysPerSeason; ++I)
			{
				DryRain += FFarmModel::RainWateringsForDay(Weather, SeasonStart(ESeason::Dry) + I);
				MonsoonRain += FFarmModel::RainWateringsForDay(Weather, SeasonStart(ESeason::Monsoon) + I);
			}
			TestTrue(TEXT("Lluvia no negativa"), DryRain >= 0.0f);
			TestTrue(*FString::Printf(TEXT("Monzón %.2f frente a seca %.2f"), MonsoonRain, DryRain), MonsoonRain > DryRain * 2.0f + 1.0f);
		});

		It("fuera de estación crece a la mitad y no da fruta nueva", [this]()
		{
			FFarmModel Model(DataPlants(), 1);
			const FName Platanera(TEXT("platanera"));
			// Plantada a mitad del monzón: 4 días en estación y el resto en la de ciclones.
			const int32 Day0 = SeasonStart(ESeason::Monsoon) + 4;
			const int32 PlotId = PlantNew(Model, Platanera, Day0);
			int32 ReadyDay = INDEX_NONE;
			for (int32 Day = Day0; Day < Day0 + 20 && ReadyDay == INDEX_NONE; ++Day)
			{
				WaterAndEnd(Model, PlotId, Day);
				if (Model.GetStageView(PlotId).bHarvestReady)
				{
					ReadyDay = Day + 1;
				}
			}
			TestEqual(TEXT("4 días enteros + 4 a medias (8 días)"), ReadyDay, Day0 + 4 + 8);

			FFarmHarvest Harvest;
			TestEqual(TEXT("Cosecha"), Model.Harvest(PlotId, ReadyDay, Harvest), EFarmResult::Ok);
			for (int32 Day = ReadyDay; Day < SeasonStart(ESeason::Count); ++Day)
			{
				WaterAndEnd(Model, PlotId, Day);
			}
			TestFalse(TEXT("Sin racimo nuevo en ciclones"), Model.GetStageView(PlotId).bHarvestReady);
			TestEqual(TEXT("Ni siquiera avanza el reloj de cosecha"), Model.FindPlot(PlotId)->Crop.HarvestClock, 0.0f);
		});

		It("el compost acelera un 50 % durante ocho días", [this]()
		{
			FFarmModel Model(DataPlants(), 1);
			const FName Batata(TEXT("batata"));
			const int32 PlotId = PlantNew(Model, Batata, 2);
			TestEqual(TEXT("Compost"), Model.ApplyCompost(PlotId), EFarmResult::Ok);
			int32 ReadyDay = INDEX_NONE;
			for (int32 Day = 2; Day < 12 && ReadyDay == INDEX_NONE; ++Day)
			{
				WaterAndEnd(Model, PlotId, Day);
				if (Model.GetStageView(PlotId).bHarvestReady)
				{
					ReadyDay = Day + 1;
				}
			}
			TestEqual(TEXT("6 días de batata en 4"), ReadyDay, 2 + 4);
			TestEqual(TEXT("Quedan 4 días de compost"), Model.FindPlot(PlotId)->CompostDaysLeft, FFarmModel::CompostDays - 4);
			for (int32 Day = 6; Day < 12; ++Day)
			{
				Model.EndDay(Day, 0.0f);
			}
			TestEqual(TEXT("Se agota"), Model.FindPlot(PlotId)->CompostDaysLeft, 0);
		});
	});

	Describe(TEXT("sequía"), [this, Limonero]()
	{
		It("sedienta, marchita y muerta tras cuatro días secos", [this]()
		{
			FFarmModel Model(DataPlants(), 1);
			const int32 PlotId = PlantNew(Model, FName(TEXT("batata")), 2);
			TestEqual(TEXT("Sana"), Model.GetHealth(PlotId), EPlantHealth::Healthy);
			Model.EndDay(2, 0.0f);
			TestEqual(TEXT("1 día"), Model.GetHealth(PlotId), EPlantHealth::Thirsty);
			Model.EndDay(3, 0.0f);
			TestEqual(TEXT("2 días"), Model.GetHealth(PlotId), EPlantHealth::Wilting);
			Model.EndDay(4, 0.0f);
			TestEqual(TEXT("3 días"), Model.GetHealth(PlotId), EPlantHealth::Wilting);
			Model.EndDay(5, 0.0f);
			TestEqual(TEXT("4 días"), Model.GetHealth(PlotId), EPlantHealth::Dead);
			TestEqual(TEXT("No se riega un muerto"), Model.Water(PlotId), EFarmResult::PlantDead);
			FFarmHarvest Harvest;
			TestEqual(TEXT("Ni se cosecha"), Model.Harvest(PlotId, 6, Harvest), EFarmResult::PlantDead);
			TestEqual(TEXT("Se arrancan los restos"), Model.ClearPlot(PlotId), EFarmResult::Ok);
			TestFalse(TEXT("Parcela libre"), Model.GetStageView(PlotId).bHasPlant);
		});

		It("un riego a tiempo la recupera", [this]()
		{
			FFarmModel Model(DataPlants(), 1);
			const int32 PlotId = PlantNew(Model, FName(TEXT("batata")), 2);
			for (int32 Day = 2; Day < 5; ++Day)
			{
				Model.EndDay(Day, 0.0f);
			}
			TestEqual(TEXT("Marchita"), Model.GetHealth(PlotId), EPlantHealth::Wilting);
			WaterAndEnd(Model, PlotId, 5);
			TestEqual(TEXT("Recuperada"), Model.GetHealth(PlotId), EPlantHealth::Healthy);
			TestEqual(TEXT("Y crece"), Model.FindPlot(PlotId)->Crop.GrowthDays, 1.0f);
		});

		It("la piña se conforma con la lluvia de la estación", [this]()
		{
			FFarmModel Model(DataPlants(), 1);
			const int32 PlotId = PlantNew(Model, FName(TEXT("pina")), 0);
			for (int32 Day = 0; Day < 10; ++Day)
			{
				Model.EndDay(Day, 0.0f);
			}
			TestEqual(TEXT("Sana sin regar"), Model.GetHealth(PlotId), EPlantHealth::Healthy);
			TestTrue(TEXT("Con fruto a los 10 días"), Model.GetStageView(PlotId).bHarvestReady);
		});

		It("el limonero nunca muere ni se arranca: solo se pausa", [this, Limonero]()
		{
			FFarmModel Model(DataPlants(), 1);
			const int32 PlotId = PlantNew(Model, Limonero, 2);
			for (int32 Day = 2; Day < 5; ++Day)
			{
				WaterAndEnd(Model, PlotId, Day);
			}
			for (int32 Day = 5; Day < 45; ++Day)
			{
				Model.EndDay(Day, 0.0f);
			}
			TestEqual(TEXT("Marchito, no muerto"), Model.GetHealth(PlotId), EPlantHealth::Wilting);
			TestEqual(TEXT("Pausado en 3 días"), Model.FindPlot(PlotId)->Crop.GrowthDays, 3.0f);
			TestEqual(TEXT("No se arranca"), Model.ClearPlot(PlotId), EFarmResult::NeverRemoved);
			TestEqual(TEXT("Ni se quita su arriate"), Model.RemovePlot(PlotId), EFarmResult::NeverRemoved);
			WaterAndEnd(Model, PlotId, 45);
			TestEqual(TEXT("Revive al regarlo"), Model.GetHealth(PlotId), EPlantHealth::Healthy);
			TestEqual(TEXT("Y sigue creciendo"), Model.FindPlot(PlotId)->Crop.GrowthDays, 4.0f);
		});
	});

	Describe(TEXT("cosecha"), [this, Limonero]()
	{
		It("el limonero repite cada tres días buenos y rinde entre 2 y 4", [this, Limonero]()
		{
			FFarmModel Model(DataPlants(), 7);
			const int32 PlotId = PlantNew(Model, Limonero, 2);
			for (int32 Day = 2; Day < 14; ++Day)
			{
				WaterAndEnd(Model, PlotId, Day);
			}
			TArray<int32> HarvestDays;
			for (int32 Day = 14; Day < 60; ++Day)
			{
				FFarmHarvest Harvest;
				if (Model.GetStageView(PlotId).bHarvestReady)
				{
					TestEqual(TEXT("Cosecha"), Model.Harvest(PlotId, Day, Harvest), EFarmResult::Ok);
					TestEqual(TEXT("Limones"), Harvest.Item, FarmIds::Lemon());
					TestTrue(TEXT("Entre 2 y 4"), Harvest.Count >= 2 && Harvest.Count <= 4);
					TestFalse(TEXT("El árbol sigue"), Harvest.bPlantRemoved);
					HarvestDays.Add(Day);
				}
				else
				{
					TestEqual(TEXT("Sin fruta no hay cosecha"), Model.Harvest(PlotId, Day, Harvest), EFarmResult::NotReady);
				}
				WaterAndEnd(Model, PlotId, Day);
			}
			TestTrue(TEXT("Varias cosechas"), HarvestDays.Num() >= 10);
			for (int32 I = 1; I < HarvestDays.Num(); ++I)
			{
				TestEqual(TEXT("Cada tres días"), HarvestDays[I] - HarvestDays[I - 1], 3);
			}
		});

		It("los tubérculos se arrancan al cosecharlos", [this]()
		{
			FFarmModel Model(DataPlants(), 1);
			const int32 PlotId = PlantNew(Model, FName(TEXT("batata")), 2);
			for (int32 Day = 2; Day < 8; ++Day)
			{
				WaterAndEnd(Model, PlotId, Day);
			}
			FFarmHarvest Harvest;
			TestEqual(TEXT("Cosecha"), Model.Harvest(PlotId, 8, Harvest), EFarmResult::Ok);
			TestTrue(TEXT("Arrancada"), Harvest.bPlantRemoved);
			TestTrue(TEXT("Parcela vacía"), Model.FindPlot(PlotId)->Crop.IsEmpty());
			TestEqual(TEXT("Otra vez"), Model.Harvest(PlotId, 8, Harvest), EFarmResult::PlotEmpty);
		});

		It("las cantidades cubren el rango de los datos", [this]()
		{
			FFarmModel Model(DataPlants(), 11);
			const FName Platanera(TEXT("platanera"));
			const FPlantDef& Def = *Model.FindPlant(Platanera);
			const int32 Day0 = SeasonStart(ESeason::FirstRains);
			TArray<int32> Plots;
			for (int32 I = 0; I < 60; ++I)
			{
				Plots.Add(PlantNew(Model, Platanera, Day0, FVector(I * 100.0, 0.0, 0.0)));
			}
			Model.AddScarecrow(FVector(3000.0, 0.0, 0.0)); // cubre las parcelas del centro, no todas
			for (int32 Day = Day0; Day < Day0 + 8; ++Day)
			{
				for (const int32 PlotId : Plots)
				{
					Model.Water(PlotId);
				}
				Model.EndDay(Day, 0.0f);
			}
			int32 Lowest = MAX_int32;
			int32 Highest = 0;
			for (const int32 PlotId : Plots)
			{
				FFarmHarvest Harvest;
				TestEqual(TEXT("Cosecha"), Model.Harvest(PlotId, Day0 + 8, Harvest), EFarmResult::Ok);
				const int32 Total = Harvest.Count + Harvest.LostToBirds;
				TestTrue(TEXT("En rango"), Total >= Def.Harvest.Min && Total <= Def.Harvest.Max);
				if (Model.IsProtectedFromBirds(PlotId))
				{
					TestEqual(TEXT("El espantapájaros protege"), Harvest.LostToBirds, 0);
				}
				Lowest = FMath::Min(Lowest, Total);
				Highest = FMath::Max(Highest, Total);
			}
			TestEqual(TEXT("Sale el mínimo"), Lowest, Def.Harvest.Min);
			TestEqual(TEXT("Sale el máximo"), Highest, Def.Harvest.Max);
		});

		It("sin espantapájaros las aves se llevan parte de la fruta", [this]()
		{
			FFarmModel Model(DataPlants(), 3);
			const FName Maracuya(TEXT("maracuya"));
			const int32 Day0 = SeasonStart(ESeason::FirstRains);
			TArray<int32> Plots;
			for (int32 I = 0; I < 40; ++I)
			{
				Plots.Add(PlantNew(Model, Maracuya, Day0, FVector(0.0, I * 5000.0, 0.0)));
			}
			TestFalse(TEXT("Sin protección"), Model.IsProtectedFromBirds(Plots[0]));
			for (int32 Day = Day0; Day < Day0 + 8; ++Day)
			{
				for (const int32 PlotId : Plots)
				{
					Model.Water(PlotId);
				}
				Model.EndDay(Day, 0.0f);
			}
			int32 Lost = 0;
			for (const int32 PlotId : Plots)
			{
				FFarmHarvest Harvest;
				Model.Harvest(PlotId, Day0 + 8, Harvest);
				TestTrue(TEXT("Nunca más de lo que hay"), Harvest.Count >= 0 && Harvest.LostToBirds >= 0);
				Lost += Harvest.LostToBirds;
			}
			TestTrue(TEXT("Las aves se llevan algo"), Lost > 0);

			Model.AddScarecrow(FVector::ZeroVector);
			TestTrue(TEXT("Protegida"), Model.IsProtectedFromBirds(Plots[0]));
			TestFalse(TEXT("Lejos no protege"), Model.IsProtectedFromBirds(Plots[1]));
			TestTrue(TEXT("Se retira"), Model.RemoveScarecrow(FVector::ZeroVector));
			TestFalse(TEXT("Ya no protege"), Model.IsProtectedFromBirds(Plots[0]));
		});
	});

	Describe(TEXT("estado"), [this, Limonero]()
	{
		It("es determinista con la misma semilla", [this, Limonero]()
		{
			auto Run = [Limonero](uint32 Seed)
			{
				FFarmModel Model(DataPlants(), Seed);
				const int32 Lemon = PlantNew(Model, Limonero, 2);
				const int32 Banana = PlantNew(Model, FName(TEXT("platanera")), 9, FVector(9000.0, 0.0, 0.0));
				TArray<int32> Yields;
				for (int32 Day = 2; Day < 40; ++Day)
				{
					for (const int32 PlotId : {Lemon, Banana})
					{
						FFarmHarvest Harvest;
						if (Model.Harvest(PlotId, Day, Harvest) == EFarmResult::Ok)
						{
							Yields.Add(Harvest.Count);
						}
						if (Day % 5 != 0)
						{
							Model.Water(PlotId);
						}
					}
					Model.EndDay(Day, Day % 7 == 0 ? 1.0f : 0.0f);
				}
				return Yields;
			};
			const TArray<int32> A = Run(42);
			TestTrue(TEXT("Hay cosechas"), A.Num() > 5);
			TestTrue(TEXT("Misma semilla, mismas cosechas"), A == Run(42));
			bool bAnyDifferent = false;
			for (uint32 Seed = 43; Seed < 53 && !bAnyDifferent; ++Seed)
			{
				bAnyDifferent = !(A == Run(Seed));
			}
			TestTrue(TEXT("Otra semilla cambia las cantidades"), bAnyDifferent);
		});

		It("cierra cada día una sola vez", [this, Limonero]()
		{
			FFarmModel Model(DataPlants(), 1);
			const int32 PlotId = PlantNew(Model, Limonero, 2);
			TestTrue(TEXT("Primer cierre"), Model.EndDay(2, 1.0f));
			TestFalse(TEXT("Repetido"), Model.EndDay(2, 1.0f));
			TestFalse(TEXT("Hacia atrás"), Model.EndDay(1, 1.0f));
			TestEqual(TEXT("Creció una vez"), Model.FindPlot(PlotId)->Crop.GrowthDays, 1.0f);
		});

		It("guarda y restaura el huerto en datos planos", [this, Limonero]()
		{
			FFarmModel Model(DataPlants(), 5);
			const int32 PlotId = PlantNew(Model, Limonero, 2);
			Model.AddScarecrow(FVector(10.0, 0.0, 0.0));
			Model.ApplyCompost(PlotId);
			for (int32 Day = 2; Day < 6; ++Day)
			{
				WaterAndEnd(Model, PlotId, Day);
			}
			FFarmState Saved = Model.GetState();

			FFarmModel Loaded(DataPlants(), 5);
			Loaded.SetState(Saved);
			for (int32 Day = 6; Day < 20; ++Day)
			{
				WaterAndEnd(Model, PlotId, Day);
				WaterAndEnd(Loaded, PlotId, Day);
			}
			FFarmHarvest A, B;
			TestEqual(TEXT("Cosecha original"), Model.Harvest(PlotId, 20, A), EFarmResult::Ok);
			TestEqual(TEXT("Cosecha restaurada"), Loaded.Harvest(PlotId, 20, B), EFarmResult::Ok);
			TestEqual(TEXT("Misma cantidad"), A.Count, B.Count);
			TestEqual(TEXT("Mismo crecimiento"), Model.FindPlot(PlotId)->Crop.GrowthDays, Loaded.FindPlot(PlotId)->Crop.GrowthDays);
			TestNotEqual(TEXT("Las parcelas nuevas no repiten id"), Loaded.AddPlot(FVector::ZeroVector, {}), PlotId);

			// Un cultivo que ya no existe en los datos se descarta al cargar.
			Saved.Plots[0].Crop.PlantId = FName(TEXT("arroz"));
			Loaded.SetState(Saved);
			TestTrue(TEXT("Descartado"), Loaded.FindPlot(PlotId)->Crop.IsEmpty());
		});
	});
}

#endif
