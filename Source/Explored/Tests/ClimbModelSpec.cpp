#include "Misc/AutomationTest.h"

#include "Player/ClimbModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace ClimbSpecDetail
{
	const float Nan = std::numeric_limits<float>::quiet_NaN();
	const float Inf = std::numeric_limits<float>::infinity();

	FClimbRoute Palm(float TopM)
	{
		FClimbRoute R;
		R.Surface = EClimbSurface::Palm;
		R.TopHeightM = TopM;
		return R;
	}

	FClimbRoute Rock(float TopM, float SlopeDeg = 80.0f)
	{
		FClimbRoute R;
		R.Surface = EClimbSurface::Rock;
		R.TopHeightM = TopM;
		R.SlopeDeg = SlopeDeg;
		return R;
	}

	FClimbInput Up(float Energy = 100.0f)
	{
		FClimbInput In;
		In.Vertical = 1.0f;
		In.Energy = Energy;
		return In;
	}

	FClimbInput Down(float Energy = 100.0f)
	{
		FClimbInput In;
		In.Vertical = -1.0f;
		In.Energy = Energy;
		return In;
	}

	FClimbInput Hold(float Energy = 100.0f)
	{
		FClimbInput In;
		In.Vertical = 0.0f;
		In.Energy = Energy;
		return In;
	}

	/** Sube a la copa (o al tope), se suelta y aterriza en tierra: el resultado de la caída. */
	FFallResult FallFromTop(const FClimbRoute& Route)
	{
		FClimbModel Model;
		Model.Start(Route, FClimberInfo(), 100.0f);
		Model.Tick(Up(), 60.0f);
		Model.Release();
		return Model.Land(ELandingSurface::Ground);
	}
}

BEGIN_DEFINE_SPEC(FClimbModelSpec, "Explored.Climb",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FClimbModelSpec)

