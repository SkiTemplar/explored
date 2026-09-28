#include "Save/SaveSystemStates.h"

// Nombres de los enums que se guardan (el índice es el valor del enumerador). Solo
// se usan en este fichero: las demás unidades guardan a través de estas funciones.

template <>
struct TSaveEnumNames<EMapMarkSource>
{
	static constexpr const TCHAR* Names[] = { TEXT("Hand"), TEXT("Spyglass"), TEXT("Sextant") };
};

template <>
struct TSaveEnumNames<EFireLevel>
{
	static constexpr const TCHAR* Names[] = { TEXT("Fogata"), TEXT("Hoguera"), TEXT("HornoArcilla") };
};

template <>
struct TSaveEnumNames<EFireStatus>
{
	static constexpr const TCHAR* Names[] = { TEXT("Unlit"), TEXT("Burning"), TEXT("Embers") };
};

template <>
struct TSaveEnumNames<EPotStatus>
{
	static constexpr const TCHAR* Names[] = { TEXT("Empty"), TEXT("Cooking"), TEXT("Done"), TEXT("Burnt") };
};

template <>
struct TSaveEnumNames<ECookTechnique>
{
	static constexpr const TCHAR* Names[] = { TEXT("Roast"), TEXT("Boil"), TEXT("Stew"), TEXT("Smoke"), TEXT("Salt"), TEXT("Dry"), TEXT("Bake") };
};

template <>
struct TSaveEnumNames<EBoatType>
{
	static constexpr const TCHAR* Names[] = { TEXT("Raft"), TEXT("Canoe"), TEXT("Outrigger"), TEXT("Limon") };
};

template <>
struct TSaveEnumNames<EBoatCondition>
{
	static constexpr const TCHAR* Names[] = { TEXT("Afloat"), TEXT("Swamped"), TEXT("Capsized"), TEXT("Wrecked") };
};

template <>
struct TSaveEnumNames<ETrapKind>
{
	static constexpr const TCHAR* Names[] = { TEXT("Nasa"), TEXT("CrabTrap"), TEXT("StoneCorral") };
};

template <>
struct TSaveEnumNames<EFishHabitat>
{
	static constexpr const TCHAR* Names[] = { TEXT("Shore"), TEXT("Lagoon"), TEXT("Reef"), TEXT("Slope"), TEXT("Deep") };
};

template <>
struct TSaveEnumNames<EFishBait>
{
	static constexpr const TCHAR* Names[] = { TEXT("None"), TEXT("Lombriz"), TEXT("Visceras"), TEXT("FrutaFermentada"), TEXT("Cangrejo"), TEXT("Senuelo") };
};

template <>
struct TSaveEnumNames<EInventorySize>
{
	static constexpr const TCHAR* Names[] = { TEXT("Pequeno"), TEXT("Mediano"), TEXT("Grande"), TEXT("DosManos") };
};

template <>
struct TSaveEnumNames<ESurvivalMode>
{
	static constexpr const TCHAR* Names[] = { TEXT("Explorer"), TEXT("Survivor"), TEXT("Castaway"), TEXT("Custom") };
};

template <>
struct TSaveEnumNames<EWeatherState>
{
	static constexpr const TCHAR* Names[] = { TEXT("Clear"), TEXT("Cloudy"), TEXT("MorningFog"), TEXT("LightRain"), TEXT("Shower"),
		TEXT("Thunderstorm"), TEXT("HeatWave"), TEXT("Gale"), TEXT("Cyclone") };
};

static_assert(UE_ARRAY_COUNT(TSaveEnumNames<EWeatherState>::Names) == static_cast<int32>(EWeatherState::Count), "Nombres de EWeatherState");
static_assert(UE_ARRAY_COUNT(TSaveEnumNames<EFishBait>::Names) == static_cast<int32>(EFishBait::Count), "Nombres de EFishBait");
static_assert(UE_ARRAY_COUNT(TSaveEnumNames<EFishHabitat>::Names) == static_cast<int32>(EFishHabitat::Count), "Nombres de EFishHabitat");
static_assert(UE_ARRAY_COUNT(TSaveEnumNames<ETrapKind>::Names) == static_cast<int32>(ETrapKind::Count), "Nombres de ETrapKind");
static_assert(UE_ARRAY_COUNT(TSaveEnumNames<EBoatType>::Names) == static_cast<int32>(EBoatType::Count), "Nombres de EBoatType");
static_assert(UE_ARRAY_COUNT(TSaveEnumNames<ECookTechnique>::Names) == static_cast<int32>(ECookTechnique::Count), "Nombres de ECookTechnique");
static_assert(UE_ARRAY_COUNT(TSaveEnumNames<EFireLevel>::Names) == static_cast<int32>(EFireLevel::Count), "Nombres de EFireLevel");

namespace SaveSystemStatesDetail
{
	/** Escribe una lista de objetos: cada elemento en su propio archivo. */
	template <typename T, typename FWriteOne>
	void WriteList(FSaveArchive& Ar, const TCHAR* Key, const TArray<T>& Items, FWriteOne&& WriteOne)
	{
		TArray<FSaveArchive> Out;
		Out.Reserve(Items.Num());
		for (const T& Item : Items)
		{
			FSaveArchive& Child = Out.AddDefaulted_GetRef();
			WriteOne(Child, Item);
		}
		Ar.Write(Key, Out);
	}

	/**
	 * Lee una lista de objetos de forma tolerante: un elemento que no es un objeto
	 * se descarta; ReadOne devuelve false para descartar uno que no tiene sentido.
	 */
	template <typename T, typename FReadOne>
	void ReadList(const FSaveArchive& Ar, const TCHAR* Key, TArray<T>& Out, FReadOne&& ReadOne)
	{
		Out.Reset();
		const FSaveValue* List = Ar.FindValue(Key);
		if (!List || !List->IsArray())
		{
			return;
		}
		for (int32 I = 0; I < List->Num(); ++I)
		{
			const FSaveValue& Entry = List->At(I);
			if (!Entry.IsObject())
			{
				continue;
			}
			T Item{};
			if (ReadOne(FSaveArchive(Entry), Item))
			{
				Out.Add(MoveTemp(Item));
			}
		}
	}

	void WriteStatValues(FSaveArchive& Ar, const FAchievementStatValues& Values)
	{
		Ar.Write(TEXT("numbers"), Values.Numbers);
		Ar.Write(TEXT("sets"), Values.Sets);
		Ar.Write(TEXT("flags"), Values.Flags);
	}

