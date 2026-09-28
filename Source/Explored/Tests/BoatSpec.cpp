#include "Misc/AutomationTest.h"

#include "Boats/BoatModel.h"
#include "Ocean/OceanWaves.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace BoatSpecDetail
{
	constexpr float Dt = 1.0f / 60.0f;

	/** Simulación de prueba: reloj de olas propio y mandos fijos. */
	struct FRun
	{
		FBoatModel Model;
		FBoatEnvironment Env;
		FBoatControls Controls;
		float Time = 0.0f;
		bool bAlternate = false;
		bool bOneSide = false;
		EBoatSide OneSide = EBoatSide::Port;
		EBoatSide NextSide = EBoatSide::Starboard;

		FRun(EBoatType Type, const FVector& Location = FVector::ZeroVector, float Yaw = 0.0f)
			: Model(Type, Location, Yaw)
		{
			Model.SetCrewAboard(true);
		}

		void Advance(float Seconds, float FrameDt = Dt)
		{
			const int32 Frames = FMath::RoundToInt(Seconds / FrameDt);
			for (int32 I = 0; I < Frames; ++I)
			{
				if (!Model.IsStroking())
				{
					if (bAlternate)
					{
						Model.TryStroke(NextSide);
						NextSide = NextSide == EBoatSide::Port ? EBoatSide::Starboard : EBoatSide::Port;
					}
					else if (bOneSide)
					{
						Model.TryStroke(OneSide);
					}
				}
				Time += FrameDt;
				Env.WaveTimeSeconds = Time;
				Model.Step(FrameDt, Controls, Env);
			}
		}

		/** Velocidad a lo largo de la proa (m/s): lo que avanza de verdad, sin la deriva hacia atrás. */
		float ForwardSpeedMS() const
		{
			const float Yaw = FMath::DegreesToRadians(Model.GetState().YawDeg);
			const FVector2D Forward(FMath::Cos(Yaw), FMath::Sin(Yaw));
			return static_cast<float>(FVector2D::DotProduct(Model.GetState().VelocityCmS, Forward) / 100.0);
		}
	};

	/** Velocidad estable a vela con viento del norte (sopla hacia -X) y un ángulo verdadero dado. */
	float SteadySailSpeedMS(EBoatType Type, float TrueWindAngleDeg, float Wind01 = 0.2f, bool bAutoTrim = true, float Trim = 0.5f)
	{
		FRun Run(Type, FVector::ZeroVector, TrueWindAngleDeg);
		Run.Model.SetSailRaised(true);
		Run.Env.WindCmS = FBoatWind::VelocityCmS(Wind01, 0.0f);
		Run.Controls.bAutoTrim = bAutoTrim;
		Run.Controls.SailTrim01 = Trim;
		Run.Advance(90.0f);
		return Run.ForwardSpeedMS();
	}

	/** Cuántas de 8 salidas (rumbos y posiciones distintos) vuelcan en 2 minutos con un mar y viento dados. */
	int32 CountCapsizes(EBoatType Type, float SeaState, float Wind01, float& OutMaxRoll)
	{
		const FOceanWaves Waves = FOceanWaves::Make(SeaState);
		int32 Capsized = 0;
		OutMaxRoll = 0.0f;
		for (int32 I = 0; I < 8; ++I)
		{
			FRun Run(Type, FVector(I * 1234.0, I * -777.0, 0.0), I * 45.0f);
			Run.Env.Waves = &Waves;
			Run.Env.WindCmS = FBoatWind::VelocityCmS(Wind01, 90.0f);
			Run.Advance(120.0f);
			OutMaxRoll = FMath::Max(OutMaxRoll, Run.Model.GetMaxAbsRollDeg());
			Capsized += Run.Model.GetState().Condition == EBoatCondition::Capsized ? 1 : 0;
		}
		return Capsized;
	}

	/** Arrecife al este: 3 m de agua hasta X = 2000 cm y 5 cm encima de la plataforma. */
	float ReefDepth(const FVector2D& P)
	{
		return P.X < 2000.0 ? 300.0f : 5.0f;
	}

	/** Borde de la plataforma: 3 m de agua hasta X = 3000 cm y 50 m más allá. */
	float ShelfDepth(const FVector2D& P)
	{
		return P.X < 3000.0 ? 300.0f : 5000.0f;
	}
}