void FClimbModelSpec::Define()
{
	using namespace ClimbSpecDetail;

	Describe("Transiciones", [this]()
	{
		It("empieza agarrado en la base", [this]()
		{
			FClimbModel Model;
			TestEqual(TEXT("Aceptada"), Model.Start(Palm(8.0f), FClimberInfo(), 100.0f), EClimbReject::None);
			TestEqual(TEXT("Agarrado"), Model.GetState(), EClimbState::Grabbing);
			TestEqual(TEXT("En la base"), Model.GetHeightM(), 0.0f);
		});

		It("rechaza las órdenes ilegales desde el suelo", [this]()
		{
			FClimbModel Model;
			TestEqual(TEXT("Descansar"), Model.Rest(), EClimbReject::IllegalTransition);
			TestEqual(TEXT("Reanudar"), Model.Resume(), EClimbReject::IllegalTransition);
			TestEqual(TEXT("Soltarse"), Model.Release(), EClimbReject::IllegalTransition);
			TestEqual(TEXT("Clavar"), Model.DrivePiton(), EClimbReject::IllegalTransition);
			const FFallResult Fall = Model.Land(ELandingSurface::Rock);
			TestEqual(TEXT("Aterrizar sin caer no hace daño"), Fall.Damage, 0.0f);
			TestEqual(TEXT("Sigue en el suelo"), Model.GetState(), EClimbState::None);
			TestFalse(TEXT("El tick no hace nada"), Model.Tick(Up(), 1.0f).bStartedFalling);
			TestEqual(TEXT("Sin Energía gastada"), Model.Tick(Up(), 1.0f).EnergyDelta, 0.0f);
		});

		It("no se vuelve a empezar ya agarrado ni se reanuda sin descansar", [this]()
		{
			FClimbModel Model;
			Model.Start(Palm(8.0f), FClimberInfo(), 100.0f);
			TestEqual(TEXT("Empezar otra vez"), Model.Start(Palm(8.0f), FClimberInfo(), 100.0f), EClimbReject::IllegalTransition);
			TestEqual(TEXT("Reanudar agarrado"), Model.Resume(), EClimbReject::IllegalTransition);
			TestEqual(TEXT("Sigue agarrado"), Model.GetState(), EClimbState::Grabbing);
		});

		It("solo descansa en un anclaje", [this]()
		{
			FClimbModel Model;
			Model.Start(Palm(8.0f), FClimberInfo(), 100.0f);
			Model.Tick(Up(), 2.0f);
			TestEqual(TEXT("A media palmera"), Model.Rest(), EClimbReject::NoAnchor);
			TestEqual(TEXT("Sigue trepando"), Model.GetState(), EClimbState::Climbing);
		});

		It("cayendo no se agarra, ni descansa, ni clava, ni se mueve", [this]()
		{
			FClimbModel Model;
			Model.Start(Rock(10.0f), FClimberInfo(), 100.0f);
			Model.Tick(Up(), 2.0f);
			TestEqual(TEXT("Se suelta"), Model.Release(), EClimbReject::None);
			const float Height = Model.GetFallHeightM();
			TestEqual(TEXT("Empezar"), Model.Start(Rock(10.0f), FClimberInfo(), 100.0f), EClimbReject::IllegalTransition);
			TestEqual(TEXT("Descansar"), Model.Rest(), EClimbReject::IllegalTransition);
			TestEqual(TEXT("Reanudar"), Model.Resume(), EClimbReject::IllegalTransition);
			TestEqual(TEXT("Soltarse otra vez"), Model.Release(), EClimbReject::IllegalTransition);
			TestEqual(TEXT("Clavar"), Model.DrivePiton(), EClimbReject::IllegalTransition);
			const FClimbStep Step = Model.Tick(Up(), 1.0f);
			TestEqual(TEXT("Sin gasto"), Step.EnergyDelta, 0.0f);
			TestEqual(TEXT("Cayendo"), Model.GetState(), EClimbState::Falling);
			TestEqual(TEXT("Altura de caída intacta"), Model.GetFallHeightM(), Height);
		});

		It("no clava mientras sube ni en una palmera", [this]()
		{
			FClimbModel Rocky;
			Rocky.Start(Rock(10.0f), FClimberInfo(), 100.0f);
			Rocky.Tick(Up(), 1.0f);
			TestEqual(TEXT("Trepando"), Rocky.DrivePiton(), EClimbReject::IllegalTransition);

			FClimbModel Palmy;
			Palmy.Start(Palm(8.0f), FClimberInfo(), 100.0f);
			TestEqual(TEXT("Palmera"), Palmy.DrivePiton(), EClimbReject::NotRock);
		});

		It("un rechazo al empezar deja el modelo en el suelo", [this]()
		{
			FClimberInfo Sledge;
			Sledge.bHasSledge = true;
			FClimbModel Model;
			TestEqual(TEXT("Angarillas"), Model.Start(Palm(8.0f), Sledge, 100.0f), EClimbReject::CarryingSledge);
			TestEqual(TEXT("Aviso de angarillas"), FClimbModel::NoticeFor(EClimbReject::CarryingSledge), EClimbNotice::CarryingSledge);
			TestEqual(TEXT("Sin Energía"), Model.Start(Palm(8.0f), FClimberInfo(), 0.0f), EClimbReject::NoEnergy);
			TestEqual(TEXT("Aviso de agotado"), FClimbModel::NoticeFor(EClimbReject::NoEnergy), EClimbNotice::Exhausted);
			TestEqual(TEXT("Transición ilegal, en silencio"), FClimbModel::NoticeFor(EClimbReject::IllegalTransition), EClimbNotice::None);
			TestEqual(TEXT("En el suelo"), Model.GetState(), EClimbState::None);
		});

		It("rechaza rutas corruptas", [this]()
		{
			FClimbModel Model;
			TestEqual(TEXT("Copa NaN"), Model.Start(Palm(Nan), FClimberInfo(), 100.0f), EClimbReject::InvalidRoute);
			TestEqual(TEXT("Copa infinita"), Model.Start(Palm(Inf), FClimberInfo(), 100.0f), EClimbReject::InvalidRoute);
			TestEqual(TEXT("Copa a cero"), Model.Start(Palm(0.0f), FClimberInfo(), 100.0f), EClimbReject::InvalidRoute);
			TestEqual(TEXT("Pendiente NaN"), Model.Start(Rock(5.0f, Nan), FClimberInfo(), 100.0f), EClimbReject::InvalidRoute);
			FClimbRoute Rope = Rock(5.0f);
			Rope.FixedRopeTopM = -1.0f;
			TestEqual(TEXT("Cuerda negativa"), Model.Start(Rope, FClimberInfo(), 100.0f), EClimbReject::InvalidRoute);
			TestEqual(TEXT("Energía NaN"), Model.Start(Palm(8.0f), FClimberInfo(), Nan), EClimbReject::NoEnergy);
			TestEqual(TEXT("En el suelo"), Model.GetState(), EClimbState::None);
		});
	});

	Describe("Palmeras", [this]()
	{
		It("solo se trepa a la especie Palm", [this]()
		{
			TestTrue(TEXT("Palm"), FClimbModel::IsClimbableSpecies(FName(TEXT("Palm"))));
			TestFalse(TEXT("JungleGiant"), FClimbModel::IsClimbableSpecies(FName(TEXT("JungleGiant"))));
		});

		It("sube a 0,7 m/s y gasta 9 × peso por segundo a pulso", [this]()
		{
			FClimbModel Model;
			Model.Start(Palm(10.0f), FClimberInfo(), 100.0f);
			const FClimbStep Step = Model.Tick(Up(), 1.0f);
			TestEqual(TEXT("Altura"), Step.HeightM, 0.7f, 1e-5f);
			TestEqual(TEXT("Energía"), Step.EnergyDelta, -9.0f, 1e-4f);
			TestEqual(TEXT("Trepando"), Model.GetState(), EClimbState::Climbing);
		});

		It("con pie de palmera sube a 1,3 m/s y gasta 6 × peso", [this]()
		{
			FClimberInfo Who;
			Who.bHasPalmFoot = true;
			FClimbModel Model;
			Model.Start(Palm(10.0f), Who, 100.0f);
			const FClimbStep Step = Model.Tick(Up(), 1.0f);
			TestEqual(TEXT("Altura"), Step.HeightM, 1.3f, 1e-5f);
			TestEqual(TEXT("Energía"), Step.EnergyDelta, -6.0f, 1e-4f);
		});

		It("el pie de palmera no ayuda en la roca", [this]()
		{
			FClimberInfo Who;
			Who.bHasPalmFoot = true;
			TestEqual(TEXT("Mismo gasto que a pulso"), FClimbModel::ClimbDrainPerSecond(EClimbSurface::Rock, Who, FClimbTuning()), 9.0f);
		});

		It("la carga encarece la trepa con el mismo factor que esprintar", [this]()
		{
			FClimberInfo Heavy;
			Heavy.CarriedWeightRatio = 1.0f;
			TestEqual(TEXT("× 1,6"), FClimbModel::ClimbDrainPerSecond(EClimbSurface::Palm, Heavy, FClimbTuning()), 9.0f * 1.6f, 1e-4f);
			Heavy.CarriedWeightRatio = Nan;
			TestEqual(TEXT("Peso NaN cuenta como nada"), FClimbModel::ClimbDrainPerSecond(EClimbSurface::Palm, Heavy, FClimbTuning()), 9.0f);
		});

		It("agarrado y quieto gasta un tercio", [this]()
		{
			FClimbModel Model;
			Model.Start(Palm(10.0f), FClimberInfo(), 100.0f);
			const FClimbStep Step = Model.Tick(Hold(), 1.0f);
			TestEqual(TEXT("Energía"), Step.EnergyDelta, -3.0f, 1e-4f);
			TestEqual(TEXT("Agarrado"), Model.GetState(), EClimbState::Grabbing);
			TestEqual(TEXT("No se mueve"), Step.HeightM, 0.0f);
		});

		It("en la copa descansa, avisa una vez y recupera Energía", [this]()
		{
			FClimbModel Model;
			Model.Start(Palm(7.0f), FClimberInfo(), 100.0f);
			const FClimbStep Top = Model.Tick(Up(), 30.0f);
			TestTrue(TEXT("Llega a la copa"), Top.bReachedCrown);
			TestEqual(TEXT("A la altura de la copa"), Top.HeightM, 7.0f);
			TestEqual(TEXT("Solo paga los 10 s de subida"), Top.EnergyDelta, -90.0f, 1e-3f);
			TestEqual(TEXT("Descansando"), Model.GetState(), EClimbState::Resting);
			TestTrue(TEXT("Recupera"), Model.Tick(Hold(), 1.0f).EnergyDelta > 0.0f);

			TestEqual(TEXT("Reanuda"), Model.Resume(), EClimbReject::None);
			Model.Tick(Down(), 1.0f);
			const FClimbStep Again = Model.Tick(Up(), 5.0f);
			TestFalse(TEXT("La copa cuenta una vez por trepada"), Again.bReachedCrown);
		});

		It("baja hasta el suelo y suelta el tronco", [this]()
		{
			FClimbModel Model;
			Model.Start(Palm(10.0f), FClimberInfo(), 100.0f);
			Model.Tick(Up(), 2.0f);
			const FClimbStep Step = Model.Tick(Down(), 10.0f);
			TestTrue(TEXT("En la base"), Step.bReachedBase);
			TestEqual(TEXT("Altura cero"), Step.HeightM, 0.0f);
			TestEqual(TEXT("De pie"), Model.GetState(), EClimbState::None);
		});
	});

	Describe("Energía a mitad de la trepa", [this]()
	{
		It("se agota a mitad de un tick: sube lo que le da y cae desde ahí", [this]()
		{
			FClimbModel Model;
			Model.Start(Palm(12.0f), FClimberInfo(), 100.0f);
			const FClimbStep Step = Model.Tick(Up(4.5f), 1.0f);
			TestTrue(TEXT("Empieza a caer"), Step.bStartedFalling);
			TestEqual(TEXT("Aviso"), Step.Notice, EClimbNotice::Exhausted);
			TestEqual(TEXT("Gasta justo lo que tenía"), Step.EnergyDelta, -4.5f, 1e-5f);
			TestEqual(TEXT("Medio segundo de subida"), Step.HeightM, 0.35f, 1e-5f);
			TestEqual(TEXT("Cayendo"), Model.GetState(), EClimbState::Falling);
			TestEqual(TEXT("Cae desde 0,35 m"), Model.GetFallHeightM(), 0.35f, 1e-5f);
			TestEqual(TEXT("Una caída tan corta no duele"), Model.Land(ELandingSurface::Ground).Damage, 0.0f);
			TestEqual(TEXT("En el suelo"), Model.GetState(), EClimbState::None);
		});

		It("la altura recorrida cuenta como altura de caída", [this]()
		{
			FClimbModel Model;
			Model.Start(Palm(12.0f), FClimberInfo(), 100.0f);
			// 50 de Energía a 9/s: 5,56 s a 0,7 m/s = 3,89 m.
			Model.Tick(Up(50.0f), 20.0f);
			TestEqual(TEXT("Altura de caída"), Model.GetFallHeightM(), 50.0f / 9.0f * 0.7f, 1e-4f);
			const FFallResult Fall = Model.Land(ELandingSurface::Ground);
			TestTrue(TEXT("Duele"), Fall.Damage > 0.0f);
			TestEqual(TEXT("Sin esguince por debajo de 4,5 m"), Fall.SprainHours, 0.0f);
		});

		It("da igual el paso de simulación: 1 tick o 600 llegan al mismo sitio", [this]()
		{
			FClimbModel Coarse;
			FClimbModel Fine;
			Coarse.Start(Palm(12.0f), FClimberInfo(), 100.0f);
			Fine.Start(Palm(12.0f), FClimberInfo(), 100.0f);
			Coarse.Tick(Up(50.0f), 10.0f);
			float Energy = 50.0f;
			for (int32 I = 0; I < 600 && Fine.GetState() != EClimbState::Falling; ++I)
			{
				Energy += Fine.Tick(Up(Energy), 1.0f / 60.0f).EnergyDelta;
			}
			TestEqual(TEXT("Los dos caen"), Fine.GetState(), EClimbState::Falling);
			TestEqual(TEXT("Misma altura de caída"), Fine.GetFallHeightM(), Coarse.GetFallHeightM(), 1e-3f);
			TestTrue(TEXT("Energía a cero"), FMath::Abs(Energy) < 1e-3f);
		});

		It("agarrado y quieto también se agota", [this]()
		{
			FClimbModel Model;
			Model.Start(Palm(12.0f), FClimberInfo(), 100.0f);
			Model.Tick(Up(), 1.0f);
			const FClimbStep Step = Model.Tick(Hold(1.5f), 1.0f);
			TestTrue(TEXT("Cae"), Step.bStartedFalling);
			TestEqual(TEXT("Gasta lo que quedaba"), Step.EnergyDelta, -1.5f, 1e-5f);
			TestEqual(TEXT("Desde donde estaba"), Model.GetFallHeightM(), 0.7f, 1e-5f);
		});

		It("con la Energía a cero o corrupta se suelta en el acto", [this]()
		{
			FClimbModel Zero;
			Zero.Start(Palm(12.0f), FClimberInfo(), 100.0f);
			Zero.Tick(Up(), 1.0f);
			TestTrue(TEXT("Cero"), Zero.Tick(Up(0.0f), 1.0f).bStartedFalling);
			TestEqual(TEXT("No sube más"), Zero.GetFallHeightM(), 0.7f, 1e-5f);

			FClimbModel Corrupt;
			Corrupt.Start(Palm(12.0f), FClimberInfo(), 100.0f);
			TestTrue(TEXT("NaN"), Corrupt.Tick(Up(Nan), 1.0f).bStartedFalling);
		});

		It("descansar en la copa no se agota nunca", [this]()
		{
			FClimbModel Model;
			Model.Start(Palm(3.0f), FClimberInfo(), 100.0f);
			Model.Tick(Up(), 10.0f);
			const FClimbStep Step = Model.Tick(Hold(0.0f), 60.0f);
			TestFalse(TEXT("No cae"), Step.bStartedFalling);
			TestEqual(TEXT("Descansando"), Model.GetState(), EClimbState::Resting);
		});
	});

	Describe("Roca: pendiente", [this]()
	{
		It("exactamente 60° no se escala; por poco más, sí", [this]()
		{
			const FClimbTuning Tuning;
			TestFalse(TEXT("60°"), FClimbModel::IsClimbableSlope(60.0f, Tuning));
			TestFalse(TEXT("59,9°"), FClimbModel::IsClimbableSlope(59.9f, Tuning));
			TestTrue(TEXT("60,01°"), FClimbModel::IsClimbableSlope(60.01f, Tuning));
			TestTrue(TEXT("90°"), FClimbModel::IsClimbableSlope(90.0f, Tuning));
			TestFalse(TEXT("NaN"), FClimbModel::IsClimbableSlope(Nan, Tuning));

			FClimbModel Model;
			TestEqual(TEXT("Empezar a 60°"), Model.Start(Rock(5.0f, 60.0f), FClimberInfo(), 100.0f), EClimbReject::SlopeTooGentle);
			TestEqual(TEXT("Sin aviso"), FClimbModel::NoticeFor(EClimbReject::SlopeTooGentle), EClimbNotice::None);
			TestEqual(TEXT("Empezar a 60,01°"), Model.Start(Rock(5.0f, 60.01f), FClimberInfo(), 100.0f), EClimbReject::None);
		});

		It("la palmera no mira la pendiente", [this]()
		{
			FClimbRoute Route = Palm(8.0f);
			Route.SlopeDeg = 0.0f;
			FClimbModel Model;
			TestEqual(TEXT("Aceptada"), Model.Start(Route, FClimberInfo(), 100.0f), EClimbReject::None);
		});

		It("avisa al apuntar una clavija a una pared de 60° o menos", [this]()
		{
			const FClimbTuning Tuning;
			TestEqual(TEXT("60°"), FClimbModel::PitonNotice(60.0f, Tuning), EClimbNotice::NoPitonSpot);
			TestEqual(TEXT("75°"), FClimbModel::PitonNotice(75.0f, Tuning), EClimbNotice::None);
		});
	});

	Describe("Roca: tope de 3 m", [this]()
	{
		It("sin clavijas se para a 3 m, sin aviso", [this]()
		{
			FClimbModel Model;
			Model.Start(Rock(10.0f), FClimberInfo(), 100.0f);
			const FClimbStep Step = Model.Tick(Up(), 10.0f);
			TestEqual(TEXT("A 3 m"), Step.HeightM, 3.0f);
			TestTrue(TEXT("En el tope"), Step.bAtReachLimit);
			TestEqual(TEXT("Sin aviso"), Step.Notice, EClimbNotice::None);
			TestEqual(TEXT("Agarrado"), Model.GetState(), EClimbState::Grabbing);
			TestFalse(TEXT("No corona"), Step.bToppedOut);
		});

		It("una pared de menos de 3 m se corona", [this]()
		{
			FClimbModel Model;
			Model.Start(Rock(2.5f), FClimberInfo(), 100.0f);
			const FClimbStep Step = Model.Tick(Up(), 10.0f);
			TestTrue(TEXT("Corona"), Step.bToppedOut);
			TestEqual(TEXT("De pie arriba"), Model.GetState(), EClimbState::None);
		});

		It("cada clavija alcanzable abre otros 3 m", [this]()
		{
			const FClimbTuning Tuning;
			FClimbRoute Route = Rock(20.0f);
			TestEqual(TEXT("Sin clavijas"), FClimbModel::ReachM(Route, Tuning), 3.0f);
			Route.PitonHeightsM = {9.5f, 2.5f, 5.0f};
			// 3 → 2,5 + 3 = 5,5 → 5 + 3 = 8; la de 9,5 queda fuera de alcance.
			TestEqual(TEXT("Encadenadas"), FClimbModel::ReachM(Route, Tuning), 8.0f);
			Route.PitonHeightsM = {Nan, -2.0f, 3.0f};
			TestEqual(TEXT("Corruptas ignoradas"), FClimbModel::ReachM(Route, Tuning), 6.0f);
			Route.TopHeightM = 4.0f;
			TestEqual(TEXT("Nunca más que la cima"), FClimbModel::ReachM(Route, Tuning), 4.0f);
		});

		It("clavar en el tope deja seguir subiendo", [this]()
		{
			FClimbModel Model;
			Model.Start(Rock(10.0f), FClimberInfo(), 100.0f);
			Model.Tick(Up(), 10.0f);
			TestEqual(TEXT("Clava"), Model.DrivePiton(), EClimbReject::None);
			TestEqual(TEXT("Una clavija"), Model.GetRoute().PitonHeightsM.Num(), 1);
			TestEqual(TEXT("Clavar dos veces en el mismo sitio no gasta otra"), Model.DrivePiton(), EClimbReject::None);
			TestEqual(TEXT("Sigue una"), Model.GetRoute().PitonHeightsM.Num(), 1);
			TestEqual(TEXT("Descansa en la clavija"), Model.Rest(), EClimbReject::None);
			TestEqual(TEXT("Reanuda"), Model.Resume(), EClimbReject::None);
			const FClimbStep Step = Model.Tick(Up(), 10.0f);
			TestEqual(TEXT("A 6 m"), Step.HeightM, 6.0f, 1e-5f);
		});

		It("la cuerda fija sube a 1,3 m/s sin gastar Energía y amplía el tope", [this]()
		{
			FClimbRoute Route = Rock(12.0f);
			Route.FixedRopeTopM = 4.0f;
			TestEqual(TEXT("Tope"), FClimbModel::ReachM(Route, FClimbTuning()), 7.0f);
			FClimbModel Model;
			Model.Start(Route, FClimberInfo(), 100.0f);
			const FClimbStep Step = Model.Tick(Up(), 1.0f);
			TestEqual(TEXT("1,3 m"), Step.HeightM, 1.3f, 1e-5f);
			TestEqual(TEXT("Gratis"), Step.EnergyDelta, 0.0f);
			TestEqual(TEXT("Quieto en la cuerda, gratis"), Model.Tick(Hold(), 5.0f).EnergyDelta, 0.0f);
			TestEqual(TEXT("Descansa colgado de la cuerda"), Model.Rest(), EClimbReject::None);
			Model.Resume();
			const FClimbStep ToRopeTop = Model.Tick(Up(), 10.0f);
			TestEqual(TEXT("Se para al final de la cuerda"), ToRopeTop.HeightM, 4.0f, 1e-5f);
			TestEqual(TEXT("Sigue gratis"), ToRopeTop.EnergyDelta, 0.0f);
			const FClimbStep Free = Model.Tick(Up(), 10.0f);
			TestEqual(TEXT("3 m más por encima de la cuerda"), Free.HeightM, 7.0f, 1e-5f);
			TestTrue(TEXT("Por encima ya cuesta"), Free.EnergyDelta < 0.0f);
		});

		It("sin Energía en la cuerda fija no se cae", [this]()
		{
			FClimbRoute Route = Rock(12.0f);
			Route.FixedRopeTopM = 4.0f;
			FClimbModel Model;
			Model.Start(Route, FClimberInfo(), 100.0f);
			const FClimbStep Step = Model.Tick(Up(0.0f), 1.0f);
			TestFalse(TEXT("No cae"), Step.bStartedFalling);
			TestEqual(TEXT("Sube"), Step.HeightM, 1.3f, 1e-5f);
		});
	});

	Describe("Caídas", [this]()
	{
		It("desde 2,99 m no hace daño", [this]()
		{
			const FFallResult Fall = FallFromTop(Palm(2.99f));
			TestEqual(TEXT("Sin daño"), Fall.Damage, 0.0f);
			TestEqual(TEXT("Sin esguince"), Fall.SprainHours, 0.0f);
		});

		It("desde 3 m, el tope sin clavijas, tampoco", [this]()
		{
			FClimbModel Model;
			Model.Start(Rock(10.0f), FClimberInfo(), 100.0f);
			Model.Tick(Up(), 10.0f);
			TestEqual(TEXT("En el tope"), Model.GetHeightM(), 3.0f);
			Model.Release();
			const FFallResult Fall = Model.Land(ELandingSurface::Ground);
			TestEqual(TEXT("Sin daño"), Fall.Damage, 0.0f);
			TestEqual(TEXT("Sin esguince"), Fall.SprainHours, 0.0f);
		});

		It("desde 3,01 m ya duele un poco, sin esguince", [this]()
		{
			const FFallResult Fall = FallFromTop(Palm(3.01f));
			TestTrue(TEXT("Algo de daño"), Fall.Damage > 0.0f);
			TestTrue(TEXT("Poco"), Fall.Damage < 0.1f);
			TestEqual(TEXT("Sin esguince"), Fall.SprainHours, 0.0f);
		});

		It("el daño crece con la altura y a 4,5 m llega el esguince", [this]()
		{
			const float D301 = FallFromTop(Palm(3.01f)).Damage;
			const float D4 = FallFromTop(Palm(4.0f)).Damage;
			const FFallResult F45 = FallFromTop(Palm(4.5f));
			const float D8 = FallFromTop(Palm(8.0f)).Damage;
			TestTrue(TEXT("3,01 < 4"), D301 < D4);
			TestTrue(TEXT("4 < 4,5"), D4 < F45.Damage);
			TestTrue(TEXT("4,5 < 8"), F45.Damage < D8);
			TestTrue(TEXT("Esguince a 4,5 m"), F45.SprainHours > 0.0f);
		});

		It("lo que queda por debajo de la base suma a la caída", [this]()
		{
			FClimbModel Model;
			Model.Start(Rock(10.0f), FClimberInfo(), 100.0f);
			Model.Tick(Up(), 10.0f);
			Model.Release();
			TestTrue(TEXT("3 m + 2 m de ladera"), Model.Land(ELandingSurface::Ground, 2.0f).Damage > 0.0f);

			FClimbModel Corrupt;
			Corrupt.Start(Rock(10.0f), FClimberInfo(), 100.0f);
			Corrupt.Tick(Up(), 10.0f);
			Corrupt.Release();
			TestEqual(TEXT("Caída extra NaN ignorada"), Corrupt.Land(ELandingSurface::Ground, Nan).Damage, 0.0f);
		});

		It("caer al agua amortigua", [this]()
		{
			FClimbModel Model;
			Model.Start(Palm(8.0f), FClimberInfo(), 100.0f);
			Model.Tick(Up(), 30.0f);
			Model.Release();
			TestEqual(TEXT("8 m al agua"), Model.Land(ELandingSurface::Water).Damage, 0.0f);
		});
	});

	Describe("Red", [this]()
	{
		It("la instantánea va en centímetros", [this]()
		{
			FClimbModel Model;
			Model.Start(Palm(10.0f), FClimberInfo(), 100.0f);
			Model.Tick(Up(), 1.0f);
			const FClimbSnapshot Snap = Model.Snapshot();
			TestEqual(TEXT("Estado"), Snap.State, EClimbState::Climbing);
			TestEqual(TEXT("70 cm"), Snap.HeightCm, 70);
		});

		It("acepta hasta 8 cm de error y corrige a partir de ahí", [this]()
		{
			const FClimbTuning Tuning;
			FClimbSnapshot Server;
			Server.State = EClimbState::Climbing;
			Server.HeightCm = 200;
			FClimbSnapshot Client = Server;
			TestEqual(TEXT("Igual"), FClimbModel::ValidateClientMove(Server, Client, Tuning), EClimbValidation::Accept);
			Client.HeightCm = 208;
			TestEqual(TEXT("8 cm"), FClimbModel::ValidateClientMove(Server, Client, Tuning), EClimbValidation::Accept);
			Client.HeightCm = 191;
			TestEqual(TEXT("9 cm"), FClimbModel::ValidateClientMove(Server, Client, Tuning), EClimbValidation::Correct);
			Client = Server;
			Client.State = EClimbState::Grabbing;
			TestEqual(TEXT("Otro estado"), FClimbModel::ValidateClientMove(Server, Client, Tuning), EClimbValidation::Correct);
		});

		It("cliente y servidor con las mismas entradas coinciden paso a paso", [this]()
		{
			FClimbModel Client;
			FClimbModel Server;
			FClimbRoute Route = Rock(15.0f);
			Route.PitonHeightsM = {2.8f, 5.5f};
			Client.Start(Route, FClimberInfo(), 80.0f);
			Server.Start(Route, FClimberInfo(), 80.0f);
			float ClientEnergy = 80.0f;
			float ServerEnergy = 80.0f;
			bool bAllAccepted = true;
			for (int32 I = 0; I < 900; ++I)
			{
				FClimbInput In;
				In.Vertical = FMath::Sin(static_cast<float>(I) * 0.05f) + 0.3f;
				In.Energy = ClientEnergy;
				ClientEnergy += Client.Tick(In, 1.0f / 30.0f).EnergyDelta;
				In.Energy = ServerEnergy;
				ServerEnergy += Server.Tick(In, 1.0f / 30.0f).EnergyDelta;
				bAllAccepted &= FClimbModel::ValidateClientMove(Server.Snapshot(), Client.Snapshot(), Client.GetTuning()) == EClimbValidation::Accept;
				bAllAccepted &= Server.Snapshot() == Client.Snapshot();
			}
			TestTrue(TEXT("Sin correcciones"), bAllAccepted);
			TestEqual(TEXT("Misma Energía"), ClientEnergy, ServerEnergy);
		});

		It("si el cliente cree tener más Energía, el servidor lo corrige al caer", [this]()
		{
			FClimbModel Client;
			FClimbModel Server;
			Client.Start(Palm(12.0f), FClimberInfo(), 100.0f);
			Server.Start(Palm(12.0f), FClimberInfo(), 100.0f);
			Client.Tick(Up(100.0f), 1.0f);
			Server.Tick(Up(4.5f), 1.0f);
			TestEqual(TEXT("El servidor cae"), Server.GetState(), EClimbState::Falling);
			TestEqual(TEXT("Corrige"), FClimbModel::ValidateClientMove(Server.Snapshot(), Client.Snapshot(), Server.GetTuning()),
				EClimbValidation::Correct);
		});
	});

	Describe("Entradas corruptas", [this]()
	{
		It("un paso NaN, negativo o nulo no mueve ni gasta", [this]()
		{
			FClimbModel Model;
			Model.Start(Palm(10.0f), FClimberInfo(), 100.0f);
			for (const float Dt : {Nan, -1.0f, 0.0f, Inf})
			{
				const FClimbStep Step = Model.Tick(Up(), Dt);
				TestEqual(TEXT("Altura"), Step.HeightM, 0.0f);
				TestEqual(TEXT("Energía"), Step.EnergyDelta, 0.0f);
			}
			TestEqual(TEXT("Sigue agarrado"), Model.GetState(), EClimbState::Grabbing);
		});

		It("el eje vertical se recorta y NaN cuenta como quieto", [this]()
		{
			FClimbModel Fast;
			Fast.Start(Palm(10.0f), FClimberInfo(), 100.0f);
			FClimbInput In = Up();
			In.Vertical = 50.0f;
			TestEqual(TEXT("No sube más rápido"), Fast.Tick(In, 1.0f).HeightM, 0.7f, 1e-5f);

			FClimbModel Still;
			Still.Start(Palm(10.0f), FClimberInfo(), 100.0f);
			In.Vertical = Nan;
			const FClimbStep Step = Still.Tick(In, 1.0f);
			TestEqual(TEXT("Quieto"), Step.HeightM, 0.0f);
			TestEqual(TEXT("Gasto de agarre"), Step.EnergyDelta, -3.0f, 1e-4f);
		});

		It("las clavijas corruptas de la ruta se descartan al empezar", [this]()
		{
			FClimbRoute Route = Rock(6.0f);
			Route.PitonHeightsM = {Nan, -1.0f, 50.0f, 2.0f};
			FClimbModel Model;
			Model.Start(Route, FClimberInfo(), 100.0f);
			TestEqual(TEXT("Solo la buena"), Model.GetRoute().PitonHeightsM.Num(), 1);
		});
	});

	Describe("Avisos", [this]()
	{
		It("cada aviso tiene texto y None no", [this]()
		{
			TestTrue(TEXT("None vacío"), FClimbModel::NoticeText(EClimbNotice::None).IsEmpty());
			TestEqual(TEXT("Agotado"), FClimbModel::NoticeText(EClimbNotice::Exhausted).ToString(), TEXT("No llego más arriba así."));
			TestFalse(TEXT("Clavija"), FClimbModel::NoticeText(EClimbNotice::NoPitonSpot).IsEmpty());
			TestFalse(TEXT("Angarillas"), FClimbModel::NoticeText(EClimbNotice::CarryingSledge).IsEmpty());
		});
	});
}

#endif