	void ReadStatValues(const FSaveArchive& Ar, FAchievementStatValues& Out)
	{
		Out = FAchievementStatValues();
		TMap<FName, double> Numbers;
		Ar.Read(TEXT("numbers"), Numbers);
		for (const TPair<FName, double>& Number : Numbers)
		{
			// "NaN" o "Infinity" (el formato los lee como reales) desbloquearían o romperían el progreso.
			if (FMath::IsFinite(Number.Value))
			{
				Out.Numbers.Add(Number.Key, Number.Value);
			}
		}
		TMap<FName, TArray<FName>> Sets;
		Ar.Read(TEXT("sets"), Sets);
		for (const TPair<FName, TArray<FName>>& Set : Sets)
		{
			// Sin repetidos: ["a", "a"] contaría dos en GetSetSize.
			TArray<FName>& Items = Out.Sets.Add(Set.Key);
			for (const FName& Item : Set.Value)
			{
				Items.AddUnique(Item);
			}
		}
		Ar.Read(TEXT("flags"), Out.Flags);
	}

	void WriteInventoryItem(FSaveArchive& Ar, const FInventoryItem& Item)
	{
		Ar.Write(TEXT("instanceId"), Item.InstanceId);
		Ar.Write(TEXT("definitionId"), Item.DefinitionId);
		Ar.Write(TEXT("weightKg"), Item.WeightKg);
		Ar.Write(TEXT("volumeLiters"), Item.VolumeLiters);
		Ar.Write(TEXT("size"), Item.Size);
		Ar.Write(TEXT("tags"), Item.Tags);
		Ar.Write(TEXT("liquidLiters"), Item.LiquidLiters);
		Ar.Write(TEXT("liquidCapacityLiters"), Item.LiquidCapacityLiters);
	}

	void ReadInventoryItem(const FSaveArchive& Ar, FInventoryItem& Out)
	{
		Out = FInventoryItem();
		Ar.Read(TEXT("instanceId"), Out.InstanceId);
		Ar.Read(TEXT("definitionId"), Out.DefinitionId);
		Ar.Read(TEXT("weightKg"), Out.WeightKg);
		Ar.Read(TEXT("volumeLiters"), Out.VolumeLiters);
		Ar.Read(TEXT("size"), Out.Size);
		Ar.Read(TEXT("tags"), Out.Tags);
		Ar.Read(TEXT("liquidLiters"), Out.LiquidLiters);
		Ar.Read(TEXT("liquidCapacityLiters"), Out.LiquidCapacityLiters);
	}

	void WriteItemField(FSaveArchive& Ar, const TCHAR* Key, const FInventoryItem& Item)
	{
		FSaveArchive Child;
		WriteInventoryItem(Child, Item);
		Ar.Write(Key, Child);
	}

	void ReadItemField(const FSaveArchive& Ar, const TCHAR* Key, FInventoryItem& Out)
	{
		FSaveArchive Child;
		Out = FInventoryItem();
		if (Ar.Read(Key, Child))
		{
			ReadInventoryItem(Child, Out);
		}
	}

	void WriteContainerField(FSaveArchive& Ar, const TCHAR* Key, const FInventoryContainer& Container)
	{
		FSaveArchive Child;
		ExploredSaveStates::SaveInventoryContainer(Child, Container);
		Ar.Write(Key, Child);
	}

	void ReadContainerField(const FSaveArchive& Ar, const TCHAR* Key, FInventoryContainer& Out)
	{
		FSaveArchive Child;
		Out = FInventoryContainer();
		if (Ar.Read(Key, Child))
		{
			ExploredSaveStates::LoadInventoryContainer(Child, Out);
		}
	}

	/**
	 * Coordenada de celda guardada como real: false si no es finita o no cabe en
	 * int32 ("cell": [1e30, 0, 0] o ["NaN", 0, 0]); convertirla sería UB.
	 */
	bool RoundCellCoord(double Value, int32& Out)
	{
		if (!FMath::IsFinite(Value) || Value < -2147483648.0 || Value > 2147483647.0)
		{
			return false;
		}
		Out = FMath::RoundToInt(Value);
		return true;
	}

	/**
	 * Real cargado dentro de [Lo, Hi]; uno no finito toma Default. El formato lee
	 * "NaN" e "Infinity" como reales y FMath::Clamp(NaN) devolvería Hi.
	 */
	float SaneFloat(float Value, float Lo, float Hi, float Default)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, Lo, Hi) : Default;
	}

	/** Une B en A sin repetir y conservando el orden de A. */
	void UnionInto(TArray<FName>& A, const TArray<FName>& B)
	{
		for (const FName& Item : B)
		{
			A.AddUnique(Item);
		}
	}
}

namespace ExploredSaveStates
{
	using namespace SaveSystemStatesDetail;

	// --- Construcción ------------------------------------------------------------------

	void SaveBuilding(FSaveArchive& Ar, const FBuildingSaveState& State)
	{
		Ar.Write(TEXT("version"), State.Version);
		Ar.Write(TEXT("nextPieceId"), State.NextPieceId);
		Ar.Write(TEXT("nextBaseId"), State.NextBaseId);
		WriteList(Ar, TEXT("bases"), State.Bases, [](FSaveArchive& Out, const FBuildingBaseState& Base)
		{
			Out.Write(TEXT("id"), Base.Id);
			Out.Write(TEXT("origin"), Base.Origin);
		});
		WriteList(Ar, TEXT("pieces"), State.Pieces, [](FSaveArchive& Out, const FBuildingPieceState& Piece)
		{
			Out.Write(TEXT("id"), Piece.Id);
			Out.Write(TEXT("defId"), Piece.DefId);
			Out.Write(TEXT("baseId"), Piece.Placement.BaseId);
			Out.Write(TEXT("cell"), FVector(Piece.Placement.Cell.X, Piece.Placement.Cell.Y, Piece.Placement.Cell.Z));
			Out.Write(TEXT("rotation"), Piece.Placement.Rotation);
			Out.Write(TEXT("integrity"), Piece.Integrity);
			Out.Write(TEXT("groundContact"), Piece.bGroundContact);
			Out.Write(TEXT("lit"), Piece.bLit);
		});
	}

	void LoadBuilding(const FSaveArchive& Ar, FBuildingSaveState& OutState)
	{
		OutState = FBuildingSaveState();
		Ar.Read(TEXT("version"), OutState.Version);
		Ar.Read(TEXT("nextPieceId"), OutState.NextPieceId);
		Ar.Read(TEXT("nextBaseId"), OutState.NextBaseId);
		ReadList(Ar, TEXT("bases"), OutState.Bases, [](const FSaveArchive& In, FBuildingBaseState& Base)
		{
			return In.Read(TEXT("id"), Base.Id) && In.Read(TEXT("origin"), Base.Origin);
		});
		ReadList(Ar, TEXT("pieces"), OutState.Pieces, [](const FSaveArchive& In, FBuildingPieceState& Piece)
		{
			FVector Cell = FVector::ZeroVector;
			if (!In.Read(TEXT("id"), Piece.Id) || !In.Read(TEXT("defId"), Piece.DefId) || !In.Read(TEXT("cell"), Cell)
				|| !RoundCellCoord(Cell.X, Piece.Placement.Cell.X) || !RoundCellCoord(Cell.Y, Piece.Placement.Cell.Y)
				|| !RoundCellCoord(Cell.Z, Piece.Placement.Cell.Z))
			{
				return false;
			}
			In.Read(TEXT("baseId"), Piece.Placement.BaseId);
			In.Read(TEXT("rotation"), Piece.Placement.Rotation);
			In.Read(TEXT("integrity"), Piece.Integrity);
			In.Read(TEXT("groundContact"), Piece.bGroundContact);
			In.Read(TEXT("lit"), Piece.bLit);
			return true;
		});
	}

