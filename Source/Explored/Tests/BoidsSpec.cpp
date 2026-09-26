#include "Misc/AutomationTest.h"

#include "HAL/PlatformTime.h"

#include "Core/ExploredRandom.h"
#include "Fauna/BoidsModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BoidsSpecDetail
{
	/** Parámetros sin objetivo ni crucero para aislar las tres reglas clásicas. */
	FBoidsParams PureRules()
	{
		FBoidsParams P;
		P.NeighborRadiusCm = 300.0f;
		P.SeparationRadiusCm = 60.0f;
		P.CruiseWeight = 0.0f;
		P.MaxSpeedCmS = 250.0f;
		P.MaxAccelCmS2 = 600.0f;
		P.VerticalAgility = 1.0f;
		return P;
	}

	void Scatter(FBoidsModel& Model, int32 Count, uint32 Seed, float RadiusCm, float SpeedCmS, const FVector& Center = FVector::ZeroVector)
	{
		FExploredRandom Random(Seed);
		for (int32 I = 0; I < Count; ++I)
		{
			const FVector P = Center + FVector(Random.RangeFloat(-1.0f, 1.0f), Random.RangeFloat(-1.0f, 1.0f), Random.RangeFloat(-0.5f, 0.5f)) * RadiusCm;
			const FVector V = FVector(Random.RangeFloat(-1.0f, 1.0f), Random.RangeFloat(-1.0f, 1.0f), Random.RangeFloat(-0.3f, 0.3f)).GetSafeNormal() * SpeedCmS;
			Model.AddAgent(P, V);
		}
	}

	float MinPairDistance(const FBoidsModel& Model)
	{
		double Best = 1.0e12;
		const TArray<FBoidAgent>& Agents = Model.GetAgents();
		for (int32 I = 0; I < Agents.Num(); ++I)
		{
			for (int32 J = I + 1; J < Agents.Num(); ++J)
			{
				Best = FMath::Min(Best, FVector::Distance(Agents[I].Position, Agents[J].Position));
			}
		}
		return static_cast<float>(Best);
	}

	/** Fondo en pendiente con una isla (tierra) en (8000, 0) de 2000 cm de radio. Centímetros. */
	double SeabedZ(double X, double Y)
	{
		if (FVector2D::Distance(FVector2D(X, Y), FVector2D(8000.0, 0.0)) < 2000.0)
		{
			return 300.0;
		}
		return -1500.0 + X * 0.1;
	}
}

BEGIN_DEFINE_SPEC(FBoidsSpec, "Explored.Fauna.Boids",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FBoidsSpec)

