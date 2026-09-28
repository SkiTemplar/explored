#include "Misc/AutomationTest.h"

#include "Boats/RaftYardModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace RaftYardSpecDetail
{
	constexpr float Dt = 1.0f / 60.0f;
	const float NaN = std::numeric_limits<float>::quiet_NaN();

	FHullPiece Piece(EHullPieceType Type, const FVector& Center, const FVector& Size = FVector::ZeroVector)
	{
		FHullPiece P;
		P.Type = Type;
		P.CenterCm = Center;
		P.SizeCm = Size;
		return P;
	}

	/**
	 * Balsa de 6 troncos de 3 m (uno junto a otro en Y, quilla en z = 0) con dos
	 * travesaños de tablón encima en X = ±100. 17 uniones: 5 entre troncos y 12 de
	 * travesaño a tronco, todas con el mismo tipo.
	 */
	FRaftYardModel SixLogRaft(ERaftJointKind Kind = ERaftJointKind::Rope)
	{
		FRaftYardModel Yard;
		const double Width = FHullAssemblyModel::Spec(EHullPieceType::Log).DefaultSizeCm.Y;
		for (int32 I = 0; I < 6; ++I)
		{
			Yard.AddPiece(Piece(EHullPieceType::Log, FVector(0.0, (I - 2.5) * Width, 11.0)));
		}
		const int32 Fore = Yard.AddPiece(Piece(EHullPieceType::Plank, FVector(100.0, 0.0, 24.0), FVector(25.0, 140.0, 4.0)));
		const int32 Aft = Yard.AddPiece(Piece(EHullPieceType::Plank, FVector(-100.0, 0.0, 24.0), FVector(25.0, 140.0, 4.0)));
		for (int32 I = 0; I < 5; ++I)
		{
			Yard.AddJoint(I, I + 1, Kind);
		}
		for (int32 I = 0; I < 6; ++I)
		{
			Yard.AddJoint(Fore, I, Kind);
			Yard.AddJoint(Aft, I, Kind);
		}
		return Yard;
	}

	FLaunchPath FlatPath(ELaunchSurface Surface, float LengthCm)
	{
		FLaunchPath Path;
		FLaunchSegment Segment;
		Segment.Surface = Surface;
		Segment.LengthCm = LengthCm;
		Path.Segments.Add(Segment);
		return Path;
	}

	float MinHealth(const FRaftYardModel& Yard)
	{
		float Min = 1.0f;
		for (const FRaftJoint& Joint : Yard.GetJoints())
		{
			Min = FMath::Min(Min, Joint.Health01);
		}
		return Min;
	}

	double PiecesMass(const TArray<FHullPiece>& Pieces)
	{
		double Mass = 0.0;
		for (const FHullPiece& P : Pieces)
		{
			Mass += FHullAssemblyModel::PieceMassKg(P);
		}
		return Mass;
	}

	/** Empuja con fuerza constante hasta que el centro pase TargetS, se pare o se acabe el tiempo. */
	FRaftPushReport PushUntil(FRaftYardModel& Yard, float ForceN, float TargetS, float FrameDt = Dt, float MaxSeconds = 120.0f)
	{
		FRaftPushReport Total;
		for (float T = 0.0f; T < MaxSeconds && Yard.GetCenterS() < TargetS && Yard.GetState() == ERaftYardState::Ashore; T += FrameDt)
		{
			const FRaftPushReport R = Yard.Push(ForceN, FrameDt);
			Total.MovedCm += R.MovedCm;
			Total.bOnRollers = R.bOnRollers;
			Total.bAtPathEnd |= R.bAtPathEnd;
			Total.bLaunched |= R.bLaunched;
			Total.WaterSupport01 = R.WaterSupport01;
			Total.Damage.Append(R.Damage);
			if (R.bAtPathEnd)
			{
				break;
			}
		}
		return Total;
	}
}