	// --- Huerto --------------------------------------------------------------------------

	void SaveFarm(FSaveArchive& Ar, const FFarmState& State)
	{
		Ar.Write(TEXT("lastEndedDay"), State.LastEndedDay);
		Ar.Write(TEXT("nextPlotId"), State.NextPlotId);
		Ar.Write(TEXT("scarecrows"), State.Scarecrows);
		WriteList(Ar, TEXT("plots"), State.Plots, [](FSaveArchive& Out, const FFarmPlot& Plot)
		{
			Out.Write(TEXT("id"), Plot.Id);
			Out.Write(TEXT("location"), Plot.Location);
			Out.Write(TEXT("pieces"), Plot.Pieces);
			Out.Write(TEXT("compostDaysLeft"), Plot.CompostDaysLeft);
			FSaveArchive Crop;
			Crop.Write(TEXT("plantId"), Plot.Crop.PlantId);
			Crop.Write(TEXT("plantedDay"), Plot.Crop.PlantedDay);
			Crop.Write(TEXT("growthDays"), Plot.Crop.GrowthDays);
			Crop.Write(TEXT("harvestClock"), Plot.Crop.HarvestClock);
			Crop.Write(TEXT("waterToday"), Plot.Crop.WaterToday);
			Crop.Write(TEXT("dryDays"), Plot.Crop.DryDays);
			Crop.Write(TEXT("harvestReady"), Plot.Crop.bHarvestReady);
			Crop.Write(TEXT("dead"), Plot.Crop.bDead);
			Crop.Write(TEXT("harvests"), Plot.Crop.Harvests);
			Out.Write(TEXT("crop"), Crop);
		});
	}

	void LoadFarm(const FSaveArchive& Ar, FFarmState& OutState)
	{
		OutState = FFarmState();
		Ar.Read(TEXT("lastEndedDay"), OutState.LastEndedDay);
		Ar.Read(TEXT("nextPlotId"), OutState.NextPlotId);
		Ar.Read(TEXT("scarecrows"), OutState.Scarecrows);
		ReadList(Ar, TEXT("plots"), OutState.Plots, [](const FSaveArchive& In, FFarmPlot& Plot)
		{
			if (!In.Read(TEXT("id"), Plot.Id))
			{
				return false;
			}
			In.Read(TEXT("location"), Plot.Location);
			In.Read(TEXT("pieces"), Plot.Pieces);
			In.Read(TEXT("compostDaysLeft"), Plot.CompostDaysLeft);
			FSaveArchive Crop;
			if (In.Read(TEXT("crop"), Crop))
			{
				Crop.Read(TEXT("plantId"), Plot.Crop.PlantId);
				Crop.Read(TEXT("plantedDay"), Plot.Crop.PlantedDay);
				Crop.Read(TEXT("growthDays"), Plot.Crop.GrowthDays);
				Crop.Read(TEXT("harvestClock"), Plot.Crop.HarvestClock);
				Crop.Read(TEXT("waterToday"), Plot.Crop.WaterToday);
				Crop.Read(TEXT("dryDays"), Plot.Crop.DryDays);
				Crop.Read(TEXT("harvestReady"), Plot.Crop.bHarvestReady);
				Crop.Read(TEXT("dead"), Plot.Crop.bDead);
				Crop.Read(TEXT("harvests"), Plot.Crop.Harvests);
			}
			return true;
		});
	}

	// --- Mapa --------------------------------------------------------------------------------

	void SaveCartography(FSaveArchive& Ar, const FCartographyState& State)
	{
		WriteList(Ar, TEXT("strokes"), State.Strokes, [](FSaveArchive& Out, const FMapStroke& Stroke)
		{
			Out.Write(TEXT("island"), Stroke.IslandIndex);
			Out.Write(TEXT("points"), Stroke.Points);
			Out.Write(TEXT("ink"), Stroke.Ink);
			Out.Write(TEXT("blur"), Stroke.Blur);
			Out.Write(TEXT("toleranceMeters"), Stroke.ToleranceMeters);
		});
		WriteList(Ar, TEXT("sketches"), State.Sketches, [](FSaveArchive& Out, const FMapSketch& Sketch)
		{
			Out.Write(TEXT("island"), Sketch.IslandIndex);
			Out.Write(TEXT("points"), Sketch.Points);
			Out.Write(TEXT("confirmed"), Sketch.Confirmed);
			Out.Write(TEXT("ink"), Sketch.Ink);
		});
		WriteList(Ar, TEXT("marks"), State.Marks, [](FSaveArchive& Out, const FMapMark& Mark)
		{
			Out.Write(TEXT("stamp"), Mark.StampId);
			Out.Write(TEXT("position"), Mark.Position);
			Out.Write(TEXT("text"), Mark.Text);
			Out.Write(TEXT("source"), Mark.Source);
			Out.Write(TEXT("ink"), Mark.Ink);
		});
		WriteList(Ar, TEXT("recipes"), State.Recipes, [](FSaveArchive& Out, const FMapRecipeNote& Note)
		{
			Out.Write(TEXT("recipe"), Note.RecipeId);
			Out.Write(TEXT("doodle"), Note.bDoodle);
		});
		WriteList(Ar, TEXT("coverage"), State.Coverage, [](FSaveArchive& Out, const FMapIslandCoverage& Coverage)
		{
			Out.Write(TEXT("island"), Coverage.IslandIndex);
			Out.Write(TEXT("coast"), Coverage.Coast);
			Out.Write(TEXT("visited"), Coverage.Visited);
		});
		Ar.Write(TEXT("wetness"), State.Wetness);
		Ar.Write(TEXT("inkRunProgress"), State.InkRunProgress);
		Ar.Write(TEXT("inkRuns"), State.InkRuns);
		Ar.Write(TEXT("hasDrawnAnyCoast"), State.bHasDrawnAnyCoast);
		Ar.Write(TEXT("drift"), State.Drift);
		Ar.Write(TEXT("travelDistance"), State.TravelDistance);
		Ar.Write(TEXT("markSerial"), State.MarkSerial);
	}

