#include "Misc/AutomationTest.h"

#include "Player/SwimModel.h"
#include "Survival/SurvivalModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SwimSpecDetail
{
	// Superficie del agua en Z = 0 y cápsula de 90 cm de semialtura: la cabeza
	// (HeadHeightCm = 70) queda a 160 cm sobre los pies.
	constexpr float HalfHeight = 90.0f;

	/** Entradas con Coverage cm de agua sobre los pies. */
	FSwimInputs AtCoverage(float Coverage, bool bDiveHeld = false)
	{
		FSwimInputs In;
		In.bHasWater = true;
		In.WaterZ = 0.0f;
		In.HalfHeight = HalfHeight;
		In.CenterZ = HalfHeight - Coverage;
		In.bDiveHeld = bDiveHeld;
		return In;
	}

	/** Entradas con la cabeza HeadDepth cm bajo la superficie (negativo: asoma). */
	FSwimInputs AtHeadDepth(const FSwimTuning& Tuning, float HeadDepth, bool bDiveHeld = false)
	{
		return AtCoverage(HeadDepth + HalfHeight + Tuning.HeadHeightCm, bDiveHeld);
	}

	/** Oxígeno gastado en Seconds segundos en apnea quieto, a Fps fotogramas por segundo. */
	float DrainOverSeconds(int32 Fps, float Seconds)
	{
		const FSwimTuning Tuning;
		FSwimModel Model;
		const FSwimInputs In = AtHeadDepth(Tuning, 300.0f);
		const float Dt = 1.0f / static_cast<float>(Fps);
		const int32 Frames = FMath::RoundToInt(Seconds * static_cast<float>(Fps));
		for (int32 I = 0; I < Frames; ++I)
		{
			Model.Tick(Tuning, In, Dt);
		}
		return 100.0f - Model.GetOxygen();
	}
}

