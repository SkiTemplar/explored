#include "Misc/AutomationTest.h"

#include "Save/SaveArchive.h"
#include "Save/SaveSystemStates.h"
#include "Save/SaveValue.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SaveSystemsTest
{
	/** Pasa un archivo por el texto de la partida y lo vuelve a leer (lo que hace el disco). */
	FSaveArchive ThroughText(const FSaveArchive& Ar)
	{
		FSaveValue Parsed;
		FString Error;
		FSaveText::Parse(FSaveText::Write(Ar.GetRoot(), ESaveTextStyle::Compact), Parsed, Error);
		return FSaveArchive(Parsed);
	}

	/** Texto canónico de un archivo: dos estados iguales dan el mismo texto. */
	FString Canonical(const FSaveArchive& Ar)
	{
		return FSaveText::Write(Ar.GetRoot(), ESaveTextStyle::Compact);
	}

	/** Objeto de prueba con piezas, como FItemInstance (que no se compila en el host). */
	struct FTestItem
	{
		FName DefinitionId;
		int32 Count = 1;
		float Durability = 1.0f;
		FString Name;
		TArray<FTestItem> Components;
	};

	FTestItem MakeAxe()
	{
		FTestItem Flake{FName(TEXT("lasca_obsidiana")), 1, 0.75f, FString(), {}};
		FTestItem Cord{FName(TEXT("cuerda_fibra")), 1, 1.0f, FString(), {}};
		FTestItem Handle{FName(TEXT("mango")), 1, 0.5f, FString(), {Cord}};
		return FTestItem{FName(TEXT("hacha")), 1, 0.9f, TEXT("Hacha de obsidiana con mango de «caña»"), {Flake, Handle}};
	}

	bool SameTree(const FTestItem& A, const FTestItem& B)
	{
		if (A.DefinitionId != B.DefinitionId || A.Count != B.Count || A.Durability != B.Durability || A.Name != B.Name ||
			A.Components.Num() != B.Components.Num())
		{
			return false;
		}
		for (int32 I = 0; I < A.Components.Num(); ++I)
		{
			if (!SameTree(A.Components[I], B.Components[I]))
			{
				return false;
			}
		}
		return true;
	}
}

BEGIN_DEFINE_SPEC(FSaveSystemsSpec, "Explored.Save.Systems",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FSaveSystemsSpec)