	void LoadCartography(const FSaveArchive& Ar, FCartographyState& OutState)
	{
		OutState = FCartographyState();
		ReadList(Ar, TEXT("strokes"), OutState.Strokes, [](const FSaveArchive& In, FMapStroke& Stroke)
		{
			In.Read(TEXT("island"), Stroke.IslandIndex);
			In.Read(TEXT("ink"), Stroke.Ink);
			In.Read(TEXT("blur"), Stroke.Blur);
			In.Read(TEXT("toleranceMeters"), Stroke.ToleranceMeters);
			return In.Read(TEXT("points"), Stroke.Points);
		});
		ReadList(Ar, TEXT("sketches"), OutState.Sketches, [](const FSaveArchive& In, FMapSketch& Sketch)
		{
			In.Read(TEXT("island"), Sketch.IslandIndex);
			In.Read(TEXT("ink"), Sketch.Ink);
			if (!In.Read(TEXT("points"), Sketch.Points))
			{
				return false;
			}
			In.Read(TEXT("confirmed"), Sketch.Confirmed);
			// Un boceto con una marca por punto: si no cuadra, se da por no confirmado.
			if (Sketch.Confirmed.Num() != Sketch.Points.Num())
			{
				Sketch.Confirmed.Init(0, Sketch.Points.Num());
			}
			return true;
		});
		ReadList(Ar, TEXT("marks"), OutState.Marks, [](const FSaveArchive& In, FMapMark& Mark)
		{
			In.Read(TEXT("stamp"), Mark.StampId);
			In.Read(TEXT("text"), Mark.Text);
			In.Read(TEXT("source"), Mark.Source);
			In.Read(TEXT("ink"), Mark.Ink);
			return In.Read(TEXT("position"), Mark.Position);
		});
		ReadList(Ar, TEXT("recipes"), OutState.Recipes, [](const FSaveArchive& In, FMapRecipeNote& Note)
		{
			In.Read(TEXT("doodle"), Note.bDoodle);
			return In.Read(TEXT("recipe"), Note.RecipeId);
		});
		ReadList(Ar, TEXT("coverage"), OutState.Coverage, [](const FSaveArchive& In, FMapIslandCoverage& Coverage)
		{
			if (!In.Read(TEXT("island"), Coverage.IslandIndex) || !In.Read(TEXT("coast"), Coverage.Coast))
			{
				return false;
			}
			In.Read(TEXT("visited"), Coverage.Visited);
			if (Coverage.Visited.Num() != Coverage.Coast.Num())
			{
				Coverage.Visited.Init(0, Coverage.Coast.Num());
			}
			return true;
		});
		Ar.Read(TEXT("wetness"), OutState.Wetness);
		Ar.Read(TEXT("inkRunProgress"), OutState.InkRunProgress);
		Ar.Read(TEXT("inkRuns"), OutState.InkRuns);
		Ar.Read(TEXT("hasDrawnAnyCoast"), OutState.bHasDrawnAnyCoast);
		Ar.Read(TEXT("drift"), OutState.Drift);
		Ar.Read(TEXT("travelDistance"), OutState.TravelDistance);
		Ar.Read(TEXT("markSerial"), OutState.MarkSerial);
	}

	// --- Ruinas y museo ------------------------------------------------------------------

	void SaveRuins(FSaveArchive& Ar, const FRuinsState& Ruins, const FMuseumState& Museum)
	{
		Ar.Write(TEXT("discoveredElements"), Ruins.DiscoveredElements);
		Ar.Write(TEXT("completedSites"), Ruins.CompletedSites);
		WriteList(Ar, TEXT("artifacts"), Museum.Records, [](FSaveArchive& Out, const FArtifactRecord& Record)
		{
			Out.Write(TEXT("id"), Record.ArtifactId);
			Out.Write(TEXT("found"), Record.bFound);
			Out.Write(TEXT("photographed"), Record.bPhotographed);
			Out.Write(TEXT("foundAt"), Record.FoundAt);
			Out.Write(TEXT("foundIsland"), Record.FoundIsland);
		});
		WriteList(Ar, TEXT("displays"), Museum.Displays, [](FSaveArchive& Out, const FDisplayState& Display)
		{
			Out.Write(TEXT("key"), Display.Key);
			Out.Write(TEXT("display"), Display.DisplayId);
			Out.Write(TEXT("slots"), Display.Slots);
		});
	}

	void LoadRuins(const FSaveArchive& Ar, FRuinsState& OutRuins, FMuseumState& OutMuseum)
	{
		OutRuins = FRuinsState();
		OutMuseum = FMuseumState();
		Ar.Read(TEXT("discoveredElements"), OutRuins.DiscoveredElements);
		Ar.Read(TEXT("completedSites"), OutRuins.CompletedSites);
		ReadList(Ar, TEXT("artifacts"), OutMuseum.Records, [](const FSaveArchive& In, FArtifactRecord& Record)
		{
			In.Read(TEXT("found"), Record.bFound);
			In.Read(TEXT("photographed"), Record.bPhotographed);
			In.Read(TEXT("foundAt"), Record.FoundAt);
			In.Read(TEXT("foundIsland"), Record.FoundIsland);
			return In.Read(TEXT("id"), Record.ArtifactId);
		});
		ReadList(Ar, TEXT("displays"), OutMuseum.Displays, [](const FSaveArchive& In, FDisplayState& Display)
		{
			In.Read(TEXT("slots"), Display.Slots);
			return In.Read(TEXT("key"), Display.Key) && In.Read(TEXT("display"), Display.DisplayId);
		});
	}

	// --- Fuego -----------------------------------------------------------------------------

	void SaveFire(FSaveArchive& Ar, const FSavedFire& Fire)
	{
		Ar.Write(TEXT("location"), Fire.Location);
		FSaveArchive State;
		State.Write(TEXT("level"), Fire.Fire.Level);
		State.Write(TEXT("status"), Fire.Fire.Status);
		State.Write(TEXT("fuelHours"), Fire.Fire.FuelHours);
		State.Write(TEXT("fuelHeat"), Fire.Fire.FuelHeat);
		State.Write(TEXT("tinderCharges"), Fire.Fire.TinderCharges);
		State.Write(TEXT("dampness"), Fire.Fire.Dampness);
		State.Write(TEXT("emberHours"), Fire.Fire.EmberHours);
		State.Write(TEXT("signalSmokeHours"), Fire.Fire.SignalSmokeHours);
		State.Write(TEXT("heat"), Fire.Fire.Heat);
		State.Write(TEXT("smoke"), Fire.Fire.Smoke);
		Ar.Write(TEXT("fire"), State);
		FSaveArchive Pot;
		Pot.Write(TEXT("status"), Fire.Pot.Status);
		Pot.Write(TEXT("technique"), Fire.Pot.Technique);
		Pot.Write(TEXT("vessel"), Fire.Pot.VesselId);
		Pot.Write(TEXT("recipe"), Fire.Pot.RecipeId);
		Pot.Write(TEXT("ingredients"), Fire.Pot.IngredientIds);
		Pot.Write(TEXT("progressMinutes"), Fire.Pot.ProgressMinutes);
		Ar.Write(TEXT("pot"), Pot);
	}