BEGIN_DEFINE_SPEC(FSwimSpec, "Explored.Swim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FSwimSpec)

void FSwimSpec::Define()
{
	using namespace SwimSpecDetail;

	Describe("Estados respecto al agua", [this]()
	{
		It("pasa de tierra a vadear y a nadar según la cobertura, con una sola zambullida", [this]()
		{
			const FSwimTuning Tuning;
			FSwimModel Model;

			FSwimStep Step = Model.Tick(Tuning, AtCoverage(0.0f), 1.0f / 60.0f);
			TestTrue(TEXT("En seco"), Model.GetState() == ESwimState::OnLand);
			TestFalse(TEXT("Sin zambullida en seco"), Step.bEnteredWater);

			Step = Model.Tick(Tuning, AtCoverage(60.0f), 1.0f / 60.0f);
			TestTrue(TEXT("Agua a la cintura: vadea"), Model.GetState() == ESwimState::Wading);
			TestFalse(TEXT("Vadear no es nadar"), Step.bSwimming);

			Step = Model.Tick(Tuning, AtCoverage(Tuning.SwimDepthCm), 1.0f / 60.0f);
			TestTrue(TEXT("En el umbral nada"), Model.GetState() == ESwimState::Swimming);
			TestTrue(TEXT("Zambullida al entrar"), Step.bEnteredWater);

			Step = Model.Tick(Tuning, AtCoverage(Tuning.SwimDepthCm + 20.0f), 1.0f / 60.0f);
			TestFalse(TEXT("La zambullida no se repite"), Step.bEnteredWater);
		});

		It("no parpadea al oscilar la cobertura alrededor del umbral de nado", [this]()
		{
			const FSwimTuning Tuning;
			FSwimModel Model;
			int32 Entries = 0;
			int32 Exits = 0;
			for (int32 I = 0; I < 600; ++I)
			{
				// Punto de equilibrio de la flotación (≈ SwimDepthCm) con el vaivén de las olas.
				const float Coverage = Tuning.SwimDepthCm + 12.0f * FMath::Sin(I * 0.37f);
				const FSwimStep Step = Model.Tick(Tuning, AtCoverage(Coverage), 1.0f / 60.0f);
				Entries += Step.bEnteredWater ? 1 : 0;
				Exits += Step.bLeftWater ? 1 : 0;
				if (I > 0 && !TestTrue(TEXT("Sigue nadando"), Model.GetState() == ESwimState::Swimming))
				{
					break;
				}
			}
			TestEqual(TEXT("Una sola entrada al agua"), Entries, 1);
			TestEqual(TEXT("Ninguna salida"), Exits, 0);
		});

		It("sale del agua solo por debajo del umbral de salida y no vuelve a entrar al oscilar allí", [this]()
		{
			const FSwimTuning Tuning;
			FSwimModel Model;
			Model.Tick(Tuning, AtCoverage(Tuning.SwimDepthCm + 10.0f), 1.0f / 60.0f);
			const float ExitDepth = Tuning.SwimDepthCm - Tuning.SwimExitHysteresisCm;

			FSwimStep Step = Model.Tick(Tuning, AtCoverage(ExitDepth + 5.0f), 1.0f / 60.0f);
			TestTrue(TEXT("Sobre el umbral de salida sigue nadando"), Model.GetState() == ESwimState::Swimming);

			Step = Model.Tick(Tuning, AtCoverage(ExitDepth - 5.0f), 1.0f / 60.0f);
			TestTrue(TEXT("Bajo el umbral de salida hace pie"), Model.GetState() == ESwimState::Wading);
			TestTrue(TEXT("Evento de salida"), Step.bLeftWater);

			int32 Entries = 0;
			for (int32 I = 0; I < 300; ++I)
			{
				const float Coverage = ExitDepth + 8.0f * FMath::Sin(I * 0.41f);
				Entries += Model.Tick(Tuning, AtCoverage(Coverage), 1.0f / 60.0f).bEnteredWater ? 1 : 0;
			}
			TestEqual(TEXT("No vuelve a zambullirse"), Entries, 0);
			TestTrue(TEXT("Sigue vadeando"), Model.GetState() == ESwimState::Wading);
		});

		It("sin océano está en tierra y avisa si estaba nadando", [this]()
		{
			const FSwimTuning Tuning;
			FSwimModel Model;
			Model.Tick(Tuning, AtCoverage(200.0f), 1.0f / 60.0f);
			FSwimInputs NoWater = AtCoverage(200.0f);
			NoWater.bHasWater = false;
			const FSwimStep Step = Model.Tick(Tuning, NoWater, 1.0f / 60.0f);
			TestTrue(TEXT("En tierra"), Model.GetState() == ESwimState::OnLand);
			TestTrue(TEXT("Sale del agua"), Step.bLeftWater);
			TestFalse(TEXT("Cabeza fuera"), Model.IsHeadUnderwater());
		});
	});

	Describe("Apnea", [this]()
	{
		It("consume oxígeno con la cabeza bajo el agua aunque se suelte la tecla de bucear", [this]()
		{
			const FSwimTuning Tuning;
			FSwimModel Model;
			// A 3 m de profundidad con la tecla suelta (el exploit de H4).
			const FSwimInputs Deep = AtHeadDepth(Tuning, 300.0f, false);
			float Previous = Model.GetOxygen();
			for (int32 I = 0; I < 120; ++I)
			{
				Model.Tick(Tuning, Deep, 1.0f / 60.0f);
				if (!TestTrue(TEXT("El oxígeno nunca sube bajo el agua"), Model.GetOxygen() <= Previous))
				{
					break;
				}
				Previous = Model.GetOxygen();
			}
			TestTrue(TEXT("Bucea sin la tecla"), Model.GetState() == ESwimState::Diving);
			TestTrue(TEXT("Cabeza sumergida"), Model.IsHeadUnderwater());
			TestTrue(TEXT("Ha gastado oxígeno"), Model.GetOxygen() < 99.0f);

			const float Before = Model.GetOxygen();
			Model.Tick(Tuning, AtHeadDepth(Tuning, 300.0f, true), 1.0f / 60.0f);
			TestTrue(TEXT("Con la tecla también gasta"), Model.GetOxygen() < Before);
		});

		It("solo recupera oxígeno con la cabeza fuera, aunque mantenga la tecla en la superficie", [this]()
		{
			const FSwimTuning Tuning;
			FSwimModel Model;
			Model.SetOxygen(50.0f);
			// Pulsando bucear en la superficie la cabeza aún no se ha mojado: respira.
			Model.Tick(Tuning, AtHeadDepth(Tuning, -30.0f, true), 1.0f / 60.0f);
			TestTrue(TEXT("Estado de buceo por la tecla"), Model.GetState() == ESwimState::Diving);
			TestTrue(TEXT("Respira con la cabeza fuera"), Model.GetOxygen() > 50.0f);

			for (int32 I = 0; I < 600; ++I)
			{
				Model.Tick(Tuning, AtHeadDepth(Tuning, -30.0f, false), 1.0f / 60.0f);
			}
			TestEqual(TEXT("Pulmones llenos en la superficie"), Model.GetOxygen(), 100.0f, 1e-3f);
		});

		It("no alterna apnea y respiración cuando el oleaje roza la cabeza", [this]()
		{
			const FSwimTuning Tuning;
			FSwimModel Model;
			Model.Tick(Tuning, AtHeadDepth(Tuning, 20.0f), 1.0f / 60.0f);
			TestTrue(TEXT("Sumergido"), Model.IsHeadUnderwater());
			int32 Gasps = 0;
			for (int32 I = 0; I < 300; ++I)
			{
				const float Depth = 0.8f * Tuning.HeadSurfaceHysteresisCm * FMath::Sin(I * 0.29f);
				Gasps += Model.Tick(Tuning, AtHeadDepth(Tuning, Depth), 1.0f / 60.0f).bGasp ? 1 : 0;
				if (!TestTrue(TEXT("Sigue sumergido"), Model.IsHeadUnderwater()))
				{
					break;
				}
			}
			TestEqual(TEXT("Sin jadeos"), Gasps, 0);

			Model.Tick(Tuning, AtHeadDepth(Tuning, -Tuning.HeadSurfaceHysteresisCm - 1.0f), 1.0f / 60.0f);
			TestFalse(TEXT("Asoma con margen"), Model.IsHeadUnderwater());
		});

		It("jadea una vez al sacar la cabeza apurado de aire", [this]()
		{
			const FSwimTuning Tuning;
			FSwimModel Model;
			Model.Tick(Tuning, AtHeadDepth(Tuning, 200.0f), 1.0f / 60.0f);
			Model.SetOxygen(Tuning.GaspThreshold - 10.0f);
			FSwimStep Step = Model.Tick(Tuning, AtHeadDepth(Tuning, -30.0f), 1.0f / 60.0f);
			TestTrue(TEXT("Jadeo al emerger"), Step.bGasp);
			Step = Model.Tick(Tuning, AtHeadDepth(Tuning, -30.0f), 1.0f / 60.0f);
			TestFalse(TEXT("Solo una vez"), Step.bGasp);
		});

		It("avisa del daño por ahogo con los pulmones vacíos", [this]()
		{
			const FSwimTuning Tuning;
			FSwimModel Model;
			Model.SetOxygen(0.01f);
			const FSwimStep Step = Model.Tick(Tuning, AtHeadDepth(Tuning, 200.0f), 0.1f);
			TestEqual(TEXT("Sin aire"), Model.GetOxygen(), 0.0f);
			TestEqual(TEXT("Daño por ahogo"), Step.DrowningDamagePerSecond, Tuning.DrowningDamagePerSecond);
		});

		It("gasta lo mismo en 1 s a 10 fps que a 1000 y 10000 fps", [this]()
		{
			const float Expected = FSurvivalModel::OxygenDrainPerSecond(0.0f, 1.0f, false);
			const float At10 = DrainOverSeconds(10, 1.0f);
			const float At1000 = DrainOverSeconds(1000, 1.0f);
			const float At10000 = DrainOverSeconds(10000, 1.0f);
			TestTrue(TEXT("Gasta algo"), Expected > 0.5f);
			TestEqual(TEXT("10 fps"), At10, Expected, 1e-3f);
			TestEqual(TEXT("1000 fps igual que 10 fps"), At1000, At10, 1e-3f);
			TestEqual(TEXT("10000 fps igual que 10 fps"), At10000, At10, 1e-3f);
		});
	});

	Describe("Movimiento", [this]()
	{
		It("aplica la corriente como velocidad, sin tocar la velocidad propia", [this]()
		{
			const FSwimTuning Tuning;
			const FVector2D Current(120.0, -40.0);
			for (const int32 Fps : { 10, 60, 1000 })
			{
				FSwimModel Model;
				FSwimInputs In = AtCoverage(Tuning.SwimDepthCm + 10.0f);
				In.CurrentCmPerSecond = Current;
				In.Velocity = FVector(30.0, 0.0, 0.0);
				FVector2D Drift = FVector2D::ZeroVector;
				for (int32 I = 0; I < Fps; ++I)
				{
					const FSwimStep Step = Model.Tick(Tuning, In, 1.0f / static_cast<float>(Fps));
					Drift += Step.CurrentOffset;
					TestEqual(TEXT("La corriente no se suma a la velocidad X"), Step.Velocity.X, 30.0, 1e-9);
					TestEqual(TEXT("La corriente no se suma a la velocidad Y"), Step.Velocity.Y, 0.0, 1e-9);
				}
				// En 1 s arrastra exactamente la corriente en cm/s, sin importar el frenado ni los FPS.
				TestTrue(TEXT("Arrastre de 1 s"), Drift.Equals(Current, 1e-2));
			}

			FSwimModel OnLand;
			FSwimInputs Dry = AtCoverage(0.0f);
			Dry.CurrentCmPerSecond = Current;
			TestTrue(TEXT("En tierra no arrastra"), OnLand.Tick(Tuning, Dry, 0.5f).CurrentOffset.IsNearlyZero());
		});

		It("sube despacio por flotación al soltar la tecla en profundidad y vuelve a flotar al asomar", [this]()
		{
			const FSwimTuning Tuning;
			FSwimModel Model;
			FVector Velocity = FVector::ZeroVector;

			// Bajando con la tecla.
			for (int32 I = 0; I < 120; ++I)
			{
				FSwimInputs In = AtHeadDepth(Tuning, 300.0f, true);
				In.Velocity = Velocity;
				Velocity = Model.Tick(Tuning, In, 1.0f / 60.0f).Velocity;
			}
			TestTrue(TEXT("Baja con la tecla"), Velocity.Z < -0.5f * Tuning.DiveSpeed * 0.6f);

			// Suelta la tecla a 3 m: la rama de flotación lenta.
			for (int32 I = 0; I < 300; ++I)
			{
				FSwimInputs In = AtHeadDepth(Tuning, 300.0f, false);
				In.Velocity = Velocity;
				Velocity = Model.Tick(Tuning, In, 1.0f / 60.0f).Velocity;
			}
			TestTrue(TEXT("Sigue buceando sin la tecla"), Model.GetState() == ESwimState::Diving);
			TestEqual(TEXT("Sube a la velocidad de flotación"), static_cast<float>(Velocity.Z), Tuning.BuoyancyRiseSpeedCm, 0.5f);

			FSwimInputs Surfaced = AtHeadDepth(Tuning, -Tuning.HeadSurfaceHysteresisCm - 5.0f, false);
			Surfaced.Velocity = Velocity;
			Model.Tick(Tuning, Surfaced, 1.0f / 60.0f);
			TestTrue(TEXT("Al asomar la cabeza vuelve a nadar en superficie"), Model.GetState() == ESwimState::Swimming);
		});

		It("carga pesada reduce la velocidad máxima de nado", [this]()
		{
			const FSwimTuning Tuning;
			FSwimModel Light;
			FSwimModel Heavy;
			FSwimInputs In = AtCoverage(Tuning.SwimDepthCm + 10.0f);
			const float LightSpeed = Light.Tick(Tuning, In, 1.0f / 60.0f).MaxSpeed;
			In.CarriedWeightKg = Tuning.ComfortableWeightKg * 1.5f;
			const float HeavySpeed = Heavy.Tick(Tuning, In, 1.0f / 60.0f).MaxSpeed;
			TestEqual(TEXT("Sin carga, velocidad de nado"), LightSpeed, Tuning.SwimSpeed);
			TestTrue(TEXT("Cargado, más lento"), HeavySpeed < LightSpeed * 0.6f);
		});
	});
}

#endif
