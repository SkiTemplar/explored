#include "Misc/AutomationTest.h"

#include "Boats/HullAssemblyModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace HullAssemblySpecDetail
{
	/** Balsa de N troncos por defecto (3 m × 22 cm) uno junto a otro a lo largo de X, centrada en Y. Quilla en z = 0. */
	FHullAssemblyModel Raft(int32 Logs = 6, double LengthCm = 0.0)
	{
		FHullAssemblyModel Model;
		const double Width = FHullAssemblyModel::Spec(EHullPieceType::Log).DefaultSizeCm.Y;
		for (int32 I = 0; I < Logs; ++I)
		{
			FHullPiece Log;
			Log.Type = EHullPieceType::Log;
			Log.CenterCm = FVector(0.0, (I - (Logs - 1) * 0.5) * Width, 11.0);
			Log.SizeCm = FVector(LengthCm, 0.0, 0.0);
			Model.AddPiece(Log);
		}
		return Model;
	}

	/** Pasajero de pie: su centro de masas a 90 cm sobre la cubierta de troncos (z = 22). */
	FHullLoad Passenger(double Y = 0.0, double X = 0.0)
	{
		FHullLoad Load;
		Load.MassKg = FHullAssemblyModel::PassengerMassKg;
		Load.CenterCm = FVector(X, Y, 22.0 + 90.0);
		Load.bPassenger = true;
		return Load;
	}

	FHullLoad Cargo(float MassKg, double Y = 0.0, double X = 0.0)
	{
		FHullLoad Load;
		Load.MassKg = MassKg;
		Load.CenterCm = FVector(X, Y, 22.0 + 20.0);
		return Load;
	}

	void AddPiece(FHullAssemblyModel& Model, EHullPieceType Type, const FVector& Center, const FVector& Size = FVector::ZeroVector)
	{
		FHullPiece Piece;
		Piece.Type = Type;
		Piece.CenterCm = Center;
		Piece.SizeCm = Size;
		Model.AddPiece(Piece);
	}

	FString Verdict(const FHullHydrostatics& H) { return LexToString(H.Verdict); }
}