	void LoadFire(const FSaveArchive& Ar, FSavedFire& OutFire)
	{
		OutFire = FSavedFire();
		Ar.Read(TEXT("location"), OutFire.Location);
		FSaveArchive State;
		if (Ar.Read(TEXT("fire"), State))
		{
			State.Read(TEXT("level"), OutFire.Fire.Level);
			State.Read(TEXT("status"), OutFire.Fire.Status);
			State.Read(TEXT("fuelHours"), OutFire.Fire.FuelHours);
			State.Read(TEXT("fuelHeat"), OutFire.Fire.FuelHeat);
			State.Read(TEXT("tinderCharges"), OutFire.Fire.TinderCharges);
			State.Read(TEXT("dampness"), OutFire.Fire.Dampness);
			State.Read(TEXT("emberHours"), OutFire.Fire.EmberHours);
			State.Read(TEXT("signalSmokeHours"), OutFire.Fire.SignalSmokeHours);
			State.Read(TEXT("heat"), OutFire.Fire.Heat);
			State.Read(TEXT("smoke"), OutFire.Fire.Smoke);
			// El formato admite NaN/Infinity: un combustible NaN dejaría el fuego ardiendo para siempre.
			FFireModel::Sanitize(OutFire.Fire);
		}
		FSaveArchive Pot;
		if (Ar.Read(TEXT("pot"), Pot))
		{
			Pot.Read(TEXT("status"), OutFire.Pot.Status);
			Pot.Read(TEXT("technique"), OutFire.Pot.Technique);
			Pot.Read(TEXT("vessel"), OutFire.Pot.VesselId);
			Pot.Read(TEXT("recipe"), OutFire.Pot.RecipeId);
			Pot.Read(TEXT("ingredients"), OutFire.Pot.IngredientIds);
			Pot.Read(TEXT("progressMinutes"), OutFire.Pot.ProgressMinutes);
		}
	}

	// --- Eventos -----------------------------------------------------------------------------

	void SaveWorldEvents(FSaveArchive& Ar, const FWorldEventsState& State)
	{
		Ar.Write(TEXT("consumedOutcomes"), State.ConsumedOutcomes);
	}

	void LoadWorldEvents(const FSaveArchive& Ar, FWorldEventsState& OutState)
	{
		OutState = FWorldEventsState();
		Ar.Read(TEXT("consumedOutcomes"), OutState.ConsumedOutcomes);
	}

	// --- Logros ------------------------------------------------------------------------------

	void SaveAchievements(FSaveArchive& Ar, const FAchievementsState& State)
	{
		FSaveArchive Profile, Run;
		WriteStatValues(Profile, State.Profile);
		WriteStatValues(Run, State.Run);
		Ar.Write(TEXT("profile"), Profile);
		Ar.Write(TEXT("run"), Run);
		Ar.Write(TEXT("runMode"), State.RunMode);
		Ar.Write(TEXT("unlocked"), State.Unlocked);
	}

	void LoadAchievements(const FSaveArchive& Ar, FAchievementsState& OutState)
	{
		OutState = FAchievementsState();
		FSaveArchive Profile, Run;
		if (Ar.Read(TEXT("profile"), Profile))
		{
			ReadStatValues(Profile, OutState.Profile);
		}
		if (Ar.Read(TEXT("run"), Run))
		{
			ReadStatValues(Run, OutState.Run);
		}
		Ar.Read(TEXT("runMode"), OutState.RunMode);
		Ar.Read(TEXT("unlocked"), OutState.Unlocked);
	}

	FAchievementsState MergeLoadedAchievements(const FAchievementsState& Current, const FAchievementsState& Loaded)
	{
		FAchievementsState Out;
		Out.Run = Loaded.Run;
		Out.RunMode = Loaded.RunMode;

		Out.Profile = Current.Profile;
		for (const TPair<FName, double>& Number : Loaded.Profile.Numbers)
		{
			// Max(Valor, NaN) devuelve NaN y pisaría el progreso del perfil.
			if (!FMath::IsFinite(Number.Value))
			{
				continue;
			}
			double& Value = Out.Profile.Numbers.FindOrAdd(Number.Key, Number.Value);
			Value = FMath::Max(Value, Number.Value);
		}
		for (const TPair<FName, TArray<FName>>& Set : Loaded.Profile.Sets)
		{
			UnionInto(Out.Profile.Sets.FindOrAdd(Set.Key), Set.Value);
		}
		UnionInto(Out.Profile.Flags, Loaded.Profile.Flags);

		Out.Unlocked = Current.Unlocked;
		UnionInto(Out.Unlocked, Loaded.Unlocked);
		return Out;
	}

	// --- Inventario ----------------------------------------------------------------------------

	void SaveInventoryContainer(FSaveArchive& Ar, const FInventoryContainer& Container)
	{
		Ar.Write(TEXT("id"), Container.Id);
		FSaveArchive Spec;
		Spec.Write(TEXT("maxSlots"), Container.Spec.MaxSlots);
		Spec.Write(TEXT("maxVolumeLiters"), Container.Spec.MaxVolumeLiters);
		Spec.Write(TEXT("maxWeightKg"), Container.Spec.MaxWeightKg);
		Spec.Write(TEXT("maxSize"), Container.Spec.MaxSize);
		Spec.Write(TEXT("acceptedTags"), Container.Spec.AcceptedTags);
		Spec.Write(TEXT("waterproof"), Container.Spec.bWaterproof);
		Ar.Write(TEXT("spec"), Spec);
		WriteList(Ar, TEXT("entries"), Container.Entries, [](FSaveArchive& Out, const FInventoryEntry& Entry)
		{
			WriteInventoryItem(Out, Entry.Item);
			Out.Write(TEXT("slotIndex"), Entry.SlotIndex);
		});
	}

	void LoadInventoryContainer(const FSaveArchive& Ar, FInventoryContainer& OutContainer)
	{
		OutContainer = FInventoryContainer();
		Ar.Read(TEXT("id"), OutContainer.Id);
		FSaveArchive Spec;
		if (Ar.Read(TEXT("spec"), Spec))
		{
			Spec.Read(TEXT("maxSlots"), OutContainer.Spec.MaxSlots);
			Spec.Read(TEXT("maxVolumeLiters"), OutContainer.Spec.MaxVolumeLiters);
			Spec.Read(TEXT("maxWeightKg"), OutContainer.Spec.MaxWeightKg);
			Spec.Read(TEXT("maxSize"), OutContainer.Spec.MaxSize);
			Spec.Read(TEXT("acceptedTags"), OutContainer.Spec.AcceptedTags);
			Spec.Read(TEXT("waterproof"), OutContainer.Spec.bWaterproof);
		}
		ReadList(Ar, TEXT("entries"), OutContainer.Entries, [](const FSaveArchive& In, FInventoryEntry& Entry)
		{
			ReadInventoryItem(In, Entry.Item);
			In.Read(TEXT("slotIndex"), Entry.SlotIndex);
			// Un id negativo o enorme haría rechazar el inventario entero en ValidateState:
			// se pierde ese objeto y no todo lo demás.
			return Entry.Item.IsValid() && FInventoryModel::IsUsableInstanceId(Entry.Item.InstanceId);
		});
	}

