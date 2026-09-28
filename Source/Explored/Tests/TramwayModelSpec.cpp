#include "Misc/AutomationTest.h"

#include "Tramway/TramwayModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TramwaySpecDetail
{
	/** Vía recta hacia +X desde Start: N tramos que suben Rise escalones cada uno. Devuelve el último nodo. */
	FIntVector Line(FTramwayModel& Model, const FIntVector& Start, int32 Count, int32 Rise, ERailDir Dir = ERailDir::PosX)
	{
		FIntVector A = Start;
		for (int32 I = 0; I < Count; ++I)
		{
			const FIntVector B = A + FTramwayModel::DirOffset(Dir) + FIntVector(0, 0, Rise);
			verify(Model.Place(A, B) == ERailPlaceResult::Ok);
			A = B;
		}
		return A;
	}

	FCartControl Push(int32 Sign = 1)
	{
		FCartControl C;
		C.Propulsion = ECartPropulsion::Push;
		C.PushSign = Sign;
		return C;
	}

	FCartControl Winch()
	{
		FCartControl C;
		C.Propulsion = ECartPropulsion::Winch;
		return C;
	}

	/** Avanza hasta Seconds o hasta que Stop devuelva true. Devuelve el tiempo simulado. */
	double Run(const FTramwayModel& Model, FMineCart& Cart, const FCartControl& Control, double Seconds,
		TFunctionRef<bool(const FMineCart&, const FCartStepResult&)> Stop)
	{
		double Acc = 0.0;
		const double Dt = 1.0 / 60.0;
		double T = 0.0;
		while (T < Seconds)
		{
			const FCartStepResult R = Model.Step(Cart, Control, Dt, Acc);
			T += Dt;
			if (Stop(Cart, R))
			{
				break;
			}
		}
		return T;
	}

	double RunFor(const FTramwayModel& Model, FMineCart& Cart, const FCartControl& Control, double Seconds)
	{
		return Run(Model, Cart, Control, Seconds, [](const FMineCart&, const FCartStepResult&) { return false; });
	}

	/** Vía en L: 3 tramos hacia +X que bajan Drop escalones cada uno y 3 tramos llanos hacia +Y desde la esquina. */
	FIntVector LTrack(FTramwayModel& Model, int32 Drop)
	{
		const FIntVector Corner = Line(Model, FIntVector(0, 0, 0), 3, -Drop);
		Line(Model, Corner, 3, 0, ERailDir::PosY);
		return Corner;
	}
}

