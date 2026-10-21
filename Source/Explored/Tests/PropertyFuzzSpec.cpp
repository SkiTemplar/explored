#include "Misc/AutomationTest.h"

#include "Boats/BoatModel.h"
#include "Carry/InventoryModel.h"
#include "Core/ExploredRandom.h"
#include "Player/SwimModel.h"
#include "Survival/BodyModel.h"
#include "Weather/RainCatchModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Tests de propiedades con fuzz sencillo y semilla fija: secuencias aleatorias de operaciones
 * sobre los modelos con más lógica y comprobación de invariantes tras cada paso. Si falla,
 * el mensaje lleva la semilla y el paso para reproducirlo.
 */
namespace PropertyFuzzDetail
{
	constexpr int32 Seeds = 24;

	// ------------------------------------------------------------------ inventario

	FInventoryItem Make(FInventoryModel& Model, const TCHAR* Id, float WeightKg, float VolumeLiters, EInventorySize Size, TArray<FName> Tags)
	{
		FInventoryItem Item;
		Item.InstanceId = Model.AllocateInstanceId();
		Item.DefinitionId = FName(Id);
		Item.WeightKg = WeightKg;
		Item.VolumeLiters = VolumeLiters;
		Item.Size = Size;
		Item.Tags = MoveTemp(Tags);
		return Item;
	}

	/** Un objeto del catálogo de InventorySpec, con el peso algo alterado. */
	FInventoryItem RandomItem(FInventoryModel& Model, FExploredRandom& Rng)
	{
		const float Jitter = Rng.RangeFloat(0.5f, 2.0f);
		switch (Rng.RangeInt(0, 11))
		{
		case 0: return Make(Model, TEXT("coco_maduro"), 0.6f * Jitter, 0.6f, EInventorySize::Pequeno, { FName(TEXT("comida")), FName(TEXT("coco")) });
		case 1: return Make(Model, TEXT("basalto"), 1.0f * Jitter, 0.4f, EInventorySize::Pequeno, { FName(TEXT("piedra")) });
		case 2: return Make(Model, TEXT("tronco_pequeno"), 8.0f * Jitter, 6.0f, EInventorySize::DosManos, { FName(TEXT("madera")) });
		case 3: return Make(Model, TEXT("hacha"), 1.0f * Jitter, 1.0f, EInventorySize::Mediano, { FName(TEXT("herramienta")), FName(TEXT("corte")) });
		case 4: return Make(Model, TEXT("cuchillo"), 0.3f * Jitter, 0.2f, EInventorySize::Pequeno, { FName(TEXT("herramienta")), FName(TEXT("corte")) });
		case 5: return Make(Model, TEXT("palo_recto"), 0.5f * Jitter, 0.6f, EInventorySize::Mediano, { FName(TEXT("madera")), FName(TEXT("mango")) });
		case 6: return Make(Model, TEXT("cerillas"), 0.02f, 0.02f, EInventorySize::Pequeno, { FName(TEXT("rescatado")), FName(TEXT("fuego")) });
		case 7: return Make(Model, TEXT("bolsa_impermeable"), 0.1f, 0.3f, EInventorySize::Pequeno, { FName(TEXT("contenedor")), FName(TEXT("impermeable")) });
		case 8: return Make(Model, TEXT("mochila_fibra"), 0.9f, 2.0f, EInventorySize::Grande, { FName(TEXT("mochila")), FName(TEXT("fibra")) });
		case 9: return Make(Model, TEXT("cinturon_cuero"), 0.4f, 0.3f, EInventorySize::Pequeno, { FName(TEXT("cinturon")), FName(TEXT("piel")) });
		case 10: return Make(Model, TEXT("angarillas"), 6.0f, 20.0f, EInventorySize::DosManos, { FName(TEXT("angarillas")), FName(TEXT("madera")) });
		default:
		{
			FInventoryItem Item = Make(Model, TEXT("cantimplora"), 0.3f, 1.0f, EInventorySize::Pequeno,
				{ FName(TEXT("rescatado")), FName(TEXT("recipiente")), FName(TEXT("cantimplora")) });
			Item.LiquidCapacityLiters = FInventoryModel::LiquidCapacityFromRecipiente(4.0f);
			return Item;
		}
		}
	}