	void SaveInventory(FSaveArchive& Ar, const FInventoryState& State)
	{
		WriteItemField(Ar, TEXT("handLeft"), State.HandLeft);
		WriteItemField(Ar, TEXT("handRight"), State.HandRight);
		Ar.Write(TEXT("handsHoldTwoHanded"), State.bHandsHoldTwoHanded);
		WriteContainerField(Ar, TEXT("pockets"), State.Pockets);
		WriteContainerField(Ar, TEXT("belt"), State.Belt);
		WriteContainerField(Ar, TEXT("pouch"), State.Pouch);
		WriteContainerField(Ar, TEXT("backpack"), State.Backpack);
		WriteContainerField(Ar, TEXT("sledge"), State.Sledge);
		Ar.Write(TEXT("hasBackpack"), State.bHasBackpack);
		WriteItemField(Ar, TEXT("backpackItem"), State.BackpackItem);
		Ar.Write(TEXT("backpackComfortBonusKg"), State.BackpackComfortBonusKg);
		Ar.Write(TEXT("backpackWaterproofPocket"), State.bBackpackWaterproofPocket);
		WriteItemField(Ar, TEXT("beltItem"), State.BeltItem);
		Ar.Write(TEXT("hasSledge"), State.bHasSledge);
		WriteItemField(Ar, TEXT("sledgeItem"), State.SledgeItem);
		Ar.Write(TEXT("nextInstanceId"), State.NextInstanceId);
	}

	void LoadInventory(const FSaveArchive& Ar, FInventoryState& OutState)
	{
		OutState = FInventoryState();
		ReadItemField(Ar, TEXT("handLeft"), OutState.HandLeft);
		ReadItemField(Ar, TEXT("handRight"), OutState.HandRight);
		Ar.Read(TEXT("handsHoldTwoHanded"), OutState.bHandsHoldTwoHanded);
		ReadContainerField(Ar, TEXT("pockets"), OutState.Pockets);
		ReadContainerField(Ar, TEXT("belt"), OutState.Belt);
		ReadContainerField(Ar, TEXT("pouch"), OutState.Pouch);
		ReadContainerField(Ar, TEXT("backpack"), OutState.Backpack);
		ReadContainerField(Ar, TEXT("sledge"), OutState.Sledge);
		Ar.Read(TEXT("hasBackpack"), OutState.bHasBackpack);
		ReadItemField(Ar, TEXT("backpackItem"), OutState.BackpackItem);
		Ar.Read(TEXT("backpackComfortBonusKg"), OutState.BackpackComfortBonusKg);
		Ar.Read(TEXT("backpackWaterproofPocket"), OutState.bBackpackWaterproofPocket);
		ReadItemField(Ar, TEXT("beltItem"), OutState.BeltItem);
		Ar.Read(TEXT("hasSledge"), OutState.bHasSledge);
		ReadItemField(Ar, TEXT("sledgeItem"), OutState.SledgeItem);
		Ar.Read(TEXT("nextInstanceId"), OutState.NextInstanceId);
	}

	void SaveItemNodes(FSaveArchive& Ar, const FString& Key, const TArray<FSavedItemNode>& Nodes)
	{
		WriteList(Ar, *Key, Nodes, [](FSaveArchive& Out, const FSavedItemNode& Node)
		{
			Out.Write(TEXT("parent"), Node.Parent);
			Out.Write(TEXT("def"), Node.DefinitionId);
			Out.Write(TEXT("quality"), Node.Quality);
			Out.Write(TEXT("durability"), Node.Durability);
			Out.Write(TEXT("count"), Node.Count);
			Out.Write(TEXT("liquidLiters"), Node.LiquidLiters);
			if (!Node.GeneratedName.IsEmpty())
			{
				Out.Write(TEXT("name"), Node.GeneratedName);
			}
		});
	}

	void LoadItemNodes(const FSaveArchive& Ar, const FString& Key, TArray<FSavedItemNode>& OutNodes)
	{
		// Se leen todos (también los rotos) para que los índices de padre sigan cuadrando;
		// RebuildItemTrees descarta después los que no encajan.
		OutNodes.Reset();
		const FSaveValue* List = Ar.FindValue(Key);
		if (!List || !List->IsArray())
		{
			return;
		}
		for (int32 I = 0; I < List->Num(); ++I)
		{
			FSavedItemNode& Node = OutNodes.AddDefaulted_GetRef();
			const FSaveArchive In(List->At(I));
			if (!In.Read(TEXT("parent"), Node.Parent) || Node.Parent >= I)
			{
				// Padre ilegible o posterior: se marca como roto (padre imposible).
				Node.Parent = I;
			}
			In.Read(TEXT("def"), Node.DefinitionId);
			In.Read(TEXT("quality"), Node.Quality);
			In.Read(TEXT("durability"), Node.Durability);
			In.Read(TEXT("count"), Node.Count);
			In.Read(TEXT("liquidLiters"), Node.LiquidLiters);
			In.Read(TEXT("name"), Node.GeneratedName);
		}
	}

	// --- Cuerpo ----------------------------------------------------------------------------------

	void SaveSurvival(FSaveArchive& Ar, const FSurvivalState& State, ESurvivalMode Mode)
	{
		Ar.Write(TEXT("mode"), Mode);
		Ar.Write(TEXT("health"), State.Health);
		Ar.Write(TEXT("hunger"), State.Hunger);
		Ar.Write(TEXT("thirst"), State.Thirst);
		Ar.Write(TEXT("energy"), State.Energy);
		Ar.Write(TEXT("rest"), State.Rest);
		Ar.Write(TEXT("morale"), State.Morale);
		Ar.Write(TEXT("bodyTemperature"), State.BodyTemperature);
		Ar.Write(TEXT("wetness"), State.Wetness);
		Ar.Write(TEXT("protein"), State.Protein);
		Ar.Write(TEXT("carbs"), State.Carbs);
		Ar.Write(TEXT("vitamins"), State.Vitamins);
		TArray<float> Conditions;
		for (const float Hours : State.ConditionTime)
		{
			Conditions.Add(Hours);
		}
		Ar.Write(TEXT("conditionHours"), Conditions);
		Ar.Write(TEXT("scurvySeverity"), State.ScurvySeverity);
		Ar.Write(TEXT("sunDose"), State.SunDose);
		Ar.Write(TEXT("monotonyHours"), State.MonotonyHours);
		WriteList(Ar, TEXT("wounds"), State.Wounds, [](FSaveArchive& Out, const FWound& Wound)
		{
			Out.Write(TEXT("depth"), Wound.Depth);
			Out.Write(TEXT("bleeding"), Wound.Bleeding);
			Out.Write(TEXT("hoursUntreated"), Wound.HoursUntreated);
			Out.Write(TEXT("healed"), Wound.Healed);
			Out.Write(TEXT("bandaged"), Wound.bBandaged);
			Out.Write(TEXT("medicinal"), Wound.bMedicinal);
			Out.Write(TEXT("infected"), Wound.bInfected);
		});
	}

