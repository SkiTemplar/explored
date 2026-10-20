#include "Misc/AutomationTest.h"

#include "Save/SaveArchive.h"
#include "Save/SaveSystemStates.h"
#include "Save/SaveValue.h"

#include <limits>

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

		It("descarta los objetos con id negativo o enorme sin perder el resto", [this]()
		{
			FInventoryState State;
			const int64 Ids[] = { 3, -5, std::numeric_limits<int64>::max() };
			for (const int64 Id : Ids)
			{
				FInventoryEntry Entry;
				Entry.Item.InstanceId = Id;
				Entry.Item.DefinitionId = FName(TEXT("basalto"));
				Entry.Item.WeightKg = 1.0f;
				Entry.SlotIndex = State.Pockets.Entries.Num();
				State.Pockets.Entries.Add(Entry);
			}
			State.NextInstanceId = 10;

			FSaveArchive Ar;
			SaveInventory(Ar, State);
			FInventoryState Loaded;
			LoadInventory(ThroughText(Ar), Loaded);
			TestEqual(TEXT("Solo queda la piedra buena"), Loaded.Pockets.Entries.Num(), 1);
			TestEqual(TEXT("Es la del id 3"), Loaded.Pockets.Num() == 1 ? Loaded.Pockets.Entries[0].Item.InstanceId : int64(0), int64(3));
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			TestTrue(TEXT("El inventario cargado es válido"), Model.LoadState(Loaded, Fail));
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

		It("sanea valores no finitos o fuera de rango", [this]()
		{
			FSurvivalState State;
			State.AddCondition(ECondition::Fever, 6.0f);
			FSaveArchive Ar;
			SaveSurvival(Ar, State, ESurvivalMode::Survivor);
			Ar.SetValue(TEXT("health"), FSaveValue::MakeString(TEXT("NaN")));
			Ar.SetValue(TEXT("hunger"), FSaveValue::MakeDouble(500.0));
			Ar.SetValue(TEXT("thirst"), FSaveValue::MakeDouble(-20.0));
			Ar.SetValue(TEXT("bodyTemperature"), FSaveValue::MakeString(TEXT("Infinity")));
			Ar.SetValue(TEXT("wetness"), FSaveValue::MakeString(TEXT("-Infinity")));
			Ar.SetValue(TEXT("sunDose"), FSaveValue::MakeString(TEXT("NaN")));
			FSaveValue Conditions = FSaveValue::MakeArray();
			Conditions.Add(FSaveValue::MakeString(TEXT("NaN")));
			Ar.SetValue(TEXT("conditionHours"), Conditions);
			FSaveValue Wounds = FSaveValue::MakeArray();
			FSaveValue Deep = FSaveValue::MakeObject();
			Deep.Set(TEXT("depth"), FSaveValue::MakeDouble(2.0));
			Deep.Set(TEXT("bleeding"), FSaveValue::MakeDouble(-50.0));
			Deep.Set(TEXT("healed"), FSaveValue::MakeString(TEXT("NaN")));
			Deep.Set(TEXT("hoursUntreated"), FSaveValue::MakeDouble(-5.0));
			Wounds.Add(Deep);
			FSaveValue NaNDepth = FSaveValue::MakeObject();
			NaNDepth.Set(TEXT("depth"), FSaveValue::MakeString(TEXT("NaN")));
			Wounds.Add(NaNDepth);
			Ar.SetValue(TEXT("wounds"), Wounds);

			FSurvivalState Loaded;
			ESurvivalMode Mode = ESurvivalMode::Survivor;
			LoadSurvival(ThroughText(Ar), Loaded, Mode);
			const FSurvivalState Defaults;
			TestEqual(TEXT("Salud NaN: la de partida nueva"), Loaded.Health, Defaults.Health);
			TestEqual(TEXT("Hambre hasta 100"), Loaded.Hunger, 100.0f);
			TestEqual(TEXT("Sed desde 0"), Loaded.Thirst, 0.0f);
			TestEqual(TEXT("Temperatura infinita: la de partida nueva"), Loaded.BodyTemperature, Defaults.BodyTemperature);
			TestEqual(TEXT("Humedad -inf: la de partida nueva"), Loaded.Wetness, Defaults.Wetness);
			TestEqual(TEXT("Sol NaN: la de partida nueva"), Loaded.SunDose, Defaults.SunDose);
			TestEqual(TEXT("Estado NaN: sin estado"), Loaded.ConditionTime[0], 0.0f);
			TestEqual(TEXT("La herida sin profundidad se descarta"), Loaded.Wounds.Num(), 1);
			if (Loaded.Wounds.Num() == 1)
			{
				TestEqual(TEXT("Profundidad hasta 1"), Loaded.Wounds[0].Depth, 1.0f);
				TestEqual(TEXT("Sangrado desde 0"), Loaded.Wounds[0].Bleeding, 0.0f);
				TestEqual(TEXT("Cicatrización NaN: 0"), Loaded.Wounds[0].Healed, 0.0f);
				TestEqual(TEXT("Horas sin tratar desde 0"), Loaded.Wounds[0].HoursUntreated, 0.0f);
			}
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
			TestFalse(TEXT("Sin amarre"), Loaded.bMoored);
		});

		It("conserva el amarre de una balsa", [this]()
		{
			FBoatSaveData Boat;
			Boat.bMoored = true;
			Boat.MooringAnchorCm = FVector2D(1234.5, -678.0);
			Boat.MooringLengthCm = 350.0f;
			FSaveArchive Ar;
			SaveBoat(Ar, Boat);
			FBoatSaveData Loaded;
			LoadBoat(ThroughText(Ar), Loaded);
			TestTrue(TEXT("Amarrada"), Loaded.bMoored);
			TestTrue(TEXT("Poste"), Loaded.MooringAnchorCm == FVector2D(1234.5, -678.0));
			TestEqual(TEXT("Cabo"), Loaded.MooringLengthCm, 350.0f);
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

		It("no carga una hora NaN ni un clima forzado para siempre", [this]()
		{
			FSavedClock Clock;
			Clock.ForcedWeather = EWeatherState::Cyclone;
			Clock.ForcedUntilDays = 18.1f;
			FSaveArchive Ar;
			SaveClock(Ar, Clock);
			Ar.SetValue(TEXT("hours"), FSaveValue::MakeString(TEXT("NaN")));
			Ar.SetValue(TEXT("forcedUntilDays"), FSaveValue::MakeString(TEXT("Infinity")));
			FSavedClock Loaded;
			LoadClock(ThroughText(Ar), Loaded);
			TestEqual(TEXT("Hora NaN: la de partida nueva"), Loaded.Hours, FSavedClock().Hours);
			TestEqual(TEXT("Sin clima forzado"), Loaded.ForcedWeather, EWeatherState::Count);
			TestTrue(TEXT("Plazo finito"), FMath::IsFinite(Loaded.ForcedUntilDays));
		});
	});
}

#endif