	void AddId(TArray<int64>& Out, const FInventoryItem& Item)
	{
		if (Item.InstanceId != 0)
		{
			Out.Add(Item.InstanceId);
		}
	}

	void AddIds(TArray<int64>& Out, const FInventoryContainer& Container)
	{
		for (const FInventoryEntry& Entry : Container.Entries)
		{
			AddId(Out, Entry.Item);
		}
	}

	/** Ids de todo lo que lleva el jugador (un DosManos cuenta una vez). */
	TArray<int64> CarriedIds(const FInventoryState& S)
	{
		TArray<int64> Ids;
		AddId(Ids, S.HandLeft);
		if (!S.bHandsHoldTwoHanded)
		{
			AddId(Ids, S.HandRight);
		}
		AddIds(Ids, S.Pockets);
		AddIds(Ids, S.Belt);
		AddIds(Ids, S.Pouch);
		AddIds(Ids, S.Backpack);
		AddIds(Ids, S.Sledge);
		AddId(Ids, S.BackpackItem);
		AddId(Ids, S.BeltItem);
		AddId(Ids, S.SledgeItem);
		return Ids;
	}

	EInventorySlot RandomSlot(FExploredRandom& Rng)
	{
		return static_cast<EInventorySlot>(Rng.RangeInt(static_cast<int32>(EInventorySlot::HandLeft), static_cast<int32>(EInventorySlot::Sledge)));
	}

	EInventorySlot RandomHand(FExploredRandom& Rng)
	{
		return Rng.Chance(0.5f) ? EInventorySlot::HandLeft : EInventorySlot::HandRight;
	}

	int64 RandomOf(const TArray<int64>& Ids, FExploredRandom& Rng)
	{
		return Ids.Num() == 0 ? 0 : Ids[Rng.RangeInt(0, Ids.Num() - 1)];
	}

	// ------------------------------------------------------------------ valores hostiles

	/**
	 * La mitad de las veces un valor normal en [Lo, Hi); la otra, uno de los que llegan de un
	 * guardado corrupto o de un fotograma roto: NaN, infinitos, enormes, negativos, denormales.
	 */
	float Hostile(FExploredRandom& Rng, float Lo, float Hi)
	{
		static const float Pool[] = {
			std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
			-std::numeric_limits<float>::infinity(), 1.0e30f, -1.0e30f, TNumericLimits<float>::Max(),
			-1.0f, 0.0f, 1.0e-40f, 1.0e7f };
		if (Rng.Chance(0.5f))
		{
			return Rng.RangeFloat(Lo, Hi);
		}
		return Pool[Rng.RangeInt(0, UE_ARRAY_COUNT(Pool) - 1)];
	}

	bool IsFiniteVec(const FVector& V) { return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z); }
	bool IsFiniteVec(const FVector2D& V) { return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y); }
}

BEGIN_DEFINE_SPEC(FPropertyFuzzSpec, "Explored.Fuzz",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FPropertyFuzzSpec)