BEGIN_DEFINE_SPEC(FBoatSpec, "Explored.Boats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FBoatSpec)

void FBoatSpec::Define()
{
	using namespace BoatSpecDetail;

	Describe("Definiciones", [this]()
	{
		It("siguen la progresión balsa → canoa → balancín → «Limón» (GDD §8.10)", [this]()
		{
			const FBoatDefinition& Raft = FBoatModel::Definition(EBoatType::Raft);
			const FBoatDefinition& Canoe = FBoatModel::Definition(EBoatType::Canoe);
			const FBoatDefinition& Outrigger = FBoatModel::Definition(EBoatType::Outrigger);
			const FBoatDefinition& Limon = FBoatModel::Definition(EBoatType::Limon);
			TestTrue(TEXT("Solo la balsa se limita a aguas someras"), Raft.bShallowWaterOnly
				&& !Canoe.bShallowWaterOnly && !Outrigger.bShallowWaterOnly && !Limon.bShallowWaterOnly);
			TestTrue(TEXT("La balsa es la más lenta remando"), Raft.MaxPaddleSpeedCmS < Canoe.MaxPaddleSpeedCmS);
			TestTrue(TEXT("Solo el balancín y el «Limón» llevan vela"), !Raft.HasSail() && !Canoe.HasSail()
				&& Outrigger.HasSail() && Limon.HasSail());
			TestTrue(TEXT("La carga crece con la progresión"), Raft.MaxCargoKg < Canoe.MaxCargoKg
				&& Canoe.MaxCargoKg < Outrigger.MaxCargoKg && Outrigger.MaxCargoKg < Limon.MaxCargoKg);
			TestTrue(TEXT("El balancín aguanta más escora que la canoa"), Outrigger.CapsizeRollDeg > Canoe.CapsizeRollDeg);
			TestEqual(TEXT("Malla de la balsa (boats.py)"), FString(Raft.MeshName), FString(TEXT("SM_Raft")));
			TestEqual(TEXT("Malla de la canoa"), FString(Canoe.MeshName), FString(TEXT("SM_Canoe")));
			TestEqual(TEXT("Malla del balancín"), FString(Outrigger.MeshName), FString(TEXT("SM_Canoe_Outrigger")));
		});

		It("caben la carga máxima y el tripulante sin llegar a la borda", [this]()
		{
			for (int32 T = 0; T < static_cast<int32>(EBoatType::Count); ++T)
			{
				FBoatModel Model(static_cast<EBoatType>(T), FVector::ZeroVector, 0.0f);
				Model.SetCrewAboard(true);
				TestTrue(TEXT("Admite la carga máxima"), Model.TryAddCargo(Model.GetDefinition().MaxCargoKg));
				TestTrue(TEXT("Francobordo de al menos un tercio del casco"),
					Model.EquilibriumDraftCm() < Model.GetDefinition().HullDepthCm * 0.67f);
				TestFalse(TEXT("Rechaza la sobrecarga"), Model.TryAddCargo(1.0f));
			}
		});

		It("rechaza cargas no finitas sin tocar la carga", [this]()
		{
			FBoatModel Model(EBoatType::Canoe, FVector::ZeroVector, 0.0f);
			TestTrue(TEXT("carga normal"), Model.TryAddCargo(10.0f));
			TestFalse(TEXT("NaN"), Model.TryAddCargo(std::numeric_limits<float>::quiet_NaN()));
			TestFalse(TEXT("infinito"), Model.TryAddCargo(std::numeric_limits<float>::infinity()));
			TestEqual(TEXT("sigue con 10 kg"), Model.GetState().CargoKg, 10.0f);
		});
	});

	Describe("Flotación", [this]()
	{
		It("flota en su calado de equilibrio en agua quieta", [this]()
		{
			for (int32 T = 0; T < static_cast<int32>(EBoatType::Count); ++T)
			{
				FRun Run(static_cast<EBoatType>(T));
				Run.Advance(15.0f);
				const FBoatDefinition& D = Run.Model.GetDefinition();
				const float Expected = (D.HullMassKg + FBoatModel::CrewMassKg)
					/ (FBoatModel::WaterDensity * D.WaterplaneAreaM2()) * 100.0f;
				TestEqual(TEXT("Calado = masa / (ρ · área de flotación)"), Run.Model.EquilibriumDraftCm(), Expected, 0.01f);
				TestEqual(TEXT("Quilla a -calado"), static_cast<float>(Run.Model.GetState().LocationCm.Z), -Expected, 0.5f);
				TestTrue(TEXT("Adrizado"), FMath::Abs(Run.Model.GetState().RollDeg) < 0.1f
					&& FMath::Abs(Run.Model.GetState().PitchDeg) < 0.1f);
				TestTrue(TEXT("Calado de centímetros, no de metros"), Expected > 3.0f && Expected < 20.0f);
			}
		});

		It("se hunde más con carga", [this]()
		{
			FRun Empty(EBoatType::Canoe);
			FRun Loaded(EBoatType::Canoe);
			TestTrue(TEXT("Carga aceptada"), Loaded.Model.TryAddCargo(100.0f));
			Empty.Advance(15.0f);
			Loaded.Advance(15.0f);
			TestTrue(TEXT("Más calado cargada"), Loaded.Model.GetState().LocationCm.Z < Empty.Model.GetState().LocationCm.Z - 3.0);
			TestEqual(TEXT("Descarga lo pedido"), Loaded.Model.RemoveCargo(40.0f), 40.0f, 1e-4f);
		});

		It("sigue las olas: sube y baja con el agua y cabecea y balancea", [this]()
		{
			const FOceanWaves Waves = FOceanWaves::Make(0.3f);
			FRun Run(EBoatType::Canoe, FVector(500.0, 300.0, 0.0), 30.0f);
			Run.Env.Waves = &Waves;
			Run.Advance(5.0f);
			const float Draft = Run.Model.EquilibriumDraftCm();
			float ErrorSum = 0.0f;
			float MinWater = TNumericLimits<float>::Max();
			float MaxWater = -TNumericLimits<float>::Max();
			float MinBoat = TNumericLimits<float>::Max();
			float MaxBoat = -TNumericLimits<float>::Max();
			float MaxPitch = 0.0f;
			float MaxRoll = 0.0f;
			int32 Samples = 0;
			for (int32 I = 0; I < 300; ++I)
			{
				Run.Advance(0.1f);
				const FBoatState& S = Run.Model.GetState();
				const float Water = Waves.HeightAt(FVector2D(S.LocationCm.X, S.LocationCm.Y), Run.Time);
				const float Waterline = static_cast<float>(S.LocationCm.Z) + Draft;
				ErrorSum += FMath::Abs(Waterline - Water);
				MinWater = FMath::Min(MinWater, Water);
				MaxWater = FMath::Max(MaxWater, Water);
				MinBoat = FMath::Min(MinBoat, Waterline);
				MaxBoat = FMath::Max(MaxBoat, Waterline);
				MaxPitch = FMath::Max(MaxPitch, FMath::Abs(S.PitchDeg));
				MaxRoll = FMath::Max(MaxRoll, FMath::Abs(S.RollDeg));
				++Samples;
			}
			const float WaterRange = MaxWater - MinWater;
			TestTrue(TEXT("Hay olas de verdad"), WaterRange > 30.0f);
			TestTrue(TEXT("La flotación acompaña al agua"), ErrorSum / Samples < 0.2f * WaterRange);
			TestTrue(TEXT("Sube y baja casi tanto como el agua"), MaxBoat - MinBoat > 0.7f * WaterRange);
			TestTrue(TEXT("Cabecea"), MaxPitch > 0.5f);
			TestTrue(TEXT("Balancea"), MaxRoll > 0.5f);
			TestTrue(TEXT("Sin volcar con mar moderado"), Run.Model.GetState().Condition == EBoatCondition::Afloat);
		});

		It("integra igual con cualquier paso de fotograma", [this]()
		{
			const FOceanWaves Waves = FOceanWaves::Make(0.4f);
			auto Simulate = [&Waves](float FrameDt)
			{
				FRun Run(EBoatType::Outrigger, FVector(0.0, 0.0, 0.0), 80.0f);
				Run.Model.SetSailRaised(true);
				Run.Env.Waves = &Waves;
				Run.Env.WindCmS = FBoatWind::VelocityCmS(0.3f, 0.0f);
				Run.Env.CurrentCmS = FVector2D(30.0, -20.0);
				Run.Controls.Rudder = 0.2f;
				Run.Advance(20.0f, FrameDt);
				return Run.Model.GetState();
			};
			const FBoatState Reference = Simulate(1.0f / 60.0f);
			TestTrue(TEXT("Se ha movido"), FVector2D(Reference.LocationCm.X, Reference.LocationCm.Y).Size() > 3000.0);
			const float FrameDts[] = {1.0f / 144.0f, 1.0f / 30.0f, 0.1f, 0.25f, 1.0f};
			for (const float FrameDt : FrameDts)
			{
				const FBoatState S = Simulate(FrameDt);
				const double PositionError = FVector2D(S.LocationCm.X - Reference.LocationCm.X, S.LocationCm.Y - Reference.LocationCm.Y).Size();
				TestTrue(FString::Printf(TEXT("Posición estable con dt=%.4f (%.1f cm)"), FrameDt, PositionError), PositionError < 15.0);
				TestTrue(TEXT("Arfada estable"), FMath::Abs(S.LocationCm.Z - Reference.LocationCm.Z) < 3.0);
				TestTrue(TEXT("Rumbo estable"), FMath::Abs(FMath::FindDeltaAngleDegrees(S.YawDeg, Reference.YawDeg)) < 1.0f);
				TestTrue(TEXT("Sin valores no finitos"), FMath::IsFinite(S.RollDeg) && FMath::IsFinite(S.PitchDeg)
					&& FMath::IsFinite(static_cast<float>(S.LocationCm.Z)));
			}
		});

		It("no se hunde nunca a flote; anegado flota a ras y solo el casco destrozado se va a pique", [this]()
		{
			const FOceanWaves Waves = FOceanWaves::Make(0.45f);
			FRun Full(EBoatType::Canoe);
			Full.Env.Waves = &Waves;
			Full.Model.TryAddCargo(Full.Model.GetDefinition().MaxCargoKg);
			Full.Advance(60.0f);
			TestTrue(TEXT("Cargada del todo sigue a flote"), Full.Model.GetState().Condition == EBoatCondition::Afloat);

			// Una vía de agua anega la canoa, pero la madera flota a ras.
			FRun Leaky(EBoatType::Canoe);
			Leaky.Model.ApplyDamage(0.9f);
			Leaky.Advance(300.0f);
			const FBoatDefinition& D = Leaky.Model.GetDefinition();
			TestTrue(TEXT("Anegada"), Leaky.Model.GetState().Condition == EBoatCondition::Swamped);
			TestEqual(TEXT("Borda a ras del agua"), static_cast<float>(Leaky.Model.GetState().LocationCm.Z) + D.HullDepthCm, 0.0f, 1.0f);

			// Tapada la vía de agua y achicando, vuelve a navegar.
			Leaky.Model.Repair(0.9f);
			Leaky.Controls.bBailing = true;
			Leaky.Advance(120.0f);
			TestTrue(TEXT("Achicada vuelve a flote"), Leaky.Model.GetState().Condition == EBoatCondition::Afloat);

			// Destrozada del todo se hunde hasta el fondo.
			Leaky.Controls.bBailing = false;
			Leaky.Env.DepthBelowSeaLevelCm = [](const FVector2D&) { return 800.0f; };
			Leaky.Model.ApplyDamage(1.0f);
			Leaky.Advance(60.0f);
			TestTrue(TEXT("Naufragio"), Leaky.Model.GetState().Condition == EBoatCondition::Wrecked);
			TestEqual(TEXT("En el fondo"), static_cast<float>(Leaky.Model.GetState().LocationCm.Z), -800.0f, 1.0f);
		});
	});

	Describe("Remo", [this]()
	{
		It("da velocidades creíbles: balsa ~1 m/s, canoa ~2,5 m/s", [this]()
		{
			auto PaddleSpeed = [](EBoatType Type)
			{
				FRun Run(Type);
				Run.bAlternate = true;
				Run.Advance(60.0f);
				return Run.Model.SpeedCmS() / 100.0f;
			};
			const float Raft = PaddleSpeed(EBoatType::Raft);
			const float Canoe = PaddleSpeed(EBoatType::Canoe);
			const float Outrigger = PaddleSpeed(EBoatType::Outrigger);
			TestTrue(FString::Printf(TEXT("Balsa %.2f m/s"), Raft), Raft > 0.8f && Raft < 1.2f);
			TestTrue(FString::Printf(TEXT("Canoa %.2f m/s"), Canoe), Canoe > 2.2f && Canoe < 2.8f);
			TestTrue(TEXT("El balancín rema algo más lento que la canoa"), Outrigger < Canoe && Outrigger > 1.5f);
		});

		It("gira al remar por una banda y mantiene el rumbo alternando", [this]()
		{
			FRun Port(EBoatType::Canoe);
			Port.bOneSide = true;
			Port.OneSide = EBoatSide::Port;
			Port.Advance(10.0f);
			FRun Starboard(EBoatType::Canoe);
			Starboard.bOneSide = true;
			Starboard.OneSide = EBoatSide::Starboard;
			Starboard.Advance(10.0f);
			const float PortTurn = FMath::FindDeltaAngleDegrees(0.0f, Port.Model.GetState().YawDeg);
			const float StarboardTurn = FMath::FindDeltaAngleDegrees(0.0f, Starboard.Model.GetState().YawDeg);
			TestTrue(TEXT("Remar por babor cae a estribor"), PortTurn > 45.0f);
			TestTrue(TEXT("Remar por estribor cae a babor"), StarboardTurn < -45.0f);

			FRun Alternate(EBoatType::Canoe);
			Alternate.bAlternate = true;
			Alternate.Advance(60.0f);
			TestTrue(TEXT("Alternando mantiene el rumbo"), FMath::Abs(FMath::FindDeltaAngleDegrees(0.0f, Alternate.Model.GetState().YawDeg)) < 10.0f);
			TestTrue(TEXT("Y avanza hacia proa"), Alternate.Model.GetState().LocationCm.X > 10000.0);
		});

		It("el timón solo gobierna con arrancada", [this]()
		{
			FRun Still(EBoatType::Canoe);
			Still.Controls.Rudder = 1.0f;
			Still.Advance(10.0f);
			TestTrue(TEXT("Parada no gira"), FMath::Abs(FMath::FindDeltaAngleDegrees(0.0f, Still.Model.GetState().YawDeg)) < 0.5f);

			FRun Moving(EBoatType::Canoe);
			Moving.bAlternate = true;
			Moving.Advance(20.0f);
			const float Before = Moving.Model.GetState().YawDeg;
			Moving.Controls.Rudder = 1.0f;
			Moving.Advance(5.0f);
			TestTrue(TEXT("Con arrancada cae a estribor"), FMath::FindDeltaAngleDegrees(Before, Moving.Model.GetState().YawDeg) > 20.0f);
		});

		It("no rema sin tripulante ni volcado", [this]()
		{
			FBoatModel Empty(EBoatType::Canoe, FVector::ZeroVector, 0.0f);
			TestFalse(TEXT("Sin tripulante"), Empty.TryStroke(EBoatSide::Port));
			Empty.SetCrewAboard(true);
			TestTrue(TEXT("Con tripulante"), Empty.TryStroke(EBoatSide::Port));
			TestFalse(TEXT("Una palada cada vez"), Empty.TryStroke(EBoatSide::Starboard));
		});
	});

	Describe("Vela", [this]()
	{
		It("no avanza en la zona muerta y va más rápido de través", [this]()
		{
			TArray<float> Speeds;
			int32 Fastest = 0;
			for (int32 Angle = 0; Angle <= 180; Angle += 15)
			{
				Speeds.Add(SteadySailSpeedMS(EBoatType::Outrigger, static_cast<float>(Angle)));
				if (Speeds.Last() > Speeds[Fastest])
				{
					Fastest = Speeds.Num() - 1;
				}
			}
			TestTrue(TEXT("Proa al viento no avanza"), Speeds[0] < 0.1f);
			TestTrue(TEXT("A 30° no avanza"), Speeds[2] < 0.2f);
			TestTrue(TEXT("A 45° apenas avanza"), Speeds[3] < 0.3f * Speeds[6]);
			TestEqual(TEXT("Lo más rápido es de través (90°)"), Fastest * 15, 90);
			TestTrue(TEXT("De popa más lento que de través"), Speeds[12] < 0.6f * Speeds[6]);
			TestTrue(FString::Printf(TEXT("Balancín de través con alisio: %.2f m/s"), Speeds[6]), Speeds[6] > 4.0f && Speeds[6] < 6.0f);
		});

		It("el «Limón» navega bien de través y más rápido que remando", [this]()
		{
			const float Sailing = SteadySailSpeedMS(EBoatType::Limon, 90.0f);
			TestTrue(FString::Printf(TEXT("«Limón» de través %.2f m/s"), Sailing), Sailing > 3.5f && Sailing < 6.5f);
			TestTrue(TEXT("Más rápido que remando"), Sailing * 100.0f > 2.0f * FBoatModel::Definition(EBoatType::Limon).MaxPaddleSpeedCmS);
		});

		It("el viento importa: más viento, más velocidad", [this]()
		{
			const float Light = SteadySailSpeedMS(EBoatType::Outrigger, 90.0f, 0.05f);
			const float Fresh = SteadySailSpeedMS(EBoatType::Outrigger, 90.0f, 0.3f);
			TestTrue(TEXT("Con brisa floja va más lento"), Light < 0.6f * Fresh);
		});

		It("mal trimada pierde velocidad y la escota óptima se abre con el viento aparente", [this]()
		{
			const float Auto = SteadySailSpeedMS(EBoatType::Outrigger, 90.0f);
			const float Oversheeted = SteadySailSpeedMS(EBoatType::Outrigger, 90.0f, 0.2f, false, 1.0f);
			TestTrue(TEXT("Largada del todo de través va más lenta"), Oversheeted < 0.7f * Auto);
			TestTrue(TEXT("Escota óptima creciente"), FBoatModel::OptimalSailTrim01(45.0f) < FBoatModel::OptimalSailTrim01(90.0f)
				&& FBoatModel::OptimalSailTrim01(90.0f) < FBoatModel::OptimalSailTrim01(170.0f));
			TestEqual(TEXT("Rendimiento pleno en la óptima"), FBoatModel::SailTrimEfficiency(FBoatModel::OptimalSailTrim01(70.0f), 70.0f), 1.0f, 1e-4f);
		});

		It("abate a sotavento y escora al ceñir", [this]()
		{
			// Viento del norte y proa al 60°: sopla desde babor, así que sotavento es estribor.
			FRun Run(EBoatType::Outrigger, FVector::ZeroVector, 60.0f);
			Run.Model.SetSailRaised(true);
			Run.Env.WindCmS = FBoatWind::VelocityCmS(0.3f, 0.0f);
			Run.Advance(60.0f);
			TestTrue(FString::Printf(TEXT("Abatimiento %.1f°"), Run.Model.GetLeewayDeg()),
				Run.Model.GetLeewayDeg() > 1.0f && Run.Model.GetLeewayDeg() < 10.0f);
			TestTrue(TEXT("Escora a sotavento (estribor abajo)"), Run.Model.GetState().RollDeg > 1.0f);
			TestTrue(TEXT("El viento aparente entra más de proa que el verdadero"), Run.Model.GetApparentWindAngleDeg() < 60.0f);
		});

		It("sin vela la canoa y la balsa no izan nada", [this]()
		{
			FBoatModel Canoe(EBoatType::Canoe, FVector::ZeroVector, 0.0f);
			TestFalse(TEXT("La canoa no tiene vela"), Canoe.SetSailRaised(true));
			FBoatModel Outrigger(EBoatType::Outrigger, FVector::ZeroVector, 0.0f);
			TestTrue(TEXT("El balancín sí"), Outrigger.SetSailRaised(true));
		});
	});

	Describe("Corrientes y mareas", [this]()
	{
		It("la corriente arrastra el barco a la deriva", [this]()
		{
			FRun Run(EBoatType::Canoe, FVector::ZeroVector, 90.0f);
			Run.Env.CurrentCmS = FVector2D(100.0, 0.0);
			Run.Advance(60.0f);
			const FBoatState& S = Run.Model.GetState();
			TestTrue(TEXT("Va a la velocidad del agua"), (S.VelocityCmS - Run.Env.CurrentCmS).Size() < 5.0);
			TestTrue(TEXT("Recorre casi lo que el agua"), S.LocationCm.X > 0.8 * 100.0 * 60.0);
			TestTrue(TEXT("Sin desviarse"), FMath::Abs(S.LocationCm.Y) < 50.0);
		});

		It("remando contra la corriente se avanza menos que a favor", [this]()
		{
			auto Distance = [](double CurrentX)
			{
				FRun Run(EBoatType::Canoe);
				Run.bAlternate = true;
				Run.Env.CurrentCmS = FVector2D(CurrentX, 0.0);
				Run.Advance(30.0f);
				return Run.Model.GetState().LocationCm.X;
			};
			TestTrue(TEXT("A favor > en calma > en contra"), Distance(100.0) > Distance(0.0) && Distance(0.0) > Distance(-100.0));
		});

		It("la marea sube dos veces al día, más en marea viva, y deja varado en la bajamar", [this]()
		{
			TestTrue(TEXT("Pleamar viva"), FBoatModel::TideOffsetCm(0.125f, 0.0f) > 0.95f * FBoatModel::TideAmplitudeCm);
			TestTrue(TEXT("Bajamar viva"), FBoatModel::TideOffsetCm(0.375f, 0.0f) < -0.95f * FBoatModel::TideAmplitudeCm);
			TestTrue(TEXT("Marea muerta menor"), FBoatModel::TideOffsetCm(0.125f, 0.25f) < 0.5f * FBoatModel::TideAmplitudeCm);

			// Bajío de 40 cm: con la pleamar flota, con la bajamar descansa en el fondo.
			auto Shoal = [](const FVector2D&) { return 40.0f; };
			FRun High(EBoatType::Canoe);
			High.Env.DepthBelowSeaLevelCm = Shoal;
			High.Env.TideOffsetCm = FBoatModel::TideOffsetCm(0.125f, 0.0f);
			High.Advance(10.0f);
			FRun Low(EBoatType::Canoe);
			Low.Env.DepthBelowSeaLevelCm = Shoal;
			Low.Env.TideOffsetCm = FBoatModel::TideOffsetCm(0.375f, 0.0f);
			Low.Advance(10.0f);
			TestFalse(TEXT("Pleamar: a flote"), High.Model.GetState().bGrounded);
			TestTrue(TEXT("Bajamar: varada"), Low.Model.GetState().bGrounded);
			TestEqual(TEXT("Descansa en el fondo"), static_cast<float>(Low.Model.GetState().LocationCm.Z), -40.0f, 0.5f);
		});
	});

	Describe("Fondo", [this]()
	{
		It("encalla en el arrecife, se daña si iba rápido y puede volver a aguas hondas", [this]()
		{
			FRun Run(EBoatType::Canoe);
			Run.Env.DepthBelowSeaLevelCm = &ReefDepth;
			Run.bAlternate = true;
			Run.Advance(40.0f);
			const FBoatState& S = Run.Model.GetState();
			TestTrue(TEXT("Encallada"), S.bGrounded);
			TestTrue(TEXT("No cruza el arrecife"), S.LocationCm.X < 2000.0);
			TestTrue(TEXT("El golpe a ~2,5 m/s daña el casco"), S.HullDamage01 > 0.0f);
			TestTrue(TEXT("Pero no lo destroza"), S.HullDamage01 < 0.5f);

			// Media vuelta con paladas por babor y a remar: se aleja.
			Run.bAlternate = false;
			Run.bOneSide = true;
			Run.OneSide = EBoatSide::Port;
			Run.Advance(12.0f);
			const double XAfterTurn = Run.Model.GetState().LocationCm.X;
			Run.bOneSide = false;
			Run.bAlternate = true;
			Run.Advance(10.0f);
			TestTrue(TEXT("Se aleja del arrecife"), Run.Model.GetState().LocationCm.X < XAfterTurn - 500.0);
			TestFalse(TEXT("Ya no toca fondo"), Run.Model.GetState().bGrounded);
		});

		It("la balsa despacio no se daña contra el fondo", [this]()
		{
			FRun Run(EBoatType::Raft);
			Run.Env.DepthBelowSeaLevelCm = &ReefDepth;
			Run.bAlternate = true;
			Run.Advance(60.0f);
			TestTrue(TEXT("Varada"), Run.Model.GetState().bGrounded);
			TestEqual(TEXT("Sin daño a 1 m/s"), Run.Model.GetState().HullDamage01, 0.0f, 1e-4f);
		});

		It("la balsa se niega a salir a mar abierto; la canoa no", [this]()
		{
			FRun Raft(EBoatType::Raft);
			Raft.Env.DepthBelowSeaLevelCm = &ShelfDepth;
			Raft.bAlternate = true;
			Raft.Advance(60.0f);
			TestTrue(TEXT("Balsa en el borde"), Raft.Model.GetState().bAtOpenOceanLimit);
			TestTrue(TEXT("Balsa no pasa"), Raft.Model.GetState().LocationCm.X < 3000.0);

			// Ni siquiera la corriente la saca.
			Raft.bAlternate = false;
			Raft.Env.CurrentCmS = FVector2D(150.0, 0.0);
			Raft.Advance(30.0f);
			TestTrue(TEXT("La corriente tampoco la saca"), Raft.Model.GetState().LocationCm.X < 3000.0);

			// Pero puede volver hacia la costa.
			Raft.Env.CurrentCmS = FVector2D(-100.0, 0.0);
			Raft.Advance(20.0f);
			TestTrue(TEXT("Vuelve hacia aguas someras"), Raft.Model.GetState().LocationCm.X < 2500.0);

			FRun Canoe(EBoatType::Canoe);
			Canoe.Env.DepthBelowSeaLevelCm = &ShelfDepth;
			Canoe.bAlternate = true;
			Canoe.Advance(30.0f);
			TestTrue(TEXT("La canoa sale a mar abierto"), Canoe.Model.GetState().LocationCm.X > 5000.0);
		});
	});

	Describe("Vuelco", [this]()
	{
		It("con mar y viento moderados no vuelca ninguno", [this]()
		{
			for (int32 T = 0; T < static_cast<int32>(EBoatType::Count); ++T)
			{
				float MaxRoll = 0.0f;
				const int32 Capsized = CountCapsizes(static_cast<EBoatType>(T), 0.45f, 0.45f, MaxRoll);
				TestEqual(FString::Printf(TEXT("%s sin vuelcos (escora máx. %.1f°)"), LexToString(static_cast<EBoatType>(T)), MaxRoll), Capsized, 0);
			}
		});

		It("con temporal vuelcan la balsa y la canoa, pero no el balancín ni el «Limón»", [this]()
		{
			float MaxRoll = 0.0f;
			TestTrue(TEXT("La canoa vuelca"), CountCapsizes(EBoatType::Canoe, 1.0f, 1.0f, MaxRoll) >= 4);
			TestTrue(TEXT("La balsa vuelca"), CountCapsizes(EBoatType::Raft, 1.0f, 1.0f, MaxRoll) >= 1);
			TestEqual(TEXT("El balancín aguanta"), CountCapsizes(EBoatType::Outrigger, 1.0f, 1.0f, MaxRoll), 0);
			TestTrue(TEXT("Con margen"), MaxRoll < 0.6f * FBoatModel::Definition(EBoatType::Outrigger).CapsizeRollDeg);
			TestEqual(TEXT("El «Limón» aguanta"), CountCapsizes(EBoatType::Limon, 1.0f, 1.0f, MaxRoll), 0);
		});

		It("volcado deriva quilla arriba y al adrizarlo queda anegado", [this]()
		{
			const FOceanWaves Waves = FOceanWaves::Make(1.0f);
			FRun Run(EBoatType::Canoe);
			Run.Env.Waves = &Waves;
			Run.Env.WindCmS = FBoatWind::VelocityCmS(1.0f, 90.0f);
			Run.Advance(120.0f);
			TestTrue(TEXT("Volcada"), Run.Model.GetState().Condition == EBoatCondition::Capsized);
			TestTrue(TEXT("Quilla arriba"), FMath::Abs(Run.Model.GetState().RollDeg) > 150.0f);
			TestFalse(TEXT("No se rema volcado"), Run.Model.TryStroke(EBoatSide::Port));
			TestTrue(TEXT("Se adriza"), Run.Model.TryRight());
			TestTrue(TEXT("Anegada tras adrizar"), Run.Model.GetState().Condition == EBoatCondition::Swamped);

			FBoatModel Raft(EBoatType::Raft, FVector::ZeroVector, 0.0f);
			TestFalse(TEXT("No se adriza lo que no está volcado"), Raft.TryRight());
		});
	});

	Describe("Amarre y ficha propia", [this]()
	{
		It("amarrada a un poste no se aleja más que el cabo, pero sigue cabeceando con el oleaje", [this]()
		{
			const FOceanWaves Waves = FOceanWaves::Make(0.4f);
			FRun Free(EBoatType::Raft);
			Free.Model.SetCrewAboard(false);
			Free.Env.Waves = &Waves;
			Free.Env.CurrentCmS = FVector2D(50.0, 20.0);
			Free.Advance(60.0f);

			FRun Moored(EBoatType::Raft);
			Moored.Model.SetCrewAboard(false);
			Moored.Env.Waves = &Waves;
			Moored.Env.CurrentCmS = FVector2D(50.0, 20.0);
			TestTrue(TEXT("amarra"), Moored.Model.Moor(FVector2D(-100.0, 0.0), 300.0f));
			float MinPitch = 1000.0f, MaxPitch = -1000.0f;
			double MaxDistance = 0.0;
			for (int32 I = 0; I < 60; ++I)
			{
				Moored.Advance(1.0f);
				const FBoatState& S = Moored.Model.GetState();
				MinPitch = FMath::Min(MinPitch, S.PitchDeg);
				MaxPitch = FMath::Max(MaxPitch, S.PitchDeg);
				MaxDistance = FMath::Max(MaxDistance, (FVector2D(S.LocationCm.X, S.LocationCm.Y) - FVector2D(-100.0, 0.0)).Size());
			}
			const FVector2D FreeAt(Free.Model.GetState().LocationCm.X, Free.Model.GetState().LocationCm.Y);
			TestTrue(TEXT("suelta deriva lejos"), FreeAt.Size() > 2000.0);
			TestTrue(FString::Printf(TEXT("amarrada: %.1f cm ≤ 300"), MaxDistance), MaxDistance <= 300.0 + 1e-3);
			TestTrue(TEXT("el cabo va tenso"), Moored.Model.GetState().bMooringTaut);
			TestTrue(TEXT("cabecea"), MaxPitch - MinPitch > 1.0f);

			Moored.Model.CastOff();
			Moored.Advance(20.0f);
			const FBoatState& S = Moored.Model.GetState();
			TestTrue(TEXT("suelta el cabo y se va"), (FVector2D(S.LocationCm.X, S.LocationCm.Y) - FVector2D(-100.0, 0.0)).Size() > 600.0);
		});

		It("con el cabo tenso pierde solo la velocidad que lo aleja y se desliza de lado por el círculo", [this]()
		{
			FRun Run(EBoatType::Raft);
			Run.Model.SetCrewAboard(false);
			const FVector2D Anchor(-300.0, 0.0);
			TestTrue(TEXT("amarra con el cabo justo"), Run.Model.Moor(Anchor, 300.0f));
			// Corriente hacia fuera (+X) y de lado (+Y).
			Run.Env.CurrentCmS = FVector2D(80.0, 40.0);
			Run.Advance(3.0f);
			const FBoatState& S = Run.Model.GetState();
			const FVector2D Here(S.LocationCm.X, S.LocationCm.Y);
			const FVector2D Out = (Here - Anchor).GetSafeNormal();
			const FVector2D Side(-Out.Y, Out.X);
			TestTrue(TEXT("tenso"), S.bMooringTaut);
			TestTrue(TEXT("en el borde del círculo"), (Here - Anchor).Size() <= 300.0 + 1e-3);
			TestTrue(TEXT("sin velocidad hacia fuera"), FVector2D::DotProduct(S.VelocityCmS, Out) <= 1e-6);
			TestTrue(TEXT("conserva la de lado"), FVector2D::DotProduct(S.VelocityCmS, Side) > 5.0);
			TestTrue(TEXT("y se ha deslizado por el círculo"), S.LocationCm.Y > 10.0);
		});

		It("no amarra a un poste fuera del alcance del cabo, sin cabo ni destrozada, y el amarre se guarda", [this]()
		{
			FBoatModel Model(EBoatType::Raft, FVector::ZeroVector, 0.0f);
			TestFalse(TEXT("demasiado lejos"), Model.Moor(FVector2D(500.0, 0.0), 300.0f));
			TestFalse(TEXT("cabo de 0"), Model.Moor(FVector2D(0.0, 0.0), 0.0f));
			TestFalse(TEXT("cabo NaN"), Model.Moor(FVector2D(0.0, 0.0), std::numeric_limits<float>::quiet_NaN()));
			TestFalse(TEXT("poste NaN"), Model.Moor(FVector2D(std::numeric_limits<double>::quiet_NaN(), 0.0), 300.0f));
			TestFalse(TEXT("cabo infinito"), Model.Moor(FVector2D(0.0, 0.0), std::numeric_limits<float>::infinity()));
			TestFalse(TEXT("poste infinito"), Model.Moor(FVector2D(0.0, std::numeric_limits<double>::infinity()), 300.0f));
			TestFalse(TEXT("sin amarrar tras los intentos fallidos"), Model.IsMoored());
			TestTrue(TEXT("justo en el largo"), Model.Moor(FVector2D(300.0, 0.0), 300.0f));
			const FBoatModel Loaded = FBoatModel::FromSaveData(Model.ToSaveData());
			TestTrue(TEXT("se guarda amarrada"), Loaded.IsMoored());
			TestEqual(TEXT("con su cabo"), Loaded.GetState().MooringLengthCm, 300.0f);
			TestTrue(TEXT("y su poste"), Loaded.GetState().MooringAnchorCm == FVector2D(300.0, 0.0));

			FBoatModel Wreck(EBoatType::Raft, FVector::ZeroVector, 0.0f);
			Wreck.ApplyDamage(1.0f);
			TestFalse(TEXT("destrozada no"), Wreck.Moor(FVector2D(0.0, 0.0), 100.0f));
		});

		It("con una ficha propia navega con ella y SetDefinition conserva el estado y recorta la carga", [this]()
		{
			FBoatDefinition Heavy = FBoatModel::Definition(EBoatType::Raft);
			Heavy.HullMassKg *= 2.0f;
			const FBoatModel Light(EBoatType::Raft, FVector::ZeroVector, 0.0f);
			FBoatModel Custom(Heavy, FVector(10.0, 20.0, 0.0), 45.0f);
			TestTrue(TEXT("tipo de la ficha"), Custom.GetState().Type == EBoatType::Raft);
			TestTrue(TEXT("más pesada cala más"), Custom.EquilibriumDraftCm() > Light.EquilibriumDraftCm());
			TestTrue(TEXT("carga"), Custom.TryAddCargo(100.0f));

			FBoatDefinition Small = Heavy;
			Small.MaxCargoKg = 40.0f;
			Small.LengthCm = 0.0f;
			Custom.SetDefinition(Small);
			TestEqual(TEXT("carga recortada"), Custom.GetState().CargoKg, 40.0f);
			TestTrue(TEXT("eslora degenerada saneada"), Custom.GetDefinition().LengthCm >= 10.0f);
			FBoatDefinition Unstable = Small;
			Unstable.MetacentricHeightCm = -20.0f;
			Unstable.CapsizeRollDeg = 0.0f;
			Unstable.RollPeriodS = 0.0f;
			Custom.SetDefinition(Unstable);
			TestTrue(TEXT("GM saneada"), Custom.GetDefinition().MetacentricHeightCm >= 1.0f);
			TestTrue(TEXT("vuelco saneado"), Custom.GetDefinition().CapsizeRollDeg >= 5.0f && Custom.GetDefinition().CapsizeRollDeg < 90.0f);
			TestTrue(TEXT("periodo saneado"), Custom.GetDefinition().RollPeriodS > 0.0f);
			TestEqual(TEXT("misma posición"), Custom.GetState().LocationCm.Y, 20.0, 1e-9);
			TestEqual(TEXT("mismo rumbo"), Custom.GetState().YawDeg, 45.0f, 1e-4f);
			const FBoatModel Loaded = FBoatModel::FromSaveData(Custom.ToSaveData(), &Small);
			TestEqual(TEXT("la ficha propia se restaura"), Loaded.GetDefinition().MaxCargoKg, 40.0f);
		});
	});

	Describe("Navegación nocturna", [this]()
	{
		It("mide el error de rumbo frente a la estrella con el signo de la caída", [this]()
		{
			TestEqual(TEXT("Norte = +X"), FBoatNavigation::BearingDeg(FVector2D::ZeroVector, FVector2D(100.0, 0.0)), 0.0f, 1e-3f);
			TestEqual(TEXT("Este = +Y"), FBoatNavigation::BearingDeg(FVector2D::ZeroVector, FVector2D(0.0, 100.0)), 90.0f, 1e-3f);
			TestEqual(TEXT("Oeste"), FBoatNavigation::BearingDeg(FVector2D::ZeroVector, FVector2D(0.0, -100.0)), 270.0f, 1e-3f);
			TestEqual(TEXT("Caer 20° a estribor"), FBoatNavigation::HeadingErrorDeg(350.0f, 10.0f), 20.0f, 1e-3f);
			TestEqual(TEXT("Caer 20° a babor"), FBoatNavigation::HeadingErrorDeg(10.0f, 350.0f), -20.0f, 1e-3f);
			TestEqual(TEXT("Rumbo opuesto"), FMath::Abs(FBoatNavigation::HeadingErrorDeg(0.0f, 180.0f)), 180.0f, 1e-3f);
		});

		It("separa el error de proa del rumbo real con corriente de través", [this]()
		{
			FStarPath Path;
			Path.OriginCm = FVector2D::ZeroVector;
			Path.DestinationCm = FVector2D(100000.0, 0.0);
			Path.StarBearingDeg = 0.0f;

			FRun Run(EBoatType::Canoe);
			Run.bAlternate = true;
			Run.Env.CurrentCmS = FVector2D(0.0, 80.0);
			Run.Advance(30.0f);
			const FNightNavigationReading Reading = FBoatNavigation::Evaluate(Run.Model.GetState(), Path, 10.0f);
			TestTrue(TEXT("La proa apunta a la estrella"), FMath::Abs(Reading.HeadingErrorDeg) < 10.0f);
			TestTrue(TEXT("Pero la corriente la saca a estribor"), Reading.CourseErrorDeg < -10.0f && Reading.CrossTrackCm > 0.0f);
			TestFalse(TEXT("Así que no va en rumbo"), Reading.bOnCourse);
			TestTrue(TEXT("Queda camino"), Reading.DistanceToGoCm < 100000.0f);
		});

		It("gobernando por la estrella se llega a la isla", [this]()
		{
			FStarPath Path;
			Path.OriginCm = FVector2D::ZeroVector;
			Path.DestinationCm = FVector2D(30000.0, 30000.0);
			Path.StarBearingDeg = FBoatNavigation::BearingDeg(Path.OriginCm, Path.DestinationCm);

			// Balancín a vela con viento del sur-oeste y la proa inicialmente al norte.
			FRun Run(EBoatType::Outrigger);
			Run.Model.SetSailRaised(true);
			Run.Env.WindCmS = FBoatWind::VelocityCmS(0.25f, 315.0f);
			float Closest = TNumericLimits<float>::Max();
			for (int32 I = 0; I < 1200; ++I)
			{
				const FNightNavigationReading Reading = FBoatNavigation::Evaluate(Run.Model.GetState(), Path);
				Closest = FMath::Min(Closest, Reading.DistanceToGoCm);
				Run.Controls.Rudder = FMath::Clamp(Reading.HeadingErrorDeg / 20.0f, -1.0f, 1.0f);
				Run.Advance(0.1f);
			}
			TestTrue(FString::Printf(TEXT("Llega a menos de 40 m (%.0f cm)"), Closest), Closest < 4000.0f);
		});
	});

	Describe("Viento", [this]()
	{
		It("traduce la fuerza del tiempo a m/s y sopla desde el alisio de la estación", [this]()
		{
			TestTrue(TEXT("Despejado ~6,5 m/s"), FMath::IsNearlyEqual(FBoatWind::SpeedMS(0.2f), 6.5f, 0.01f));
			TestTrue(TEXT("Galerna > 20 m/s"), FBoatWind::SpeedMS(0.85f) > 20.0f);
			const FVector2D FromNorth = FBoatWind::VelocityCmS(0.2f, 0.0f);
			TestTrue(TEXT("Viento del norte sopla hacia el sur (-X)"), FromNorth.X < -600.0 && FMath::Abs(FromNorth.Y) < 1.0);

			// Seca (días 0–8): alisio del ESE; monzón (días 16–24): del oeste.
			for (float Day = 0.5f; Day < 7.0f; Day += 0.37f)
			{
				const float From = FBoatWind::PrevailingFromDeg(Day);
				TestTrue(TEXT("Seca: del este-sudeste"), From > 80.0f && From < 140.0f);
			}
			for (float Day = 16.5f; Day < 23.0f; Day += 0.37f)
			{
				const float From = FBoatWind::PrevailingFromDeg(Day);
				TestTrue(TEXT("Monzón: del oeste"), From > 250.0f && From < 330.0f);
			}
			TestEqual(TEXT("Determinista"), FBoatWind::PrevailingFromDeg(3.3f), FBoatWind::PrevailingFromDeg(3.3f));
		});
	});

	Describe("Guardado y determinismo", [this]()
	{
		It("guarda y restaura el barco en datos planos", [this]()
		{
			FBoatModel Model(EBoatType::Outrigger, FVector(1200.0, -300.0, -8.0), 75.0f);
			Model.SetCrewAboard(true);
			Model.TryAddCargo(80.0f);
			Model.ApplyDamage(0.2f);
			Model.SetSailRaised(true);
			const FBoatSaveData Data = Model.ToSaveData();
			const FBoatModel Restored = FBoatModel::FromSaveData(Data);
			TestTrue(TEXT("Tipo"), Restored.GetState().Type == EBoatType::Outrigger);
			TestEqual(TEXT("Posición"), Restored.GetState().LocationCm, FVector(1200.0, -300.0, -8.0), 1e-3f);
			TestEqual(TEXT("Rumbo"), Restored.GetState().YawDeg, 75.0f, 1e-3f);
			TestEqual(TEXT("Carga"), Restored.GetState().CargoKg, 80.0f, 1e-3f);
			TestEqual(TEXT("Daño"), Restored.GetState().HullDamage01, 0.2f, 1e-4f);
			TestTrue(TEXT("Vela izada"), Restored.GetState().bSailRaised);

			FBoatSaveData Broken = Data;
			Broken.CargoKg = 99999.0f;
			Broken.HullDamage01 = 3.0f;
			const FBoatModel Clamped = FBoatModel::FromSaveData(Broken);
			TestTrue(TEXT("Carga recortada"), Clamped.GetState().CargoKg <= Clamped.GetDefinition().MaxCargoKg);
			TestTrue(TEXT("Daño total = naufragio"), Clamped.GetState().Condition == EBoatCondition::Wrecked);
		});

		It("un guardado con NaN o infinitos vuelve a valores por defecto", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			const float Inf = std::numeric_limits<float>::infinity();
			FBoatModel Model(EBoatType::Canoe, FVector(100.0, 200.0, 0.0), 30.0f);
			TestTrue(TEXT("amarra"), Model.Moor(FVector2D(100.0, 300.0), 300.0f));
			FBoatSaveData Data = Model.ToSaveData();
			Data.LocationCm.X = static_cast<double>(NaN);
			Data.YawDeg = Inf;
			Data.HullDamage01 = NaN;
			Data.CargoKg = NaN;
			Data.WaterInHullKg = NaN;
			Data.MooringLengthCm = Inf;
			const FBoatModel Loaded = FBoatModel::FromSaveData(Data);
			const FBoatState& S = Loaded.GetState();
			TestTrue(TEXT("posición finita"), FMath::IsFinite(S.LocationCm.X) && FMath::IsFinite(S.LocationCm.Y));
			TestTrue(TEXT("rumbo finito"), FMath::IsFinite(S.YawDeg));
			TestEqual(TEXT("daño NaN: sin daño"), S.HullDamage01, 0.0f);
			TestTrue(TEXT("y no naufraga"), S.Condition != EBoatCondition::Wrecked);
			TestEqual(TEXT("sin carga"), S.CargoKg, 0.0f);
			TestEqual(TEXT("sin agua"), S.WaterInHullKg, 0.0f);
			TestFalse(TEXT("cabo infinito: sin amarre"), Loaded.IsMoored());

			Data = Model.ToSaveData();
			Data.MooringAnchorCm.Y = static_cast<double>(NaN);
			TestFalse(TEXT("poste NaN: sin amarre"), FBoatModel::FromSaveData(Data).IsMoored());

			FBoatModel Launched(EBoatType::Raft, FVector::ZeroVector, 0.0f);
			Launched.SetVelocityCmS(FVector2D(static_cast<double>(NaN), 10.0));
			TestTrue(TEXT("velocidad NaN: quieto"), Launched.GetState().VelocityCmS.IsZero());
		});

		It("da exactamente el mismo resultado con las mismas entradas", [this]()
		{
			const FOceanWaves Waves = FOceanWaves::Make(0.6f);
			auto Simulate = [&Waves]()
			{
				FRun Run(EBoatType::Outrigger, FVector(321.0, 654.0, 0.0), 110.0f);
				Run.Model.SetSailRaised(true);
				Run.Env.Waves = &Waves;
				Run.Env.WindCmS = FBoatWind::VelocityCmS(0.5f, FBoatWind::PrevailingFromDeg(2.0f));
				Run.Env.CurrentCmS = FVector2D(40.0, 10.0);
				Run.Env.DepthBelowSeaLevelCm = &ReefDepth;
				Run.bAlternate = true;
				Run.Controls.Rudder = -0.3f;
				Run.Advance(45.0f);
				return Run.Model.GetState();
			};
			const FBoatState A = Simulate();
			const FBoatState B = Simulate();
			TestTrue(TEXT("Posición idéntica"), A.LocationCm == B.LocationCm);
			TestTrue(TEXT("Velocidad idéntica"), A.VelocityCmS == B.VelocityCmS);
			TestTrue(TEXT("Actitud idéntica"), A.YawDeg == B.YawDeg && A.PitchDeg == B.PitchDeg && A.RollDeg == B.RollDeg);
			TestTrue(TEXT("Estado idéntico"), A.Condition == B.Condition && A.HullDamage01 == B.HullDamage01
				&& A.WaterInHullKg == B.WaterInHullKg);
		});

		It("ignora pasos de tiempo nulos, negativos o no finitos sin estropear el estado", [this]()
		{
			FRun Run(EBoatType::Canoe);
			Run.bAlternate = true;
			Run.Advance(2.0f);
			const FBoatState Before = Run.Model.GetState();
			for (const float Bad : { 0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
			{
				Run.Model.Step(Bad, Run.Controls, Run.Env);
			}
			const FBoatState& After = Run.Model.GetState();
			TestTrue(TEXT("no se ha movido"), After.LocationCm == Before.LocationCm && After.VelocityCmS == Before.VelocityCmS);
			TestTrue(TEXT("acumulador intacto"), After.PendingTimeS == Before.PendingTimeS);
			Run.Advance(2.0f);
			const FBoatState& Later = Run.Model.GetState();
			TestTrue(TEXT("y sigue navegando con valores finitos"), FMath::IsFinite(Later.PendingTimeS)
				&& FMath::IsFinite(Later.LocationCm.X) && FMath::IsFinite(Later.LocationCm.Y) && Later.LocationCm != Before.LocationCm);
		});
	});
}

#endif