BEGIN_DEFINE_SPEC(FHullAssemblyModelSpec, "Explored.HullAssembly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FHullAssemblyModelSpec)

void FHullAssemblyModelSpec::Define()
{
	using namespace HullAssemblySpecDetail;

	Describe("la flotación", [this]()
	{
		It("una balsa simétrica flota nivelada con francobordo", [this]()
		{
			FHullAssemblyModel Model = Raft();
			Model.AddLoad(Passenger());
			const FHullHydrostatics H = Model.Evaluate();
			TestTrue(FString::Printf(TEXT("flota nivelada (%s)"), *Verdict(H)), H.Verdict == EHullVerdict::Floats);
			TestEqual(TEXT("sin escora"), H.HeelDeg, 0.0f, 0.05f);
			TestEqual(TEXT("sin asiento"), H.TrimDeg, 0.0f, 0.05f);
			TestTrue(TEXT("francobordo positivo"), H.FreeboardCm > 0.0f);
			TestTrue(TEXT("GM positiva"), H.GMCm > 0.0f);
		});

		It("desplaza exactamente su peso (Arquímedes) y el calado de una caja es masa / (ρ · flotación)", [this]()
		{
			FHullAssemblyModel Model = Raft();
			Model.AddLoad(Passenger());
			const FHullHydrostatics H = Model.Evaluate();
			TestEqual(TEXT("ρ · volumen sumergido = masa"), H.DisplacementM3 * FHullAssemblyModel::WaterDensity, H.TotalMassKg, H.TotalMassKg * 1e-4f);
			TestEqual(TEXT("flotación 3 m × 1,32 m"), H.WaterplaneAreaM2, 3.0f * 1.32f, 1e-3f);
			const float Expected = H.TotalMassKg / (FHullAssemblyModel::WaterDensity * H.WaterplaneAreaM2) * 100.0f;
			TestEqual(TEXT("calado"), H.DraftCm, Expected, 0.01f);
		});

		It("la altura metacéntrica de una barcaza coincide con KB + BM − KG, en los dos ejes", [this]()
		{
			// Caja de 4 × 2 × 0,5 m de tablón (ρ 550): T = 0,5 · 550 / 1025, KB = T/2, BM = B²/12T, BML = L²/12T, KG = 25 cm.
			FHullAssemblyModel Model;
			AddPiece(Model, EHullPieceType::Plank, FVector(0.0, 0.0, 25.0), FVector(400.0, 200.0, 50.0));
			const FHullHydrostatics H = Model.Evaluate();
			const double T = 50.0 * 550.0 / 1025.0;
			const double GM = T / 2.0 + 200.0 * 200.0 / (12.0 * T) - 25.0;
			const double GML = T / 2.0 + 400.0 * 400.0 / (12.0 * T) - 25.0;
			TestEqual(TEXT("calado"), H.DraftCm, static_cast<float>(T), 0.01f);
			TestEqual(TEXT("KB"), H.KBCm, static_cast<float>(T / 2.0), 0.01f);
			TestEqual(TEXT("GM transversal (±1 %)"), H.GMCm, static_cast<float>(GM), static_cast<float>(GM * 0.01));
			TestEqual(TEXT("GM longitudinal (±1 %)"), H.GMLongCm, static_cast<float>(GML), static_cast<float>(GML * 0.01));
			TestEqual(TEXT("KM = KG + GM"), H.KMCm, H.KGCm + H.GMCm, 1e-3f);
		});
	});

	Describe("la carga", [this]()
	{
		It("una carga descentrada escora hacia su lado, más cuanto más lejos, y en espejo al otro lado", [this]()
		{
			auto HeelWith = [](double Y)
			{
				FHullAssemblyModel Model = Raft();
				Model.AddLoad(Passenger());
				Model.AddLoad(Cargo(60.0f, Y));
				return Model.Evaluate();
			};
			const FHullHydrostatics Near = HeelWith(40.0);
			const FHullHydrostatics Far = HeelWith(65.0);
			const FHullHydrostatics Mirror = HeelWith(-40.0);
			TestTrue(FString::Printf(TEXT("escora (%s)"), *Verdict(Near)), Near.Verdict == EHullVerdict::Lists);
			TestTrue(TEXT("estribor abajo"), Near.HeelDeg > FHullAssemblyModel::LevelToleranceDeg);
			TestTrue(TEXT("más lejos, más escora"), Far.HeelDeg > Near.HeelDeg);
			TestEqual(TEXT("en espejo"), Mirror.HeelDeg, -Near.HeelDeg, 0.01f);
		});

		It("la escora pequeña cumple tan φ ≈ w · e / (Δ · GM)", [this]()
		{
			FHullAssemblyModel Centered = Raft();
			Centered.AddLoad(Passenger());
			Centered.AddLoad(Cargo(20.0f));
			const FHullHydrostatics C = Centered.Evaluate();

			FHullAssemblyModel Moved = Raft();
			Moved.AddLoad(Passenger());
			Moved.AddLoad(Cargo(20.0f, 30.0));
			const FHullHydrostatics M = Moved.Evaluate();

			const float Predicted = FMath::RadiansToDegrees(FMath::Atan(20.0f * 30.0f / (C.TotalMassKg * C.GMCm)));
			TestEqual(TEXT("escora predicha (±10 %)"), M.HeelDeg, Predicted, Predicted * 0.1f);
		});

		It("una carga hacia proa asienta de proa", [this]()
		{
			FHullAssemblyModel Model = Raft();
			Model.AddLoad(Passenger());
			Model.AddLoad(Cargo(60.0f, 0.0, 120.0));
			const FHullHydrostatics H = Model.Evaluate();
			TestTrue(TEXT("proa abajo"), H.TrimDeg > 0.0f);
			TestEqual(TEXT("sin escora"), H.HeelDeg, 0.0f, 0.05f);
		});

		It("más carga, más calado y menos francobordo, hasta embarcar agua y hundirse", [this]()
		{
			float LastDraft = -1.0f;
			float LastFreeboard = 1e9f;
			bool bSawSwamp = false;
			bool bSawSink = false;
			for (float Kg = 0.0f; Kg <= 800.0f; Kg += 50.0f)
			{
				FHullAssemblyModel Model = Raft();
				Model.AddLoad(Passenger());
				Model.AddLoad(Cargo(Kg));
				const FHullHydrostatics H = Model.Evaluate();
				if (H.Verdict == EHullVerdict::Sinks)
				{
					bSawSink = true;
					TestTrue(TEXT("solo se hunde con más masa que flotación"), H.LoadRatio > 1.0f);
					continue;
				}
				TestFalse(TEXT("no vuelve a flotar tras hundirse"), bSawSink);
				TestTrue(TEXT("el calado crece"), H.DraftCm > LastDraft);
				TestTrue(TEXT("el francobordo baja"), H.FreeboardCm < LastFreeboard);
				LastDraft = H.DraftCm;
				LastFreeboard = H.FreeboardCm;
				bSawSwamp |= H.Verdict == EHullVerdict::Swamps;
			}
			TestTrue(TEXT("pasa por anegada"), bSawSwamp);
			TestTrue(TEXT("acaba hundida"), bSawSink);
		});

		It("pasarse de masa la hunde justo al superar su flotación máxima", [this]()
		{
			FHullAssemblyModel Probe = Raft();
			const float Spare = Probe.Evaluate().MaxBuoyancyKg - Probe.Evaluate().StructureMassKg;

			FHullAssemblyModel Under = Raft();
			Under.AddLoad(Cargo(Spare - 1.0f));
			FHullAssemblyModel Over = Raft();
			Over.AddLoad(Cargo(Spare + 1.0f));
			TestTrue(TEXT("1 kg por debajo no se hunde"), Under.Evaluate().Verdict != EHullVerdict::Sinks);
			TestTrue(TEXT("1 kg por encima se hunde"), Over.Evaluate().Verdict == EHullVerdict::Sinks);
		});
	});

	Describe("la estabilidad", [this]()
	{
		It("un centro de masas por encima del metacentro la vuelca; un balancín la salva", [this]()
		{
			// Dos troncos (44 cm de manga) con un pasajero de pie: KG ≈ 45 cm y KM ≈ 18 cm.
			FHullAssemblyModel Narrow = Raft(2);
			Narrow.AddLoad(Passenger());
			const FHullHydrostatics H = Narrow.Evaluate();
			TestTrue(TEXT("G por encima de M"), H.KGCm > H.KMCm);
			TestTrue(TEXT("GM negativa"), H.GMCm < 0.0f);
			TestTrue(FString::Printf(TEXT("vuelca (%s)"), *Verdict(H)), H.Verdict == EHullVerdict::Capsizes);

			// Balancín: un travesaño de bambú y dos flotadores a 1,5 m a cada lado.
			FHullAssemblyModel Outrigger = Raft(2);
			Outrigger.AddLoad(Passenger());
			AddPiece(Outrigger, EHullPieceType::Bamboo, FVector(0.0, 0.0, 27.0), FVector(10.0, 330.0, 10.0));
			AddPiece(Outrigger, EHullPieceType::Float, FVector(0.0, 150.0, 10.0), FVector(60.0, 30.0, 20.0));
			AddPiece(Outrigger, EHullPieceType::Float, FVector(0.0, -150.0, 10.0), FVector(60.0, 30.0, 20.0));
			const FHullHydrostatics O = Outrigger.Evaluate();
			TestTrue(TEXT("GM positiva con balancín"), O.GMCm > 0.0f);
			TestTrue(FString::Printf(TEXT("a flote con balancín (%s)"), *Verdict(O)), O.IsAfloat());
			TestEqual(TEXT("y nivelada"), O.HeelDeg, 0.0f, 0.05f);
			TestEqual(TEXT("la manga efectiva no cuenta el hueco del balancín"), O.EffectiveBeamCm, 44.0f + 2.0f * 30.0f, 1e-3f);
			TestEqual(TEXT("aunque la manga total sí"), O.WaterlineBeamCm, 330.0f, 1e-3f);
		});

		It("una carga muy descentrada en una balsa estrecha la vuelca aunque de pie aguante", [this]()
		{
			FHullAssemblyModel Model = Raft(3);
			Model.AddLoad(Cargo(40.0f));
			const FHullHydrostatics Stable = Model.Evaluate();
			TestTrue(FString::Printf(TEXT("centrada, a flote (%s)"), *Verdict(Stable)), Stable.IsAfloat());

			FHullAssemblyModel Edge = Raft(3);
			FHullLoad High = Cargo(40.0f, 33.0);
			High.CenterCm.Z = 120.0;
			Edge.AddLoad(High);
			const FHullHydrostatics Tipped = Edge.Evaluate();
			TestTrue(FString::Printf(TEXT("alta y en la borda, vuelca (%s)"), *Verdict(Tipped)), Tipped.Verdict == EHullVerdict::Capsizes);
		});

		It("el brazo adrizante es cero adrizada, endereza a ambos lados y se anula al volcar", [this]()
		{
			FHullAssemblyModel Model = Raft();
			Model.AddLoad(Passenger());
			TestEqual(TEXT("GZ(0) = 0"), Model.RightingArmCm(0.0f), 0.0f, 1e-3f);
			TestTrue(TEXT("GZ(10°) > 0"), Model.RightingArmCm(10.0f) > 0.0f);
			TestTrue(TEXT("GZ(−10°) < 0"), Model.RightingArmCm(-10.0f) < 0.0f);
			const FHullHydrostatics H = Model.Evaluate();
			TestTrue(TEXT("ángulo de estabilidad nula entre 10° y 90°"), H.VanishingStabilityDeg > 10.0f && H.VanishingStabilityDeg < 90.0f);
			TestTrue(TEXT("pasado ese ángulo ya no endereza"), Model.RightingArmCm(H.VanishingStabilityDeg + 1.0f) <= 0.0f);
		});
	});

	Describe("el cálculo", [this]()
	{
		It("es determinista y no depende del orden de las piezas", [this]()
		{
			FHullAssemblyModel A = Raft();
			AddPiece(A, EHullPieceType::Mast, FVector(0.0, 0.0, 222.0));
			AddPiece(A, EHullPieceType::Sail, FVector(-20.0, 0.0, 250.0));
			A.AddLoad(Passenger(20.0, -50.0));
			A.AddLoad(Cargo(30.0f, -40.0, 80.0));

			FHullAssemblyModel B;
			for (int32 I = A.GetPieces().Num() - 1; I >= 0; --I)
			{
				B.AddPiece(A.GetPieces()[I]);
			}
			for (int32 I = A.GetLoads().Num() - 1; I >= 0; --I)
			{
				B.AddLoad(A.GetLoads()[I]);
			}

			const FHullHydrostatics A1 = A.Evaluate();
			const FHullHydrostatics A2 = A.Evaluate();
			const FHullHydrostatics B1 = B.Evaluate();
			TestTrue(TEXT("mismo resultado dos veces, bit a bit"),
				A1.HeelDeg == A2.HeelDeg && A1.TrimDeg == A2.TrimDeg && A1.DraftCm == A2.DraftCm && A1.GMCm == A2.GMCm && A1.FreeboardCm == A2.FreeboardCm);
			TestTrue(TEXT("mismo veredicto en otro orden"), A1.Verdict == B1.Verdict);
			TestEqual(TEXT("escora en otro orden"), B1.HeelDeg, A1.HeelDeg, 1e-3f);
			TestEqual(TEXT("asiento en otro orden"), B1.TrimDeg, A1.TrimDeg, 1e-3f);
			TestEqual(TEXT("calado en otro orden"), B1.DraftCm, A1.DraftCm, 1e-3f);
			TestEqual(TEXT("GM en otro orden"), B1.GMCm, A1.GMCm, 1e-2f);
		});

		It("el mástil no cuenta como cubierta para el francobordo", [this]()
		{
			FHullAssemblyModel Model = Raft();
			Model.AddLoad(Passenger());
			const FHullHydrostatics Bare = Model.Evaluate();
			AddPiece(Model, EHullPieceType::Mast, FVector(0.0, 0.0, 222.0));
			const FHullHydrostatics Masted = Model.Evaluate();
			TestEqual(TEXT("cubierta a 22 cm"), Masted.DeckHeightCm, 22.0f, 1e-3f);
			TestTrue(TEXT("el francobordo apenas cambia (el mástil pesa y flota poco)"), FMath::Abs(Masted.FreeboardCm - Bare.FreeboardCm) < 1.0f);

			FHullAssemblyModel MastOnly;
			AddPiece(MastOnly, EHullPieceType::Mast, FVector(0.0, 0.0, 200.0));
			TestEqual(TEXT("solo mástil: su punta es la cubierta"), MastOnly.Evaluate().DeckHeightCm, 400.0f, 1e-3f);
		});

		It("resuelve los casos degenerados sin romperse", [this]()
		{
			FHullAssemblyModel Empty;
			TestTrue(TEXT("sin piezas: vacío"), Empty.Evaluate().Verdict == EHullVerdict::Empty);

			FHullAssemblyModel NoVolume;
			AddPiece(NoVolume, EHullPieceType::Sail, FVector::ZeroVector);
			AddPiece(NoVolume, EHullPieceType::Oars, FVector::ZeroVector);
			TestTrue(TEXT("solo vela y remos: nada flota"), NoVolume.Evaluate().Verdict == EHullVerdict::Empty);

			FHullAssemblyModel Defaults;
			AddPiece(Defaults, EHullPieceType::Log, FVector(0.0, 0.0, 11.0), FVector(0.0, -5.0, 0.0));
			TestEqual(TEXT("tamaño ≤ 0 toma el del tipo"), FHullAssemblyModel::EffectiveSizeCm(Defaults.GetPieces()[0]), FVector(300.0, 22.0, 22.0));
			TestTrue(TEXT("un tronco solo flota"), Defaults.Evaluate().IsAfloat());

			FHullAssemblyModel Negative = Raft();
			const FHullHydrostatics Before = Negative.Evaluate();
			Negative.AddLoad(Cargo(-500.0f));
			TestEqual(TEXT("una masa negativa no cuenta"), Negative.Evaluate().TotalMassKg, Before.TotalMassKg);

			TestFalse(TEXT("quitar una pieza que no existe"), Negative.RemovePiece(99));
			TestTrue(TEXT("quitar una que sí"), Negative.RemovePiece(0));
			TestEqual(TEXT("queda una menos"), Negative.GetPieces().Num(), 5);

			const FHullAssemblyModel Solo;
			TestEqual(TEXT("sin piezas, sin brazo"), Solo.RightingArmCm(20.0f), 0.0f);
		});

		It("rechaza piezas y cargas con valores no finitos", [this]()
		{
			const double NaN = std::numeric_limits<double>::quiet_NaN();
			FHullAssemblyModel Model = Raft();
			const FHullHydrostatics Before = Model.Evaluate();
			FHullPiece Bad;
			Bad.CenterCm = FVector(NaN, 0.0, 11.0);
			TestEqual(TEXT("centro NaN"), Model.AddPiece(Bad), INDEX_NONE);
			Bad.CenterCm = FVector::ZeroVector;
			Bad.SizeCm = FVector(std::numeric_limits<double>::infinity(), 0.0, 0.0);
			TestEqual(TEXT("tamaño infinito"), Model.AddPiece(Bad), INDEX_NONE);
			Model.AddLoad(Cargo(std::numeric_limits<float>::quiet_NaN()));
			Model.AddLoad(Cargo(50.0f, NaN));
			TestEqual(TEXT("sin cargas nuevas"), Model.GetLoads().Num(), 0);
			const FHullHydrostatics After = Model.Evaluate();
			TestEqual(TEXT("misma masa"), After.TotalMassKg, Before.TotalMassKg);
			TestTrue(TEXT("sigue a flote"), After.IsAfloat());
		});
	});

	Describe("la propulsión y la forma", [this]()
	{
		It("sin tripulante no avanza; los remos empujan más que la pala", [this]()
		{
			FHullAssemblyModel Model = Raft();
			AddPiece(Model, EHullPieceType::Paddle, FVector(0.0, 0.0, 25.0));
			const FHullHydrostatics Alone = Model.Evaluate();
			TestEqual(TEXT("sin nadie, quieta"), Model.Performance(Alone, 0.0f).MaxSpeedCmS, 0.0f);

			Model.AddLoad(Passenger());
			const FHullPerformance Paddle = Model.Performance(Model.Evaluate(), 0.0f);
			TestTrue(TEXT("con pala"), Paddle.Propulsion == EHullPropulsion::Paddle && Paddle.MaxSpeedCmS > 0.0f);

			AddPiece(Model, EHullPieceType::Oars, FVector(0.0, 0.0, 25.0));
			const FHullPerformance Oars = Model.Performance(Model.Evaluate(), 0.0f);
			TestTrue(TEXT("con remos"), Oars.Propulsion == EHullPropulsion::Oars);
			TestTrue(TEXT("remos más rápidos que pala"), Oars.MaxSpeedCmS > Paddle.MaxSpeedCmS);
		});

		It("la vela necesita mástil y, con viento, supera a los remos", [this]()
		{
			FHullAssemblyModel Model = Raft();
			Model.AddLoad(Passenger());
			AddPiece(Model, EHullPieceType::Oars, FVector(0.0, 0.0, 25.0));
			AddPiece(Model, EHullPieceType::Sail, FVector(0.0, 0.0, 250.0));
			const FHullPerformance NoMast = Model.Performance(Model.Evaluate(), 10.0f);
			TestEqual(TEXT("sin mástil no hay vela"), NoMast.SailAreaM2, 0.0f);
			TestTrue(TEXT("rema"), NoMast.Propulsion == EHullPropulsion::Oars);

			AddPiece(Model, EHullPieceType::Mast, FVector(0.0, 0.0, 222.0));
			const FHullHydrostatics H = Model.Evaluate();
			const FHullPerformance Calm = Model.Performance(H, 1.0f);
			const FHullPerformance Breeze = Model.Performance(H, 10.0f);
			TestEqual(TEXT("vela de 4 m²"), Breeze.SailAreaM2, 4.0f, 1e-4f);
			TestTrue(TEXT("en calma, remos"), Calm.Propulsion == EHullPropulsion::Oars);
			TestTrue(TEXT("con 10 m/s, vela"), Breeze.Propulsion == EHullPropulsion::Sail);
			TestTrue(TEXT("y más rápido"), Breeze.MaxSpeedCmS > Calm.MaxSpeedCmS);
		});

		It("un casco esbelto corre más y va más recto; uno chato gira más", [this]()
		{
			// Mismo volumen y masa: 3 troncos de 6 m frente a 6 de 3 m.
			FHullAssemblyModel Long = Raft(3, 600.0);
			FHullAssemblyModel Wide = Raft(6);
			for (FHullAssemblyModel* Model : { &Long, &Wide })
			{
				Model->AddLoad(Passenger());
				AddPiece(*Model, EHullPieceType::Oars, FVector(0.0, 0.0, 25.0));
			}
			const FHullHydrostatics HL = Long.Evaluate();
			const FHullHydrostatics HW = Wide.Evaluate();
			TestEqual(TEXT("mismo calado"), HL.DraftCm, HW.DraftCm, 0.01f);
			const FHullPerformance PL = Long.Performance(HL, 0.0f);
			const FHullPerformance PW = Wide.Performance(HW, 0.0f);
			TestTrue(TEXT("esbelta más rápida"), PL.MaxSpeedCmS > PW.MaxSpeedCmS);
			TestTrue(TEXT("esbelta más recta"), PL.CourseStability01 > PW.CourseStability01);
			TestTrue(TEXT("chata gira más"), PW.TurnRateDegS > PL.TurnRateDegS);
		});

		It("pasar de la velocidad de casco cuesta mucho más empuje", [this]()
		{
			FHullAssemblyModel Model = Raft();
			Model.AddLoad(Passenger());
			AddPiece(Model, EHullPieceType::Mast, FVector(0.0, 0.0, 222.0));
			for (int32 I = 0; I < 3; ++I)
			{
				AddPiece(Model, EHullPieceType::Sail, FVector(0.0, 0.0, 250.0));
			}
			const FHullHydrostatics H = Model.Evaluate();
			const FHullPerformance Strong = Model.Performance(H, 15.0f);
			const FHullPerformance Gale = Model.Performance(H, 30.0f);
			TestEqual(TEXT("velocidad de casco 0,4 · √(g · L)"), Strong.HullSpeedCmS, 0.4f * FMath::Sqrt(9.81f * 3.0f) * 100.0f, 0.1f);
			TestTrue(TEXT("con 15 m/s ya la pasa"), Strong.MaxSpeedCmS > Strong.HullSpeedCmS);
			// Sin la pared, el doble de viento daría el doble de velocidad.
			TestTrue(TEXT("el doble de viento no da ni 1,5 veces la velocidad"), Gale.MaxSpeedCmS < 1.5f * Strong.MaxSpeedCmS);
		});

		It("más carga, más lenta con el mismo empuje", [this]()
		{
			FHullAssemblyModel Light = Raft();
			Light.AddLoad(Passenger());
			AddPiece(Light, EHullPieceType::Oars, FVector(0.0, 0.0, 25.0));
			FHullAssemblyModel Heavy = Light;
			Heavy.AddLoad(Cargo(150.0f));
			TestTrue(TEXT("cargada, más lenta"),
				Heavy.Performance(Heavy.Evaluate(), 0.0f).MaxSpeedCmS < Light.Performance(Light.Evaluate(), 0.0f).MaxSpeedCmS);
		});

		It("volcada o hundida no avanza", [this]()
		{
			FHullAssemblyModel Narrow = Raft(2);
			Narrow.AddLoad(Passenger());
			AddPiece(Narrow, EHullPieceType::Oars, FVector(0.0, 0.0, 25.0));
			const FHullHydrostatics H = Narrow.Evaluate();
			TestTrue(TEXT("volcada"), H.Verdict == EHullVerdict::Capsizes);
			TestEqual(TEXT("velocidad 0"), Narrow.Performance(H, 0.0f).MaxSpeedCmS, 0.0f);
		});
	});

	Describe("el paso a FBoatModel", [this]()
	{
		It("da una definición con los números de lo armado y un calado coherente", [this]()
		{
			FHullAssemblyModel Model = Raft();
			Model.AddLoad(Passenger());
			AddPiece(Model, EHullPieceType::Oars, FVector(0.0, 0.0, 25.0));
			AddPiece(Model, EHullPieceType::Mast, FVector(0.0, 0.0, 222.0));
			AddPiece(Model, EHullPieceType::Sail, FVector(0.0, 0.0, 250.0));
			const FHullHydrostatics H = Model.Evaluate();
			const FBoatDefinition D = Model.ToBoatDefinition(H);
			TestEqual(TEXT("eslora"), D.LengthCm, 300.0f, 1e-3f);
			TestEqual(TEXT("manga"), D.BeamCm, 132.0f, 1e-3f);
			TestEqual(TEXT("masa del casco"), D.HullMassKg, H.StructureMassKg);
			TestEqual(TEXT("GM"), D.MetacentricHeightCm, H.GMCm);
			TestEqual(TEXT("vela"), D.SailAreaM2, 4.0f, 1e-4f);
			TestTrue(TEXT("centro vélico sobre la flotación"), D.SailCenterOfEffortCm > 200.0f);
			TestTrue(TEXT("vuelco entre 5° y 55°"), D.CapsizeRollDeg >= 5.0f && D.CapsizeRollDeg <= FHullAssemblyModel::CapsizeHeelDeg);
			// FBoatModel calcula el calado como masa / (ρ · flotación): debe dar el del modelo hidrostático.
			const float BoatDraft = H.TotalMassKg / (FHullAssemblyModel::WaterDensity * D.WaterplaneAreaM2()) * 100.0f;
			TestEqual(TEXT("mismo calado"), BoatDraft, H.DraftCm, H.DraftCm * 0.01f);
			TestTrue(TEXT("carga máxima positiva"), D.MaxCargoKg > 0.0f);
		});
	});
}

#endif