void FPropertyFuzzSpec::Define()
{
	using namespace PropertyFuzzDetail;

	Describe("el inventario con operaciones al azar", [this]()
	{
		It("siempre es válido, no crea ni pierde objetos, no cambia nada al fallar y se guarda igual", [this]()
		{
			int32 Failures = 0;
			for (int32 Seed = 1; Seed <= Seeds && Failures < 5; ++Seed)
			{
				FExploredRandom Rng(static_cast<uint64>(Seed) * 7919u);
				FInventoryModel Model;
				FInventoryContainer Chest;
				Chest.Id = FName(TEXT("arcon_fuzz"));
				Chest.Spec = FInventoryContainerSpec::Chest();
				TArray<int64> Known;   // todo lo que ha entrado alguna vez
				TArray<int64> Gone;    // soltado al suelo o consumido
				for (int32 Step = 0; Step < 300 && Failures < 5; ++Step)
				{
					// El candidato se crea antes de la foto: reservar su id ya cambia NextInstanceId.
					const FInventoryItem Candidate = RandomItem(Model, Rng);
					const FInventoryState Before = Model.GetState();
					const FInventoryContainer ChestBefore = Chest;
					const TArray<int64> Carried = CarriedIds(Before);
					EInventoryFail Fail = EInventoryFail::None;
					bool bOk = true;
					const int32 Op = Rng.RangeInt(0, 11);
					switch (Op)
					{
					case 0:
					{
						bOk = Model.PickUp(Candidate, Fail);
						if (bOk)
						{
							Known.Add(Candidate.InstanceId);
						}
						break;
					}
					case 1: bOk = Model.Move(RandomOf(Carried, Rng), RandomSlot(Rng), Fail); break;
					case 2: bOk = Model.EquipFromHand(RandomHand(Rng), Fail); break;
					case 3: bOk = Rng.Chance(0.5f) ? Model.UnequipBackpack(RandomHand(Rng), Fail) : Model.UnequipBelt(RandomHand(Rng), Fail); break;
					case 4: bOk = Model.StoreInWorld(RandomOf(Carried, Rng), Chest, Fail); break;
					case 5:
					{
						TArray<int64> InChest;
						AddIds(InChest, Chest);
						bOk = Model.TakeFromWorld(Chest, RandomOf(InChest, Rng), RandomSlot(Rng), Fail);
						break;
					}
					case 6:
					{
						FInventoryItem Dropped;
						bOk = Model.RemoveFromHand(RandomHand(Rng), Dropped, Fail);
						if (bOk)
						{
							Gone.Add(Dropped.InstanceId);
						}
						break;
					}
					case 7:
						if (Rng.Chance(0.5f))
						{
							Model.FillLiquid(RandomOf(Carried, Rng), Rng.RangeFloat(0.0f, 3.0f), Fail);
						}
						else
						{
							Model.DrinkFrom(RandomOf(Carried, Rng), Rng.RangeFloat(0.0f, 3.0f));
						}
						break;
					case 8: Model.SwapHands(); break;
					case 9:
					{
						EInventorySlot Where = EInventorySlot::None;
						bOk = Model.AutoStowFromHand(RandomHand(Rng), Where, Fail);
						break;
					}
					case 10:
					{
						FInventoryItem Eaten;
						bOk = Model.ConsumeItem(RandomOf(Carried, Rng), Eaten, Fail);
						if (bOk)
						{
							Gone.Add(Eaten.InstanceId);
						}
						break;
					}
					default:
					{
						// Guardado y carga en un modelo nuevo: el mismo estado exacto.
						FInventoryModel Loaded;
						bOk = Loaded.LoadState(Model.GetState(), Fail);
						if (!bOk || Loaded.GetState() != Model.GetState())
						{
							AddError(FString::Printf(TEXT("semilla %d paso %d: la carga no reproduce el estado (%s)"), Seed, Step, LexToString(Fail)));
							++Failures;
						}
						bOk = true;
						break;
					}
					}

					const FInventoryState& After = Model.GetState();
					const FString Where = FString::Printf(TEXT("semilla %d paso %d op %d"), Seed, Step, Op);
					if (!bOk && (After != Before || !(Chest.Entries == ChestBefore.Entries)))
					{
						AddError(Where + TEXT(": una operación fallida ha cambiado el estado"));
						++Failures;
					}
					EInventoryFail Why = EInventoryFail::None;
					if (!FInventoryModel::ValidateState(After, Why))
					{
						AddError(Where + FString::Printf(TEXT(": estado inválido (%s)"), LexToString(Why)));
						++Failures;
					}
					const float Weight = Model.GetBodyWeightKg();
					const float Ratio = Model.GetCarriedWeightRatio();
					if (!FMath::IsFinite(Weight) || Weight < 0.0f || !FMath::IsFinite(Ratio) || Ratio < 0.0f
						|| !FMath::IsFinite(Model.GetSwimLoadRatio()) || !FMath::IsFinite(Model.GetMoveSpeedMultiplier()))
					{
						AddError(Where + FString::Printf(TEXT(": peso o ratio no válidos (%f, %f)"), Weight, Ratio));
						++Failures;
					}

					// Conservación: cada objeto conocido está en un solo sitio.
					TArray<int64> All = CarriedIds(After);
					AddIds(All, Chest);
					All.Append(Gone);
					TArray<int64> Sorted = All;
					Sorted.Sort();
					TArray<int64> Expected = Known;
					Expected.Sort();
					if (Sorted != Expected)
					{
						AddError(Where + FString::Printf(TEXT(": %d objetos localizados para %d conocidos"), Sorted.Num(), Expected.Num()));
						++Failures;
					}
				}
			}
			TestEqual(TEXT("sin fallos de invariantes"), Failures, 0);
		});
	});

	Describe("los valores hostiles en las entradas públicas", [this]()
	{
		It("no dejan NaN ni infinitos en el estado del barco y el nado, ni cuelgan", [this]()
		{
			for (int32 Seed = 1; Seed <= Seeds; ++Seed)
			{
				FExploredRandom Rng(static_cast<uint64>(Seed) * 2654435761u);
				const FString At = FString::Printf(TEXT("semilla %d"), Seed);

				// Barco: pasos, carga, velocidad y amarre con valores rotos.
				FBoatModel Boat(EBoatType::Canoe, FVector(100.0, 200.0, 0.0), 30.0f);
				Boat.SetCrewAboard(true);
				FBoatControls Controls;
				FBoatEnvironment Env;
				for (int32 I = 0; I < 200; ++I)
				{
					switch (Rng.RangeInt(0, 3))
					{
					case 0: Boat.TryAddCargo(Hostile(Rng, 0.0f, 50.0f)); break;
					case 1: Boat.SetVelocityCmS(FVector2D(Hostile(Rng, -300.0f, 300.0f), Hostile(Rng, -300.0f, 300.0f))); break;
					case 2: Boat.Moor(FVector2D(Hostile(Rng, -500.0f, 500.0f), Hostile(Rng, -500.0f, 500.0f)), Hostile(Rng, 0.0f, 800.0f)); break;
					default: break;
					}
					Env.WaveTimeSeconds += 1.0f / 60.0f;
					Boat.Step(Hostile(Rng, 0.0f, 0.05f), Controls, Env);
				}
				const FBoatState& B = Boat.GetState();
				TestTrue(At + TEXT(": barco finito"), IsFiniteVec(B.LocationCm) && IsFiniteVec(B.VelocityCmS)
					&& FMath::IsFinite(B.YawDeg) && FMath::IsFinite(B.CargoKg) && FMath::IsFinite(B.PendingTimeS)
					&& FMath::IsFinite(B.MooringLengthCm) && IsFiniteVec(B.MooringAnchorCm));

				// Nado: pasos de tiempo y oxígeno rotos, con la cabeza bajo el agua.
				const FSwimTuning Tuning;
				FSwimModel Swim;
				FSwimInputs In;
				In.bHasWater = true;
				In.WaterZ = 0.0f;
				In.CenterZ = -300.0f;
				for (int32 I = 0; I < 200; ++I)
				{
					if (Rng.Chance(0.1f))
					{
						Swim.SetOxygen(Hostile(Rng, 0.0f, 100.0f));
					}
					Swim.Tick(Tuning, In, Hostile(Rng, 0.0f, 0.05f));
				}
				TestTrue(At + TEXT(": oxígeno finito y en rango"), FMath::IsFinite(Swim.GetOxygen()) && Swim.GetOxygen() >= 0.0f && Swim.GetOxygen() <= 100.0f);

				// Funciones puras: resultado finito y en rango, sin conversiones con UB (UBSan).
				for (int32 I = 0; I < 50; ++I)
				{
					const FFallResult Fall = FBodyModel::FallDamage(Hostile(Rng, 0.0f, 20.0f), ELandingSurface::Rock);
					const float Rain = FRainCatchModel::RainMmPerHour(Hostile(Rng, 0.0f, 1.0f));
					if (!FMath::IsFinite(Fall.Damage) || Fall.Damage < 0.0f || !FMath::IsFinite(Fall.SprainHours)
						|| !FMath::IsFinite(Rain) || Rain < 0.0f)
					{
						AddError(At + TEXT(": caída o lluvia no finitas"));
					}
				}
			}
		});
	});
}

#endif