void FBoidsSpec::Define()
{
	using namespace BoidsSpecDetail;

	It("separa a dos agentes demasiado juntos", [this]()
	{
		FBoidsParams P = PureRules();
		P.CohesionWeight = 0.0f;
		P.AlignmentWeight = 0.0f;
		FBoidsModel Model(P);
		Model.AddAgent(FVector(0.0, 0.0, 0.0), FVector::ZeroVector);
		Model.AddAgent(FVector(20.0, 0.0, 0.0), FVector::ZeroVector);
		Model.Step(1.0f, FBoidsEnvironment());
		const double Distance = FVector::Distance(Model.GetAgents()[0].Position, Model.GetAgents()[1].Position);
		TestTrue(FString::Printf(TEXT("Se alejan (%.1f cm)"), Distance), Distance > 60.0);
	});

	It("separa también a dos agentes en el mismo punto sin producir NaN", [this]()
	{
		FBoidsModel Model(PureRules());
		Model.AddAgent(FVector(5.0, 5.0, 5.0), FVector::ZeroVector);
		Model.AddAgent(FVector(5.0, 5.0, 5.0), FVector::ZeroVector);
		Model.Step(0.5f, FBoidsEnvironment());
		const FVector A = Model.GetAgents()[0].Position;
		const FVector B = Model.GetAgents()[1].Position;
		TestFalse(TEXT("Sin NaN"), FMath::IsNaN(A.X) || FMath::IsNaN(B.X));
		TestTrue(TEXT("Separados"), FVector::Distance(A, B) > 10.0);
	});

	It("separa vecinos a ambos lados de una frontera de celda con coordenadas negativas", [this]()
	{
		FBoidsParams P = PureRules();
		P.CohesionWeight = 0.0f;
		P.AlignmentWeight = 0.0f;
		FBoidsModel Model(P);
		Model.AddAgent(FVector(-10.0, -300.0, -1.0), FVector::ZeroVector);
		Model.AddAgent(FVector(10.0, -300.0, 1.0), FVector::ZeroVector);
		Model.Step(0.5f, FBoidsEnvironment());
		TestTrue(TEXT("Se ven a través de la frontera"), FVector::Distance(Model.GetAgents()[0].Position, Model.GetAgents()[1].Position) > 20.5);
	});

	It("cohesiona un grupo disperso sin que colapse", [this]()
	{
		FBoidsModel Model(PureRules());
		Scatter(Model, 30, 7, 220.0f, 40.0f);
		const float Before = Model.Spread();
		for (int32 I = 0; I < 300; ++I)
		{
			Model.Step(1.0f / 30.0f, FBoidsEnvironment());
		}
		const float After = Model.Spread();
		TestTrue(FString::Printf(TEXT("Más compacto (%.0f → %.0f cm)"), Before, After), After < Before * 0.8f);
		TestTrue(FString::Printf(TEXT("Sin colapsar en un punto (%.1f cm)"), MinPairDistance(Model)), MinPairDistance(Model) > 60.0f * 0.25f);
	});

	It("alinea las direcciones del grupo", [this]()
	{
		FBoidsModel Model(PureRules());
		Scatter(Model, 40, 11, 150.0f, 150.0f);
		const float Before = Model.Polarization();
		for (int32 I = 0; I < 300; ++I)
		{
			Model.Step(1.0f / 30.0f, FBoidsEnvironment());
		}
		const float After = Model.Polarization();
		TestTrue(FString::Printf(TEXT("Polarización baja al principio (%.2f)"), Before), Before < 0.5f);
		TestTrue(FString::Printf(TEXT("Polarización alta al final (%.2f)"), After), After > 0.9f);
	});

	It("respeta la velocidad máxima y la mínima (vuelo) aun con fuerzas grandes", [this]()
	{
		FBoidsParams P = PureRules();
		P.MinSpeedCmS = 120.0f;
		P.MaxSpeedCmS = 300.0f;
		P.FleeWeight = 50.0f;
		FBoidsModel Model(P);
		Scatter(Model, 200, 3, 400.0f, 1000.0f);
		FBoidsEnvironment Env;
		Env.Threats.Add({FVector::ZeroVector, 800.0f, 1.0f});
		Env.bHasTarget = true;
		Env.Target = FVector(5000.0, 0.0, 0.0);
		double MaxSeen = 0.0;
		double MinSeen = 1.0e9;
		for (int32 Step = 0; Step < 120; ++Step)
		{
			Model.Step(1.0f / 30.0f, Env);
			for (const FBoidAgent& Agent : Model.GetAgents())
			{
				MaxSeen = FMath::Max(MaxSeen, Agent.Velocity.Size());
				MinSeen = FMath::Min(MinSeen, Agent.Velocity.Size());
			}
		}
		TestTrue(FString::Printf(TEXT("Máxima (%.3f)"), MaxSeen), MaxSeen <= 300.0 + 1.0e-6);
		TestTrue(FString::Printf(TEXT("Mínima (%.3f)"), MinSeen), MinSeen >= 120.0 - 1.0e-6);
	});

	It("huye de las amenazas y busca el objetivo", [this]()
	{
		FBoidsModel Fleeing(PureRules());
		Scatter(Fleeing, 20, 5, 100.0f, 10.0f, FVector(200.0, 0.0, 0.0));
		FBoidsEnvironment Threat;
		Threat.Threats.Add({FVector::ZeroVector, 1000.0f, 1.0f});
		const double Before = FVector::Distance(Fleeing.Centroid(), FVector::ZeroVector);
		for (int32 I = 0; I < 60; ++I)
		{
			Fleeing.Step(1.0f / 30.0f, Threat);
		}
		TestTrue(TEXT("Se alejan de la amenaza"), FVector::Distance(Fleeing.Centroid(), FVector::ZeroVector) > Before + 150.0);

		FBoidsModel Seeking(PureRules());
		Scatter(Seeking, 20, 6, 100.0f, 10.0f);
		FBoidsEnvironment Target;
		Target.bHasTarget = true;
		Target.Target = FVector(3000.0, 1000.0, 0.0);
		const double Start = FVector::Distance(Seeking.Centroid(), Target.Target);
		for (int32 I = 0; I < 600; ++I)
		{
			Seeking.Step(1.0f / 30.0f, Target);
		}
		TestTrue(TEXT("Se acercan al objetivo"), FVector::Distance(Seeking.Centroid(), Target.Target) < Start * 0.5);
	});

	It("rodea los obstáculos esféricos", [this]()
	{
		FBoidsParams P = PureRules();
		P.CruiseSpeedCmS = 200.0f;
		FBoidsModel Model(P);
		Model.AddAgent(FVector(-1000.0, 5.0, 0.0), FVector(200.0, 0.0, 0.0));
		FBoidsEnvironment Env;
		Env.Obstacles.Add({FVector::ZeroVector, 150.0f});
		Env.bHasTarget = true;
		Env.Target = FVector(2000.0, 0.0, 0.0);
		double Closest = 1.0e9;
		for (int32 I = 0; I < 450; ++I)
		{
			Model.Step(1.0f / 30.0f, Env);
			Closest = FMath::Min(Closest, Model.GetAgents()[0].Position.Size());
		}
		TestTrue(FString::Printf(TEXT("No atraviesa la roca (mínimo %.0f cm)"), Closest), Closest > 100.0);
		TestTrue(TEXT("Llega al otro lado"), Model.GetAgents()[0].Position.X > 500.0);
	});

	It("mantiene la franja vertical y no entra en tierra", [this]()
	{
		FBoidsParams P = PureRules();
		P.CruiseWeight = 0.5f;
		P.CruiseSpeedCmS = 200.0f;
		FBoidsModel Model(P);
		Scatter(Model, 60, 9, 300.0f, 200.0f, FVector(4000.0, 0.0, -600.0));
		double Time = 0.0;
		FBoidsEnvironment Env;
		Env.Band = [&Time](const FVector& Pos)
		{
			FBoidBand Band;
			Band.MinZ = SeabedZ(Pos.X, Pos.Y) + 40.0;
			Band.MaxZ = 30.0 * FMath::Sin(Pos.X * 0.002 + Time) - 40.0;
			return Band;
		};
		// Rumbo a la isla: la franja se estrecha hasta desaparecer.
		Env.bHasTarget = true;
		Env.Target = FVector(8000.0, 0.0, -100.0);
		int32 Violations = 0;
		for (int32 Step = 0; Step < 600; ++Step)
		{
			Time = Step / 30.0;
			Model.Step(1.0f / 30.0f, Env);
			for (const FBoidAgent& Agent : Model.GetAgents())
			{
				const FBoidBand Band = Env.Band(Agent.Position);
				if (!Band.IsValid(P.MinBandThicknessCm) || Agent.Position.Z < Band.MinZ - 1.0e-6 || Agent.Position.Z > Band.MaxZ + 1.0e-6)
				{
					++Violations;
				}
			}
		}
		TestEqual(TEXT("Siempre dentro del agua y fuera de la isla"), Violations, 0);
	});

	It("no sale de los límites horizontales", [this]()
	{
		FBoidsParams P = PureRules();
		P.bUseBounds = true;
		P.BoundsMin = FVector2D(-500.0, -500.0);
		P.BoundsMax = FVector2D(500.0, 500.0);
		P.CruiseWeight = 1.0f;
		P.CruiseSpeedCmS = 250.0f;
		FBoidsModel Model(P);
		Scatter(Model, 50, 13, 400.0f, 250.0f);
		bool bInside = true;
		for (int32 Step = 0; Step < 300; ++Step)
		{
			Model.Step(1.0f / 30.0f, FBoidsEnvironment());
			for (const FBoidAgent& Agent : Model.GetAgents())
			{
				bInside &= FMath::Abs(Agent.Position.X) <= 500.0 && FMath::Abs(Agent.Position.Y) <= 500.0;
			}
		}
		TestTrue(TEXT("Dentro de la caja"), bInside);
	});

	It("es determinista", [this]()
	{
		auto Run = []()
		{
			FBoidsModel Model(PureRules());
			Scatter(Model, 80, 21, 500.0f, 120.0f);
			FBoidsEnvironment Env;
			Env.Threats.Add({FVector(100.0, 0.0, 0.0), 400.0f, 1.0f});
			for (int32 I = 0; I < 200; ++I)
			{
				Model.Step(1.0f / 30.0f, Env);
			}
			return Model.GetAgents();
		};
		const TArray<FBoidAgent> A = Run();
		const TArray<FBoidAgent> B = Run();
		bool bSame = A.Num() == B.Num();
		for (int32 I = 0; bSame && I < A.Num(); ++I)
		{
			bSame = A[I].Position == B[I].Position && A[I].Velocity == B[I].Velocity && A[I].Id == B[I].Id;
		}
		TestTrue(TEXT("Mismo resultado bit a bit"), bSame);
	});

	It("mueve 300 agentes durante 600 pasos dentro del presupuesto", [this]()
	{
		FBoidsParams P = PureRules();
		P.CruiseWeight = 0.5f;
		FBoidsModel Model(P);
		Scatter(Model, 300, 99, 1500.0f, 150.0f);
		FBoidsEnvironment Env;
		Env.Band = [](const FVector& Pos)
		{
			FBoidBand Band;
			Band.MinZ = -3000.0;
			Band.MaxZ = -40.0;
			return Band;
		};
		Env.Threats.Add({FVector::ZeroVector, 500.0f, 1.0f});
		const double Start = FPlatformTime::Seconds();
		for (int32 I = 0; I < 600; ++I)
		{
			Model.Step(1.0f / 60.0f, Env);
		}
		const double Elapsed = FPlatformTime::Seconds() - Start;
		AddInfo(FString::Printf(TEXT("300 agentes × 600 pasos: %.3f s"), Elapsed));
		// Holgado para ASan/UBSan y el editor en Debug; en Release son unas decenas de milisegundos.
		TestTrue(FString::Printf(TEXT("Presupuesto (%.3f s)"), Elapsed), Elapsed < 3.0);
		TestEqual(TEXT("Siguen todos"), Model.Num(), 300);
	});

	It("quita agentes sin cambiar el Id de los demás", [this]()
	{
		FBoidsModel Model(PureRules());
		Model.AddAgent(FVector::ZeroVector, FVector::ZeroVector);
		Model.AddAgent(FVector(500.0, 0.0, 0.0), FVector::ZeroVector);
		Model.AddAgent(FVector(1000.0, 0.0, 0.0), FVector::ZeroVector);
		const uint32 LastId = Model.GetAgents()[2].Id;
		Model.RemoveAgent(1);
		TestEqual(TEXT("Quedan dos"), Model.Num(), 2);
		TestTrue(TEXT("Id estable"), Model.GetAgents()[1].Id == LastId);
	});
}

#endif
