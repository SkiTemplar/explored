#include "Misc/AutomationTest.h"

#include "Boats/RaftModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace RaftTest
{
	constexpr double RaftLastHour = 42.0;    // D2 18:00
	constexpr double ForcedReturnHour = 42.75; // D2 18:45

	FRaftSiteState Built()
	{
		FRaftSiteState Site;
		Site.LogsPlaced = FRaftTuning::DefaultLogsNeeded;
		Site.bBuilt = true;
		return Site;
	}
}

BEGIN_DEFINE_SPEC(FRaftSpec, "Explored.Raft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FRaftSpec)

void FRaftSpec::Define()
{
	using namespace RaftTest;

	Describe(TEXT("sitio de la balsa"), [this]()
	{
		It(TEXT("se dejan 4 troncos de uno en uno y luego se ata"), [this]()
		{
			FRaftSiteState Site;
			for (int32 Log = 1; Log <= 4; ++Log)
			{
				TestEqual(TEXT("Antes de 4 no se ata"), FRaftModel::CanTie(Site), ERaftSiteResult::MissingLogs);
				FRaftSiteState Next;
				TestEqual(TEXT("Tronco"), FRaftModel::PlaceLog(Site, true, Next), ERaftSiteResult::Ok);
				TestEqual(TEXT("Cuenta"), Next.LogsPlaced, Log);
				Site = Next;
			}
			FRaftSiteState Fifth;
			TestEqual(TEXT("Quinto no cabe"), FRaftModel::PlaceLog(Site, true, Fifth), ERaftSiteResult::SiteFull);
			FRaftSiteState Tied;
			TestEqual(TEXT("Atar"), FRaftModel::Tie(Site, Tied), ERaftSiteResult::Ok);
			TestTrue(TEXT("Hecha"), Tied.bBuilt);
			FRaftSiteState Again;
			TestEqual(TEXT("No se ata dos veces"), FRaftModel::Tie(Tied, Again), ERaftSiteResult::AlreadyBuilt);
		});

		It(TEXT("sin tronco en las manos no se deja nada"), [this]()
		{
			FRaftSiteState Out;
			TestEqual(TEXT("Sin tronco"), FRaftModel::PlaceLog(FRaftSiteState(), false, Out), ERaftSiteResult::NoLog);
			TestEqual(TEXT("Sin cambio"), Out.LogsPlaced, 0);
		});

		It(TEXT("un estado guardado incoherente no vale"), [this]()
		{
			FRaftSiteState Bad;
			Bad.bBuilt = true;
			Bad.LogsPlaced = 2;
			FRaftSiteState Negative;
			Negative.LogsPlaced = -1;
			TestFalse(TEXT("Hecha con 2 troncos"), FRaftModel::IsValid(Bad));
			TestFalse(TEXT("Negativo"), FRaftModel::IsValid(Negative));
			TestTrue(TEXT("Hecha"), FRaftModel::IsValid(Built()));
		});
	});

	Describe(TEXT("embarcar y el mar cerrado"), [this]()
	{
		It(TEXT("con remo y antes de las 18:00 del día 2 se embarca"), [this]()
		{
			TestEqual(TEXT("Ok"), FRaftModel::CanBoard(Built(), true, 30.0, RaftLastHour), ERaftBoard::Ok);
			TestEqual(TEXT("Sin remo"), FRaftModel::CanBoard(Built(), false, 30.0, RaftLastHour), ERaftBoard::NoPaddle);
			TestEqual(TEXT("Sin balsa"), FRaftModel::CanBoard(FRaftSiteState(), true, 30.0, RaftLastHour), ERaftBoard::NotBuilt);
			TestEqual(TEXT("18:00 cerrado"), FRaftModel::CanBoard(Built(), true, RaftLastHour, RaftLastHour), ERaftBoard::SeaClosed);
			TestEqual(TEXT("Día 3 cerrado"), FRaftModel::CanBoard(Built(), true, 55.0, RaftLastHour), ERaftBoard::SeaClosed);
		});

		It(TEXT("a las 18:45 en el islote vuelve a Playa Norte con sed y hambre -20"), [this]()
		{
			TestFalse(TEXT("18:44 aún no"), FRaftModel::ShouldForceReturn(ForcedReturnHour - 0.01, ForcedReturnHour, true));
			TestTrue(TEXT("18:45 sí"), FRaftModel::ShouldForceReturn(ForcedReturnHour, ForcedReturnHour, true));
			TestFalse(TEXT("En la isla no"), FRaftModel::ShouldForceReturn(50.0, ForcedReturnHour, false));
			FBodyBars Bars;
			Bars.Thirst = 15.0f;
			const FBodyBars After = FRaftModel::ForcedReturnBars(Bars);
			TestEqual(TEXT("Sed con suelo 0"), After.Thirst, 0.0f);
			TestEqual(TEXT("Hambre -20"), After.Hunger, Bars.Hunger - 20.0f);
			TestEqual(TEXT("Energía igual"), After.Energy, Bars.Energy);
		});
	});

	Describe(TEXT("remar"), [this]()
	{
		It(TEXT("W lleva a 2 m/s y 150 m se cruzan en unos 75 s"), [this]()
		{
			FRaftMotion Motion;
			FRaftControls Controls;
			Controls.Forward = 1.0f;
			double Seconds = 0.0;
			while (Motion.PositionM.X < 150.0 && Seconds < 200.0)
			{
				Motion = FRaftModel::Step(Motion, Controls, FVector2D::ZeroVector, 1.0f / 30.0f);
				Seconds += 1.0 / 30.0;
			}
			TestEqual(TEXT("Velocidad"), Motion.SpeedMps, 2.0f, 1e-3f);
			TestTrue(TEXT("75-78 s"), Seconds >= 75.0 && Seconds <= 78.0);
			TestEqual(TEXT("Recto"), Motion.PositionM.Y, 0.0, 1e-6);
		});

		It(TEXT("A/D giran, sin remo ni avanza ni gira, y la corriente empuja a la izquierda"), [this]()
		{
			FRaftControls Turn;
			Turn.Turn = 1.0f;
			const FRaftMotion Right = FRaftModel::Step(FRaftMotion(), Turn, FVector2D::ZeroVector, 1.0f);
			TestEqual(TEXT("40° a la derecha"), Right.YawDeg, 40.0f, 1e-4f);
			FRaftControls NoPaddle;
			NoPaddle.Forward = 1.0f;
			NoPaddle.Turn = 1.0f;
			NoPaddle.bHasPaddle = false;
			const FRaftMotion Still = FRaftModel::Step(FRaftMotion(), NoPaddle, FVector2D::ZeroVector, 1.0f);
			TestEqual(TEXT("Sin rumbo"), Still.YawDeg, 0.0f);
			TestEqual(TEXT("Sin velocidad"), Still.SpeedMps, 0.0f);
			const FVector2D Current = FRaftModel::LateralCurrent(0.0f);
			TestEqual(TEXT("Izquierda de rumbo norte = oeste"), Current.Y, -0.3, 1e-6);
			const FRaftMotion Drift = FRaftModel::Step(FRaftMotion(), FRaftControls(), Current, 10.0f);
			TestEqual(TEXT("Deriva 3 m"), Drift.PositionM.Y, -3.0, 1e-4);
		});

		It(TEXT("encallada no se mueve y un paso no válido no cambia nada"), [this]()
		{
			FRaftControls Grounded;
			Grounded.Forward = 1.0f;
			Grounded.bGrounded = true;
			FRaftMotion Moving;
			Moving.SpeedMps = 2.0f;
			const FRaftMotion Beached = FRaftModel::Step(Moving, Grounded, FVector2D(0.3, 0.0), 1.0f);
			TestEqual(TEXT("Parada"), Beached.SpeedMps, 0.0f);
			TestTrue(TEXT("Quieta"), Beached.PositionM == Moving.PositionM);
			const FRaftMotion Bad = FRaftModel::Step(Moving, FRaftControls(), FVector2D::ZeroVector, std::numeric_limits<float>::quiet_NaN());
			TestEqual(TEXT("NaN"), Bad.SpeedMps, Moving.SpeedMps);
		});

		It(TEXT("el cabeceo está amortiguado y nunca pasa de 4°"), [this]()
		{
			const FVector2D Steep = FRaftModel::StableTilt(10.0f, -10.0f);
			TestEqual(TEXT("Tope"), Steep.X, 4.0, 1e-6);
			TestEqual(TEXT("Tope negativo"), Steep.Y, -4.0, 1e-6);
			const FVector2D Gentle = FRaftModel::StableTilt(0.05f, 0.0f);
			TestTrue(TEXT("Mitad del ángulo"), Gentle.X > 1.0 && Gentle.X < 2.0);
			TestEqual(TEXT("NaN plano"), FRaftModel::StableTilt(std::numeric_limits<float>::quiet_NaN(), 0.0f).X, 0.0);
		});
	});
}

#endif