	void LoadSurvival(const FSaveArchive& Ar, FSurvivalState& OutState, ESurvivalMode& OutMode)
	{
		OutState = FSurvivalState();
		Ar.Read(TEXT("mode"), OutMode);
		Ar.Read(TEXT("health"), OutState.Health);
		Ar.Read(TEXT("hunger"), OutState.Hunger);
		Ar.Read(TEXT("thirst"), OutState.Thirst);
		Ar.Read(TEXT("energy"), OutState.Energy);
		Ar.Read(TEXT("rest"), OutState.Rest);
		Ar.Read(TEXT("morale"), OutState.Morale);
		Ar.Read(TEXT("bodyTemperature"), OutState.BodyTemperature);
		Ar.Read(TEXT("wetness"), OutState.Wetness);
		Ar.Read(TEXT("protein"), OutState.Protein);
		Ar.Read(TEXT("carbs"), OutState.Carbs);
		Ar.Read(TEXT("vitamins"), OutState.Vitamins);
		TArray<float> Conditions;
		if (Ar.Read(TEXT("conditionHours"), Conditions))
		{
			// Estados añadidos después de guardar quedan a 0; los que sobran se ignoran.
			const int32 Num = FMath::Min(Conditions.Num(), static_cast<int32>(ECondition::Count));
			for (int32 I = 0; I < Num; ++I)
			{
				OutState.ConditionTime[I] = SaneFloat(Conditions[I], 0.0f, TNumericLimits<float>::Max(), 0.0f);
			}
		}
		Ar.Read(TEXT("scurvySeverity"), OutState.ScurvySeverity);
		Ar.Read(TEXT("sunDose"), OutState.SunDose);
		Ar.Read(TEXT("monotonyHours"), OutState.MonotonyHours);
		ReadList(Ar, TEXT("wounds"), OutState.Wounds, [](const FSaveArchive& In, FWound& Wound)
		{
			In.Read(TEXT("bleeding"), Wound.Bleeding);
			In.Read(TEXT("hoursUntreated"), Wound.HoursUntreated);
			In.Read(TEXT("healed"), Wound.Healed);
			In.Read(TEXT("bandaged"), Wound.bBandaged);
			In.Read(TEXT("medicinal"), Wound.bMedicinal);
			In.Read(TEXT("infected"), Wound.bInfected);
			// Una herida sin profundidad legible se descarta. Fuera de [0, 1] el sangrado
			// crece sin tope (profundidad 2) o cura (sangrado -50).
			if (!In.Read(TEXT("depth"), Wound.Depth) || !FMath::IsFinite(Wound.Depth))
			{
				return false;
			}
			Wound.Depth = FMath::Clamp(Wound.Depth, 0.0f, 1.0f);
			Wound.Bleeding = SaneFloat(Wound.Bleeding, 0.0f, 1.0f, Wound.Depth);
			Wound.Healed = SaneFloat(Wound.Healed, 0.0f, 1.0f, 0.0f);
			Wound.HoursUntreated = SaneFloat(Wound.HoursUntreated, 0.0f, TNumericLimits<float>::Max(), 0.0f);
			return true;
		});

		// Valores del cuerpo en los rangos que usa FSurvivalModel; uno no finito vuelve al de partida nueva.
		const FSurvivalState Defaults;
		OutState.Health = SaneFloat(OutState.Health, 0.0f, 100.0f, Defaults.Health);
		OutState.Hunger = SaneFloat(OutState.Hunger, 0.0f, 100.0f, Defaults.Hunger);
		OutState.Thirst = SaneFloat(OutState.Thirst, 0.0f, 100.0f, Defaults.Thirst);
		OutState.Energy = SaneFloat(OutState.Energy, 0.0f, 100.0f, Defaults.Energy);
		OutState.Rest = SaneFloat(OutState.Rest, 0.0f, 100.0f, Defaults.Rest);
		OutState.Morale = SaneFloat(OutState.Morale, 0.0f, 100.0f, Defaults.Morale);
		OutState.Protein = SaneFloat(OutState.Protein, 0.0f, 100.0f, Defaults.Protein);
		OutState.Carbs = SaneFloat(OutState.Carbs, 0.0f, 100.0f, Defaults.Carbs);
		OutState.Vitamins = SaneFloat(OutState.Vitamins, 0.0f, 100.0f, Defaults.Vitamins);
		// Margen amplio: el modelo nunca se aleja tanto de 37 °C.
		OutState.BodyTemperature = SaneFloat(OutState.BodyTemperature, 25.0f, 45.0f, Defaults.BodyTemperature);
		OutState.Wetness = SaneFloat(OutState.Wetness, 0.0f, 1.0f, Defaults.Wetness);
		OutState.ScurvySeverity = SaneFloat(OutState.ScurvySeverity, 0.0f, 1.0f, Defaults.ScurvySeverity);
		OutState.SunDose = SaneFloat(OutState.SunDose, 0.0f, TNumericLimits<float>::Max(), Defaults.SunDose);
		OutState.MonotonyHours = SaneFloat(OutState.MonotonyHours, 0.0f, TNumericLimits<float>::Max(), Defaults.MonotonyHours);
	}

	// --- Embarcaciones -------------------------------------------------------------------------

	void SaveBoat(FSaveArchive& Ar, const FBoatSaveData& Boat)
	{
		Ar.Write(TEXT("type"), Boat.Type);
		Ar.Write(TEXT("condition"), Boat.Condition);
		Ar.Write(TEXT("location"), Boat.LocationCm);
		Ar.Write(TEXT("yaw"), Boat.YawDeg);
		Ar.Write(TEXT("hullDamage"), Boat.HullDamage01);
		Ar.Write(TEXT("cargoKg"), Boat.CargoKg);
		Ar.Write(TEXT("waterInHullKg"), Boat.WaterInHullKg);
		Ar.Write(TEXT("sailRaised"), Boat.bSailRaised);
		// Amarre (astillero de balsas, GDD v2 §3.17): solo si lo hay, para no cambiar las partidas antiguas.
		if (Boat.bMoored)
		{
			Ar.Write(TEXT("moored"), Boat.bMoored);
			Ar.Write(TEXT("mooringAnchor"), Boat.MooringAnchorCm);
			Ar.Write(TEXT("mooringLength"), Boat.MooringLengthCm);
		}
	}

