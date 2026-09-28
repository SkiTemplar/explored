#include "Misc/AutomationTest.h"

#include "Combat/CombatModel.h"
#include "Core/ExploredRandom.h"
#include "Survival/BodyModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FCombatModelSpec, "Explored.Combat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	/** Una propiedad de items.json (nombre sin tildes y valor 0–5). */
	struct FProp
	{
		const TCHAR* Name;
		float Value;
	};
	static TMap<FName, float> Props(std::initializer_list<FProp> List)
	{
		TMap<FName, float> Out;
		for (const FProp& P : List)
		{
			Out.Add(FName(P.Name), P.Value);
		}
		return Out;
	}
	static FCombatStrike Strike(ECombatDamageKind Kind, int32 Property)
	{
		FCombatStrike S;
		S.Kind = Kind;
		S.PropertyTenths = Property * 10;
		return S;
	}
	static FCombatHit Damage(int32 HealthDamage)
	{
		FCombatHit H;
		H.Kind = ECombatDamageKind::Blunt;
		H.HealthDamage = HealthDamage;
		return H;
	}
	static int32 Hp(ECombatSwing Swing, ECombatDamageKind Kind, int32 Property)
	{
		return FCombatModel::HitFor(Strike(Kind, Property), Swing).HealthDamage;
	}
	static FSurvivalModeSettings Survivor() { return FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor); }
	static FSurvivalModeSettings Explorer() { return FSurvivalModeSettings::FromMode(ESurvivalMode::Explorer); }
	static bool SameTiming(const FCombatTimingState& A, const FCombatTimingState& B)
	{
		return A.NowMs == B.NowMs && A.Phase == B.Phase && A.PhaseStartMs == B.PhaseStartMs && A.PhaseEndMs == B.PhaseEndMs
			&& A.ChainCount == B.ChainCount && A.LastQuickEndMs == B.LastQuickEndMs && A.bHasQuick == B.bHasQuick
			&& A.DodgeStartMs == B.DodgeStartMs && A.bHasDodged == B.bHasDodged
			&& A.bChargedImpactPending == B.bChargedImpactPending && A.ChargedImpactMs == B.ChargedImpactMs;
	}
END_DEFINE_SPEC(FCombatModelSpec)

