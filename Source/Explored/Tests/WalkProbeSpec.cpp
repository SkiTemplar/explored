#include "Misc/AutomationTest.h"

#include "Debug/WalkProbeModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FWalkProbeSpec, "Explored.Debug.WalkProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FWalkProbeSpec)

void FWalkProbeSpec::Define()
{
	It("llega al final andando hacia cada punto de paso", [this]()
	{
		FWalkProbeModel Probe;
		Probe.Start({ FVector2D(10.0, 0.0), FVector2D(10.0, 10.0) });
		FVector2D Position(0.0, 0.0);
		const float Speed = 3.0f;
		const float Dt = 0.1f;
		int32 Steps = 0;
		while (Probe.Step(Position, Dt) == EWalkProbeStatus::Walking && Steps++ < 1000)
		{
			Position += Probe.DesiredDirection(Position) * Speed * Dt;
		}
		TestEqual(TEXT("llega"), Probe.GetStatus(), EWalkProbeStatus::Arrived);
		TestEqual(TEXT("dos puntos"), Probe.GetWaypointIndex(), 2);
		TestEqual(TEXT("unos 18 m"), Probe.GetTravelledMeters(), 18.0f, 2.0f);
		TestTrue(TEXT("dirección nula al llegar"), Probe.DesiredDirection(Position).IsZero());
	});

	It("se declara atascado si no avanza (caso negativo)", [this]()
	{
		FWalkProbeModel Probe;
		FWalkProbeConfig Config;
		Config.StuckSeconds = 2.0f;
		Probe.Start({ FVector2D(50.0, 0.0) }, Config);
		EWalkProbeStatus Status = EWalkProbeStatus::Walking;
		for (int32 I = 0; I < 50 && Status == EWalkProbeStatus::Walking; ++I)
		{
			Status = Probe.Step(FVector2D(0.1 * (I % 2), 0.0), 0.1f);
		}
		TestEqual(TEXT("atascado"), Status, EWalkProbeStatus::Stuck);
		TestEqual(TEXT("sigue en el primer punto"), Probe.GetWaypointIndex(), 0);
	});

	It("se rinde al agotar el tiempo aunque avance", [this]()
	{
		FWalkProbeModel Probe;
		FWalkProbeConfig Config;
		Config.TimeoutSeconds = 3.0f;
		Probe.Start({ FVector2D(1000.0, 0.0) }, Config);
		FVector2D Position(0.0, 0.0);
		EWalkProbeStatus Status = EWalkProbeStatus::Walking;
		for (int32 I = 0; I < 100 && Status == EWalkProbeStatus::Walking; ++I)
		{
			Position.X += 1.0;
			Status = Probe.Step(Position, 0.1f);
		}
		TestEqual(TEXT("tiempo agotado"), Status, EWalkProbeStatus::TimedOut);
		TestEqual(TEXT("texto"), FString(LexToString(Status)), FString(TEXT("timeout")));
	});

	It("da por recorrida una ruta vacía e ignora pasos no finitos", [this]()
	{
		FWalkProbeModel Probe;
		Probe.Start({});
		TestEqual(TEXT("vacía"), Probe.Step(FVector2D::ZeroVector, 0.1f), EWalkProbeStatus::Arrived);
		Probe.Start({ FVector2D(5.0, 0.0) });
		Probe.Step(FVector2D::ZeroVector, NAN);
		TestEqual(TEXT("sin tiempo"), Probe.GetElapsedSeconds(), 0.0f);
		TestEqual(TEXT("sigue andando"), Probe.GetStatus(), EWalkProbeStatus::Walking);
	});
}

#endif