BEGIN_DEFINE_SPEC(FRaftYardModelSpec, "Explored.RaftYard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FRaftYardModelSpec)

void FRaftYardModelSpec::Define()
{
	using namespace RaftYardSpecDetail;

	Describe("las uniones", [this]()
	{
		It("solo unen piezas distintas, existentes, sin repetir y con un hueco de 5 cm como mucho", [this]()
		{
			FRaftYardModel Yard;
			const int32 A = Yard.AddPiece(Piece(EHullPieceType::Log, FVector(0.0, 0.0, 11.0)));
			const int32 B = Yard.AddPiece(Piece(EHullPieceType::Log, FVector(0.0, 22.0, 11.0)));
			const int32 Edge = Yard.AddPiece(Piece(EHullPieceType::Log, FVector(0.0, 22.0 + 22.0 + 5.0, 11.0)));
			const int32 Far = Yard.AddPiece(Piece(EHullPieceType::Log, FVector(0.0, -22.0 - 5.01, 11.0)));
			TestEqual(TEXT("consigo misma no"), Yard.AddJoint(A, A, ERaftJointKind::Rope), INDEX_NONE);
			TestEqual(TEXT("pieza inexistente no"), Yard.AddJoint(A, 99, ERaftJointKind::Rope), INDEX_NONE);
			TestEqual(TEXT("índice negativo no"), Yard.AddJoint(-1, A, ERaftJointKind::Rope), INDEX_NONE);
			TestTrue(TEXT("tocándose sí"), Yard.AddJoint(A, B, ERaftJointKind::Rope) != INDEX_NONE);
			TestEqual(TEXT("repetida (al revés) no"), Yard.AddJoint(B, A, ERaftJointKind::Nails), INDEX_NONE);
			TestTrue(TEXT("con 5 cm justos de hueco sí"), Yard.AddJoint(B, Edge, ERaftJointKind::Fiber) != INDEX_NONE);
			TestEqual(TEXT("con 5,01 cm no"), Yard.AddJoint(A, Far, ERaftJointKind::Fiber), INDEX_NONE);
			TestEqual(TEXT("tipo fuera de rango no"), Yard.AddJoint(A, Far, ERaftJointKind::Count), INDEX_NONE);
			TestEqual(TEXT("dos uniones"), Yard.GetJoints().Num(), 2);
		});

		It("desmontar una pieza quita sus uniones y renumera las demás", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			TestTrue(TEXT("desmonta el tronco 2"), Yard.RemovePiece(2));
			TestEqual(TEXT("quedan 7 piezas"), Yard.GetHull().GetPieces().Num(), 7);
			// Se van la 1–2, la 2–3 y sus dos travesaños: 17 − 4.
			TestEqual(TEXT("quedan 13 uniones"), Yard.GetJoints().Num(), 13);
			for (const FRaftJoint& Joint : Yard.GetJoints())
			{
				TestTrue(TEXT("índices válidos"), Yard.GetHull().GetPieces().IsValidIndex(Joint.PieceA) && Yard.GetHull().GetPieces().IsValidIndex(Joint.PieceB));
				TestTrue(TEXT("siguen tocándose"), FRaftYardModel::GapCm(Yard.GetHull().GetPieces()[Joint.PieceA], Yard.GetHull().GetPieces()[Joint.PieceB]) <= FRaftYardModel::MaxJointGapCm);
			}
			TestFalse(TEXT("índice inexistente"), Yard.RemovePiece(7));
		});

		It("una unión rota suelta la pieza que solo ella sujetaba y no se pierde masa", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			const int32 Float = Yard.AddPiece(Piece(EHullPieceType::Float, FVector(0.0, 66.0 + 20.0, 20.0)));
			const int32 Joint = Yard.AddJoint(Float, 5, ERaftJointKind::Fiber);
			TestTrue(TEXT("flotador atado"), Joint != INDEX_NONE);
			const double MassBefore = PiecesMass(Yard.GetHull().GetPieces());
			const int32 PiecesBefore = Yard.GetHull().GetPieces().Num();

			const FRaftDamageReport R = Yard.DamageJoint(Joint, 1.0f);
			TestEqual(TEXT("una rota"), R.JointsBroken, 1);
			if (TestEqual(TEXT("suelta una pieza"), R.Released.Num(), 1))
			{
				TestTrue(TEXT("es el flotador"), R.Released[0].Type == EHullPieceType::Float);
			}
			TestEqual(TEXT("piezas: casco + sueltas"), Yard.GetHull().GetPieces().Num() + R.Released.Num(), PiecesBefore);
			TestEqual(TEXT("masa: casco + sueltas"), PiecesMass(Yard.GetHull().GetPieces()) + PiecesMass(R.Released), MassBefore, 1e-6);
			TestEqual(TEXT("la unión del flotador desaparece"), Yard.GetJoints().Num(), 17);
		});

		It("una unión rota con otras que sujetan no suelta nada y se repara", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			const FRaftDamageReport R = Yard.DamageJoint(0, 5.0f);
			TestEqual(TEXT("rota"), R.JointsBroken, 1);
			TestEqual(TEXT("no suelta nada: los travesaños sujetan"), R.Released.Num(), 0);
			TestTrue(TEXT("sigue en la lista, rota"), Yard.GetJoints()[0].IsBroken());
			TestEqual(TEXT("salud 0, no negativa"), Yard.GetJoints()[0].Health01, 0.0f);
			TestTrue(TEXT("primera reparación"), Yard.RepairJoint(0));
			TestEqual(TEXT("media"), Yard.GetJoints()[0].Health01, 0.5f, 1e-6f);
			TestTrue(TEXT("segunda"), Yard.RepairJoint(0));
			TestEqual(TEXT("intacta"), Yard.GetJoints()[0].Health01, 1.0f);
			TestFalse(TEXT("intacta no se repara"), Yard.RepairJoint(0));
			TestFalse(TEXT("inexistente"), Yard.RepairJoint(99));
			TestEqual(TEXT("integridad completa"), Yard.Integrity01(), 1.0f);
		});

		It("se queda el grupo de más masa, aunque no tenga la pieza 0, sin depender del orden de las uniones", [this]()
		{
			auto Build = [](bool bReverse)
			{
				FRaftYardModel Yard;
				Yard.AddPiece(Piece(EHullPieceType::Float, FVector(0.0, -31.0, 11.0)));
				Yard.AddPiece(Piece(EHullPieceType::Log, FVector(0.0, 0.0, 11.0)));
				Yard.AddPiece(Piece(EHullPieceType::Log, FVector(0.0, 22.0, 11.0)));
				if (bReverse)
				{
					Yard.AddJoint(2, 1, ERaftJointKind::Rope);
					Yard.AddJoint(1, 0, ERaftJointKind::Fiber);
				}
				else
				{
					Yard.AddJoint(0, 1, ERaftJointKind::Fiber);
					Yard.AddJoint(1, 2, ERaftJointKind::Rope);
				}
				return Yard;
			};
			for (bool bReverse : { false, true })
			{
				FRaftYardModel Yard = Build(bReverse);
				// La unión del flotador es la de fibra, esté donde esté en la lista.
				const int32 FiberJoint = Yard.GetJoints()[0].Kind == ERaftJointKind::Fiber ? 0 : 1;
				const FRaftDamageReport R = Yard.DamageJoint(FiberJoint, 1.0f);
				if (TestEqual(TEXT("suelta una"), R.Released.Num(), 1))
				{
					TestTrue(TEXT("suelta el flotador (pieza 0), no los troncos"), R.Released[0].Type == EHullPieceType::Float);
				}
				TestEqual(TEXT("quedan los dos troncos"), Yard.GetHull().GetPieces().Num(), 2);
				TestTrue(TEXT("la unión entre troncos se renumera"), Yard.GetJoints().Num() == 1 && Yard.GetJoints()[0].PieceA == 0 && Yard.GetJoints()[0].PieceB == 1);
			}
		});
	});

	Describe("los golpes y el roce", [this]()
	{
		It("un golpe por debajo de la velocidad segura de FBoatModel no hace nada", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			const FRaftDamageReport R = Yard.ApplyImpact(FBoatModel::SafeImpactSpeedCmS, FVector2D(1.0, 0.0));
			TestEqual(TEXT("nada dañado"), R.JointsDamaged, 0);
			TestEqual(TEXT("intacta"), Yard.Integrity01(), 1.0f);
			TestEqual(TEXT("NaN no hace nada"), Yard.ApplyImpact(NaN, FVector2D(1.0, 0.0)).JointsDamaged, 0);
		});

		It("un golpe de proa daña más las uniones de proa que las de popa", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.ApplyImpact(300.0f, FVector2D(1.0, 0.0));
			float Fore = 1.0f, Aft = 1.0f;
			for (const FRaftJoint& Joint : Yard.GetJoints())
			{
				const TArray<FHullPiece>& P = Yard.GetHull().GetPieces();
				const double X = (P[Joint.PieceA].CenterCm.X + P[Joint.PieceB].CenterCm.X) * 0.5;
				if (X > 20.0) { Fore = FMath::Min(Fore, Joint.Health01); }
				if (X < -20.0) { Aft = FMath::Min(Aft, Joint.Health01); }
			}
			TestTrue(FString::Printf(TEXT("proa %.3f < popa %.3f"), Fore, Aft), Fore < Aft);
			// 3 m/s: 2,2 m/s de más × 0,25 / 1,2 (cuerda) ≈ 0,46 como mucho en la unión más cercana.
			TestTrue(TEXT("proa dañada, sin romperse"), Fore < 0.9f && Fore > 0.5f);
		});

		It("la cuerda aguanta mejor los golpes y los clavos, el roce", [this]()
		{
			FRaftYardModel Rope = SixLogRaft(ERaftJointKind::Rope);
			FRaftYardModel Nails = SixLogRaft(ERaftJointKind::Nails);
			Rope.ApplyImpact(250.0f, FVector2D(1.0, 0.0));
			Nails.ApplyImpact(250.0f, FVector2D(1.0, 0.0));
			TestTrue(TEXT("golpe: cuerda mejor"), Rope.Integrity01() > Nails.Integrity01());

			FRaftYardModel RopeScrape = SixLogRaft(ERaftJointKind::Rope);
			FRaftYardModel NailsScrape = SixLogRaft(ERaftJointKind::Nails);
			RopeScrape.ApplyScrapeWork(20000.0f, ELaunchSurface::Sand);
			NailsScrape.ApplyScrapeWork(20000.0f, ELaunchSurface::Sand);
			TestTrue(TEXT("roce: clavos mejor"), NailsScrape.Integrity01() > RopeScrape.Integrity01());
		});

		It("el roce solo gasta las uniones de las piezas que tocan el suelo", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			const int32 Mast = Yard.AddPiece(Piece(EHullPieceType::Mast, FVector(100.0, 0.0, 26.0 + 200.0)));
			const int32 MastJoint = Yard.AddJoint(Mast, 6, ERaftJointKind::Rope);
			TestTrue(TEXT("mástil atado al travesaño de proa"), MastJoint != INDEX_NONE);
			TestFalse(TEXT("el travesaño no toca el suelo"), Yard.IsBottomPiece(6));
			TestTrue(TEXT("el tronco sí"), Yard.IsBottomPiece(0));
			const FRaftDamageReport R = Yard.ApplyScrapeWork(10000.0f, ELaunchSurface::Rock);
			TestEqual(TEXT("17 uniones con el fondo"), R.JointsDamaged, 17);
			TestEqual(TEXT("mástil intacto"), Yard.GetJoints()[MastJoint].Health01, 1.0f);
			// Archard: 10 kN·m × 0,40 / 17 uniones.
			TestEqual(TEXT("desgaste exacto"), Yard.GetJoints()[0].Health01, 1.0f - 0.40f * 10.0f / 17.0f, 1e-5f);
			TestEqual(TEXT("trabajo negativo no hace nada"), Yard.ApplyScrapeWork(-5.0f, ELaunchSurface::Rock).JointsDamaged, 0);
		});
	});

	Describe("en tierra", [this]()
	{
		It("es estable: sin empujar no se mueve ni se gasta, y una persona no la arrastra por la arena", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(FlatPath(ELaunchSurface::Sand, 2000.0f), 200.0f);
			const FRaftPushReport Still = Yard.Push(0.0f, 10.0f);
			TestEqual(TEXT("quieta"), Still.MovedCm, 0.0f);
			const float Required = Yard.RequiredPushForceN();
			// 451 kg × 9,81 × 0,55 ≈ 2,4 kN: nueve personas.
			TestTrue(FString::Printf(TEXT("hace falta %.0f N > 1 persona"), Required), Required > FRaftYardModel::PushForceN(1));
			TestEqual(TEXT("en personas"), FMath::CeilToInt(Required / FRaftYardModel::PushForcePerPersonN), 9);
			const FRaftPushReport One = Yard.Push(FRaftYardModel::PushForceN(1), 5.0f);
			TestEqual(TEXT("una persona no la mueve"), One.MovedCm, 0.0f);
			TestEqual(TEXT("ni la gasta"), Yard.Integrity01(), 1.0f);
			TestTrue(TEXT("sigue en tierra"), Yard.GetState() == ERaftYardState::Ashore);
		});

		It("sobre rodillos la mueve una persona sin gastarla, y los rodillos avanzan la mitad y se quedan atrás", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(FlatPath(ELaunchSurface::Sand, 2000.0f), 200.0f);
			Yard.PlaceRoller(110.0f);
			Yard.PlaceRoller(290.0f);
			Yard.PlaceRoller(600.0f);
			TestTrue(TEXT("sobre rodillos"), Yard.IsOnRollers());
			TestTrue(TEXT("basta una persona"), Yard.RequiredPushForceN() <= FRaftYardModel::PushForceN(1));
			TestFalse(TEXT("el rodillo de debajo no se puede quitar"), Yard.TakeRoller(0));

			const float Before = Yard.GetCenterS();
			const FRaftPushReport R = Yard.Push(FRaftYardModel::PushForceN(1), 1.0f);
			const float Moved = Yard.GetCenterS() - Before;
			TestTrue(TEXT("avanza"), Moved > 0.0f && R.bOnRollers);
			TestEqual(TEXT("rodillo de popa: la mitad"), Yard.GetRollers()[0], 110.0f + Moved * 0.5f, 1e-3f);
			TestEqual(TEXT("rodillo de proa: la mitad"), Yard.GetRollers()[1], 290.0f + Moved * 0.5f, 1e-3f);
			TestEqual(TEXT("el de delante aún no está debajo"), Yard.GetRollers()[2], 600.0f);
			TestEqual(TEXT("sin desgaste"), Yard.Integrity01(), 1.0f);

			// Sigue empujando: el de popa sale por detrás y la balsa se detiene en la arena.
			PushUntil(Yard, FRaftYardModel::PushForceN(1), 2000.0f, Dt, 30.0f);
			TestFalse(TEXT("ya no rueda"), Yard.IsOnRollers());
			TestEqual(TEXT("parada"), Yard.GetVelocityCmS(), 0.0f);
			TestTrue(TEXT("el de popa se ha quedado atrás"), Yard.GetRollers()[0] < Yard.GetCenterS() - Yard.HullLengthCm() * 0.5f);
			TestTrue(TEXT("apenas se ha gastado (solo el frenazo)"), Yard.Integrity01() > 0.99f);

			// Se recoge y se pone delante: vuelve a rodar.
			TestTrue(TEXT("se recoge"), Yard.TakeRoller(0));
			Yard.PlaceRoller(Yard.GetCenterS() - 100.0f);
			TestTrue(TEXT("rueda otra vez"), Yard.IsOnRollers());
			TestTrue(TEXT("y avanza"), Yard.Push(FRaftYardModel::PushForceN(1), 1.0f).MovedCm > 0.0f);
		});

		It("arrastrarla 10 m por la arena gasta ~31 % las uniones de cuerda (Archard exacto)", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(FlatPath(ELaunchSurface::Sand, 3000.0f), 200.0f);
			const float Normal = Yard.TotalMassKg() * FRaftYardModel::Gravity;
			const FRaftPushReport R = PushUntil(Yard, FRaftYardModel::PushForceN(10), 1200.0f);
			const float Moved = R.MovedCm;
			const float Expected = 1.0f - 0.12f * Normal / 1000.0f * Moved / 100.0f / 17.0f;
			TestTrue(TEXT("ha recorrido 10 m"), Moved >= 1000.0f);
			TestEqual(FString::Printf(TEXT("salud %.3f"), Yard.GetJoints()[0].Health01), Yard.GetJoints()[0].Health01, Expected, 1e-3f);
			TestTrue(TEXT("~31 % por 10 m"), (1.0f - Expected) / (Moved / 1000.0f) > 0.28f && (1.0f - Expected) / (Moved / 1000.0f) < 0.34f);
			TestEqual(TEXT("sin soltar nada"), R.Damage.Released.Num(), 0);
		});

		It("por la roca rompe las uniones y suelta piezas, pero no pierde ninguna", [this]()
		{
			FRaftYardModel Yard = SixLogRaft(ERaftJointKind::Fiber);
			const int32 PiecesBefore = Yard.GetHull().GetPieces().Num();
			const double MassBefore = PiecesMass(Yard.GetHull().GetPieces());
			Yard.PlaceOnPath(FlatPath(ELaunchSurface::Rock, 3000.0f), 200.0f);
			const FRaftPushReport R = PushUntil(Yard, FRaftYardModel::PushForceN(10), 1200.0f);
			TestTrue(TEXT("rompe uniones"), R.Damage.JointsBroken > 0);
			TestTrue(TEXT("suelta piezas"), R.Damage.Released.Num() > 0);
			TestEqual(TEXT("piezas conservadas"), Yard.GetHull().GetPieces().Num() + R.Damage.Released.Num(), PiecesBefore);
			TestEqual(TEXT("masa conservada"), PiecesMass(Yard.GetHull().GetPieces()) + PiecesMass(R.Damage.Released), MassBefore, 1e-6);
		});

		It("el desgaste cruza el borde entre tramos sin saltos y no depende del ritmo de fotogramas", [this]()
		{
			auto Run = [](float FrameDt, float& OutHealth)
			{
				FRaftYardModel Yard = SixLogRaft();
				FLaunchPath Path;
				Path.Segments.Add({ ELaunchSurface::Sand, 600.0f, 0.0f });
				Path.Segments.Add({ ELaunchSurface::Rock, 300.0f, 0.0f });
				Path.Segments.Add({ ELaunchSurface::Sand, 2000.0f, 0.0f });
				Yard.PlaceOnPath(Path, 150.0f);
				for (int32 I = 0; I < FMath::RoundToInt(4.0f / FrameDt); ++I)
				{
					Yard.Push(FRaftYardModel::PushForceN(12), FrameDt);
				}
				OutHealth = Yard.GetJoints()[0].Health01;
				return Yard.GetCenterS();
			};
			float H60 = 0.0f, H30 = 0.0f, H10 = 0.0f;
			const float S60 = Run(1.0f / 60.0f, H60);
			const float S30 = Run(1.0f / 30.0f, H30);
			const float S10 = Run(1.0f / 10.0f, H10);
			TestTrue(FString::Printf(TEXT("ha cruzado la roca (S = %.0f)"), S60), S60 > 900.0f);
			TestEqual(TEXT("misma posición a 30 fps"), S30, S60, 0.5f);
			TestEqual(TEXT("misma posición a 10 fps"), S10, S60, 0.5f);
			TestEqual(TEXT("mismo desgaste a 30 fps"), H30, H60, 1e-4f);
			TestEqual(TEXT("mismo desgaste a 10 fps"), H10, H60, 1e-4f);

			// Contra la cuenta a mano: arena hasta 600, roca de 600 a 900, arena después.
			FRaftYardModel Ref = SixLogRaft();
			const float Kn = Ref.TotalMassKg() * FRaftYardModel::Gravity / 1000.0f;
			const float Expected = 1.0f - Kn / 17.0f * (0.12f * (450.0f + (S60 - 900.0f)) / 100.0f + 0.40f * 3.0f);
			TestEqual(TEXT("desgaste por tramos"), H60, Expected, 2e-3f);
		});

		It("una rampa de tablones empinada la deja caer sola y una suave ayuda", [this]()
		{
			FRaftYardModel Flat = SixLogRaft();
			Flat.PlaceOnPath(FlatPath(ELaunchSurface::PlankRamp, 1000.0f), 200.0f);
			FRaftYardModel Gentle = SixLogRaft();
			FLaunchPath GentlePath = FlatPath(ELaunchSurface::PlankRamp, 1000.0f);
			GentlePath.Segments[0].DropCm = 1000.0f * FMath::Tan(FMath::DegreesToRadians(8.0f));
			Gentle.PlaceOnPath(GentlePath, 200.0f);
			FRaftYardModel Steep = SixLogRaft();
			FLaunchPath SteepPath = FlatPath(ELaunchSurface::PlankRamp, 1000.0f);
			SteepPath.Segments[0].DropCm = 1000.0f * FMath::Tan(FMath::DegreesToRadians(18.0f));
			Steep.PlaceOnPath(SteepPath, 200.0f);

			TestTrue(TEXT("rampa suave: menos que en llano"), Gentle.RequiredPushForceN() < Flat.RequiredPushForceN());
			TestTrue(TEXT("pero hacen falta brazos"), Gentle.RequiredPushForceN() > 0.0f);
			TestEqual(TEXT("rampa de 18°: sola"), Steep.RequiredPushForceN(), 0.0f);
			TestTrue(TEXT("baja sin empujar"), Steep.Push(0.0f, 1.0f).MovedCm > 0.0f);
			TestTrue(TEXT("la rampa apenas gasta"), Steep.Integrity01() > 0.999f);
		});

		It("la botadura: flota donde el agua cubre su calado y sale como FBoatModel con su ficha", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			FLaunchPath Path;
			Path.StartCm = FVector(1000.0, 2000.0, 0.0);
			Path.YawDeg = 90.0f;
			Path.Segments.Add({ ELaunchSurface::Sand, 400.0f, 0.0f });
			Path.Segments.Add({ ELaunchSurface::WetSand, 600.0f, 60.0f });
			Path.WaterLevelZCm = -30.0f;
			Yard.PlaceOnPath(Path, 150.0f);
			const float Draft = Yard.GetHydrostatics().DraftCm;
			TestTrue(TEXT("calado ~11 cm"), Draft > 10.0f && Draft < 12.5f);

			const FRaftPushReport R = PushUntil(Yard, FRaftYardModel::PushForceN(10), 1000.0f);
			TestTrue(TEXT("botada"), R.bLaunched);
			TestTrue(TEXT("a flote"), Yard.GetState() == ERaftYardState::Afloat);
			TestTrue(TEXT("el agua cubre el calado"), Path.WaterDepthAt(Yard.GetCenterS()) >= Draft - 0.01f);
			TestTrue(TEXT("el agua sostenía casi todo antes"), Path.WaterDepthAt(Yard.GetCenterS() - 20.0f) < Draft);
			TestEqual(TEXT("ya no se empuja en tierra"), Yard.Push(3000.0f, 1.0f).MovedCm, 0.0f);

			const FBoatModel Boat = Yard.MakeBoat();
			const FBoatState& S = Boat.GetState();
			TestEqual(TEXT("quilla en el calado"), static_cast<float>(S.LocationCm.Z), -30.0f - Draft, 1e-3f);
			TestEqual(TEXT("en el camino (X)"), S.LocationCm.X, 1000.0, 1e-3);
			TestEqual(TEXT("en el camino (Y)"), static_cast<float>(S.LocationCm.Y), 2000.0f + Yard.GetCenterS(), 1e-2f);
			TestEqual(TEXT("proa al mar"), S.YawDeg, 90.0f, 1e-3f);
			TestTrue(TEXT("con la arrancada"), S.VelocityCmS.Y > 0.0 && FMath::Abs(S.VelocityCmS.X) < 1e-3);
			TestEqual(TEXT("eslora de la ficha"), Boat.GetDefinition().LengthCm, 300.0f, 0.5f);
			TestEqual(TEXT("masa del casco"), Boat.GetDefinition().HullMassKg, Yard.TotalMassKg(), 0.01f);
			TestEqual(TEXT("el daño de las uniones pasa al casco"), S.HullDamage01, Yard.HullDamage01(), 1e-6f);
		});

		It("una pieza sin atar se va flotando al botarla", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.AddPiece(Piece(EHullPieceType::Float, FVector(0.0, 66.0 + 20.0, 20.0)));
			FLaunchPath Path = FlatPath(ELaunchSurface::WetSand, 2000.0f);
			Path.Segments[0].DropCm = 200.0f;
			Path.WaterLevelZCm = -40.0f;
			Yard.PlaceOnPath(Path, 150.0f);
			const FRaftPushReport R = PushUntil(Yard, FRaftYardModel::PushForceN(10), 2000.0f);
			TestTrue(TEXT("botada"), R.bLaunched);
			if (TestEqual(TEXT("una suelta"), R.Damage.Released.Num(), 1))
			{
				TestTrue(TEXT("el flotador"), R.Damage.Released[0].Type == EHullPieceType::Float);
			}
			TestEqual(TEXT("el casco se queda con lo atado"), Yard.GetHull().GetPieces().Num(), 8);
		});
	});

	Describe("en el agua y con FBoatModel", [this]()
	{
		It("construida en el agua ya flota y no se empuja como en tierra", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.SetAfloat();
			TestEqual(TEXT("a flote"), Yard.WaterSupport01(), 1.0f);
			TestEqual(TEXT("sin empuje de tierra"), Yard.Push(3000.0f, 1.0f).MovedCm, 0.0f);
			TestFalse(TEXT("sin rodillos en el agua"), Yard.PlaceRoller(10.0f));
		});

		It("un golpe contra el arrecife de FBoatModel se reparte en las uniones de proa", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.SetAfloat();
			FBoatDefinition Def = Yard.ToBoatDefinition();
			Def.bShallowWaterOnly = false;
			FBoatModel Boat(Def, FVector::ZeroVector, 0.0f);
			Boat.SetCrewAboard(true);
			// Va a 3 m/s hacia un arrecife a 20 m.
			Boat.SetVelocityCmS(FVector2D(300.0, 0.0));
			FBoatEnvironment Env;
			Env.CurrentCmS = FVector2D(300.0, 0.0);
			Env.DepthBelowSeaLevelCm = [](const FVector2D& P) { return P.X < 2000.0 ? 300.0f : 2.0f; };
			int32 Seen = 0;
			FRaftDamageReport Total;
			for (int32 I = 0; I < 60 * 20; ++I)
			{
				Boat.Step(Dt, FBoatControls(), Env);
				const FBoatState& S = Boat.GetState();
				if (S.ImpactCount != Seen)
				{
					Seen = S.ImpactCount;
					Total.Append(Yard.ApplyImpact(S.LastImpactSpeedCmS, S.LastImpactDirection));
				}
			}
			TestTrue(TEXT("ha golpeado"), Seen > 0);
			TestTrue(TEXT("de proa"), Boat.GetState().LastImpactDirection.X > 0.9);
			TestTrue(TEXT("uniones dañadas"), Total.JointsDamaged > 0 && Yard.Integrity01() < 1.0f);
			// Sincronía: el casco de FBoatModel refleja las uniones.
			Boat.Repair(1.0f);
			Boat.ApplyDamage(Yard.HullDamage01());
			TestEqual(TEXT("daño sincronizado"), Boat.GetState().HullDamage01, Yard.HullDamage01(), 1e-6f);
		});

		It("al entrar a 2 m/s en un bajío y arrastrarse sobre él, el roce de FBoatModel gasta las uniones del fondo", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.SetAfloat();
			FBoatModel Boat(Yard.ToBoatDefinition(), FVector(0.0, 0.0, -11.0), 0.0f);
			Boat.SetVelocityCmS(FVector2D(200.0, 0.0));
			FBoatEnvironment Env;
			// Bajío llano de 7 cm: menos que el calado (~11 cm), sin escalón que lo pare en seco.
			Env.DepthBelowSeaLevelCm = [](const FVector2D&) { return 7.0f; };
			for (int32 I = 0; I < 60 * 10; ++I)
			{
				Boat.Step(Dt, FBoatControls(), Env);
			}
			const float Work = static_cast<float>(Boat.GetState().GroundScrapeWorkNm);
			TestTrue(FString::Printf(TEXT("roce acumulado %.0f N·m"), Work), Work > 0.0f);
			const FRaftDamageReport R = Yard.ApplyScrapeWork(Work, ELaunchSurface::WetSand);
			TestEqual(TEXT("todas las del fondo"), R.JointsDamaged, 17);
			TestTrue(TEXT("gastadas"), Yard.Integrity01() < 1.0f);
		});

		It("al soltar una pieza en el agua, FBoatModel toma la ficha nueva sin perder el estado", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			const int32 Float = Yard.AddPiece(Piece(EHullPieceType::Float, FVector(0.0, 66.0 + 20.0, 20.0)));
			const int32 Joint = Yard.AddJoint(Float, 5, ERaftJointKind::Fiber);
			Yard.SetAfloat();
			FBoatModel Boat(Yard.ToBoatDefinition(), FVector(50.0, 60.0, -10.0), 30.0f);
			const float MassBefore = Boat.GetDefinition().HullMassKg;
			Yard.DamageJoint(Joint, 1.0f);
			Boat.SetDefinition(Yard.ToBoatDefinition());
			TestTrue(TEXT("más ligera"), Boat.GetDefinition().HullMassKg < MassBefore);
			TestEqual(TEXT("misma posición"), Boat.GetState().LocationCm.X, 50.0, 1e-6);
			TestEqual(TEXT("mismo rumbo"), Boat.GetState().YawDeg, 30.0f, 1e-4f);
		});
	});

	Describe("los estados degenerados", [this]()
	{
		It("un astillero vacío, un camino sin tramos y entradas no finitas no rompen nada", [this]()
		{
			FRaftYardModel Empty;
			TestEqual(TEXT("integridad sin uniones"), Empty.Integrity01(), 1.0f);
			TestEqual(TEXT("sin masa"), Empty.TotalMassKg(), 0.0f);
			TestEqual(TEXT("sin eslora"), Empty.HullLengthCm(), 0.0f);
			TestEqual(TEXT("empujar la nada"), Empty.Push(1000.0f, 1.0f).MovedCm, 0.0f);
			TestEqual(TEXT("fuerza para la nada"), Empty.RequiredPushForceN(), 0.0f);
			TestEqual(TEXT("golpe a la nada"), Empty.ApplyImpact(900.0f, FVector2D(1.0, 0.0)).JointsDamaged, 0);
			TestEqual(TEXT("soltar de la nada"), Empty.ReleaseLoosePieces().Num(), 0);

			FRaftYardModel NoPath = SixLogRaft();
			NoPath.PlaceOnPath(FLaunchPath(), 500.0f);
			TestEqual(TEXT("sin tramos, en S = 0"), NoPath.GetCenterS(), 0.0f);
			const FRaftPushReport R = NoPath.Push(3000.0f, 1.0f);
			TestEqual(TEXT("no se mueve"), R.MovedCm, 0.0f);
			TestFalse(TEXT("no flota en seco"), R.bLaunched);

			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(FlatPath(ELaunchSurface::Sand, 1000.0f), NaN);
			TestEqual(TEXT("S no finita → 0"), Yard.GetCenterS(), 0.0f);
			TestEqual(TEXT("fuerza NaN"), Yard.Push(NaN, 1.0f).MovedCm, 0.0f);
			TestEqual(TEXT("tiempo negativo"), Yard.Push(3000.0f, -1.0f).MovedCm, 0.0f);
			TestEqual(TEXT("tiempo NaN"), Yard.Push(3000.0f, NaN).MovedCm, 0.0f);
			TestEqual(TEXT("tiempo infinito"), Yard.Push(3000.0f, INFINITY).MovedCm, 0.0f);
			TestTrue(TEXT("el tiempo roto no envenena el siguiente empujón"), Yard.Push(FRaftYardModel::PushForceN(10), 0.5f).MovedCm > 0.0f);
			TestEqual(TEXT("roce NaN"), Yard.ApplyScrapeWork(NaN, ELaunchSurface::Rock).JointsDamaged, 0);
			TestEqual(TEXT("golpe infinito"), Yard.ApplyImpact(INFINITY, FVector2D(1.0, 0.0)).JointsDamaged, 0);
			TestFalse(TEXT("rodillo fuera del camino"), Yard.PlaceRoller(1000.5f));
			TestFalse(TEXT("rodillo NaN"), Yard.PlaceRoller(NaN));
			TestFalse(TEXT("quitar rodillo inexistente"), Yard.TakeRoller(0));
			TestEqual(TEXT("daño NaN"), Yard.DamageJoint(0, NaN).JointsDamaged, 0);
			TestEqual(TEXT("daño a unión inexistente"), Yard.DamageJoint(99, 1.0f).JointsDamaged, 0);
		});

		It("un camino con valores no finitos toma los valores por defecto", [this]()
		{
			FLaunchPath Path = FlatPath(ELaunchSurface::Sand, 1000.0f);
			FLaunchSegment Broken;
			Broken.LengthCm = NaN;
			Broken.DropCm = INFINITY;
			Path.Segments.Add(Broken);
			Path.StartCm = FVector(static_cast<double>(NaN), 0.0, 0.0);
			Path.YawDeg = INFINITY;
			Path.WaterLevelZCm = NaN;
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(Path, 500.0f);
			const FLaunchPath& Placed = Yard.GetPath();
			TestEqual(TEXT("largo del camino"), Placed.TotalLengthCm(), 1000.0f);
			TestEqual(TEXT("rumbo por defecto"), Placed.YawDeg, 0.0f);
			TestEqual(TEXT("agua por defecto: en seco"), Placed.WaterLevelZCm, FLaunchPath().WaterLevelZCm);
			const FVector End = Placed.WorldAt(Placed.TotalLengthCm());
			TestTrue(TEXT("posición finita"), FMath::IsFinite(End.X) && FMath::IsFinite(End.Y) && FMath::IsFinite(End.Z));
			TestTrue(TEXT("empuja sin NaN"), FMath::IsFinite(Yard.Push(FRaftYardModel::PushForceN(10), 0.5f).MovedCm));
			TestTrue(TEXT("centro finito"), FMath::IsFinite(Yard.GetCenterS()));
		});

		It("empuja hasta el final de un camino sin agua y se detiene en el borde, también marcha atrás", [this]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(FlatPath(ELaunchSurface::PlankRamp, 800.0f), 700.0f);
			const FRaftPushReport R = PushUntil(Yard, FRaftYardModel::PushForceN(10), 5000.0f);
			TestTrue(TEXT("al final"), R.bAtPathEnd);
			TestEqual(TEXT("en el borde exacto"), Yard.GetCenterS(), 800.0f);
			TestEqual(TEXT("parada"), Yard.GetVelocityCmS(), 0.0f);
			for (int32 I = 0; I < 600; ++I)
			{
				Yard.Push(-FRaftYardModel::PushForceN(10), Dt);
			}
			TestEqual(TEXT("marcha atrás hasta el principio"), Yard.GetCenterS(), 0.0f);
		});

		It("los tramos de largo cero son un escalón y el borde exacto pertenece al tramo siguiente", [this]()
		{
			FLaunchPath Path;
			Path.Segments.Add({ ELaunchSurface::Sand, 100.0f, 10.0f });
			Path.Segments.Add({ ELaunchSurface::Rock, 0.0f, 50.0f });
			Path.Segments.Add({ ELaunchSurface::WetSand, 100.0f, 0.0f });
			TestEqual(TEXT("largo"), Path.TotalLengthCm(), 200.0f);
			TestEqual(TEXT("tramo en 99,9"), Path.SegmentIndexAt(99.9f), 0);
			TestEqual(TEXT("tramo en 100: el siguiente con largo"), Path.SegmentIndexAt(100.0f), 2);
			TestEqual(TEXT("más allá del final: el último"), Path.SegmentIndexAt(500.0f), 2);
			TestEqual(TEXT("suelo antes del escalón"), Path.GroundZAt(100.0f), -10.0f, 1e-4f);
			TestEqual(TEXT("suelo después del escalón"), Path.GroundZAt(100.01f), -60.0f, 1e-4f);
			TestEqual(TEXT("la pendiente del tramo de arena"), Path.SlopeRadAt(50.0f), FMath::Atan2(10.0f, 100.0f), 1e-6f);
			TestEqual(TEXT("sin tramos, sin pendiente"), FLaunchPath().SlopeRadAt(0.0f), 0.0f);
		});
	});
}

#endif