	void LoadBoat(const FSaveArchive& Ar, FBoatSaveData& OutBoat)
	{
		OutBoat = FBoatSaveData();
		Ar.Read(TEXT("type"), OutBoat.Type);
		Ar.Read(TEXT("condition"), OutBoat.Condition);
		Ar.Read(TEXT("location"), OutBoat.LocationCm);
		Ar.Read(TEXT("yaw"), OutBoat.YawDeg);
		Ar.Read(TEXT("hullDamage"), OutBoat.HullDamage01);
		Ar.Read(TEXT("cargoKg"), OutBoat.CargoKg);
		Ar.Read(TEXT("waterInHullKg"), OutBoat.WaterInHullKg);
		Ar.Read(TEXT("sailRaised"), OutBoat.bSailRaised);
		Ar.Read(TEXT("moored"), OutBoat.bMoored);
		Ar.Read(TEXT("mooringAnchor"), OutBoat.MooringAnchorCm);
		Ar.Read(TEXT("mooringLength"), OutBoat.MooringLengthCm);
	}

	// --- Pesca ------------------------------------------------------------------------------------

	void SaveFishing(FSaveArchive& Ar, const FFishingSaveState& State)
	{
		Ar.Write(TEXT("nextTrapId"), State.NextTrapId);
		WriteList(Ar, TEXT("traps"), State.Traps, [](FSaveArchive& Out, const FPlacedTrap& Trap)
		{
			Out.Write(TEXT("id"), Trap.Id);
			Out.Write(TEXT("kind"), Trap.Kind);
			Out.Write(TEXT("habitat"), Trap.Habitat);
			Out.Write(TEXT("location"), Trap.Location);
			Out.Write(TEXT("bait"), Trap.Bait);
			Out.Write(TEXT("baitLeft"), Trap.BaitLeft01);
			Out.Write(TEXT("placedAtDays"), Trap.PlacedAtDays);
			Out.Write(TEXT("simulatedToDays"), Trap.SimulatedToDays);
			WriteList(Out, TEXT("contents"), Trap.Contents, [](FSaveArchive& CatchOut, const FTrapCatch& Catch)
			{
				CatchOut.Write(TEXT("item"), Catch.ItemId);
				CatchOut.Write(TEXT("weightKg"), Catch.WeightKg);
				CatchOut.Write(TEXT("caughtAtDays"), Catch.CaughtAtDays);
			});
		});
		Ar.Write(TEXT("caughtLegendaries"), State.CaughtLegendaries);
		WriteList(Ar, TEXT("spooked"), State.Spooked, [](FSaveArchive& Out, const FLegendarySpook& Spook)
		{
			Out.Write(TEXT("id"), Spook.Id);
			Out.Write(TEXT("untilDays"), Spook.UntilDays);
		});
		WriteList(Ar, TEXT("tidePools"), State.TidePools, [](FSaveArchive& Out, const FTidePoolRecord& Pool)
		{
			Out.Write(TEXT("id"), Pool.PoolId);
			Out.Write(TEXT("lastLowTide"), Pool.LastLowTideIndex);
		});
		WriteList(Ar, TEXT("zones"), State.Zones, [](FSaveArchive& Out, const FZonePopulation& Zone)
		{
			Out.Write(TEXT("key"), Zone.ZoneKey);
			Out.Write(TEXT("depletion"), Zone.Depletion01);
			Out.Write(TEXT("updatedAtDays"), Zone.UpdatedAtDays);
		});
	}

	void LoadFishing(const FSaveArchive& Ar, FFishingSaveState& OutState)
	{
		OutState = FFishingSaveState();
		Ar.Read(TEXT("nextTrapId"), OutState.NextTrapId);
		ReadList(Ar, TEXT("traps"), OutState.Traps, [](const FSaveArchive& In, FPlacedTrap& Trap)
		{
			In.Read(TEXT("kind"), Trap.Kind);
			In.Read(TEXT("habitat"), Trap.Habitat);
			In.Read(TEXT("location"), Trap.Location);
			In.Read(TEXT("bait"), Trap.Bait);
			In.Read(TEXT("baitLeft"), Trap.BaitLeft01);
			In.Read(TEXT("placedAtDays"), Trap.PlacedAtDays);
			In.Read(TEXT("simulatedToDays"), Trap.SimulatedToDays);
			ReadList(In, TEXT("contents"), Trap.Contents, [](const FSaveArchive& CatchIn, FTrapCatch& Catch)
			{
				CatchIn.Read(TEXT("weightKg"), Catch.WeightKg);
				CatchIn.Read(TEXT("caughtAtDays"), Catch.CaughtAtDays);
				return CatchIn.Read(TEXT("item"), Catch.ItemId);
			});
			return In.Read(TEXT("id"), Trap.Id);
		});
		Ar.Read(TEXT("caughtLegendaries"), OutState.CaughtLegendaries);
		ReadList(Ar, TEXT("spooked"), OutState.Spooked, [](const FSaveArchive& In, FLegendarySpook& Spook)
		{
			In.Read(TEXT("untilDays"), Spook.UntilDays);
			return In.Read(TEXT("id"), Spook.Id);
		});
		ReadList(Ar, TEXT("tidePools"), OutState.TidePools, [](const FSaveArchive& In, FTidePoolRecord& Pool)
		{
			In.Read(TEXT("lastLowTide"), Pool.LastLowTideIndex);
			return In.Read(TEXT("id"), Pool.PoolId);
		});
		ReadList(Ar, TEXT("zones"), OutState.Zones, [](const FSaveArchive& In, FZonePopulation& Zone)
		{
			In.Read(TEXT("depletion"), Zone.Depletion01);
			In.Read(TEXT("updatedAtDays"), Zone.UpdatedAtDays);
			return In.Read(TEXT("key"), Zone.ZoneKey);
		});
	}

	// --- Hora y clima --------------------------------------------------------------------------

	void SaveClock(FSaveArchive& Ar, const FSavedClock& Clock)
	{
		Ar.Write(TEXT("day"), Clock.Day);
		Ar.Write(TEXT("hours"), Clock.Hours);
		if (Clock.ForcedWeather != EWeatherState::Count)
		{
			Ar.Write(TEXT("forcedWeather"), Clock.ForcedWeather);
			Ar.Write(TEXT("forcedUntilDays"), Clock.ForcedUntilDays);
		}
	}

	void LoadClock(const FSaveArchive& Ar, FSavedClock& OutClock)
	{
		OutClock = FSavedClock();
		Ar.Read(TEXT("day"), OutClock.Day);
		Ar.Read(TEXT("hours"), OutClock.Hours);
		OutClock.Hours = SaneFloat(OutClock.Hours, 0.0f, 23.999f, FSavedClock().Hours);
		OutClock.Day = FMath::Max(0, OutClock.Day);
		Ar.Read(TEXT("forcedWeather"), OutClock.ForcedWeather);
		Ar.Read(TEXT("forcedUntilDays"), OutClock.ForcedUntilDays);
		// "Infinity" forzaría el clima para siempre: un plazo no finito anula el forzado.
		if (!FMath::IsFinite(OutClock.ForcedUntilDays))
		{
			OutClock.ForcedWeather = EWeatherState::Count;
			OutClock.ForcedUntilDays = 0.0f;
		}
	}
}