void FCombatModelSpec::Define()
{
	Describe("daño instantáneo leyendo las propiedades del objeto", [this]()
	{
		It("reproduce la tabla de la biblia 05 §3.1 (rápido y cargado)", [this]()
		{
			// Cuchillo de lasca y hacha de pedernal: Filo 3.
			const TMap<FName, float> Lasca = Props({ { TEXT("Filo"), 3.0f }, { TEXT("Punta"), 1.0f } });
			TestEqual(TEXT("Filo 3 rápido"), FCombatModel::HitWithItem(Lasca, ECombatSwing::Quick).HealthDamage, 6);
			TestEqual(TEXT("Filo 3 cargado"), FCombatModel::HitWithItem(Lasca, ECombatSwing::Charged).HealthDamage, 14);
			// Lanza con punta de obsidiana: Punta 5 (la lanza golpea con la punta).
			const TMap<FName, float> Lanza = Props({ { TEXT("Punta"), 5.0f }, { TEXT("Largo"), 5.0f } });
			TestEqual(TEXT("Punta 5 rápido"), FCombatModel::HitWithItem(Lanza, ECombatSwing::Quick).HealthDamage, 10);
			TestEqual(TEXT("Punta 5 cargado"), FCombatModel::HitWithItem(Lanza, ECombatSwing::Charged).HealthDamage, 24);
			// Cuchillo de obsidiana y machete de aluminio: Filo 5.
			const TMap<FName, float> Obsidiana = Props({ { TEXT("Filo"), 5.0f }, { TEXT("Punta"), 3.0f } });
			TestEqual(TEXT("Filo 5 rápido"), FCombatModel::HitWithItem(Obsidiana, ECombatSwing::Quick).HealthDamage, 10);
			TestEqual(TEXT("Filo 5 cargado"), FCombatModel::HitWithItem(Obsidiana, ECombatSwing::Charged).HealthDamage, 24);
			// Martillo de piedra: Contundente 3.
			const TMap<FName, float> Martillo = Props({ { TEXT("Contundente"), 3.0f } });
			TestEqual(TEXT("Contundente 3 rápido"), FCombatModel::HitWithItem(Martillo, ECombatSwing::Quick).HealthDamage, 8);
			TestEqual(TEXT("Contundente 3 cargado"), FCombatModel::HitWithItem(Martillo, ECombatSwing::Charged).HealthDamage, 19);
		});

		It("sin variante, Filo y Punta hacen × 3 y Contundente × 4 en toda la escala 0–5", [this]()
		{
			for (int32 P = 0; P <= 5; ++P)
			{
				TestEqual(*FString::Printf(TEXT("Filo %d"), P), Hp(ECombatSwing::Plain, ECombatDamageKind::Cut, P), 3 * P);
				TestEqual(*FString::Printf(TEXT("Punta %d"), P), Hp(ECombatSwing::Plain, ECombatDamageKind::Pierce, P), 3 * P);
				TestEqual(*FString::Printf(TEXT("Contundente %d"), P), Hp(ECombatSwing::Plain, ECombatDamageKind::Blunt, P), 4 * P);
			}
		});

		It("el rápido nunca supera al normal ni el normal al cargado, y todo crece con la propiedad", [this]()
		{
			for (const ECombatDamageKind Kind : { ECombatDamageKind::Cut, ECombatDamageKind::Pierce, ECombatDamageKind::Blunt })
			{
				int32 Prev[3] = { 0, 0, 0 };
				for (int32 Tenths = 0; Tenths <= 50; ++Tenths)
				{
					FCombatStrike S;
					S.Kind = Kind;
					S.PropertyTenths = Tenths;
					const int32 Q = FCombatModel::HitFor(S, ECombatSwing::Quick).HealthDamage;
					const int32 N = FCombatModel::HitFor(S, ECombatSwing::Plain).HealthDamage;
					const int32 C = FCombatModel::HitFor(S, ECombatSwing::Charged).HealthDamage;
					if (!TestTrue(TEXT("rápido ≤ normal ≤ cargado"), Q <= N && N <= C)
						|| !TestTrue(TEXT("monótono"), Q >= Prev[0] && N >= Prev[1] && C >= Prev[2]))
					{
						return;
					}
					Prev[0] = Q;
					Prev[1] = N;
					Prev[2] = C;
				}
			}
		});

		It("propiedades a 0 o ausentes no hacen daño, no cortan y no aturden", [this]()
		{
			const TArray<TMap<FName, float>> Empties = {
				TMap<FName, float>(),
				Props({ { TEXT("Largo"), 5.0f }, { TEXT("Rigido"), 4.0f } }),
				Props({ { TEXT("Filo"), 0.0f }, { TEXT("Punta"), 0.0f }, { TEXT("Contundente"), 0.0f } }),
			};
			for (const TMap<FName, float>& P : Empties)
			{
				for (const ECombatSwing Swing : { ECombatSwing::Plain, ECombatSwing::Quick, ECombatSwing::Charged })
				{
					const FCombatHit Hit = FCombatModel::HitWithItem(P, Swing);
					TestEqual(TEXT("sin tipo"), (int32)Hit.Kind, (int32)ECombatDamageKind::None);
					TestEqual(TEXT("sin daño"), Hit.HealthDamage, 0);
					TestEqual(TEXT("sin corte"), Hit.CutDepth, 0.0f);
					TestEqual(TEXT("sin aturdimiento"), Hit.StunMs, 0);
				}
			}
			TestEqual(TEXT("propiedad ausente"), FCombatModel::PropertyTenths(TMap<FName, float>(), FName(TEXT("Filo"))), 0);
			TestEqual(TEXT("flecha sin punta"), FCombatModel::ArrowHit(Props({ { TEXT("Filo"), 4.0f } })).HealthDamage, 0);
		});

		It("valores corruptos (negativos, NaN, infinitos, > 5) no rompen la cuenta", [this]()
		{
			const FName Filo(TEXT("Filo"));
			TestEqual(TEXT("negativo"), FCombatModel::PropertyTenths(Props({ { TEXT("Filo"), -3.0f } }), Filo), 0);
			TestEqual(TEXT("NaN"), FCombatModel::PropertyTenths(Props({ { TEXT("Filo"), NAN } }), Filo), 0);
			TestEqual(TEXT("+inf"), FCombatModel::PropertyTenths(Props({ { TEXT("Filo"), INFINITY } }), Filo), 0);
			TestEqual(TEXT("-inf"), FCombatModel::PropertyTenths(Props({ { TEXT("Filo"), -INFINITY } }), Filo), 0);
			TestEqual(TEXT("por encima de 5 se recorta"), FCombatModel::PropertyTenths(Props({ { TEXT("Filo"), 9.0f } }), Filo), 50);
			TestEqual(TEXT("una pizca no llega a una décima"), FCombatModel::PropertyTenths(Props({ { TEXT("Filo"), 0.04f } }), Filo), 0);
			TestEqual(TEXT("el nombre no distingue mayúsculas (FName)"),
				FCombatModel::PropertyTenths(Props({ { TEXT("filo"), 2.0f } }), Filo), 20);

			const FCombatHit Huge = FCombatModel::HitWithItem(Props({ { TEXT("Contundente"), 1.0e30f } }), ECombatSwing::Charged);
			TestEqual(TEXT("> 5 pega como 5"), Huge.HealthDamage, 32);
			FCombatStrike Forged;
			Forged.Kind = ECombatDamageKind::Cut;
			Forged.PropertyTenths = 1000000;
			const FCombatHit ForgedHit = FCombatModel::HitFor(Forged, ECombatSwing::Charged);
			TestEqual(TEXT("décimas fuera de escala se recortan"), ForgedHit.HealthDamage, 24);
			TestEqual(TEXT("y el corte no pasa de 1"), ForgedHit.CutDepth, 1.0f);
			Forged.Kind = ECombatDamageKind::Count;
			TestEqual(TEXT("tipo inválido: nada"), FCombatModel::HitFor(Forged, ECombatSwing::Plain).HealthDamage, 0);
			Forged.Kind = ECombatDamageKind::Cut;
			Forged.PropertyTenths = -30;
			TestEqual(TEXT("décimas negativas: nada"), FCombatModel::HitFor(Forged, ECombatSwing::Plain).HealthDamage, 0);
			const FCombatHit BadSwing = FCombatModel::HitFor(Strike(ECombatDamageKind::Cut, 3), static_cast<ECombatSwing>(77));
			TestEqual(TEXT("variante inválida cuenta como normal"), BadSwing.HealthDamage, 9);
			TestEqual(TEXT("y se normaliza"), (int32)BadSwing.Swing, (int32)ECombatSwing::Plain);
		});

		It("propiedades con décimas: el daño se redondea hacia abajo y es entero", [this]()
		{
			const FCombatHit Hit = FCombatModel::HitWithItem(Props({ { TEXT("Filo"), 2.5f } }), ECombatSwing::Plain);
			TestEqual(TEXT("7,5 → 7"), Hit.HealthDamage, 7);
			TestEqual(TEXT("corte 0,5"), Hit.CutDepth, 0.5f);
		});

		It("elige la propiedad que más daño hace y, a igualdad, la que corta", [this]()
		{
			TestEqual(TEXT("obsidiana: Filo 5"), (int32)FCombatModel::BestStrike(Props({ { TEXT("Filo"), 5.0f }, { TEXT("Punta"), 3.0f } })).Kind,
				(int32)ECombatDamageKind::Cut);
			TestEqual(TEXT("basalto tallado: Contundente 4 (16) sobre Punta 2 (6)"),
				(int32)FCombatModel::BestStrike(Props({ { TEXT("Punta"), 2.0f }, { TEXT("Contundente"), 4.0f } })).Kind,
				(int32)ECombatDamageKind::Blunt);
			TestEqual(TEXT("Filo 4 (12) empata con Contundente 3 (12): gana el filo"),
				(int32)FCombatModel::BestStrike(Props({ { TEXT("Contundente"), 3.0f }, { TEXT("Filo"), 4.0f } })).Kind,
				(int32)ECombatDamageKind::Cut);
			TestEqual(TEXT("Punta 4 empata con Contundente 3: gana la punta"),
				(int32)FCombatModel::BestStrike(Props({ { TEXT("Contundente"), 3.0f }, { TEXT("Punta"), 4.0f } })).Kind,
				(int32)ECombatDamageKind::Pierce);
			TestEqual(TEXT("Filo y Punta iguales: gana el filo"),
				(int32)FCombatModel::BestStrike(Props({ { TEXT("Punta"), 3.0f }, { TEXT("Filo"), 3.0f } })).Kind,
				(int32)ECombatDamageKind::Cut);
			TestEqual(TEXT("StrikeOfKind fuerza la punta aunque el filo pegue más"),
				FCombatModel::StrikeOfKind(Props({ { TEXT("Filo"), 5.0f }, { TEXT("Punta"), 3.0f } }), ECombatDamageKind::Pierce).PropertyTenths, 30);
		});

		It("el alcance sale de la definición: 2,2 m la lanza y 1,2 m el resto", [this]()
		{
			TestEqual(TEXT("lanza"), FCombatModel::MeleeReachM(FName(TEXT("lanza"))), 2.2f);
			TestEqual(TEXT("cuchillo"), FCombatModel::MeleeReachM(FName(TEXT("cuchillo"))), 1.2f);
			TestEqual(TEXT("hacha"), FCombatModel::MeleeReachM(FName(TEXT("hacha"))), 1.2f);
			TestEqual(TEXT("sin objeto"), FCombatModel::MeleeReachM(NAME_None), 1.2f);
		});
	});

	Describe("cortes y aturdimiento", [this]()
	{
		It("el corte tiene profundidad propiedad ÷ 5, sea cual sea la variante", [this]()
		{
			for (int32 P = 1; P <= 5; ++P)
			{
				for (const ECombatSwing Swing : { ECombatSwing::Plain, ECombatSwing::Quick, ECombatSwing::Charged })
				{
					TestEqual(TEXT("Filo"), FCombatModel::HitFor(Strike(ECombatDamageKind::Cut, P), Swing).CutDepth, P / 5.0f);
					TestEqual(TEXT("Punta"), FCombatModel::HitFor(Strike(ECombatDamageKind::Pierce, P), Swing).CutDepth, P / 5.0f);
					TestEqual(TEXT("Contundente no corta"), FCombatModel::HitFor(Strike(ECombatDamageKind::Blunt, P), Swing).CutDepth, 0.0f);
				}
			}
		});

		It("Contundente aturde 1,5 s solo desde 4; filo y punta nunca aturden", [this]()
		{
			TestEqual(TEXT("3,9"), FCombatModel::HitWithItem(Props({ { TEXT("Contundente"), 3.9f } }), ECombatSwing::Plain).StunMs, 0);
			TestEqual(TEXT("4"), FCombatModel::HitWithItem(Props({ { TEXT("Contundente"), 4.0f } }), ECombatSwing::Quick).StunMs, 1500);
			TestEqual(TEXT("5"), FCombatModel::HitWithItem(Props({ { TEXT("Contundente"), 5.0f } }), ECombatSwing::Charged).StunMs, 1500);
			TestEqual(TEXT("Filo 5"), FCombatModel::HitFor(Strike(ECombatDamageKind::Cut, 5), ECombatSwing::Charged).StunMs, 0);
		});

		It("recibir un golpe cortante quita salud y abre un corte en el sistema de heridas", [this]()
		{
			FSurvivalState Body;
			const float MoraleBefore = Body.Morale;
			TArray<ESurvivalEvent> Events;
			const FCombatHit Hit = FCombatModel::HitFor(Strike(ECombatDamageKind::Cut, 3), ECombatSwing::Quick);
			const FCombatReceiveResult R = FCombatModel::ReceiveHit(Body, FCombatTimingState(), 5000, Hit, Survivor(), Events);
			TestFalse(TEXT("no esquivado"), R.bDodged);
			TestEqual(TEXT("salud 94"), Body.Health, 94.0f);
			TestEqual(TEXT("perdida 6"), R.HealthLost, 6.0f);
			TestTrue(TEXT("corte abierto"), R.bCutOpened);
			if (TestEqual(TEXT("una herida"), Body.Wounds.Num(), 1))
			{
				TestEqual(TEXT("profundidad 0,6"), Body.Wounds[0].Depth, 0.6f);
				TestEqual(TEXT("sangra lo que mide"), Body.Wounds[0].Bleeding, 0.6f);
			}
			TestTrue(TEXT("el corte sangra con las constantes de siempre"), FBodyModel::BleedingDamagePerHour(Body) > 0.0f);
			TestTrue(TEXT("baja el ánimo (herido)"), Body.Morale < MoraleBefore);
			TestEqual(TEXT("sin eventos de muerte"), Events.Num(), 0);
		});

		It("un golpe contundente quita salud sin abrir herida", [this]()
		{
			FSurvivalState Body;
			TArray<ESurvivalEvent> Events;
			const FCombatReceiveResult R = FCombatModel::ReceiveHit(Body, FCombatTimingState(), 0,
				FCombatModel::HitFor(Strike(ECombatDamageKind::Blunt, 3), ECombatSwing::Charged), Survivor(), Events);
			TestEqual(TEXT("salud 81"), Body.Health, 81.0f);
			TestFalse(TEXT("sin corte"), R.bCutOpened);
			TestEqual(TEXT("sin heridas"), Body.Wounds.Num(), 0);
		});

		It("un golpe sin daño ni corte no toca el cuerpo", [this]()
		{
			FSurvivalState Body;
			const float Morale = Body.Morale;
			TArray<ESurvivalEvent> Events;
			FCombatModel::ReceiveHit(Body, FCombatTimingState(), 0, FCombatHit(), Survivor(), Events);
			TestEqual(TEXT("salud"), Body.Health, 100.0f);
			TestEqual(TEXT("ánimo"), Body.Morale, Morale);
			FCombatHit Bad;
			Bad.HealthDamage = -40;
			Bad.CutDepth = NAN;
			FCombatModel::ReceiveHit(Body, FCombatTimingState(), 0, Bad, Survivor(), Events);
			TestEqual(TEXT("daño negativo no cura"), Body.Health, 100.0f);
			TestEqual(TEXT("corte NaN no abre herida"), Body.Wounds.Num(), 0);
		});

		It("cada golpe del combo abre su propio corte", [this]()
		{
			FSurvivalState Body;
			FCombatTimingState Attacker;
			TArray<ESurvivalEvent> Events;
			const TMap<FName, float> Lasca = Props({ { TEXT("Filo"), 3.0f } });
			for (int64 T : { (int64)0, (int64)450, (int64)900 })
			{
				const FCombatActionResult A = FCombatModel::TryQuick(Attacker, T);
				if (!TestTrue(TEXT("golpe aceptado"), A.bAccepted))
				{
					return;
				}
				FCombatModel::ReceiveHit(Body, FCombatTimingState(), A.ImpactMs, FCombatModel::HitWithItem(Lasca, A.Swing), Survivor(), Events);
			}
			TestEqual(TEXT("3 × 6 de salud"), Body.Health, 82.0f);
			if (TestEqual(TEXT("tres cortes"), Body.Wounds.Num(), 3))
			{
				for (const FWound& W : Body.Wounds)
				{
					TestEqual(TEXT("cada uno de 0,6"), W.Depth, 0.6f);
				}
			}
			TestFalse(TEXT("el cuarto va a la pausa"), FCombatModel::TryQuick(Attacker, 1350).bAccepted);
			TestEqual(TEXT("y no abre un cuarto corte"), Body.Wounds.Num(), 3);
		});

		It("Filo 4 (0,8) lo para una venda y Filo 5 (1,0) exige sutura, como cualquier corte", [this]()
		{
			FSurvivalState Body;
			TArray<ESurvivalEvent> Events;
			FCombatModel::ReceiveHit(Body, FCombatTimingState(), 0, FCombatModel::HitFor(Strike(ECombatDamageKind::Cut, 4), ECombatSwing::Plain), Survivor(), Events);
			FBodyModel::TreatWounds(Body, EWoundTreatment::ClothBandage);
			TestEqual(TEXT("vendado no sangra"), FBodyModel::TotalBleeding(Body), 0.0f);

			FSurvivalState Deep;
			FCombatModel::ReceiveHit(Deep, FCombatTimingState(), 0, FCombatModel::HitFor(Strike(ECombatDamageKind::Cut, 5), ECombatSwing::Plain), Survivor(), Events);
			FBodyModel::TreatWounds(Deep, EWoundTreatment::ClothBandage);
			TestTrue(TEXT("la venda no basta"), FBodyModel::TotalBleeding(Deep) > 0.0f);
			FBodyModel::TreatWounds(Deep, EWoundTreatment::Suture);
			TestEqual(TEXT("la sutura sí"), FBodyModel::TotalBleeding(Deep), 0.0f);
		});

		It("en Explorador no mata (se queda en 10) y en Superviviente sí", [this]()
		{
			TArray<ESurvivalEvent> Events;
			FSurvivalState Soft;
			Soft.Health = 12.0f;
			const FCombatReceiveResult R = FCombatModel::ReceiveHit(Soft, FCombatTimingState(), 0,
				FCombatModel::HitFor(Strike(ECombatDamageKind::Pierce, 5), ECombatSwing::Charged), Explorer(), Events);
			TestEqual(TEXT("se queda en 10"), Soft.Health, 10.0f);
			TestEqual(TEXT("pierde solo 2"), R.HealthLost, 2.0f);
			TestFalse(TEXT("vivo"), R.bDied);
			TestEqual(TEXT("el corte se abre igual"), Soft.Wounds.Num(), 1);

			FSurvivalState Hard;
			Hard.Health = 5.0f;
			const FCombatReceiveResult D = FCombatModel::ReceiveHit(Hard, FCombatTimingState(), 0, Damage(10), Survivor(), Events);
			TestTrue(TEXT("muere"), D.bDied);
			TestEqual(TEXT("salud 0"), Hard.Health, 0.0f);
			TestTrue(TEXT("evento Died"), Events.Contains(ESurvivalEvent::Died));

			Events.Reset();
			const FCombatReceiveResult Again = FCombatModel::ReceiveHit(Hard, FCombatTimingState(), 0, Damage(10), Survivor(), Events);
			TestEqual(TEXT("a un muerto no se le quita más"), Again.HealthLost, 0.0f);
			TestEqual(TEXT("ni se repite la muerte"), Events.Num(), 0);
		});
	});

	Describe("golpe rápido encadenado y golpe cargado", [this]()
	{
		It("encadena 3 rápidos y luego exige 0,4 s de pausa, al milisegundo", [this]()
		{
			FCombatTimingState S;
			const FCombatActionResult A = FCombatModel::TryQuick(S, 0);
			const FCombatActionResult B = FCombatModel::TryQuick(S, 450);
			const FCombatActionResult C = FCombatModel::TryQuick(S, 900);
			TestTrue(TEXT("los tres aceptados"), A.bAccepted && B.bAccepted && C.bAccepted);
			TestEqual(TEXT("1.º"), A.ComboIndex, 1);
			TestEqual(TEXT("2.º"), B.ComboIndex, 2);
			TestEqual(TEXT("3.º"), C.ComboIndex, 3);
			TestEqual(TEXT("llega al pedirlo"), (int32)B.ImpactMs, 450);
			const FCombatActionResult D = FCombatModel::TryQuick(S, 1350);
			TestFalse(TEXT("el 4.º no"), D.bAccepted);
			TestEqual(TEXT("por la pausa"), (int32)D.Reject, (int32)ECombatReject::ChainPause);
			TestFalse(TEXT("a 1749 aún no"), FCombatModel::TryQuick(S, 1749).bAccepted);
			const FCombatActionResult E = FCombatModel::TryQuick(S, 1750);
			TestTrue(TEXT("a 1750 sí"), E.bAccepted);
			TestEqual(TEXT("y empieza racha nueva"), E.ComboIndex, 1);
		});

		It("mientras se ejecuta un rápido (0,45 s) no se puede otro", [this]()
		{
			FCombatTimingState S;
			FCombatModel::TryQuick(S, 1000);
			const FCombatActionResult Early = FCombatModel::TryQuick(S, 1449);
			TestFalse(TEXT("a 449 ms"), Early.bAccepted);
			TestEqual(TEXT("ocupado"), (int32)Early.Reject, (int32)ECombatReject::Busy);
			TestTrue(TEXT("a 450 ms"), FCombatModel::TryQuick(S, 1450).bAccepted);
		});

		It("esperar 0,4 s entre dos rápidos rompe la racha; 399 ms no", [this]()
		{
			FCombatTimingState Kept;
			FCombatModel::TryQuick(Kept, 0);
			TestEqual(TEXT("399 ms quieto: sigue la racha"), FCombatModel::TryQuick(Kept, 849).ComboIndex, 2);

			FCombatTimingState Broken;
			FCombatModel::TryQuick(Broken, 0);
			TestEqual(TEXT("400 ms quieto: racha nueva"), FCombatModel::TryQuick(Broken, 850).ComboIndex, 1);
		});

		It("con pausas intermedias nunca se llega al tercero ni a la pausa obligatoria", [this]()
		{
			FCombatTimingState S;
			for (int32 i = 0; i < 20; ++i)
			{
				const FCombatActionResult R = FCombatModel::TryQuick(S, i * 900);
				if (!TestTrue(TEXT("aceptado"), R.bAccepted) || !TestEqual(TEXT("siempre el primero"), R.ComboIndex, 1))
				{
					return;
				}
			}
		});

		It("aporrear el botón cada milisegundo nunca da más de 3 golpes sin la pausa", [this]()
		{
			FCombatTimingState S;
			TArray<int64> Impacts;
			TArray<int32> Combo;
			for (int64 T = 0; T <= 20000; ++T)
			{
				const FCombatActionResult R = FCombatModel::TryQuick(S, T);
				if (R.bAccepted)
				{
					Impacts.Add(R.ImpactMs);
					Combo.Add(R.ComboIndex);
				}
			}
			// Ciclo de 3 × 450 + 400 = 1750 ms: 20 000 / 1750 → 11 ciclos completos y el 12.º a medias.
			TestEqual(TEXT("golpes en 20 s"), Impacts.Num(), 12 * 3 - 1);
			for (int32 i = 1; i < Impacts.Num(); ++i)
			{
				const int64 Gap = Impacts[i] - Impacts[i - 1];
				const int64 Expected = Combo[i] == 1 ? 850 : 450;
				if (!TestEqual(TEXT("separación exacta"), (int32)Gap, (int32)Expected)
					|| !TestEqual(TEXT("índice cíclico"), Combo[i], (i % 3) + 1))
				{
					return;
				}
			}
		});

		It("el cargado avisa 1,2 s, llega una sola vez al acabar y recupera 0,3 s", [this]()
		{
			FCombatTimingState S;
			const FCombatActionResult Start = FCombatModel::StartCharge(S, 1000);
			TestTrue(TEXT("aceptado"), Start.bAccepted);
			TestEqual(TEXT("variante"), (int32)Start.Swing, (int32)ECombatSwing::Charged);
			TestEqual(TEXT("llegará a 2200"), (int32)Start.ImpactMs, 2200);
			TestFalse(TEXT("antes no avisa"), FCombatModel::IsTelegraphing(S, 999));
			TestTrue(TEXT("avisa al empezar"), FCombatModel::IsTelegraphing(S, 1000));
			TestTrue(TEXT("avisa a 2199"), FCombatModel::IsTelegraphing(S, 2199));
			TestFalse(TEXT("deja de avisar a 2200"), FCombatModel::IsTelegraphing(S, 2200));

			FCombatActionResult Impact;
			TestFalse(TEXT("a 2199 aún no llega"), FCombatModel::Advance(S, 2199, Impact));
			TestTrue(TEXT("a 2200 llega"), FCombatModel::Advance(S, 2200, Impact));
			TestEqual(TEXT("en su instante"), (int32)Impact.ImpactMs, 2200);
			TestEqual(TEXT("cargado"), (int32)Impact.Swing, (int32)ECombatSwing::Charged);
			TestFalse(TEXT("solo una vez"), FCombatModel::Advance(S, 2300, Impact));
			TestEqual(TEXT("recuperando"), (int32)S.Phase, (int32)ECombatPhase::Recovery);
			TestEqual(TEXT("en recuperación no se golpea"), (int32)FCombatModel::TryQuick(S, 2499).Reject, (int32)ECombatReject::Busy);
			TestTrue(TEXT("a 2500 sí"), FCombatModel::TryQuick(S, 2500).bAccepted);
		});

		It("un salto de reloj grande entrega el cargado con su instante exacto", [this]()
		{
			FCombatTimingState S;
			FCombatModel::StartCharge(S, 0);
			FCombatActionResult Impact;
			TestTrue(TEXT("llega"), FCombatModel::Advance(S, 60000, Impact));
			TestEqual(TEXT("a 1200, no a 60 000"), (int32)Impact.ImpactMs, 1200);
			TestEqual(TEXT("y ya en reposo"), (int32)S.Phase, (int32)ECombatPhase::Idle);
		});

		It("si una acción cierra la carga, el golpe queda pendiente para Advance", [this]()
		{
			FCombatTimingState S;
			FCombatModel::StartCharge(S, 0);
			TestTrue(TEXT("rápido a 1500"), FCombatModel::TryQuick(S, 1500).bAccepted);
			FCombatActionResult Impact;
			TestTrue(TEXT("el cargado no se pierde"), FCombatModel::Advance(S, 1500, Impact));
			TestEqual(TEXT("con su instante"), (int32)Impact.ImpactMs, 1200);
		});

		It("soltar antes de tiempo cancela sin golpe; a tiempo ya no se cancela", [this]()
		{
			FCombatTimingState S;
			FCombatModel::StartCharge(S, 0);
			TestTrue(TEXT("cancela a 1199"), FCombatModel::CancelCharge(S, 1199));
			FCombatActionResult Impact;
			TestFalse(TEXT("sin golpe"), FCombatModel::Advance(S, 5000, Impact));
			TestTrue(TEXT("sin recuperación: se golpea al momento"), FCombatModel::TryQuick(S, 5000).bAccepted);

			FCombatTimingState Late;
			FCombatModel::StartCharge(Late, 0);
			TestFalse(TEXT("a 1200 ya ha llegado"), FCombatModel::CancelCharge(Late, 1200));
			TestTrue(TEXT("y se entrega"), FCombatModel::Advance(Late, 1200, Impact));
			TestFalse(TEXT("cancelar sin carga"), FCombatModel::CancelCharge(Late, 9000));
		});

		It("el cargado corta la racha de rápidos y no se puede empezar en la pausa", [this]()
		{
			FCombatTimingState S;
			FCombatModel::TryQuick(S, 0);
			FCombatModel::TryQuick(S, 450);
			TestTrue(TEXT("carga a 900"), FCombatModel::StartCharge(S, 900).bAccepted);
			// Llega a 2100, recupera hasta 2400.
			const FCombatActionResult Q = FCombatModel::TryQuick(S, 2400);
			TestEqual(TEXT("racha nueva tras el cargado"), Q.ComboIndex, 1);

			FCombatTimingState P;
			FCombatModel::TryQuick(P, 0);
			FCombatModel::TryQuick(P, 450);
			FCombatModel::TryQuick(P, 900);
			TestEqual(TEXT("en la pausa no se carga"), (int32)FCombatModel::StartCharge(P, 1500).Reject, (int32)ECombatReject::ChainPause);
			TestEqual(TEXT("ni a mitad de rápido"), (int32)FCombatModel::StartCharge(P, 1000).Reject, (int32)ECombatReject::ChainPause);
			TestTrue(TEXT("al acabar la pausa sí"), FCombatModel::StartCharge(P, 1750).bAccepted);
			TestEqual(TEXT("cargando no se carga otra vez"), (int32)FCombatModel::StartCharge(P, 1800).Reject, (int32)ECombatReject::Busy);
		});

		It("el reloj no retrocede y los tiempos extremos no desbordan", [this]()
		{
			FCombatTimingState S;
			FCombatModel::TryQuick(S, 1000);
			TestEqual(TEXT("un paquete viejo cuenta como el último instante"), (int32)FCombatModel::TryQuick(S, 500).Reject, (int32)ECombatReject::Busy);
			TestEqual(TEXT("reloj"), (int32)S.NowMs, 1000);

			FCombatTimingState Neg;
			const FCombatActionResult N = FCombatModel::TryQuick(Neg, -5000);
			TestTrue(TEXT("negativo se toma como 0"), N.bAccepted && N.ImpactMs == 0);

			FCombatTimingState Big;
			const FCombatActionResult A = FCombatModel::TryQuick(Big, TNumericLimits<int64>::Max());
			TestTrue(TEXT("aceptado al final del reloj"), A.bAccepted);
			TestTrue(TEXT("sin desbordar"), Big.PhaseEndMs > Big.PhaseStartMs);
			TestEqual(TEXT("y ocupado igual"), (int32)FCombatModel::TryQuick(Big, TNumericLimits<int64>::Max()).Reject, (int32)ECombatReject::Busy);
			TestTrue(TEXT("cargar al final del reloj también está ocupado"), FCombatModel::StartCharge(Big, TNumericLimits<int64>::Max()).Reject == ECombatReject::Busy);
		});

		It("la misma secuencia de acciones da exactamente el mismo estado (determinista)", [this]()
		{
			auto Run = [](uint64 Seed, TArray<int32>& OutLog) -> FCombatTimingState
			{
				FExploredRandom Rng(Seed);
				FCombatTimingState S;
				int64 T = 0;
				for (int32 i = 0; i < 4000; ++i)
				{
					T += Rng.RangeInt(0, 700);
					FCombatActionResult R;
					switch (Rng.RangeInt(0, 4))
					{
					case 0: R = FCombatModel::TryQuick(S, T); break;
					case 1: R = FCombatModel::StartCharge(S, T); break;
					case 2: R.bAccepted = FCombatModel::CancelCharge(S, T); break;
					case 3: R = FCombatModel::TryDodge(S, T); break;
					default: R.bAccepted = FCombatModel::Advance(S, T, R); break;
					}
					OutLog.Add(R.bAccepted ? static_cast<int32>(R.ImpactMs % 100003) + R.ComboIndex * 7 : -static_cast<int32>(R.Reject));
				}
				return S;
			};
			TArray<int32> LogA, LogB, LogC;
			const FCombatTimingState A = Run(42, LogA);
			const FCombatTimingState B = Run(42, LogB);
			Run(43, LogC);
			TestTrue(TEXT("mismo estado"), SameTiming(A, B));
			TestTrue(TEXT("mismo registro"), LogA == LogB);
			TestFalse(TEXT("otra semilla, otra pelea"), LogA == LogC);
		});
	});

	Describe("esquiva al milisegundo", [this]()
	{
		It("invulnerable en [arranque, arranque + 300 ms)", [this]()
		{
			FCombatTimingState S;
			TestTrue(TEXT("aceptada"), FCombatModel::TryDodge(S, 1000).bAccepted);
			TestFalse(TEXT("999"), FCombatModel::IsInvulnerable(S, 999));
			TestTrue(TEXT("1000"), FCombatModel::IsInvulnerable(S, 1000));
			TestTrue(TEXT("1001"), FCombatModel::IsInvulnerable(S, 1001));
			TestTrue(TEXT("1299"), FCombatModel::IsInvulnerable(S, 1299));
			TestFalse(TEXT("1300"), FCombatModel::IsInvulnerable(S, 1300));
			int32 Count = 0;
			for (int64 T = 0; T < 3000; ++T)
			{
				Count += FCombatModel::IsInvulnerable(S, T) ? 1 : 0;
			}
			TestEqual(TEXT("exactamente 300 ms"), Count, 300);
		});

		It("sin esquivar nunca es invulnerable, tampoco en el instante 0", [this]()
		{
			const FCombatTimingState S;
			TestFalse(TEXT("0"), FCombatModel::IsInvulnerable(S, 0));
			TestFalse(TEXT("100"), FCombatModel::IsInvulnerable(S, 100));
			TestEqual(TEXT("sin reutilización pendiente"), FCombatModel::DodgeCooldownRemainingMs(S, 0), 0);
		});

		It("la esquiva en el instante 0 también cuenta", [this]()
		{
			FCombatTimingState S;
			TestTrue(TEXT("aceptada"), FCombatModel::TryDodge(S, 0).bAccepted);
			TestTrue(TEXT("0"), FCombatModel::IsInvulnerable(S, 0));
			TestTrue(TEXT("299"), FCombatModel::IsInvulnerable(S, 299));
			TestFalse(TEXT("300"), FCombatModel::IsInvulnerable(S, 300));
		});

		It("se reutiliza a los 1200 ms exactos", [this]()
		{
			FCombatTimingState S;
			FCombatModel::TryDodge(S, 1000);
			TestEqual(TEXT("recién hecha"), FCombatModel::DodgeCooldownRemainingMs(S, 1000), 1200);
			TestEqual(TEXT("a mitad"), FCombatModel::DodgeCooldownRemainingMs(S, 1600), 600);
			TestEqual(TEXT("falta 1 ms"), FCombatModel::DodgeCooldownRemainingMs(S, 2199), 1);
			TestEqual(TEXT("lista"), FCombatModel::DodgeCooldownRemainingMs(S, 2200), 0);
			TestEqual(TEXT("mucho después"), FCombatModel::DodgeCooldownRemainingMs(S, 99999), 0);
			const FCombatActionResult Early = FCombatModel::TryDodge(S, 2199);
			TestFalse(TEXT("a 1199 ms no"), Early.bAccepted);
			TestEqual(TEXT("por reutilización"), (int32)Early.Reject, (int32)ECombatReject::DodgeCooldown);
			TestFalse(TEXT("la rechazada no da invulnerabilidad"), FCombatModel::IsInvulnerable(S, 2199));
			TestTrue(TEXT("a 1200 ms sí"), FCombatModel::TryDodge(S, 2200).bAccepted);
			TestTrue(TEXT("y vuelve a ser invulnerable"), FCombatModel::IsInvulnerable(S, 2200));
		});

		It("un golpe dentro de la ventana no hace nada; al milisegundo siguiente sí", [this]()
		{
			FCombatTimingState Defender;
			FCombatModel::TryDodge(Defender, 5000);
			const FCombatHit Bite = FCombatModel::CreatureAttack(ECombatCreature::ReefShark);
			TArray<ESurvivalEvent> Events;

			FSurvivalState In;
			const FCombatReceiveResult Dodged = FCombatModel::ReceiveHit(In, Defender, 5299, Bite, Survivor(), Events);
			TestTrue(TEXT("esquivado a 5299"), Dodged.bDodged);
			TestEqual(TEXT("sin daño"), In.Health, 100.0f);
			TestEqual(TEXT("sin corte"), In.Wounds.Num(), 0);

			FSurvivalState Out;
			const FCombatReceiveResult Hit = FCombatModel::ReceiveHit(Out, Defender, 5300, Bite, Survivor(), Events);
			TestFalse(TEXT("a 5300 llega"), Hit.bDodged);
			TestEqual(TEXT("12 de salud"), Out.Health, 88.0f);
			TestEqual(TEXT("un corte"), Out.Wounds.Num(), 1);
		});

		It("esquivar cancela el golpe cargado propio; no se esquiva a mitad de rápido ni recuperando", [this]()
		{
			FCombatTimingState S;
			FCombatModel::StartCharge(S, 0);
			TestTrue(TEXT("esquiva cargando"), FCombatModel::TryDodge(S, 600).bAccepted);
			TestFalse(TEXT("sin aviso"), FCombatModel::IsTelegraphing(S, 700));
			FCombatActionResult Impact;
			TestFalse(TEXT("el cargado no llega"), FCombatModel::Advance(S, 5000, Impact));

			FCombatTimingState Q;
			FCombatModel::TryQuick(Q, 0);
			TestEqual(TEXT("a mitad de rápido"), (int32)FCombatModel::TryDodge(Q, 449).Reject, (int32)ECombatReject::Busy);
			TestTrue(TEXT("al acabar el rápido"), FCombatModel::TryDodge(Q, 450).bAccepted);

			FCombatTimingState R;
			FCombatModel::StartCharge(R, 0);
			TestEqual(TEXT("recuperando"), (int32)FCombatModel::TryDodge(R, 1499).Reject, (int32)ECombatReject::Busy);
			TestTrue(TEXT("al acabar la recuperación"), FCombatModel::TryDodge(R, 1500).bAccepted);

			FCombatTimingState P;
			FCombatModel::TryQuick(P, 0);
			FCombatModel::TryQuick(P, 450);
			FCombatModel::TryQuick(P, 900);
			TestTrue(TEXT("en la pausa de la racha sí"), FCombatModel::TryDodge(P, 1500).bAccepted);
			TestEqual(TEXT("y la pausa sigue"), (int32)FCombatModel::TryQuick(P, 1600).Reject, (int32)ECombatReject::ChainPause);
		});
	});

	Describe("arco por tramos de distancia", [this]()
	{
		It("100/70/40/0 % con cada límite en el tramo más lejano", [this]()
		{
			TestEqual(TEXT("0 m"), FCombatModel::BowAccuracyPct(0.0f), 100);
			TestEqual(TEXT("justo antes de 15"), FCombatModel::BowAccuracyPct(14.999999f), 100);
			TestEqual(TEXT("15"), FCombatModel::BowAccuracyPct(15.0f), 70);
			TestEqual(TEXT("justo después de 15"), FCombatModel::BowAccuracyPct(15.000001f), 70);
			TestEqual(TEXT("justo antes de 30"), FCombatModel::BowAccuracyPct(29.999998f), 70);
			TestEqual(TEXT("30"), FCombatModel::BowAccuracyPct(30.0f), 40);
			TestEqual(TEXT("justo antes de 45"), FCombatModel::BowAccuracyPct(44.999996f), 40);
			TestEqual(TEXT("45"), FCombatModel::BowAccuracyPct(45.0f), 0);
			TestEqual(TEXT("1 km"), FCombatModel::BowAccuracyPct(1000.0f), 0);
		});

		It("distancias corruptas: NaN e infinito fallan, negativa cuenta como 0 m", [this]()
		{
			TestEqual(TEXT("NaN"), FCombatModel::BowAccuracyPct(NAN), 0);
			TestEqual(TEXT("+inf"), FCombatModel::BowAccuracyPct(INFINITY), 0);
			TestEqual(TEXT("-inf"), FCombatModel::BowAccuracyPct(-INFINITY), 0);
			TestEqual(TEXT("-3 m"), FCombatModel::BowAccuracyPct(-3.0f), 100);
			TestFalse(TEXT("NaN nunca acierta"), FCombatModel::BowShotHits(NAN, 1, 1));
		});

		It("la tirada acierta en la proporción de su tramo y es determinista", [this]()
		{
			const float Distances[] = { 10.0f, 20.0f, 40.0f, 45.0f };
			const int32 Expected[] = { 100, 70, 40, 0 };
			for (int32 d = 0; d < 4; ++d)
			{
				int32 Hits = 0;
				for (uint32 Shot = 0; Shot < 10000; ++Shot)
				{
					const bool bHit = FCombatModel::BowShotHits(Distances[d], 7u, Shot);
					if (bHit != FCombatModel::BowShotHits(Distances[d], 7u, Shot))
					{
						TestTrue(TEXT("determinista"), false);
						return;
					}
					Hits += bHit ? 1 : 0;
				}
				TestTrue(*FString::Printf(TEXT("%.0f m ≈ %d %% (%d/10000)"), Distances[d], Expected[d], Hits),
					FMath::Abs(Hits - Expected[d] * 100) <= 250);
			}
			int32 Differ = 0;
			for (uint32 Shot = 0; Shot < 200; ++Shot)
			{
				Differ += FCombatModel::BowShotHits(20.0f, 1u, Shot) != FCombatModel::BowShotHits(20.0f, 2u, Shot) ? 1 : 0;
			}
			TestTrue(TEXT("otra semilla, otras tiradas"), Differ > 0);
		});

		It("la flecha hace Punta × 3 y corta Punta ÷ 5", [this]()
		{
			const FCombatHit Hit = FCombatModel::ArrowHit(Props({ { TEXT("Punta"), 3.0f }, { TEXT("Largo"), 3.0f } }));
			TestEqual(TEXT("perforante"), (int32)Hit.Kind, (int32)ECombatDamageKind::Pierce);
			TestEqual(TEXT("9"), Hit.HealthDamage, 9);
			TestEqual(TEXT("0,6"), Hit.CutDepth, 0.6f);
			TestEqual(TEXT("sin variante"), (int32)Hit.Swing, (int32)ECombatSwing::Plain);
		});
	});

	Describe("animales", [this]()
	{
		It("fichas de la biblia 05 §5", [this]()
		{
			const FCombatCreatureStats& Boar = FCombatModel::CreatureStats(ECombatCreature::WildBoar);
			TestEqual(TEXT("jabalí vida"), Boar.HealthPoints, 35);
			TestEqual(TEXT("jabalí daño"), Boar.AttackDamage, 16);
			TestEqual(TEXT("jabalí aturde 1 s"), Boar.AttackStunMs, 1000);
			TestEqual(TEXT("jabalí huye al 30 %"), Boar.FleeHealthFraction, 0.3f);
			const FCombatCreatureStats& Goat = FCombatModel::CreatureStats(ECombatCreature::WildGoat);
			TestEqual(TEXT("cabra vida"), Goat.HealthPoints, 20);
			TestEqual(TEXT("cabra daño"), Goat.AttackDamage, 4);
			const FCombatCreatureStats& Crab = FCombatModel::CreatureStats(ECombatCreature::CoconutCrab);
			TestEqual(TEXT("cangrejo vida"), Crab.HealthPoints, 15);
			TestEqual(TEXT("cangrejo daño"), Crab.AttackDamage, 8);
			TestEqual(TEXT("cangrejo huye al primer golpe"), Crab.FleeAfterHits, 1);
			const FCombatCreatureStats& Shark = FCombatModel::CreatureStats(ECombatCreature::ReefShark);
			TestEqual(TEXT("tiburón vida"), Shark.HealthPoints, 55);
			TestEqual(TEXT("tiburón daño"), Shark.AttackDamage, 12);
			TestEqual(TEXT("tiburón corte 0,6"), Shark.AttackCutDepth, 0.6f);
			TestEqual(TEXT("tiburón vuelca el 15 %"), Shark.CapsizeChancePct, 15);
		});

		It("el daño de cada animal es el de su propiedad equivalente sin variante", [this]()
		{
			for (int32 i = 0; i < static_cast<int32>(ECombatCreature::Count); ++i)
			{
				const ECombatCreature C = static_cast<ECombatCreature>(i);
				const FCombatCreatureStats& Stats = FCombatModel::CreatureStats(C);
				TestEqual(LexToString(C), Stats.AttackDamage, Hp(ECombatSwing::Plain, Stats.AttackKind, Stats.AttackProperty));
				TestTrue(TEXT("id de datos"), Stats.Id[0] != 0);
				const FCombatHit Hit = FCombatModel::CreatureAttack(C);
				TestEqual(TEXT("el ataque usa la ficha"), Hit.HealthDamage, Stats.AttackDamage);
				TestEqual(TEXT("y su corte"), Hit.CutDepth, Stats.AttackCutDepth);
			}
		});

		It("el mordisco del tiburón abre un corte de 0,6 en el jugador; la embestida del jabalí no corta", [this]()
		{
			FSurvivalState Body;
			TArray<ESurvivalEvent> Events;
			FCombatModel::ReceiveHit(Body, FCombatTimingState(), 0, FCombatModel::CreatureAttack(ECombatCreature::WildBoar), Survivor(), Events);
			TestEqual(TEXT("16 de salud"), Body.Health, 84.0f);
			TestEqual(TEXT("sin corte"), Body.Wounds.Num(), 0);
			FCombatModel::ReceiveHit(Body, FCombatTimingState(), 0, FCombatModel::CreatureAttack(ECombatCreature::ReefShark), Survivor(), Events);
			TestEqual(TEXT("12 más"), Body.Health, 72.0f);
			if (TestEqual(TEXT("un corte"), Body.Wounds.Num(), 1))
			{
				TestEqual(TEXT("de 0,6"), Body.Wounds[0].Depth, 0.6f);
			}
		});

		It("el jabalí huye al quedar con el 30 % (10 de 35), no con 11", [this]()
		{
			FCombatCreatureState Boar = FCombatModel::NewCreature(ECombatCreature::WildBoar);
			const FCombatCreatureHitResult A = FCombatModel::HitCreature(Boar, ECombatCreature::WildBoar, Damage(24));
			TestEqual(TEXT("vida 11"), Boar.Health, 11);
			TestFalse(TEXT("con 11 no huye"), A.bStartedFleeing);
			const FCombatCreatureHitResult B = FCombatModel::HitCreature(Boar, ECombatCreature::WildBoar, Damage(1));
			TestTrue(TEXT("con 10 huye"), B.bStartedFleeing && Boar.bFleeing);
			const FCombatCreatureHitResult C = FCombatModel::HitCreature(Boar, ECombatCreature::WildBoar, Damage(1));
			TestFalse(TEXT("empezar a huir es una vez"), C.bStartedFleeing);
		});

		It("el cangrejo huye tras el primer golpe, aunque no le haga daño; la cabra también", [this]()
		{
			FCombatCreatureState Crab = FCombatModel::NewCreature(ECombatCreature::CoconutCrab);
			TestTrue(TEXT("cangrejo"), FCombatModel::HitCreature(Crab, ECombatCreature::CoconutCrab, Damage(0)).bStartedFleeing);
			TestEqual(TEXT("sin daño"), Crab.Health, 15);
			FCombatCreatureState Goat = FCombatModel::NewCreature(ECombatCreature::WildGoat);
			TestTrue(TEXT("cabra"), FCombatModel::HitCreature(Goat, ECombatCreature::WildGoat, Damage(2)).bStartedFleeing);
		});

		It("el tiburón no huye por daño y muere a los 55", [this]()
		{
			FCombatCreatureState Shark = FCombatModel::NewCreature(ECombatCreature::ReefShark);
			for (int32 i = 0; i < 5; ++i)
			{
				TestFalse(TEXT("no huye"), FCombatModel::HitCreature(Shark, ECombatCreature::ReefShark, Damage(10)).bStartedFleeing);
			}
			TestEqual(TEXT("vida 5"), Shark.Health, 5);
			const FCombatCreatureHitResult Kill = FCombatModel::HitCreature(Shark, ECombatCreature::ReefShark, Damage(24));
			TestTrue(TEXT("muere"), Kill.bKilled && Shark.bDead);
			TestEqual(TEXT("solo cuenta la vida que le quedaba"), Kill.DamageDealt, 5);
			TestEqual(TEXT("vida 0"), Shark.Health, 0);
			const FCombatCreatureHitResult After = FCombatModel::HitCreature(Shark, ECombatCreature::ReefShark, Damage(24));
			TestFalse(TEXT("un muerto no muere dos veces"), After.bKilled);
			TestEqual(TEXT("ni recibe daño"), After.DamageDealt, 0);
			TestEqual(TEXT("ni cuenta golpes"), Shark.HitsTaken, 6);
		});

		It("dos golpes cargados de lanza de obsidiana matan al jabalí", [this]()
		{
			FCombatCreatureState Boar = FCombatModel::NewCreature(ECombatCreature::WildBoar);
			const FCombatHit Hit = FCombatModel::HitWithItem(Props({ { TEXT("Punta"), 5.0f } }), ECombatSwing::Charged);
			TestFalse(TEXT("el primero no"), FCombatModel::HitCreature(Boar, ECombatCreature::WildBoar, Hit).bKilled);
			TestTrue(TEXT("el segundo sí"), FCombatModel::HitCreature(Boar, ECombatCreature::WildBoar, Hit).bKilled);
		});

		It("daño negativo no cura y un animal inválido nace muerto", [this]()
		{
			FCombatCreatureState Boar = FCombatModel::NewCreature(ECombatCreature::WildBoar);
			FCombatModel::HitCreature(Boar, ECombatCreature::WildBoar, Damage(-50));
			TestEqual(TEXT("vida intacta"), Boar.Health, 35);
			const ECombatCreature Bad = ECombatCreature::Count;
			TestEqual(TEXT("ficha vacía"), FCombatModel::CreatureStats(Bad).HealthPoints, 0);
			TestTrue(TEXT("muerto"), FCombatModel::NewCreature(Bad).bDead);
			TestEqual(TEXT("sin ataque"), FCombatModel::CreatureAttack(Bad).HealthDamage, 0);
		});
	});

	Describe("red: el servidor resuelve", [this]()
	{
		It("los mensajes ocupan 7, 8 y 2 bytes", [this]()
		{
			TArray<uint8> Bytes;
			FCombatModel::EncodeAction(FCombatActionMsg(), Bytes);
			TestEqual(TEXT("acción"), Bytes.Num(), FCombatModel::ActionMsgBytes);
			TestEqual(TEXT("acción = 7"), FCombatModel::ActionMsgBytes, 7);
			FCombatModel::EncodeImpact(FCombatImpactMsg(), Bytes);
			TestEqual(TEXT("impacto"), Bytes.Num(), FCombatModel::ImpactMsgBytes);
			TestEqual(TEXT("impacto = 8"), FCombatModel::ImpactMsgBytes, 8);
			FCombatModel::EncodeNetState(FCombatNetState(), Bytes);
			TestEqual(TEXT("estado"), Bytes.Num(), FCombatModel::NetStateBytes);
			TestEqual(TEXT("estado = 2"), FCombatModel::NetStateBytes, 2);
		});

		It("la acción va y vuelve sin pérdida, también en los extremos", [this]()
		{
			for (int32 a = 0; a < static_cast<int32>(ECombatNetAction::Count); ++a)
			{
				for (const int32 Pitch : { -32768, -1, 0, 1, 32767 })
				{
					FCombatActionMsg Msg;
					Msg.Action = static_cast<ECombatNetAction>(a);
					Msg.ClientTimeMs = static_cast<uint16>(65535 - a);
					Msg.AimYaw = static_cast<uint16>(a * 13107);
					Msg.AimPitch = static_cast<int16>(Pitch);
					TArray<uint8> Bytes;
					FCombatModel::EncodeAction(Msg, Bytes);
					FCombatActionMsg Back;
					if (!TestTrue(TEXT("decodifica"), FCombatModel::DecodeAction(Bytes, Back)))
					{
						return;
					}
					TestTrue(LexToString(Msg.Action), Back.Action == Msg.Action && Back.ClientTimeMs == Msg.ClientTimeMs
						&& Back.AimYaw == Msg.AimYaw && Back.AimPitch == Msg.AimPitch);
				}
			}
		});

		It("una acción truncada, larga o desconocida se rechaza sin tocar la salida", [this]()
		{
			FCombatActionMsg Msg;
			Msg.Action = ECombatNetAction::Dodge;
			TArray<uint8> Bytes;
			FCombatModel::EncodeAction(Msg, Bytes);
			FCombatActionMsg Out;
			Out.AimYaw = 777;
			TArray<uint8> Short = Bytes;
			Short.Pop();
			TestFalse(TEXT("6 bytes"), FCombatModel::DecodeAction(Short, Out));
			TArray<uint8> Long = Bytes;
			Long.Add(0);
			TestFalse(TEXT("8 bytes"), FCombatModel::DecodeAction(Long, Out));
			TestFalse(TEXT("vacío"), FCombatModel::DecodeAction(TArray<uint8>(), Out));
			TArray<uint8> Unknown = Bytes;
			Unknown[0] = static_cast<uint8>(ECombatNetAction::Count);
			TestFalse(TEXT("acción 5"), FCombatModel::DecodeAction(Unknown, Out));
			Unknown[0] = 255;
			TestFalse(TEXT("acción 255"), FCombatModel::DecodeAction(Unknown, Out));
			TestEqual(TEXT("salida intacta"), (int32)Out.AimYaw, 777);
		});

		It("el impacto va y vuelve con todas las combinaciones de tipo, variante y banderas", [this]()
		{
			for (int32 k = 0; k < static_cast<int32>(ECombatDamageKind::Count); ++k)
			{
				for (int32 s = 0; s < static_cast<int32>(ECombatSwing::Count); ++s)
				{
					for (int32 f = 0; f < 8; ++f)
					{
						FCombatImpactMsg Msg;
						Msg.AttackerId = static_cast<uint16>(0xBEEF);
						Msg.VictimId = static_cast<uint16>(k * 1000 + s);
						Msg.Kind = static_cast<ECombatDamageKind>(k);
						Msg.Swing = static_cast<ECombatSwing>(s);
						Msg.bDodged = (f & 1) != 0;
						Msg.bKilled = (f & 2) != 0 && !Msg.bDodged;
						Msg.bFleeing = (f & 4) != 0;
						if (!Msg.bDodged)
						{
							Msg.HealthDamage = 255;
							Msg.CutDepth255 = 153;
							Msg.StunDeciseconds = 15;
						}
						TArray<uint8> Bytes;
						FCombatModel::EncodeImpact(Msg, Bytes);
						FCombatImpactMsg Back;
						if (!TestTrue(TEXT("decodifica"), FCombatModel::DecodeImpact(Bytes, Back)))
						{
							return;
						}
						TestTrue(TEXT("igual"), Back.AttackerId == Msg.AttackerId && Back.VictimId == Msg.VictimId
							&& Back.Kind == Msg.Kind && Back.Swing == Msg.Swing && Back.bDodged == Msg.bDodged
							&& Back.bKilled == Msg.bKilled && Back.bFleeing == Msg.bFleeing
							&& Back.HealthDamage == Msg.HealthDamage && Back.CutDepth255 == Msg.CutDepth255
							&& Back.StunDeciseconds == Msg.StunDeciseconds);
					}
				}
			}
		});

		It("un impacto manipulado se rechaza", [this]()
		{
			FCombatImpactMsg Msg = FCombatModel::MakeImpact(1, 2, FCombatModel::CreatureAttack(ECombatCreature::ReefShark), false, false, false);
			TArray<uint8> Good;
			FCombatModel::EncodeImpact(Msg, Good);
			FCombatImpactMsg Out;
			TArray<uint8> B = Good;
			B[4] |= 0x80;
			TestFalse(TEXT("bit reservado"), FCombatModel::DecodeImpact(B, Out));
			B = Good;
			B[4] = static_cast<uint8>((B[4] & ~0x0C) | (3 << 2));
			TestFalse(TEXT("variante 3"), FCombatModel::DecodeImpact(B, Out));
			B = Good;
			B[4] |= 0x10;
			TestFalse(TEXT("esquivado con daño"), FCombatModel::DecodeImpact(B, Out));
			B = Good;
			B.Pop();
			TestFalse(TEXT("7 bytes"), FCombatModel::DecodeImpact(B, Out));
			B = Good;
			B.Add(0);
			TestFalse(TEXT("9 bytes"), FCombatModel::DecodeImpact(B, Out));
			TestTrue(TEXT("el bueno pasa"), FCombatModel::DecodeImpact(Good, Out));
		});

		It("MakeImpact cuantiza el corte y el aturdimiento y satura el daño", [this]()
		{
			const FCombatImpactMsg Bite = FCombatModel::MakeImpact(3, 4, FCombatModel::CreatureAttack(ECombatCreature::ReefShark), false, false, false);
			TestEqual(TEXT("12"), (int32)Bite.HealthDamage, 12);
			TestEqual(TEXT("0,6 → 153/255"), (int32)Bite.CutDepth255, 153);
			const FCombatImpactMsg Ram = FCombatModel::MakeImpact(3, 4, FCombatModel::CreatureAttack(ECombatCreature::WildBoar), false, false, false);
			TestEqual(TEXT("1 s → 10 décimas"), (int32)Ram.StunDeciseconds, 10);
			const FCombatImpactMsg Hammer = FCombatModel::MakeImpact(3, 4,
				FCombatModel::HitFor(Strike(ECombatDamageKind::Blunt, 4), ECombatSwing::Plain), false, false, false);
			TestEqual(TEXT("1,5 s → 15 décimas"), (int32)Hammer.StunDeciseconds, 15);
			FCombatHit Wild;
			Wild.Kind = ECombatDamageKind::Cut;
			Wild.HealthDamage = 999;
			Wild.CutDepth = NAN;
			Wild.StunMs = 999999;
			const FCombatImpactMsg Sat = FCombatModel::MakeImpact(0, 0, Wild, false, true, false);
			TestEqual(TEXT("daño saturado"), (int32)Sat.HealthDamage, 255);
			TestEqual(TEXT("corte NaN = 0"), (int32)Sat.CutDepth255, 0);
			TestEqual(TEXT("aturdimiento saturado"), (int32)Sat.StunDeciseconds, 255);
			const FCombatImpactMsg Dodged = FCombatModel::MakeImpact(0, 0, Wild, true, true, false);
			TestTrue(TEXT("esquivado va vacío"), Dodged.bDodged && !Dodged.bKilled && Dodged.HealthDamage == 0
				&& Dodged.CutDepth255 == 0 && Dodged.StunDeciseconds == 0);
			TArray<uint8> Bytes;
			FCombatModel::EncodeImpact(Dodged, Bytes);
			FCombatImpactMsg Back;
			TestTrue(TEXT("y se decodifica"), FCombatModel::DecodeImpact(Bytes, Back));
		});

		It("el estado replicado muestra el aviso del cargado y la pausa de la racha", [this]()
		{
			FCombatTimingState S;
			FCombatModel::StartCharge(S, 1000);
			const FCombatNetState Charging = FCombatModel::MakeNetState(S, 1650);
			TestEqual(TEXT("cargando"), (int32)Charging.Phase, (int32)ECombatPhase::Charging);
			TestEqual(TEXT("0,6 s"), (int32)Charging.ElapsedDeciseconds, 6);
			const FCombatNetState After = FCombatModel::MakeNetState(S, 2250);
			TestEqual(TEXT("recuperando (sin tocar el estado)"), (int32)After.Phase, (int32)ECombatPhase::Recovery);
			TestEqual(TEXT("el estado real sigue cargando"), (int32)S.Phase, (int32)ECombatPhase::Charging);

			FCombatTimingState P;
			FCombatModel::TryQuick(P, 0);
			FCombatModel::TryQuick(P, 450);
			FCombatModel::TryQuick(P, 900);
			const FCombatNetState Pause = FCombatModel::MakeNetState(P, 1400);
			TestEqual(TEXT("pausa"), (int32)Pause.Phase, (int32)ECombatPhase::ChainPause);
			TestEqual(TEXT("racha 3"), (int32)Pause.ChainCount, 3);
			TestEqual(TEXT("reposo sin tiempo"), (int32)FCombatModel::MakeNetState(FCombatTimingState(), 5000).ElapsedDeciseconds, 0);

			for (int32 Phase = 0; Phase < static_cast<int32>(ECombatPhase::Count); ++Phase)
			{
				FCombatNetState Msg;
				Msg.Phase = static_cast<ECombatPhase>(Phase);
				Msg.ChainCount = static_cast<uint8>(Phase % 4);
				Msg.ElapsedDeciseconds = 255;
				TArray<uint8> Bytes;
				FCombatModel::EncodeNetState(Msg, Bytes);
				FCombatNetState Back;
				TestTrue(TEXT("va y vuelve"), FCombatModel::DecodeNetState(Bytes, Back) && Back.Phase == Msg.Phase
					&& Back.ChainCount == Msg.ChainCount && Back.ElapsedDeciseconds == 255);
			}
			FCombatNetState Out;
			TestFalse(TEXT("fase 5"), FCombatModel::DecodeNetState(TArray<uint8>({ 5, 0 }), Out));
			TestFalse(TEXT("fase 7"), FCombatModel::DecodeNetState(TArray<uint8>({ 7, 0 }), Out));
			TestFalse(TEXT("bits reservados"), FCombatModel::DecodeNetState(TArray<uint8>({ 0x20, 0 }), Out));
			TestFalse(TEXT("1 byte"), FCombatModel::DecodeNetState(TArray<uint8>({ 0 }), Out));
			TestFalse(TEXT("3 bytes"), FCombatModel::DecodeNetState(TArray<uint8>({ 0, 0, 0 }), Out));
		});

		It("el servidor valida alcance, reloj del cliente y cadencia", [this]()
		{
			TestTrue(TEXT("cuchillo a 1,7 m"), FCombatModel::ServerAcceptsReach(1.7f, 1.2f));
			TestFalse(TEXT("cuchillo a 1,71 m"), FCombatModel::ServerAcceptsReach(1.71f, 1.2f));
			TestTrue(TEXT("lanza a 2,7 m"), FCombatModel::ServerAcceptsReach(2.7f, 2.2f));
			TestFalse(TEXT("NaN"), FCombatModel::ServerAcceptsReach(NAN, 1.2f));
			TestFalse(TEXT("infinito"), FCombatModel::ServerAcceptsReach(1.0f, INFINITY));
			TestFalse(TEXT("negativa"), FCombatModel::ServerAcceptsReach(-0.1f, 1.2f));
			TestFalse(TEXT("sin arma de alcance"), FCombatModel::ServerAcceptsReach(0.1f, 0.0f));

			TestTrue(TEXT("mismo instante"), FCombatModel::ServerAcceptsClientTime(1000, 1000));
			TestTrue(TEXT("250 ms"), FCombatModel::ServerAcceptsClientTime(1250, 1000));
			TestFalse(TEXT("251 ms"), FCombatModel::ServerAcceptsClientTime(1251, 1000));
			TestTrue(TEXT("a través del desborde de 16 bits"), FCombatModel::ServerAcceptsClientTime(100, 65386));
			TestFalse(TEXT("251 a través del desborde"), FCombatModel::ServerAcceptsClientTime(100, 65385));
			TestFalse(TEXT("cliente por delante"), FCombatModel::ServerAcceptsClientTime(1000, 1001));

			TestEqual(TEXT("cadencia mínima 382 ms"), FCombatModel::MinQuickCadenceMs(), 382);
		});
	});
}

#endif
