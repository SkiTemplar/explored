#include "Misc/AutomationTest.h"

#include "WorldGen/MiningModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MiningSpecDetail
{
	/** Suelo plano en z = 0 (sólido debajo). */
	const auto Flat = [](const FVector& P) { return static_cast<float>(P.Z); };

	/** Golpe hacia abajo en un punto fresco de una rejilla de 2 m (cada golpe en roca sin tocar). */
	FMineHitRequest Fresh(EMineStratum Stratum, EMineTool Tool, int32 Slot, uint32 Seed = 7, int32 Day = 0)
	{
		FMineHitRequest R;
		R.Stratum = Stratum;
		R.Tool = Tool;
		R.ImpactPoint = FVector(2.0 * (Slot % 50), 2.0 * (Slot / 50), 0.0);
		R.Direction = FVector(0.0, 0.0, -1.0);
		R.Seed = Seed;
		R.Day = Day;
		return R;
	}
}

BEGIN_DEFINE_SPEC(FMiningModelSpec, "Explored.Mining",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FMiningModelSpec)

void FMiningModelSpec::Define()
{
	using namespace MiningSpecDetail;

	Describe("tablas de biblia 02 §2.2 y §2.3", [this]()
	{
		It("cada estrato tiene su objeto, su dureza y su herramienta mínima", [this]()
		{
			struct FRow { EMineStratum S; const TCHAR* Id; const TCHAR* Item; int32 Hardness; int32 MinTier; };
			const FRow Rows[] = {
				{ EMineStratum::Tierra, TEXT("tierra"), TEXT("tierra_suelta"), 1, 1 },
				{ EMineStratum::Arena, TEXT("arena"), TEXT("arena"), 1, 1 },
				{ EMineStratum::Arcilla, TEXT("arcilla"), TEXT("arcilla_roja"), 1, 1 },
				{ EMineStratum::Azufre, TEXT("azufre"), TEXT("azufre"), 1, 0 },
				{ EMineStratum::Caliza, TEXT("caliza"), TEXT("caliza"), 2, 2 },
				{ EMineStratum::VetaCobre, TEXT("veta_cobre"), TEXT("mineral_cobre"), 2, 2 },
				{ EMineStratum::Basalto, TEXT("basalto"), TEXT("basalto"), 3, 3 },
				{ EMineStratum::HierroMeteorito, TEXT("hierro_meteorito"), TEXT("hierro_meteorito"), 3, 3 },
				{ EMineStratum::Obsidiana, TEXT("obsidiana"), TEXT("obsidiana"), 4, 4 },
				{ EMineStratum::Cristal, TEXT("cristal"), TEXT("cristal_cuarzo"), 4, 4 },
			};
			TestEqual(TEXT("una fila por estrato"), static_cast<int32>(UE_ARRAY_COUNT(Rows)), static_cast<int32>(EMineStratum::Count));
			for (const FRow& Row : Rows)
			{
				const FMineStratumInfo& Info = FMiningModel::StratumInfo(Row.S);
				TestEqual(*(FString(Row.Id) + TEXT(": id")), FString(Info.Id), FString(Row.Id));
				TestEqual(*(FString(Row.Id) + TEXT(": objeto")), FString(Info.Item), FString(Row.Item));
				TestEqual(*(FString(Row.Id) + TEXT(": dureza")), Info.Hardness, Row.Hardness);
				TestEqual(*(FString(Row.Id) + TEXT(": nivel mínimo")), Info.MinToolTier, Row.MinTier);
				EMineStratum Back = EMineStratum::Count;
				TestTrue(FString(Row.Id) + TEXT(": se encuentra por id"), FMiningModel::StratumFromId(Row.Id, Back) && Back == Row.S);
			}
			EMineStratum Unused = EMineStratum::Count;
			TestFalse(TEXT("id desconocido"), FMiningModel::StratumFromId(TEXT("oro"), Unused));
			TestFalse(TEXT("sin distinguir mayúsculas, no"), FMiningModel::StratumFromId(TEXT("Caliza"), Unused));
		});

		It("vetas: cobre 10 golpes y 20 días; hierro 4 y cristal 12, sin reaparecer", [this]()
		{
			TestEqual(TEXT("cobre"), FMiningModel::StratumInfo(EMineStratum::VetaCobre).VeinUnits, 10);
			TestEqual(TEXT("cobre reaparece"), FMiningModel::StratumInfo(EMineStratum::VetaCobre).RespawnDays, 20);
			TestEqual(TEXT("hierro"), FMiningModel::StratumInfo(EMineStratum::HierroMeteorito).VeinUnits, 4);
			TestEqual(TEXT("hierro no reaparece"), FMiningModel::StratumInfo(EMineStratum::HierroMeteorito).RespawnDays, 0);
			TestEqual(TEXT("cristal"), FMiningModel::StratumInfo(EMineStratum::Cristal).VeinUnits, 12);
			TestEqual(TEXT("basalto no es veta"), FMiningModel::StratumInfo(EMineStratum::Basalto).VeinUnits, 0);
		});

		It("el radio y el ritmo son de la herramienta: 0,40/1,3 s … 0,55/1,1 s", [this]()
		{
			struct FRow { EMineTool T; const TCHAR* Id; int32 Tier; float Radius; float Seconds; int32 Durability; bool bFragile; };
			const FRow Rows[] = {
				{ EMineTool::Mano, TEXT("mano"), 0, 0.30f, 1.5f, 0, false },
				{ EMineTool::PalaTosca, TEXT("pala_tosca"), 1, 0.35f, 1.2f, 0, false },
				{ EMineTool::PicoPiedra, TEXT("pico_piedra"), 2, 0.40f, 1.3f, 55, false },
				{ EMineTool::PicoTallado, TEXT("pico_tallado"), 3, 0.42f, 1.2f, 75, false },
				{ EMineTool::PicoObsidiana, TEXT("pico_obsidiana"), 4, 0.50f, 1.0f, 30, true },
				{ EMineTool::PicoRescatado, TEXT("pico_rescatado"), 4, 0.55f, 1.1f, 95, false },
			};
			for (const FRow& Row : Rows)
			{
				const FMineToolInfo& Info = FMiningModel::ToolInfo(Row.T);
				TestEqual(*(FString(Row.Id) + TEXT(": id")), FString(Info.Id), FString(Row.Id));
				TestEqual(*(FString(Row.Id) + TEXT(": nivel")), Info.Tier, Row.Tier);
				TestEqual(*(FString(Row.Id) + TEXT(": radio")), Info.Radius, Row.Radius);
				TestEqual(*(FString(Row.Id) + TEXT(": segundos")), Info.SecondsPerHit, Row.Seconds);
				TestEqual(*(FString(Row.Id) + TEXT(": durabilidad")), Info.MaxDurability, Row.Durability);
				TestTrue(FString(Row.Id) + TEXT(": frágil"), Info.bFragile == Row.bFragile);
				EMineTool Back = EMineTool::Count;
				TestTrue(FString(Row.Id) + TEXT(": se encuentra por id"), FMiningModel::ToolFromId(Row.Id, Back) && Back == Row.T);
				// La cadencia que valida el servidor (08 §1.2) nunca es más lenta que la del pico.
				TestTrue(FString(Row.Id) + TEXT(": no baja del mínimo de red"), Info.SecondsPerHit >= FTerrainEditModel::SecondsPerPickaxeHit);
			}
		});
	});

	Describe("rebote (biblia 02 §2.1)", [this]()
	{
		It("con herramienta por debajo del mínimo no toca el terreno ni gasta durabilidad", [this]()
		{
			const struct { EMineStratum S; EMineTool T; } Cases[] = {
				{ EMineStratum::Caliza, EMineTool::PalaTosca },
				{ EMineStratum::Basalto, EMineTool::PicoPiedra },
				{ EMineStratum::HierroMeteorito, EMineTool::PicoPiedra },
				{ EMineStratum::Obsidiana, EMineTool::PicoTallado },
				{ EMineStratum::Cristal, EMineTool::PicoTallado },
				{ EMineStratum::Tierra, EMineTool::Mano },
				{ EMineStratum::VetaCobre, EMineTool::Mano },
			};
			int32 Slot = 0;
			for (const auto& Case : Cases)
			{
				FMiningModel Mining;
				FTerrainEditModel Terrain;
				const FMineHitResult R = Mining.Hit(Fresh(Case.S, Case.T, Slot++), Terrain, Flat);
				const FString What = FString(FMiningModel::StratumInfo(Case.S).Id) + TEXT(" con ") + FMiningModel::ToolInfo(Case.T).Id;
				TestTrue(What + TEXT(": rebota"), R.Cue == EMineHitCue::Rebound);
				TestFalse(What + TEXT(": no puede"), FMiningModel::CanMine(Case.S, Case.T));
				TestTrue(What + TEXT(": terreno intacto"), Terrain.IsEmpty());
				TestEqual(*(What + TEXT(": sin botín")), R.LootUnits, 0);
				TestEqual(*(What + TEXT(": sin desgaste")), R.DurabilityLoss, 0);
				TestEqual(*(What + TEXT(": pero cuesta el tiempo del golpe")), R.Seconds, FMiningModel::ToolInfo(Case.T).SecondsPerHit);
				TestEqual(*(What + TEXT(": la veta no se gasta")), Mining.NumTrackedVeins(), 0);
			}
		});

		It("golpear el aire no da nada ni gasta", [this]()
		{
			FMiningModel Mining;
			FTerrainEditModel Terrain;
			FMineHitRequest Req = Fresh(EMineStratum::Basalto, EMineTool::PicoTallado, 0);
			Req.ImpactPoint.Z = 5.0;
			const FMineHitResult R = Mining.Hit(Req, Terrain, Flat);
			TestTrue(TEXT("al aire"), R.Cue == EMineHitCue::Miss);
			TestEqual(TEXT("sin botín"), R.LootUnits, 0);
			TestEqual(TEXT("sin desgaste"), R.DurabilityLoss, 0);
		});
	});

	Describe("picado por esfera en todos los estratos", [this]()
	{
		It("con la herramienta mínima cada estrato suelta una unidad de su objeto por golpe", [this]()
		{
			for (int32 S = 0; S < static_cast<int32>(EMineStratum::Count); ++S)
			{
				const EMineStratum Stratum = static_cast<EMineStratum>(S);
				const FMineStratumInfo& Info = FMiningModel::StratumInfo(Stratum);
				// La herramienta más floja que puede con él.
				EMineTool Tool = EMineTool::Count;
				for (int32 T = 0; T < static_cast<int32>(EMineTool::Count); ++T)
				{
					if (FMiningModel::CanMine(Stratum, static_cast<EMineTool>(T)))
					{
						Tool = static_cast<EMineTool>(T);
						break;
					}
				}
				TestEqual(*(FString(Info.Id) + TEXT(": la mínima tiene su nivel")), FMiningModel::ToolInfo(Tool).Tier, Info.MinToolTier);
				FMiningModel Mining;
				FTerrainEditModel Terrain;
				// Pico de obsidiana con semilla sin mella: aquí solo cuenta el desgaste normal.
				uint32 Seed = 1;
				while (FMiningModel::RollChip(Seed))
				{
					++Seed;
				}
				const FMineHitResult R = Mining.Hit(Fresh(Stratum, Tool, S, Seed), Terrain, Flat);
				TestTrue(FString(Info.Id) + TEXT(": golpe efectivo"), R.Cue == EMineHitCue::Hit);
				TestEqual(*(FString(Info.Id) + TEXT(": objeto")), R.LootItem, FString(Info.Item));
				TestEqual(*(FString(Info.Id) + TEXT(": una unidad")), R.LootUnits, 1);
				TestEqual(*(FString(Info.Id) + TEXT(": desgaste")), R.DurabilityLoss, Tool == EMineTool::Mano ? 0 : 1);
				TestTrue(FString(Info.Id) + TEXT(": quita sólido"), R.Edit.VolumeRemoved > FMiningModel::MinEffectiveVolume);
				TestTrue(FString(Info.Id) + TEXT(": remalla"), R.Edit.DirtyChunks.Num() > 0);
			}
		});

		It("el radio es de la herramienta: mismo nivel, el rescatado abre más hueco que el de obsidiana", [this]()
		{
			double Obsidian = 0.0;
			double Salvaged = 0.0;
			for (uint32 Seed = 1; Seed <= 24; ++Seed)
			{
				FMiningModel MA;
				FMiningModel MB;
				FTerrainEditModel TA;
				FTerrainEditModel TB;
				Obsidian += MA.Hit(Fresh(EMineStratum::Tierra, EMineTool::PicoObsidiana, 0, Seed), TA, Flat).Edit.VolumeRemoved;
				Salvaged += MB.Hit(Fresh(EMineStratum::Tierra, EMineTool::PicoRescatado, 0, Seed), TB, Flat).Edit.VolumeRemoved;
			}
			TestTrue(TEXT("0,55 m > 0,50 m"), Salvaged > Obsidian * 1.1);
		});

		It("el hueco nunca sale de la esfera de la herramienta, sea cual sea el estrato", [this]()
		{
			const EMineStratum Strata[] = { EMineStratum::Tierra, EMineStratum::Caliza, EMineStratum::Basalto, EMineStratum::Cristal };
			for (const EMineStratum Stratum : Strata)
			{
				for (const EMineTool Tool : { EMineTool::PicoObsidiana, EMineTool::PicoRescatado })
				{
					FMiningModel Mining;
					FTerrainEditModel Terrain;
					const FMineHitRequest Req = Fresh(Stratum, Tool, 3, 11);
					Mining.Hit(Req, Terrain, Flat);
					const FVector Center = Req.ImpactPoint + FVector(0.0, 0.0, -FTerrainEditModel::PickaxeBite);
					const float MaxRadius = FMiningModel::ToolInfo(Tool).Radius * (1.0f + FTerrainEditModel::PickaxeIrregularity);
					const double H = Terrain.GetSettings().CellSize;
					int32 Outside = 0;
					int32 Changed = 0;
					for (int32 Z = -6; Z <= 2; ++Z)
					{
						for (int32 Y = -6; Y <= 6; ++Y)
						{
							for (int32 X = -6; X <= 6; ++X)
							{
								const FIntVector G(FMath::FloorToInt32(Req.ImpactPoint.X / H) + X, FMath::FloorToInt32(Req.ImpactPoint.Y / H) + Y, Z);
								if (Terrain.SampleDelta(G) != 0.0f)
								{
									++Changed;
									Outside += FVector::Dist(Terrain.SamplePosition(G), Center) >= MaxRadius ? 1 : 0;
								}
							}
						}
					}
					TestTrue(TEXT("algo cambia"), Changed > 0);
					TestEqual(TEXT("nada fuera de la esfera"), Outside, 0);
				}
			}
		});

		It("la roca dura cede menos con la misma herramienta: tierra > caliza > basalto > obsidiana", [this]()
		{
			const EMineStratum Order[] = { EMineStratum::Tierra, EMineStratum::Caliza, EMineStratum::Basalto, EMineStratum::Obsidiana };
			double Previous = TNumericLimits<double>::Max();
			for (const EMineStratum Stratum : Order)
			{
				FMiningModel Mining;
				FTerrainEditModel Terrain;
				double Sum = 0.0;
				// Tres golpes seguidos en el mismo punto: la dureza se nota al ahondar.
				for (uint32 Seed = 1; Seed <= 3; ++Seed)
				{
					FMineHitRequest Req = Fresh(Stratum, EMineTool::PicoRescatado, 0, Seed);
					Sum += Mining.Hit(Req, Terrain, Flat).Edit.VolumeRemoved;
				}
				TestTrue(FString(FMiningModel::StratumInfo(Stratum).Id) + TEXT(": menos que el anterior"), Sum < Previous);
				Previous = Sum;
			}
		});

		It("el azufre se arranca a mano: hueco pequeño, sin desgaste", [this]()
		{
			FMiningModel Mining;
			FTerrainEditModel HandTerrain;
			FTerrainEditModel ShovelTerrain;
			const FMineHitResult Hand = Mining.Hit(Fresh(EMineStratum::Azufre, EMineTool::Mano, 0), HandTerrain, Flat);
			const FMineHitResult Shovel = Mining.Hit(Fresh(EMineStratum::Azufre, EMineTool::PalaTosca, 0), ShovelTerrain, Flat);
			TestTrue(TEXT("a mano vale"), Hand.Cue == EMineHitCue::Hit);
			TestEqual(TEXT("a mano no se gasta nada"), Hand.DurabilityLoss, 0);
			TestEqual(TEXT("azufre"), Hand.LootItem, FString(TEXT("azufre")));
			TestTrue(TEXT("con pala, más hueco"), Shovel.Edit.VolumeRemoved > Hand.Edit.VolumeRemoved);
			TestEqual(TEXT("la pala sí se gasta"), Shovel.DurabilityLoss, 1);
		});
	});

	Describe("pico de obsidiana frágil (biblia 02 §2.2)", [this]()
	{
		It("contra dureza ≥ 3 se mella en torno al 8 % de los golpes y pierde 15 más", [this]()
		{
			const int32 N = 4000;
			int32 Chipped = 0;
			int32 Slot = 0;
			FTerrainEditModel Terrain;
			FMiningModel Mining;
			for (int32 I = 0; I < N; ++I)
			{
				FMineHitRequest Req = Fresh(EMineStratum::Basalto, EMineTool::PicoObsidiana, Slot, FMiningModel::HitSeed(42, 1, static_cast<uint32>(I)));
				Slot = (Slot + 1) % 2500;
				if (Slot == 0)
				{
					Terrain.Reset();
				}
				const FMineHitResult R = Mining.Hit(Req, Terrain, Flat);
				TestTrue(TEXT("siempre efectivo"), R.IsEffective());
				if (R.Cue == EMineHitCue::Chipped)
				{
					++Chipped;
					TestEqual(TEXT("1 + 15"), R.DurabilityLoss, 16);
				}
				else
				{
					TestEqual(TEXT("1 normal"), R.DurabilityLoss, 1);
				}
			}
			const double Rate = static_cast<double>(Chipped) / N;
			TestTrue(FString::Printf(TEXT("tasa %.3f cerca de 0,08"), Rate), Rate > 0.065 && Rate < 0.095);
		});

		It("contra dureza ≤ 2 no hay riesgo, y el resto de picos nunca se mellan", [this]()
		{
			for (uint32 Seed = 0; Seed < 600; ++Seed)
			{
				FMiningModel Mining;
				FTerrainEditModel A;
				FTerrainEditModel B;
				FTerrainEditModel C;
				TestTrue(TEXT("caliza con obsidiana"), Mining.Hit(Fresh(EMineStratum::Caliza, EMineTool::PicoObsidiana, 0, Seed), A, Flat).Cue != EMineHitCue::Chipped);
				TestTrue(TEXT("veta de cobre con obsidiana"), Mining.Hit(Fresh(EMineStratum::VetaCobre, EMineTool::PicoObsidiana, 1, Seed), B, Flat).Cue != EMineHitCue::Chipped);
				TestTrue(TEXT("basalto con rescatado"), Mining.Hit(Fresh(EMineStratum::Basalto, EMineTool::PicoRescatado, 2, Seed), C, Flat).Cue != EMineHitCue::Chipped);
			}
		});

		It("la tirada sale de la semilla: mismo golpe, misma suerte; y el rebote no tira", [this]()
		{
			for (uint32 Seed = 0; Seed < 300; ++Seed)
			{
				FMiningModel MA;
				FMiningModel MB;
				FTerrainEditModel TA;
				FTerrainEditModel TB;
				const FMineHitResult A = MA.Hit(Fresh(EMineStratum::Obsidiana, EMineTool::PicoObsidiana, 0, Seed), TA, Flat);
				const FMineHitResult B = MB.Hit(Fresh(EMineStratum::Obsidiana, EMineTool::PicoObsidiana, 0, Seed), TB, Flat);
				TestTrue(TEXT("misma señal"), A.Cue == B.Cue);
				TestEqual(TEXT("mismo desgaste"), A.DurabilityLoss, B.DurabilityLoss);
				TestTrue(TEXT("mismo hueco"), TA == TB);
				TestTrue(TEXT("coincide con RollChip"), (A.Cue == EMineHitCue::Chipped) == FMiningModel::RollChip(Seed));
			}
			TestNotEqual(TEXT("otro contador, otra semilla"), FMiningModel::HitSeed(42, 1, 5), FMiningModel::HitSeed(42, 1, 6));
			TestNotEqual(TEXT("otro jugador, otra semilla"), FMiningModel::HitSeed(42, 1, 5), FMiningModel::HitSeed(42, 2, 5));
			TestNotEqual(TEXT("otra partida, otra semilla"), FMiningModel::HitSeed(42, 1, 5), FMiningModel::HitSeed(43, 1, 5));
			TestEqual(TEXT("estable"), FMiningModel::HitSeed(42, 1, 5), FMiningModel::HitSeed(42, 1, 5));
		});
	});

	Describe("vetas finitas", [this]()
	{
		It("el cobre da 10 golpes; después se pica caliza, y a los 20 días vuelve", [this]()
		{
			FMiningModel Mining;
			FTerrainEditModel Terrain;
			const FIntVector Vein(5, 5, -2);
			int32 Copper = 0;
			for (int32 I = 0; I < 10; ++I)
			{
				FMineHitRequest Req = Fresh(EMineStratum::VetaCobre, EMineTool::PicoPiedra, I, 3, 7);
				Req.Vein = Vein;
				const FMineHitResult R = Mining.Hit(Req, Terrain, Flat);
				Copper += R.LootItem == TEXT("mineral_cobre") ? R.LootUnits : 0;
				TestTrue(TEXT("se agota en el décimo"), R.bVeinExhausted == (I == 9));
			}
			TestEqual(TEXT("diez de cobre"), Copper, 10);
			TestEqual(TEXT("agotada"), Mining.VeinRemaining(EMineStratum::VetaCobre, Vein, 7), 0);

			FMineHitRequest After = Fresh(EMineStratum::VetaCobre, EMineTool::PicoPiedra, 20, 3, 26);
			After.Vein = Vein;
			const FMineHitResult Host = Mining.Hit(After, Terrain, Flat);
			TestEqual(TEXT("a los 19 días, caliza"), Host.LootItem, FString(TEXT("caliza")));
			TestTrue(TEXT("el estrato picado es la caliza"), Host.Stratum == EMineStratum::Caliza);
			TestEqual(TEXT("día 26: sigue agotada"), Mining.VeinRemaining(EMineStratum::VetaCobre, Vein, 26), 0);
			TestEqual(TEXT("día 27: vuelve llena"), Mining.VeinRemaining(EMineStratum::VetaCobre, Vein, 27), 10);

			FMineHitRequest Back = Fresh(EMineStratum::VetaCobre, EMineTool::PicoPiedra, 21, 3, 27);
			Back.Vein = Vein;
			TestEqual(TEXT("cobre otra vez"), Mining.Hit(Back, Terrain, Flat).LootItem, FString(TEXT("mineral_cobre")));
			TestEqual(TEXT("quedan 9"), Mining.VeinRemaining(EMineStratum::VetaCobre, Vein, 27), 9);
			TestEqual(TEXT("otra veta no se ha tocado"), Mining.VeinRemaining(EMineStratum::VetaCobre, FIntVector(5, 6, -2), 27), 10);
		});

		It("el hierro de meteorito da 4 y no vuelve nunca; agotado se pica basalto", [this]()
		{
			FMiningModel Mining;
			FTerrainEditModel Terrain;
			for (int32 I = 0; I < 4; ++I)
			{
				const FMineHitResult R = Mining.Hit(Fresh(EMineStratum::HierroMeteorito, EMineTool::PicoTallado, I), Terrain, Flat);
				TestEqual(TEXT("hierro"), R.LootItem, FString(TEXT("hierro_meteorito")));
			}
			TestEqual(TEXT("mil días después, nada"), Mining.VeinRemaining(EMineStratum::HierroMeteorito, FIntVector::ZeroValue, 1000), 0);
			const FMineHitResult R = Mining.Hit(Fresh(EMineStratum::HierroMeteorito, EMineTool::PicoTallado, 9, 7, 1000), Terrain, Flat);
			TestEqual(TEXT("basalto"), R.LootItem, FString(TEXT("basalto")));
		});

		It("un rebote o un golpe al aire no gastan la veta", [this]()
		{
			FMiningModel Mining;
			FTerrainEditModel Terrain;
			Mining.Hit(Fresh(EMineStratum::HierroMeteorito, EMineTool::PicoPiedra, 0), Terrain, Flat);
			FMineHitRequest Air = Fresh(EMineStratum::HierroMeteorito, EMineTool::PicoTallado, 1);
			Air.ImpactPoint.Z = 3.0;
			Mining.Hit(Air, Terrain, Flat);
			TestEqual(TEXT("intacta"), Mining.VeinRemaining(EMineStratum::HierroMeteorito, FIntVector::ZeroValue, 0), 4);
		});

		It("un día anterior al agotamiento (reloj manipulado) no la rellena", [this]()
		{
			FMiningModel Mining;
			FTerrainEditModel Terrain;
			for (int32 I = 0; I < 10; ++I)
			{
				Mining.Hit(Fresh(EMineStratum::VetaCobre, EMineTool::PicoPiedra, I, 1, 100), Terrain, Flat);
			}
			TestEqual(TEXT("día 50"), Mining.VeinRemaining(EMineStratum::VetaCobre, FIntVector::ZeroValue, 50), 0);
			TestEqual(TEXT("día mínimo"), Mining.VeinRemaining(EMineStratum::VetaCobre, FIntVector::ZeroValue, MIN_int32), 0);
			TestEqual(TEXT("día máximo: ya ha vuelto, sin desbordar"), Mining.VeinRemaining(EMineStratum::VetaCobre, FIntVector::ZeroValue, MAX_int32), 10);
		});
	});

	Describe("determinismo y guardado", [this]()
	{
		It("misma semilla y mismos golpes dan el mismo terreno, el mismo botín y las mismas vetas", [this]()
		{
			auto Run = [](FMiningModel& Mining, FTerrainEditModel& Terrain, TArray<FString>& Log)
			{
				const EMineStratum Strata[] = { EMineStratum::Basalto, EMineStratum::VetaCobre, EMineStratum::Cristal, EMineStratum::HierroMeteorito };
				for (uint32 I = 0; I < 60; ++I)
				{
					FMineHitRequest Req;
					Req.Stratum = Strata[I % 4];
					Req.Tool = EMineTool::PicoObsidiana;
					Req.ImpactPoint = FVector(0.3 * (I % 7), 0.2 * (I % 5), -0.1 * (I / 7));
					Req.Direction = FVector(0.2, -0.1, -1.0);
					Req.Seed = FMiningModel::HitSeed(1234, 2, I);
					Req.Day = static_cast<int32>(I / 3);
					const FMineHitResult R = Mining.Hit(Req, Terrain, Flat);
					Log.Add(FString::Printf(TEXT("%d %s %d %d"), static_cast<int32>(R.Cue), *R.LootItem, R.LootUnits, R.DurabilityLoss));
				}
			};
			FMiningModel MA;
			FMiningModel MB;
			FTerrainEditModel TA;
			FTerrainEditModel TB;
			TArray<FString> LA;
			TArray<FString> LB;
			Run(MA, TA, LA);
			Run(MB, TB, LB);
			TestTrue(TEXT("mismo registro"), LA == LB);
			TestTrue(TEXT("mismo terreno"), TA == TB);
			TestTrue(TEXT("mismas vetas"), MA == MB);
			TestTrue(TEXT("mismo guardado"), MA.ToValue() == MB.ToValue());
		});

		It("ida y vuelta del guardado conserva vetas a medias y agotadas", [this]()
		{
			FMiningModel Mining;
			FTerrainEditModel Terrain;
			for (int32 I = 0; I < 13; ++I)
			{
				FMineHitRequest Req = Fresh(I < 10 ? EMineStratum::VetaCobre : EMineStratum::Cristal, EMineTool::PicoRescatado, I, 5, 3);
				Req.Vein = FIntVector(-4, 9, -30);
				Mining.Hit(Req, Terrain, Flat);
			}
			FMiningModel Loaded;
			TestTrue(TEXT("carga"), Loaded.FromValue(Mining.ToValue()));
			TestTrue(TEXT("igual"), Loaded == Mining);
			TestEqual(TEXT("cobre agotado el día 3"), Loaded.VeinRemaining(EMineStratum::VetaCobre, FIntVector(-4, 9, -30), 22), 0);
			TestEqual(TEXT("vuelve el 23"), Loaded.VeinRemaining(EMineStratum::VetaCobre, FIntVector(-4, 9, -30), 23), 10);
			TestEqual(TEXT("cristal a 9"), Loaded.VeinRemaining(EMineStratum::Cristal, FIntVector(-4, 9, -30), 23), 9);
			FMiningModel Empty;
			TestTrue(TEXT("vacío también"), Empty.FromValue(FMiningModel().ToValue()) && Empty.IsEmpty());
		});

		It("rechaza guardados rotos o manipulados y se queda vacío", [this]()
		{
			auto Row = [](const TCHAR* Id, int64 X, int64 Remaining, int64 Day)
			{
				FSaveValue R = FSaveValue::MakeArray();
				R.Add(FSaveValue::MakeString(Id));
				R.Add(FSaveValue::MakeInt(X));
				R.Add(FSaveValue::MakeInt(0));
				R.Add(FSaveValue::MakeInt(0));
				R.Add(FSaveValue::MakeInt(Remaining));
				R.Add(FSaveValue::MakeInt(Day));
				return R;
			};
			auto Doc = [](TArray<FSaveValue> Rows, int64 Version = 1)
			{
				FSaveValue Root = FSaveValue::MakeObject();
				Root.Set(TEXT("v"), FSaveValue::MakeInt(Version));
				FSaveValue List = FSaveValue::MakeArray();
				for (FSaveValue& R : Rows)
				{
					List.Add(R);
				}
				Root.Set(TEXT("veins"), List);
				return Root;
			};
			TestTrue(TEXT("válido"), FMiningModel().FromValue(Doc({ Row(TEXT("veta_cobre"), 1, 3, 0) })));

			FSaveValue Short = FSaveValue::MakeArray();
			Short.Add(FSaveValue::MakeString(TEXT("veta_cobre")));
			const TArray<FSaveValue> Bad = {
				FSaveValue::MakeInt(3),
				Doc({ Row(TEXT("veta_cobre"), 1, 3, 0) }, 2),
				Doc({ Row(TEXT("oro"), 1, 3, 0) }),
				Doc({ Row(TEXT("basalto"), 1, 3, 0) }),
				Doc({ Row(TEXT("veta_cobre"), 1, 10, 0) }),
				Doc({ Row(TEXT("veta_cobre"), 1, -1, 0) }),
				Doc({ Row(TEXT("hierro_meteorito"), 1, 4, 0) }),
				Doc({ Row(TEXT("veta_cobre"), 1, 3, 0), Row(TEXT("veta_cobre"), 1, 5, 0) }),
				Doc({ Row(TEXT("veta_cobre"), static_cast<int64>(MAX_int32) + 1, 3, 0) }),
				Doc({ Short }),
			};
			for (int32 I = 0; I < Bad.Num(); ++I)
			{
				FMiningModel Mining;
				FTerrainEditModel Terrain;
				Mining.Hit(Fresh(EMineStratum::VetaCobre, EMineTool::PicoPiedra, 0), Terrain, Flat);
				TestFalse(FString::Printf(TEXT("caso %d rechazado"), I), Mining.FromValue(Bad[I]));
				TestTrue(FString::Printf(TEXT("caso %d vacío"), I), Mining.IsEmpty());
			}
			FSaveValue Real = FSaveValue::MakeArray();
			Real.Add(FSaveValue::MakeString(TEXT("veta_cobre")));
			for (int32 K = 0; K < 3; ++K)
			{
				Real.Add(FSaveValue::MakeInt(0));
			}
			Real.Add(FSaveValue::MakeDouble(2.5));
			Real.Add(FSaveValue::MakeInt(0));
			TestFalse(TEXT("unidades con decimales"), FMiningModel().FromValue(Doc({ Real })));
		});
	});
}

#endif