void FSaveSystemsSpec::Define()
{
	using namespace SaveSystemsTest;
	using namespace ExploredSaveStates;

	Describe("Construcción", [this]()
	{
		It("guarda y carga bases y piezas sin perder nada", [this]()
		{
			FBuildingSaveState State;
			State.NextPieceId = 7;
			State.NextBaseId = 3;
			State.Bases.Add({1, FVector(12345.5, -678.25, 90.0)});
			State.Bases.Add({2, FVector(-4.0, 5.0, 6.0)});
			FBuildingPieceState Piece;
			Piece.Id = 5;
			Piece.DefId = FName(TEXT("suelo_bambu"));
			Piece.Placement.BaseId = 2;
			Piece.Placement.Cell = FIntVector(-3, 4, 1);
			Piece.Placement.Rotation = 3;
			Piece.Integrity = 37.125f;
			Piece.bGroundContact = true;
			Piece.bLit = true;
			State.Pieces.Add(Piece);

			FSaveArchive Ar;
			SaveBuilding(Ar, State);
			FBuildingSaveState Loaded;
			LoadBuilding(ThroughText(Ar), Loaded);
			TestEqual(TEXT("Siguiente pieza"), Loaded.NextPieceId, 7);
			TestEqual(TEXT("Siguiente base"), Loaded.NextBaseId, 3);
			TestTrue(TEXT("Bases"), Loaded.Bases == State.Bases);
			TestTrue(TEXT("Piezas"), Loaded.Pieces == State.Pieces);
		});

		It("descarta una pieza ilegible y conserva el resto", [this]()
		{
			FBuildingSaveState State;
			FBuildingPieceState Piece;
			Piece.Id = 1;
			Piece.DefId = FName(TEXT("pilote"));
			State.Pieces.Add(Piece);
			FSaveArchive Ar;
			SaveBuilding(Ar, State);
			// Se añade a mano una pieza sin id a la lista.
			FSaveValue Pieces = *Ar.FindValue(TEXT("pieces"));
			FSaveValue Broken = FSaveValue::MakeObject();
			Broken.Set(TEXT("defId"), FSaveValue::MakeString(TEXT("pared")));
			Pieces.Add(Broken);
			Pieces.Add(FSaveValue::MakeInt(3));
			Ar.SetValue(TEXT("pieces"), Pieces);
			FBuildingSaveState Loaded;
			LoadBuilding(Ar, Loaded);
			TestEqual(TEXT("Una pieza"), Loaded.Pieces.Num(), 1);
		});
	});

	Describe("Huerto", [this]()
	{
		It("conserva parcelas, cultivo y espantapájaros", [this]()
		{
			FFarmState State;
			State.LastEndedDay = 11;
			State.NextPlotId = 4;
			State.Scarecrows.Add(FVector(1.0, 2.0, 3.0));
			FFarmPlot Plot;
			Plot.Id = 3;
			Plot.Location = FVector(100.0, 200.0, 5.5);
			Plot.Pieces = {FName(TEXT("bancal")), FName(TEXT("espaldera"))};
			Plot.CompostDaysLeft = 2;
			Plot.Crop.PlantId = FName(TEXT("limonero"));
			Plot.Crop.PlantedDay = 5;
			Plot.Crop.GrowthDays = 3.75f;
			Plot.Crop.HarvestClock = 0.5f;
			Plot.Crop.WaterToday = 1.0f;
			Plot.Crop.DryDays = 1;
			Plot.Crop.bHarvestReady = true;
			Plot.Crop.Harvests = 2;
			State.Plots.Add(Plot);

			FSaveArchive Ar;
			SaveFarm(Ar, State);
			FFarmState Loaded;
			LoadFarm(ThroughText(Ar), Loaded);
			FSaveArchive Again;
			SaveFarm(Again, Loaded);
			TestEqual(TEXT("Mismo texto"), Canonical(Again), Canonical(Ar));
			TestEqual(TEXT("Limonero"), Loaded.Plots[0].Crop.PlantId, FName(TEXT("limonero")));
			TestEqual(TEXT("Crecimiento"), Loaded.Plots[0].Crop.GrowthDays, 3.75f);
		});

		It("carga el huerto por defecto de una partida sin sección", [this]()
		{
			FFarmState Loaded;
			Loaded.NextPlotId = 99;
			LoadFarm(FSaveArchive(), Loaded);
			TestEqual(TEXT("Por defecto"), Loaded.NextPlotId, FFarmState().NextPlotId);
			TestEqual(TEXT("Sin parcelas"), Loaded.Plots.Num(), 0);
		});
	});

	Describe("Mapa", [this]()
	{
		It("conserva trazos, bocetos, marcas y cobertura", [this]()
		{
			FCartographyState State;
			FMapStroke Stroke;
			Stroke.IslandIndex = 2;
			Stroke.Points = {FVector2D(0.1, 0.2), FVector2D(0.30000000000000004, 0.4)};
			Stroke.Ink = 0.8f;
			Stroke.Blur = 0.1f;
			State.Strokes.Add(Stroke);
			FMapSketch Sketch;
			Sketch.IslandIndex = 1;
			Sketch.Points = {FVector2D(1.0, 1.0), FVector2D(2.0, 2.0), FVector2D(3.0, 1.0)};
			Sketch.Confirmed = {1, 0, 1};
			State.Sketches.Add(Sketch);
			FMapMark Mark;
			Mark.StampId = FName(TEXT("agua_dulce"));
			Mark.Position = FVector2D(0.5, 0.25);
			Mark.Text = TEXT("Arroyo junto a la cascada — ¡agua!");
			Mark.Source = EMapMarkSource::Sextant;
			State.Marks.Add(Mark);
			State.Recipes.Add({FName(TEXT("hacha")), true});
			FMapIslandCoverage Coverage;
			Coverage.IslandIndex = 0;
			Coverage.Coast = {FVector2D(1.0, 0.0), FVector2D(0.0, 1.0)};
			Coverage.Visited = {1, 0};
			State.Coverage.Add(Coverage);
			State.Wetness = 0.3f;
			State.InkRuns = 2;
			State.bHasDrawnAnyCoast = true;
			State.Drift = FVector2D(3.5, -1.25);
			State.TravelDistance = 1234.5;
			State.MarkSerial = 9;

			FSaveArchive Ar;
			SaveCartography(Ar, State);
			FCartographyState Loaded;
			LoadCartography(ThroughText(Ar), Loaded);
			FSaveArchive Again;
			SaveCartography(Again, Loaded);
			TestEqual(TEXT("Mismo texto"), Canonical(Again), Canonical(Ar));
			TestEqual(TEXT("Sextante"), Loaded.Marks[0].Source, EMapMarkSource::Sextant);
			TestEqual(TEXT("Texto con tildes"), Loaded.Marks[0].Text, Mark.Text);
			TestTrue(TEXT("Punto exacto"), Loaded.Strokes[0].Points[1] == Stroke.Points[1]);
			TestTrue(TEXT("Costa dibujada"), Loaded.bHasDrawnAnyCoast);
		});

		It("repara un boceto con marcas de confirmación que no cuadran", [this]()
		{
			FCartographyState State;
			FMapSketch Sketch;
			Sketch.Points = {FVector2D(1.0, 1.0), FVector2D(2.0, 2.0)};
			Sketch.Confirmed = {1};
			State.Sketches.Add(Sketch);
			FSaveArchive Ar;
			SaveCartography(Ar, State);
			FCartographyState Loaded;
			LoadCartography(Ar, Loaded);
			TestEqual(TEXT("Una marca por punto"), Loaded.Sketches[0].Confirmed.Num(), 2);
		});
	});

	Describe("Ruinas y museo", [this]()
	{
		It("conserva descubrimientos, tesoros y vitrinas", [this]()
		{
			FRuinsState Ruins;
			Ruins.DiscoveredElements = {FName(TEXT("petro_03")), FName(TEXT("ruin_smoke_statue"))};
			Ruins.CompletedSites = {FName(TEXT("ruin_smoke"))};
			FMuseumState Museum;
			FArtifactRecord Record;
			Record.ArtifactId = FName(TEXT("anzuelo_hueso"));
			Record.bFound = true;
			Record.FoundAt = FName(TEXT("ruin_smoke"));
			Record.FoundIsland = 2;
			Museum.Records.Add(Record);
			FDisplayState Display;
			Display.Key = 14;
			Display.DisplayId = FName(TEXT("estante_museo"));
			Display.Slots = {FName(TEXT("anzuelo_hueso")), NAME_None};
			Museum.Displays.Add(Display);

			FSaveArchive Ar;
			SaveRuins(Ar, Ruins, Museum);
			FRuinsState LoadedRuins;
			FMuseumState LoadedMuseum;
			LoadRuins(ThroughText(Ar), LoadedRuins, LoadedMuseum);
			TestTrue(TEXT("Ruinas"), LoadedRuins == Ruins);
			TestTrue(TEXT("Museo"), LoadedMuseum == Museum);
		});
	});

	Describe("Fuego", [this]()
	{
		It("conserva el fuego y la olla", [this]()
		{
			FSavedFire Fire;
			Fire.Location = FVector(10.0, 20.0, 30.0);
			Fire.Fire.Level = EFireLevel::HornoArcilla;
			Fire.Fire.Status = EFireStatus::Embers;
			Fire.Fire.FuelHours = 1.5f;
			Fire.Fire.FuelHeat = 0.7f;
			Fire.Fire.TinderCharges = 2;
			Fire.Fire.Dampness = 0.2f;
			Fire.Fire.EmberHours = 3.25f;
			Fire.Fire.SignalSmokeHours = 0.5f;
			Fire.Fire.Heat = 0.4f;
			Fire.Fire.Smoke = 0.1f;
			Fire.Pot.Status = EPotStatus::Cooking;
			Fire.Pot.Technique = ECookTechnique::Bake;
			Fire.Pot.VesselId = FName(TEXT("vasija_barro"));
			Fire.Pot.RecipeId = FName(TEXT("pan_taro"));
			Fire.Pot.IngredientIds = {FName(TEXT("taro")), FName(TEXT("sal"))};
			Fire.Pot.ProgressMinutes = 12.5f;

			FSaveArchive Ar;
			SaveFire(Ar, Fire);
			FSavedFire Loaded;
			LoadFire(ThroughText(Ar), Loaded);
			TestTrue(TEXT("Fuego"), Loaded.Fire == Fire.Fire);
			TestTrue(TEXT("Olla"), Loaded.Pot == Fire.Pot);
			TestTrue(TEXT("Sitio"), Loaded.Location == Fire.Location);
		});
	});

	Describe("Eventos", [this]()
	{
		It("conserva los resultados consumidos con ids de 64 bits", [this]()
		{
			FWorldEventsState State;
			State.Consume(FWorldEventsModel::MakeEventId(EWorldEventType::ShipOnHorizon, 41));
			State.Consume(0xFF00000000000001ull);
			FSaveArchive Ar;
			SaveWorldEvents(Ar, State);
			FWorldEventsState Loaded;
			LoadWorldEvents(ThroughText(Ar), Loaded);
			TestTrue(TEXT("Mismos ids"), Loaded.ConsumedOutcomes == State.ConsumedOutcomes);
		});
	});

	Describe("Logros", [this]()
	{
		It("conserva perfil, partida y logros", [this]()
		{
			FAchievementsState State;
			State.Profile.Numbers.Add(FName(TEXT("fires_lit")), 3.0);
			State.Profile.Sets.Add(FName(TEXT("events_witnessed")), {FName(TEXT("Bioluminescence"))});
			State.Run.Flags.Add(FName(TEXT("coast_drawn")));
			State.Run.Numbers.Add(FName(TEXT("days_survived")), 12.0);
			State.RunMode = FName(TEXT("Castaway"));
			State.Unlocked = {FName(TEXT("primer_fuego")), FName(TEXT("luz_en_el_agua"))};
			FSaveArchive Ar;
			SaveAchievements(Ar, State);
			FAchievementsState Loaded;
			LoadAchievements(ThroughText(Ar), Loaded);
			TestTrue(TEXT("Mismo estado"), Loaded == State);
		});

		It("al cargar, el perfil nunca retrocede y la partida es la cargada", [this]()
		{
			FAchievementsState Current;
			Current.Profile.Numbers.Add(FName(TEXT("fires_lit")), 10.0);
			Current.Profile.Sets.Add(FName(TEXT("foods_eaten")), {FName(TEXT("coco"))});
			Current.Run.Numbers.Add(FName(TEXT("days_survived")), 30.0);
			Current.Unlocked = {FName(TEXT("primer_fuego"))};

			FAchievementsState Loaded;
			Loaded.Profile.Numbers.Add(FName(TEXT("fires_lit")), 4.0);
			Loaded.Profile.Numbers.Add(FName(TEXT("max_dive_depth_m")), 21.0);
			Loaded.Profile.Sets.Add(FName(TEXT("foods_eaten")), {FName(TEXT("taro")), FName(TEXT("coco"))});
			Loaded.Run.Numbers.Add(FName(TEXT("days_survived")), 2.0);
			Loaded.RunMode = FName(TEXT("Survivor"));
			Loaded.Unlocked = {FName(TEXT("pulmones_de_perla"))};

			const FAchievementsState Merged = MergeLoadedAchievements(Current, Loaded);
			TestEqual(TEXT("Fuegos: el máximo"), Merged.Profile.Numbers.FindRef(FName(TEXT("fires_lit"))), 10.0);
			TestEqual(TEXT("Buceo: el cargado"), Merged.Profile.Numbers.FindRef(FName(TEXT("max_dive_depth_m"))), 21.0);
			TestEqual(TEXT("Comidas: la unión"), Merged.Profile.Sets.FindRef(FName(TEXT("foods_eaten"))).Num(), 2);
			TestEqual(TEXT("Días: los de la partida cargada"), Merged.Run.Numbers.FindRef(FName(TEXT("days_survived"))), 2.0);
			TestEqual(TEXT("Modo cargado"), Merged.RunMode, FName(TEXT("Survivor")));
			TestEqual(TEXT("Logros: la unión"), Merged.Unlocked.Num(), 2);
			TestEqual(TEXT("En orden"), Merged.Unlocked[0], FName(TEXT("primer_fuego")));
		});
	});

	Describe("Inventario", [this]()
	{
		It("conserva manos, contenedores y equipo", [this]()
		{
			FInventoryState State;
			State.HandLeft.InstanceId = 3;
			State.HandLeft.DefinitionId = FName(TEXT("tronco"));
			State.HandLeft.WeightKg = 12.5f;
			State.HandLeft.Size = EInventorySize::DosManos;
			State.HandLeft.Tags = {FName(TEXT("madera"))};
			State.HandRight = State.HandLeft;
			State.bHandsHoldTwoHanded = true;
			FInventoryEntry Entry;
			Entry.Item.InstanceId = 7;
			Entry.Item.DefinitionId = FName(TEXT("cantimplora"));
			Entry.Item.LiquidLiters = 0.75f;
			Entry.Item.LiquidCapacityLiters = 1.0f;
			Entry.SlotIndex = 2;
			State.Belt.Entries.Add(Entry);
			State.Belt.Spec = FInventoryContainerSpec::Belt(3);
			State.Pouch.Spec = FInventoryContainerSpec::Pouch();
			State.bHasBackpack = true;
			State.BackpackItem.InstanceId = 9;
			State.BackpackItem.DefinitionId = FName(TEXT("mochila_fibra"));
			State.BackpackComfortBonusKg = 4.0f;
			State.bBackpackWaterproofPocket = true;
			State.NextInstanceId = 10;

			FSaveArchive Ar;
			SaveInventory(Ar, State);
			FInventoryState Loaded;
			LoadInventory(ThroughText(Ar), Loaded);
			TestTrue(TEXT("Mismo estado"), Loaded == State);
		});

		It("aplana y reconstruye un objeto fabricado con sus piezas", [this]()
		{
			const FTestItem Axe = MakeAxe();
			const FTestItem Stone{FName(TEXT("piedra")), 4, 1.0f, FString(), {}};
			TArray<FSavedItemNode> Nodes;
			auto ToNode = [](const FTestItem& Item)
			{
				FSavedItemNode Node;
				Node.DefinitionId = Item.DefinitionId;
				Node.Count = Item.Count;
				Node.Durability = Item.Durability;
				Node.GeneratedName = Item.Name;
				return Node;
			};
			FlattenItemTree(Axe, ToNode, Nodes);
			FlattenItemTree(Stone, ToNode, Nodes);
			TestEqual(TEXT("Cinco nodos"), Nodes.Num(), 5);
			TestEqual(TEXT("Raíz"), Nodes[0].Parent, INDEX_NONE);

			FSaveArchive Ar;
			SaveItemNodes(Ar, TEXT("items"), Nodes);
			TArray<FSavedItemNode> LoadedNodes;
			LoadItemNodes(ThroughText(Ar), TEXT("items"), LoadedNodes);
			TestTrue(TEXT("Mismos nodos"), LoadedNodes == Nodes);

			const TArray<FTestItem> Rebuilt = RebuildItemTrees<FTestItem>(LoadedNodes, [](const FSavedItemNode& Node)
			{
				return FTestItem{Node.DefinitionId, Node.Count, Node.Durability, Node.GeneratedName, {}};
			});
			TestEqual(TEXT("Dos raíces"), Rebuilt.Num(), 2);
			TestTrue(TEXT("Hacha intacta"), Rebuilt.Num() == 2 && SameTree(Rebuilt[0], Axe));
			TestTrue(TEXT("Piedras intactas"), Rebuilt.Num() == 2 && SameTree(Rebuilt[1], Stone));
		});

		It("descarta los nodos con un padre imposible", [this]()
		{
			TArray<FSavedItemNode> Nodes;
			FSavedItemNode Root;
			Root.DefinitionId = FName(TEXT("hacha"));
			Nodes.Add(Root);
			FSavedItemNode Orphan;
			Orphan.DefinitionId = FName(TEXT("lasca"));
			Orphan.Parent = 5;
			Nodes.Add(Orphan);
			FSavedItemNode Child;
			Child.DefinitionId = FName(TEXT("mango"));
			Child.Parent = 1;
			Nodes.Add(Child);
			const TArray<FTestItem> Rebuilt = RebuildItemTrees<FTestItem>(Nodes, [](const FSavedItemNode& Node)
			{
				return FTestItem{Node.DefinitionId, Node.Count, Node.Durability, Node.GeneratedName, {}};
			});
			TestEqual(TEXT("Una raíz"), Rebuilt.Num(), 1);
			TestEqual(TEXT("Sin piezas rotas"), Rebuilt.Num() == 1 ? Rebuilt[0].Components.Num() : -1, 0);
		});
	});

	Describe("Cuerpo", [this]()
	{
		It("conserva necesidades, estados, heridas y modo", [this]()
		{
			FSurvivalState State;
			State.Health = 42.5f;
			State.Hunger = 12.0f;
			State.Morale = 77.0f;
			State.BodyTemperature = 35.25f;
			State.ScurvySeverity = 0.3f;
			State.SunDose = 4.5f;
			State.MonotonyHours = 10.0f;
			State.AddCondition(ECondition::Fever, 6.0f);
			State.AddCondition(ECondition::RaySting, 2.0f);
			FWound Wound;
			Wound.Depth = 0.6f;
			Wound.Bleeding = 0.2f;
			Wound.bBandaged = true;
			Wound.bInfected = true;
			State.Wounds.Add(Wound);

			FSaveArchive Ar;
			SaveSurvival(Ar, State, ESurvivalMode::Castaway);
			FSurvivalState Loaded;
			ESurvivalMode Mode = ESurvivalMode::Survivor;
			LoadSurvival(ThroughText(Ar), Loaded, Mode);
			FSaveArchive Again;
			SaveSurvival(Again, Loaded, Mode);
			TestEqual(TEXT("Mismo texto"), Canonical(Again), Canonical(Ar));
			TestEqual(TEXT("Modo"), Mode, ESurvivalMode::Castaway);
			TestTrue(TEXT("Fiebre"), Loaded.HasCondition(ECondition::Fever));
			TestEqual(TEXT("Una herida"), Loaded.Wounds.Num(), 1);
		});
	});

	Describe("Barcos, pesca y reloj", [this]()
	{
		It("conserva una embarcación", [this]()
		{
			FBoatSaveData Boat;
			Boat.Type = EBoatType::Outrigger;
			Boat.Condition = EBoatCondition::Swamped;
			Boat.LocationCm = FVector(-150000.0, 80000.5, -12.0);
			Boat.YawDeg = 271.5f;
			Boat.HullDamage01 = 0.25f;
			Boat.CargoKg = 40.0f;
			Boat.WaterInHullKg = 120.0f;
			Boat.bSailRaised = true;
			FSaveArchive Ar;
			SaveBoat(Ar, Boat);
			FBoatSaveData Loaded;
			LoadBoat(ThroughText(Ar), Loaded);
			FSaveArchive Again;
			SaveBoat(Again, Loaded);
			TestEqual(TEXT("Mismo texto"), Canonical(Again), Canonical(Ar));
			TestEqual(TEXT("Tipo"), Loaded.Type, EBoatType::Outrigger);
		});

		It("conserva trampas, legendarias, pozas y zonas", [this]()
		{
			FFishingSaveState State;
			FPlacedTrap& Trap = State.PlaceTrap(ETrapKind::CrabTrap, EFishHabitat::Shore, FVector(1.0, 2.0, 3.0), EFishBait::Visceras, 4.5f);
			Trap.Contents.Add({FName(TEXT("cangrejo")), 0.4f, 5.0f});
			State.MarkLegendaryCaught(FName(TEXT("el_viejo")));
			State.Spooked.Add({FName(TEXT("el_errante")), 12.0f});
			State.TidePools.Add({3, MIN_int32});
			State.Zones.Add({77, 0.35f, 9.0f});
			FSaveArchive Ar;
			SaveFishing(Ar, State);
			FFishingSaveState Loaded;
			LoadFishing(ThroughText(Ar), Loaded);
			FSaveArchive Again;
			SaveFishing(Again, Loaded);
			TestEqual(TEXT("Mismo texto"), Canonical(Again), Canonical(Ar));
			TestTrue(TEXT("El Viejo"), Loaded.IsLegendaryCaught(FName(TEXT("el_viejo"))));
			TestEqual(TEXT("Poza sin vaciar"), Loaded.TidePools[0].LastLowTideIndex, static_cast<int32>(MIN_int32));
		});

		It("conserva la hora y el clima forzado", [this]()
		{
			FSavedClock Clock;
			Clock.Day = 17;
			Clock.Hours = 21.25f;
			Clock.ForcedWeather = EWeatherState::Cyclone;
			Clock.ForcedUntilDays = 18.1f;
			FSaveArchive Ar;
			SaveClock(Ar, Clock);
			FSavedClock Loaded;
			LoadClock(ThroughText(Ar), Loaded);
			TestEqual(TEXT("Día"), Loaded.Day, 17);
			TestEqual(TEXT("Hora"), Loaded.Hours, 21.25f);
			TestEqual(TEXT("Ciclón forzado"), Loaded.ForcedWeather, EWeatherState::Cyclone);

			FSaveArchive NoForced;
			SaveClock(NoForced, FSavedClock());
			FSavedClock Default;
			LoadClock(NoForced, Default);
			TestEqual(TEXT("Sin clima forzado"), Default.ForcedWeather, EWeatherState::Count);
		});
	});
}

#endif