BEGIN_DEFINE_SPEC(FTramwayModelSpec, "Explored.Tramway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FTramwayModelSpec)

void FTramwayModelSpec::Define()
{
	using namespace TramwaySpecDetail;

	Describe("la vía", [this]()
	{
		It("tiende tramos de 2 m entre nodos vecinos y rechaza los que no encajan en la rejilla", [this]()
		{
			FTramwayModel Model;
			const FIntVector O(0, 0, 0);
			TestTrue(TEXT("mismo nodo"), Model.Place(O, O) == ERailPlaceResult::Degenerate);
			TestTrue(TEXT("diagonal"), Model.Place(O, FIntVector(1, 1, 0)) == ERailPlaceResult::NotAdjacent);
			TestTrue(TEXT("salto de dos celdas"), Model.Place(O, FIntVector(2, 0, 0)) == ERailPlaceResult::NotAdjacent);
			TestTrue(TEXT("6 escalones es demasiado"), Model.Place(O, FIntVector(1, 0, 6)) == ERailPlaceResult::TooSteep);
			TestTrue(TEXT("y bajando también"), Model.Place(O, FIntVector(0, 1, -6)) == ERailPlaceResult::TooSteep);
			TestTrue(TEXT("nada tendido"), Model.IsEmpty());
			TestTrue(TEXT("5 escalones sí"), Model.Place(O, FIntVector(1, 0, 5)) == ERailPlaceResult::Ok);
			TestTrue(TEXT("mismo tramo al revés"), Model.Place(FIntVector(1, 0, 5), O) == ERailPlaceResult::Occupied);
			TestTrue(TEXT("otra altura en la misma dirección"), Model.Place(O, FIntVector(1, 0, 0)) == ERailPlaceResult::Occupied);
			TestEqual(TEXT("un tramo"), Model.NumSegments(), 1);
			TestTrue(TEXT("existe en los dos sentidos"), Model.HasSegment(O, FIntVector(1, 0, 5)) && Model.HasSegment(FIntVector(1, 0, 5), O));
			TestEqual(TEXT("longitud con la subida"), Model.SegmentLength(O, FIntVector(1, 0, 5)), FMath::Sqrt(4.0 + 0.625 * 0.625), 1e-9);
			TestEqual(TEXT("pendiente máxima 17,4°"), Model.MaxSlopeDegrees(), 17.35f, 0.05f);
		});

		It("deduce la pieza por la forma del nodo: fin, recta, curva y cambio de agujas", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 2, 0);
			Line(Model, FIntVector(2, 0, 0), 1, 0, ERailDir::PosY);
			TestTrue(TEXT("fin"), Model.NodeKind(FIntVector(0, 0, 0)) == ERailNodeKind::End);
			TestTrue(TEXT("recta"), Model.NodeKind(FIntVector(1, 0, 0)) == ERailNodeKind::Straight);
			TestTrue(TEXT("curva"), Model.NodeKind(FIntVector(2, 0, 0)) == ERailNodeKind::Curve);
			TestTrue(TEXT("sin vía"), Model.NodeKind(FIntVector(9, 9, 0)) == ERailNodeKind::None);
			Line(Model, FIntVector(1, 0, 0), 1, 0, ERailDir::NegY);
			TestTrue(TEXT("cambio"), Model.NodeKind(FIntVector(1, 0, 0)) == ERailNodeKind::Switch);
		});

		It("quitar un tramo borra los nodos vacíos y la palanca que se queda sin cambio", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 2, 0);
			Line(Model, FIntVector(1, 0, 0), 1, 0, ERailDir::PosY);
			TestTrue(TEXT("palanca"), Model.SetSwitch(FIntVector(1, 0, 0), ERailDir::PosY));
			TestTrue(TEXT("quita la rama"), Model.Remove(FIntVector(1, 1, 0), FIntVector(1, 0, 0)));
			TestFalse(TEXT("no se quita dos veces"), Model.Remove(FIntVector(1, 1, 0), FIntVector(1, 0, 0)));
			TestTrue(TEXT("ya no es un cambio"), Model.GetSwitch(FIntVector(1, 0, 0)) == ERailDir::Count);
			TestFalse(TEXT("no se pone palanca en una recta"), Model.SetSwitch(FIntVector(1, 0, 0), ERailDir::PosX));
			Model.Remove(FIntVector(0, 0, 0), FIntVector(1, 0, 0));
			Model.Remove(FIntVector(1, 0, 0), FIntVector(2, 0, 0));
			TestTrue(TEXT("vacío del todo"), Model.IsEmpty());
		});

		It("ajusta a la rejilla también en coordenadas negativas", [this]()
		{
			FTramwayModel Model;
			TestTrue(TEXT("redondeo"), Model.SnapNode(FVector(-1.1, -2.9, -0.07)) == FIntVector(-1, -1, -1));
			TestTrue(TEXT("ida y vuelta"), Model.SnapNode(Model.NodePosition(FIntVector(-7, 3, -12))) == FIntVector(-7, 3, -12));
		});
	});

	Describe("el vagón empujado", [this]()
	{
		It("se empuja en llano hasta 1,2 m/s y no pasa de ahí", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 10, 0);
			FMineCart Cart;
			TestTrue(TEXT("puesto"), Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0)));
			double MaxV = 0.0;
			Run(Model, Cart, Push(), 4.0, [&MaxV](const FMineCart& C, const FCartStepResult&) { MaxV = FMath::Max(MaxV, C.V); return false; });
			const double Target = Model.GetSettings().PushSpeed;
			TestEqual(TEXT("velocidad de crucero"), Cart.V, Target);
			TestTrue(TEXT("nunca por encima"), MaxV <= Target);
		});

		It("la carga da inercia: el vagón lleno tarda más en arrancar y lo mismo en pararse rodando", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 20, 0);
			const auto TimeToSpeed = [&Model](float Load)
			{
				FMineCart Cart;
				Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0));
				Model.SetLoad(Cart, Load);
				return Run(Model, Cart, Push(), 10.0, [](const FMineCart& C, const FCartStepResult&) { return C.V >= 1.0; });
			};
			const double Empty = TimeToSpeed(0.0f);
			const double Full = TimeToSpeed(200.0f);
			TestTrue(*FString::Printf(TEXT("lleno %.2f s frente a vacío %.2f s"), Full, Empty), Full > 3.0 * Empty);
			// (F − μ·m·g)/m: 0,88 m/s² lleno → 1,14 s hasta 1 m/s.
			TestEqual(TEXT("aceleración del lleno"), Full, 1.0 / (280.0 / 260.0 - 0.02 * 9.81), 0.02);

			for (float Load : { 0.0f, 200.0f })
			{
				FMineCart Cart;
				Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0));
				Model.SetLoad(Cart, Load);
				Cart.V = 1.2;
				const FVector Start = Model.CartPosition(Cart);
				RunFor(Model, Cart, FCartControl(), 20.0);
				const double Coast = FVector::Dist(Start, Model.CartPosition(Cart));
				TestEqual(TEXT("parado"), Cart.V, 0.0);
				// v²/(2·μ·g) = 3,67 m, independiente de la masa.
				TestEqual(TEXT("distancia rodando hasta parar"), Coast, 1.44 / (2.0 * 0.02 * 9.81), 0.05);
			}
		});

		It("el freno lo para en mucho menos camino", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 5, 0);
			FMineCart Cart;
			Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0));
			Model.SetLoad(Cart, 150.0f);
			Cart.V = 1.2;
			FCartControl Brake;
			Brake.bBrake = true;
			RunFor(Model, Cart, Brake, 3.0);
			TestEqual(TEXT("parado"), Cart.V, 0.0);
			TestEqual(TEXT("v²/(2·0,3·g)"), Cart.S, 1.44 / (2.0 * 0.3 * 9.81), 0.02);
		});

		It("lleno sube a mano una rampa de un escalón (3,6°) pero no una de dos (7,1°); vacío sube las dos", [this]()
		{
			struct FCase { int32 Rise; float Load; bool bClimbs; };
			const FCase Cases[] = { { 1, 200.0f, true }, { 2, 200.0f, false }, { 2, 0.0f, true }, { 5, 0.0f, true } };
			for (const FCase& Case : Cases)
			{
				FTramwayModel Model;
				const FIntVector Top = Line(Model, FIntVector(0, 0, 0), 3, Case.Rise);
				FMineCart Cart;
				Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, Case.Rise));
				Model.SetLoad(Cart, Case.Load);
				RunFor(Model, Cart, Push(), 15.0);
				const bool bAtTop = Cart.To == Top && Cart.S >= Model.SegmentLength(Cart.From, Cart.To) - 1e-9;
				const FString What = FString::Printf(TEXT("%d escalones con %.0f kg"), Case.Rise, Case.Load);
				TestEqual(*What, bAtTop, Case.bClimbs);
				if (!Case.bClimbs)
				{
					TestTrue(*(What + TEXT(": ni repta ni se va para atrás")), Cart.From == FIntVector(0, 0, 0) && Cart.S == 0.0 && Cart.V == 0.0);
				}
			}
		});

		It("empujado hacia atrás cruza los nodos sin darse la vuelta", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 3, 0);
			FMineCart Cart;
			Model.PlaceCart(Cart, FIntVector(1, 0, 0), FIntVector(2, 0, 0), 0.5);
			RunFor(Model, Cart, Push(-1), 1.5);
			TestTrue(TEXT("ya en el primer tramo"), Cart.From == FIntVector(0, 0, 0) && Cart.To == FIntVector(1, 0, 0));
			TestTrue(TEXT("sigue yendo marcha atrás"), Cart.V < 0.0);
			TestTrue(TEXT("posición coherente"), Cart.S > 0.0 && Cart.S < 2.0);
		});
	});

	Describe("la gravedad", [this]()
	{
		It("conserva la energía sin rozamiento: v² = 2·g·h al pie de la rampa", [this]()
		{
			FTramwaySettings Settings;
			Settings.RollingResistance = 0.0f;
			FTramwayModel Model(Settings);
			const FIntVector Bottom = Line(Model, FIntVector(0, 0, 0), 4, -5);
			Line(Model, Bottom, 20, 0);
			FMineCart Cart;
			Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, -5));
			Run(Model, Cart, FCartControl(), 10.0, [](const FMineCart& C, const FCartStepResult&) { return C.From.Z == -20 && C.To.Z == -20; });
			TestEqual(TEXT("v = √(2·9,81·2,5)"), Cart.V, FMath::Sqrt(2.0 * 9.81 * 2.5), 0.02 * FMath::Sqrt(2.0 * 9.81 * 2.5));
		});

		It("un vagón parado en llano no se mueve solo", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 2, 0);
			FMineCart Cart;
			Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0), 1.0);
			const FMineCart Before = Cart;
			RunFor(Model, Cart, FCartControl(), 5.0);
			TestTrue(TEXT("idéntico"), Cart == Before);
		});
	});

	Describe("las curvas y los topes", [this]()
	{
		It("la velocidad de vuelco baja con la carga y queda por encima del empuje y del torno", [this]()
		{
			FTramwayModel Model;
			FMineCart Empty;
			FMineCart Full;
			Model.SetLoad(Full, 200.0f);
			const double VEmpty = Model.CurveSpeedLimit(Empty);
			const double VFull = Model.CurveSpeedLimit(Full);
			TestTrue(TEXT("lleno vuelca antes"), VFull < VEmpty);
			TestTrue(TEXT("el torno pasa lleno"), VFull > Model.GetSettings().WinchSpeed);
			TestEqual(TEXT("vacío ≈ 2,56 m/s"), VEmpty, 2.56, 0.01);
		});

		It("un vagón lleno desbocado cuesta abajo vuelca en la curva; empujado despacio la toma", [this]()
		{
			{
				FTramwayModel Model;
				const FIntVector Corner = LTrack(Model, 5);
				FMineCart Cart;
				Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, -5));
				Model.SetLoad(Cart, 200.0f);
				FCartStepResult Last;
				Run(Model, Cart, FCartControl(), 10.0, [&Last](const FMineCart& C, const FCartStepResult& R) { Last = R; return C.IsDerailed(); });
				TestTrue(TEXT("vuelca en la curva"), Cart.Derailed == ECartDerailCause::Curve);
				TestTrue(TEXT("justo en la esquina"), Cart.To == Corner);
				TestTrue(TEXT("iba por encima del límite"), Last.ImpactSpeed > Model.CurveSpeedLimit(Cart));
			}
			{
				FTramwayModel Model;
				const FIntVector Corner = LTrack(Model, 0);
				FMineCart Cart;
				Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0));
				Model.SetLoad(Cart, 200.0f);
				RunFor(Model, Cart, Push(), 6.0);
				TestFalse(TEXT("no vuelca"), Cart.IsDerailed());
				TestTrue(TEXT("ha girado hacia +Y"), Cart.From.Y > Corner.Y || Cart.To.Y > Corner.Y);
			}
		});

		It("contra el tope: despacio se para, rápido vuelca", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 3, 0);
			FMineCart Cart;
			Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0));
			bool bStopped = false;
			RunFor(Model, Cart, Push(), 8.0);
			Run(Model, Cart, Push(), 0.1, [&bStopped](const FMineCart&, const FCartStepResult& R) { bStopped |= R.bStoppedAtBuffer; return false; });
			TestTrue(TEXT("parado en el tope"), bStopped && Cart.V == 0.0 && !Cart.IsDerailed());
			TestTrue(TEXT("al final de la vía"), Cart.To == FIntVector(3, 0, 0) && Cart.S == 2.0);

			FMineCart Fast;
			Model.PlaceCart(Fast, FIntVector(1, 0, 0), FIntVector(2, 0, 0));
			Fast.V = 3.0;
			RunFor(Model, Fast, FCartControl(), 2.0);
			TestTrue(TEXT("vuelca contra el tope"), Fast.Derailed == ECartDerailCause::Buffer);
		});
	});

	Describe("el cambio de agujas", [this]()
	{
		It("la palanca decide la salida, nunca da media vuelta y, si apunta atrás, sigue recto", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 2, 0);
			Line(Model, FIntVector(1, 0, 0), 1, 0, ERailDir::PosY);
			const FIntVector S(1, 0, 0);
			ERailDir Exit;
			TestTrue(TEXT("sin palanca: recto"), Model.ExitFor(S, ERailDir::PosX, Exit) && Exit == ERailDir::PosX);
			Model.SetSwitch(S, ERailDir::PosY);
			TestTrue(TEXT("palanca a +Y"), Model.ExitFor(S, ERailDir::PosX, Exit) && Exit == ERailDir::PosY);
			Model.SetSwitch(S, ERailDir::NegX);
			TestTrue(TEXT("palanca hacia atrás: recto"), Model.ExitFor(S, ERailDir::PosX, Exit) && Exit == ERailDir::PosX);
			TestTrue(TEXT("desde la rama con la palanca atrás: a −X"), Model.ExitFor(S, ERailDir::NegY, Exit) && Exit == ERailDir::NegX);
			Model.SetSwitch(S, ERailDir::PosY);
			TestTrue(TEXT("desde la rama con la palanca a la rama: el menor índice (+X)"), Model.ExitFor(S, ERailDir::NegY, Exit) && Exit == ERailDir::PosX);
			TestFalse(TEXT("fin de vía"), Model.ExitFor(FIntVector(2, 0, 0), ERailDir::PosX, Exit));
		});

		It("con el vagón en marcha: la misma vía lleva a sitios distintos según la palanca", [this]()
		{
			for (ERailDir Lever : { ERailDir::PosX, ERailDir::PosY })
			{
				FTramwayModel Model;
				Line(Model, FIntVector(0, 0, 0), 3, 0);
				Line(Model, FIntVector(1, 0, 0), 2, 0, ERailDir::PosY);
				Model.SetSwitch(FIntVector(1, 0, 0), Lever);
				FMineCart Cart;
				Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0));
				RunFor(Model, Cart, Push(), 8.0);
				const FIntVector Expected = Lever == ERailDir::PosX ? FIntVector(3, 0, 0) : FIntVector(1, 2, 0);
				TestTrue(TEXT("llega al final de la rama elegida"), Cart.To == Expected && !Cart.IsDerailed());
			}
		});
	});

	Describe("el torno", [this]()
	{
		It("sube el vagón lleno por la pendiente máxima a 2 m/s y lo sujeta al llegar", [this]()
		{
			FTramwayModel Model;
			const FIntVector Top = Line(Model, FIntVector(0, 0, 0), 5, 5);
			TestTrue(TEXT("torno"), Model.AddWinch(Top));
			// El vagón mira cuesta abajo: el torno tiene que tirar marcha atrás.
			FMineCart Cart;
			Model.PlaceCart(Cart, FIntVector(1, 0, 5), FIntVector(0, 0, 0), Model.SegmentLength(FIntVector(1, 0, 5), FIntVector(0, 0, 0)));
			Model.SetLoad(Cart, 200.0f);
			double MaxSpeed = 0.0;
			bool bArrived = false;
			Run(Model, Cart, Winch(), 20.0, [&](const FMineCart& C, const FCartStepResult& R)
			{
				MaxSpeed = FMath::Max(MaxSpeed, FMath::Abs(C.V));
				bArrived = R.bArrivedAtWinch;
				return bArrived;
			});
			TestTrue(TEXT("llega"), bArrived);
			TestEqual(TEXT("a la velocidad del torno"), MaxSpeed, static_cast<double>(Model.GetSettings().WinchSpeed));
			TestTrue(TEXT("junto al torno"), FVector::Dist(Model.CartPosition(Cart), Model.NodePosition(Top)) <= Model.GetSettings().WinchStopDistance + 0.05);
			RunFor(Model, Cart, Winch(), 3.0);
			TestTrue(TEXT("el trinquete lo sujeta en la cuesta"), FVector::Dist(Model.CartPosition(Cart), Model.NodePosition(Top)) <= Model.GetSettings().WinchStopDistance + 0.05);
		});

		It("no tira si no hay torno al alcance de la cuerda (60 m por la vía)", [this]()
		{
			FTramwayModel Model;
			const FIntVector Far = Line(Model, FIntVector(0, 0, 0), 31, 0);
			Model.AddWinch(Far);
			FMineCart Cart;
			Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0));
			double Acc = 0.0;
			const FCartStepResult R = Model.Step(Cart, Winch(), 1.0, Acc);
			TestTrue(TEXT("avisa"), R.bNoWinchInReach);
			TestEqual(TEXT("quieto"), Cart.V, 0.0);
			double Distance = 0.0;
			int32 Sign = 0;
			Model.PlaceCart(Cart, FIntVector(1, 0, 0), FIntVector(2, 0, 0));
			TestTrue(TEXT("a 60 m justos sí llega"), Model.WinchDirection(Cart, Distance, Sign) && Sign == 1);
			TestEqual(TEXT("distancia por la vía"), Distance, 60.0, 1e-9);
		});
	});

	Describe("la carga", [this]()
	{
		It("admite hasta 200 kg y rechaza lo demás sin tocar el vagón", [this]()
		{
			FTramwayModel Model;
			FMineCart Cart;
			TestTrue(TEXT("200 kg"), Model.SetLoad(Cart, 200.0f));
			TestFalse(TEXT("200,5 kg"), Model.SetLoad(Cart, 200.5f));
			TestFalse(TEXT("negativa"), Model.SetLoad(Cart, -1.0f));
			TestFalse(TEXT("NaN"), Model.SetLoad(Cart, NAN));
			TestFalse(TEXT("infinita"), Model.SetLoad(Cart, INFINITY));
			TestEqual(TEXT("se queda con 200"), Cart.LoadKg, 200.0f);
			TestEqual(TEXT("masa total"), Model.CartMass(Cart), 260.0);
		});
	});

	Describe("los daños por excavar", [this]()
	{
		It("solo dañan los tramos que pasan por el hueco, una vez, y el vagón descarrila en ellos hasta repararlos", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 5, 0);
			const auto Hit = Model.DamageInSphere(FVector(5.0, 0.3, 0.0), 0.5f);
			TestEqual(TEXT("un tramo"), Hit.Num(), 1);
			TestTrue(TEXT("el del medio"), Hit.Num() == 1 && Hit[0].Key == FIntVector(2, 0, 0) && Hit[0].Value == FIntVector(3, 0, 0));
			TestEqual(TEXT("no se repite"), Model.DamageInSphere(FVector(5.0, 0.3, 0.0), 0.5f).Num(), 0);
			TestEqual(TEXT("cuenta"), Model.NumDamaged(), 1);
			TestTrue(TEXT("dañado en los dos sentidos"), Model.IsDamaged(FIntVector(3, 0, 0), FIntVector(2, 0, 0)));
			TestEqual(TEXT("lejos no toca nada"), Model.DamageInSphere(FVector(5.0, 5.0, 0.0), 2.0f).Num(), 0);
			TestEqual(TEXT("radio NaN no toca nada"), Model.DamageInSphere(FVector(5.0, 0.3, 0.0), NAN).Num(), 0);
			TestEqual(TEXT("radio infinito no toca nada"), Model.DamageInSphere(FVector(5.0, 0.3, 0.0), INFINITY).Num(), 0);

			FMineCart Cart;
			Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0));
			RunFor(Model, Cart, Push(), 5.0);
			TestTrue(TEXT("descarrila al entrar"), Cart.Derailed == ECartDerailCause::DamagedTrack && Cart.To == FIntVector(2, 0, 0));
			const FMineCart Stuck = Cart;
			RunFor(Model, Cart, Push(), 1.0);
			TestTrue(TEXT("descarrilado no se mueve"), Cart == Stuck);

			TestTrue(TEXT("reparado"), Model.Repair(FIntVector(3, 0, 0), FIntVector(2, 0, 0)));
			Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0));
			RunFor(Model, Cart, Push(), 10.0);
			TestTrue(TEXT("ahora llega al final"), !Cart.IsDerailed() && Cart.To == FIntVector(5, 0, 0));
		});

		It("un golpe en un nodo daña los dos tramos que se juntan en él", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 3, 0);
			TestEqual(TEXT("dos tramos"), Model.DamageInSphere(Model.NodePosition(FIntVector(2, 0, 0)), 0.2f).Num(), 2);
		});

		It("si quitan el tramo bajo el vagón, descarrila", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 2, 0);
			FMineCart Cart;
			Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0), 1.0);
			Model.Remove(FIntVector(0, 0, 0), FIntVector(1, 0, 0));
			const FCartStepResult R = Model.Substep(Cart, Push());
			TestTrue(TEXT("sin vía"), R.Derailed == ECartDerailCause::MissingTrack && Cart.IsDerailed());
		});
	});

	Describe("el determinismo", [this]()
	{
		It("da lo mismo bit a bit con cualquier troceo del tiempo", [this]()
		{
			FTramwayModel Model;
			LTrack(Model, 1);
			FMineCart A;
			Model.PlaceCart(A, FIntVector(0, 0, 0), FIntVector(1, 0, -1));
			Model.SetLoad(A, 120.0f);
			FMineCart B = A;
			FMineCart C = A;
			double AccA = 0.0;
			double AccB = 0.0;
			double AccC = 0.0;
			Model.Step(A, Push(), 3.0, AccA);
			for (int32 I = 0; I < 180; ++I)
			{
				Model.Step(B, Push(), 1.0 / 60.0, AccB);
			}
			for (int32 I = 0; I < 100; ++I)
			{
				Model.Step(C, Push(), 0.03, AccC);
			}
			TestTrue(TEXT("1 × 3 s = 180 × 1/60 s"), A == B);
			TestTrue(TEXT("1 × 3 s = 100 × 0,03 s"), A == C);
			TestTrue(TEXT("se ha movido"), A.From != FIntVector(0, 0, 0));
		});

		It("ignora pasos de tiempo nulos, negativos o no finitos", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 2, 0);
			FMineCart Cart;
			Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0));
			const FMineCart Before = Cart;
			double Acc = 0.0;
			for (double Dt : { 0.0, -1.0, static_cast<double>(NAN), static_cast<double>(INFINITY) })
			{
				Model.Step(Cart, Push(), Dt, Acc);
			}
			TestTrue(TEXT("sin cambios"), Cart == Before && Acc == 0.0);
			TestFalse(TEXT("no se pone fuera de la vía"), Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0), 2.5));
			TestFalse(TEXT("ni en un tramo que no existe"), Model.PlaceCart(Cart, FIntVector(5, 0, 0), FIntVector(6, 0, 0)));
		});

		It("acota el tiempo simulado por llamada y se recupera de un acumulador roto", [this]()
		{
			FTramwaySettings Settings;
			Settings.MaxRiseSteps = 400;
			Settings.EmptyCogHeight = 0.0f;
			Settings.FullCogHeight = 0.0f;
			FTramwayModel Model(Settings);
			TestEqual(TEXT("las subidas caben en int8 sin chocar con NoEdge"), Model.GetSettings().MaxRiseSteps, 127);
			Line(Model, FIntVector(0, 0, 0), 10, 0);
			FMineCart Cart;
			Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0));
			TestTrue(TEXT("límite de curva finito con centro de masas nulo"), FMath::IsFinite(Model.CurveSpeedLimit(Cart)));
			double Acc = 0.0;
			const FCartStepResult R = Model.Step(Cart, Push(), 1e9, Acc);
			TestTrue(TEXT("un Dt enorme simula como mucho MaxStepSeconds"),
				R.Distance <= Settings.PushSpeed * (FTramwayModel::MaxStepSeconds + 0.1));
			TestTrue(TEXT("y no deja deuda"), Acc < 1.0);
			double Broken = static_cast<double>(NAN);
			const FCartStepResult R2 = Model.Step(Cart, Push(), 0.5, Broken);
			TestTrue(TEXT("un acumulador NaN se reinicia y el vagón sigue"), R2.Distance > 0.0 && FMath::IsFinite(Broken));
		});
	});

	Describe("el guardado", [this]()
	{
		It("vía, daños, palancas, tornos y vagón vuelven iguales", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(-2, 3, -4), 4, 2);
			Line(Model, FIntVector(-1, 3, -2), 2, -1, ERailDir::PosY);
			Line(Model, FIntVector(-1, 3, -2), 1, 0, ERailDir::NegY);
			Model.SetSwitch(FIntVector(-1, 3, -2), ERailDir::NegY);
			Model.AddWinch(FIntVector(2, 3, 4));
			Model.DamageInSphere(Model.NodePosition(FIntVector(0, 3, 0)), 0.3f);
			FTramwayModel Loaded;
			TestTrue(TEXT("carga"), Loaded.FromValue(Model.ToValue()));
			TestTrue(TEXT("igual"), Loaded == Model);
			TestTrue(TEXT("mismo texto"), Loaded.ToValue() == Model.ToValue());

			FMineCart Cart;
			Model.PlaceCart(Cart, FIntVector(-2, 3, -4), FIntVector(-1, 3, -2), 0.7);
			Model.SetLoad(Cart, 143.5f);
			Cart.V = -0.8123456789;
			FMineCart Back;
			TestTrue(TEXT("vagón"), FTramwayModel::CartFromValue(FTramwayModel::CartToValue(Cart), Back));
			TestTrue(TEXT("vagón igual"), Back == Cart);
		});

		It("un torno que se queda sin vía debajo no borra la vía al cargar", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 3, 0);
			TestTrue(TEXT("torno al final"), Model.AddWinch(FIntVector(3, 0, 0)));
			TestTrue(TEXT("quitan el último tramo"), Model.Remove(FIntVector(2, 0, 0), FIntVector(3, 0, 0)));
			TestTrue(TEXT("el torno sigue puesto"), Model.HasWinch(FIntVector(3, 0, 0)));
			FTramwayModel Loaded;
			TestTrue(TEXT("carga"), Loaded.FromValue(Model.ToValue()));
			TestTrue(TEXT("igual"), Loaded == Model);
			TestEqual(TEXT("los dos tramos que quedan"), Loaded.NumSegments(), 2);
		});

		It("rechaza guardados rotos y se queda vacío", [this]()
		{
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 3, 0);
			const FSaveValue Good = Model.ToValue();

			const auto Broken = [&Good](TFunctionRef<void(FSaveValue&)> Edit)
			{
				FSaveValue V = Good;
				Edit(V);
				return V;
			};
			const FSaveValue Cases[] = {
				FSaveValue::MakeInt(3),
				Broken([](FSaveValue& V) { V.Set(TEXT("v"), FSaveValue::MakeInt(2)); }),
				Broken([](FSaveValue& V) { V.Find(TEXT("seg"))->Add(FSaveValue::MakeInt(1)); }),
				// El primer tramo, repetido.
				Broken([](FSaveValue& V) { for (int32 I = 0; I < 6; ++I) { V.Find(TEXT("seg"))->Add(V.Find(TEXT("seg"))->At(I)); } }),
				// Subida de 6 escalones.
				Broken([](FSaveValue& V) { *V.Find(TEXT("seg"))->AtMutable(4) = FSaveValue::MakeInt(6); }),
				// Dirección −X (solo se guardan +X y +Y).
				Broken([](FSaveValue& V) { *V.Find(TEXT("seg"))->AtMutable(3) = FSaveValue::MakeInt(2); }),
				// Palanca en una recta.
				Broken([](FSaveValue& V) { FSaveValue& Sw = *V.Find(TEXT("sw")); Sw.Add(FSaveValue::MakeInt(1)); Sw.Add(FSaveValue::MakeInt(0)); Sw.Add(FSaveValue::MakeInt(0)); Sw.Add(FSaveValue::MakeInt(0)); }),
				// Torno repetido.
				Broken([](FSaveValue& V) { FSaveValue& W = *V.Find(TEXT("winch")); for (int32 I = 0; I < 2; ++I) { W.Add(FSaveValue::MakeInt(9)); W.Add(FSaveValue::MakeInt(9)); W.Add(FSaveValue::MakeInt(0)); } }),
				// Coordenada fuera de rango.
				Broken([](FSaveValue& V) { *V.Find(TEXT("seg"))->AtMutable(0) = FSaveValue::MakeInt(int64(1) << 40); }),
			};
			for (const FSaveValue& Case : Cases)
			{
				FTramwayModel Target;
				Line(Target, FIntVector(5, 5, 5), 1, 0);
				TestFalse(TEXT("rechazado"), Target.FromValue(Case));
				TestTrue(TEXT("vacío"), Target.IsEmpty());
			}

			FMineCart Cart;
			FSaveValue BadCart = FTramwayModel::CartToValue(Cart);
			BadCart.Set(TEXT("derail"), FSaveValue::MakeInt(99));
			TestFalse(TEXT("causa de descarrilo desconocida"), FTramwayModel::CartFromValue(BadCart, Cart));
			BadCart = FTramwayModel::CartToValue(Cart);
			BadCart.Set(TEXT("s"), FSaveValue::MakeString(TEXT("NaN")));
			TestFalse(TEXT("posición no finita"), FTramwayModel::CartFromValue(BadCart, Cart));
			BadCart = FTramwayModel::CartToValue(Cart);
			BadCart.Set(TEXT("load"), FSaveValue::MakeString(TEXT("NaN")));
			TestFalse(TEXT("carga no finita"), FTramwayModel::CartFromValue(BadCart, Cart));

			FTramwayModel Track;
			Line(Track, FIntVector(0, 0, 0), 1, 0);
			TestFalse(TEXT("colocar en S NaN"), Track.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0), static_cast<double>(NAN)));
		});

		It("un vagón cargado con la posición fuera de su tramo no cuelga el paso en una vía cerrada", [this]()
		{
			// Cuadrado de 2 × 2 celdas: vía cerrada con cuatro curvas.
			FTramwayModel Model;
			Line(Model, FIntVector(0, 0, 0), 1, 0, ERailDir::PosX);
			Line(Model, FIntVector(1, 0, 0), 1, 0, ERailDir::PosY);
			Line(Model, FIntVector(1, 1, 0), 1, 0, ERailDir::NegX);
			Line(Model, FIntVector(0, 1, 0), 1, 0, ERailDir::NegY);
			for (const double Bad : { 1.0e300, -1.0e300, 7.5, -0.5 })
			{
				FMineCart Cart;
				Model.PlaceCart(Cart, FIntVector(0, 0, 0), FIntVector(1, 0, 0));
				FSaveValue Saved = FTramwayModel::CartToValue(Cart);
				Saved.Set(TEXT("s"), FSaveValue::MakeDouble(Bad));
				FMineCart Loaded;
				if (FTramwayModel::CartFromValue(Saved, Loaded))
				{
					double Acc = 0.0;
					Model.Step(Loaded, FCartControl(), 1.0 / 60.0, Acc);
					const double Length = Model.SegmentLength(Loaded.From, Loaded.To);
					TestTrue(TEXT("sobre su tramo"), Loaded.S >= 0.0 && Loaded.S <= Length);
				}
			}
		});
	});
}

#endif
