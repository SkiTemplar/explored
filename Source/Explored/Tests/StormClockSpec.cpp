#include "Misc/AutomationTest.h"

#include "Weather/StormClockModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace StormTest
{
	double At(int32 Day, double Hour)
	{
		return FStormClockModel::ToAbsoluteHours(Day, Hour);
	}

	struct FPressureCase
	{
		int32 Day;
		double Hour;
		float Hpa;
	};

	struct FStateCase
	{
		int32 Day;
		double Hour;
		EStormState State;
	};
}

BEGIN_DEFINE_SPEC(FStormClockSpec, "Explored.StormClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	FStormClockModel Clock;
END_DEFINE_SPEC(FStormClockSpec)

void FStormClockSpec::Define()
{
	using namespace StormTest;

	Describe(TEXT("barómetro"), [this]()
	{
		It(TEXT("da los hPa de la tabla del GDD en cada punto"), [this]()
		{
			const FPressureCase Table[] = {{1, 7.0, 1014.0f}, {1, 15.0, 1010.0f}, {2, 6.0, 1008.0f}, {2, 18.0, 998.0f}, {2, 19.0, 990.0f},
				{2, 22.0, 965.0f}, {2, 23.75, 965.0f}, {3, 2.0, 995.0f}, {3, 6.0, 1012.0f}};
			for (const FPressureCase& C : Table)
			{
				TestEqual(FString::Printf(TEXT("D%d %.2f h"), C.Day, C.Hour), Clock.PressureAt(At(C.Day, C.Hour)), C.Hpa, 1.0e-3f);
			}
		});

		It(TEXT("interpola linealmente entre puntos"), [this]()
		{
			TestEqual(TEXT("D1 11:00, mitad de 1014-1010"), Clock.PressureAt(At(1, 11.0)), 1012.0f, 1.0e-3f);
			TestEqual(TEXT("D2 12:00, mitad de 1008-998"), Clock.PressureAt(At(2, 12.0)), 1003.0f, 1.0e-3f);
			TestEqual(TEXT("D2 20:30, mitad del muro 1"), Clock.PressureAt(At(2, 20.5)), 977.5f, 1.0e-3f);
		});

		It(TEXT("se mantiene constante fuera de la curva"), [this]()
		{
			TestEqual(TEXT("Despertar D1 04:10"), Clock.PressureAt(At(1, 4.0 + 10.0 / 60.0)), 1014.0f);
			TestEqual(TEXT("D3 12:00"), Clock.PressureAt(At(3, 12.0)), 1012.0f);
			TestEqual(TEXT("D9"), Clock.PressureAt(At(9, 0.0)), 1012.0f);
		});

		It(TEXT("la flecha baja, sube o se queda según el tramo"), [this]()
		{
			TestEqual(TEXT("D1 10:00 baja"), Clock.TrendAt(At(1, 10.0)), EBarometerTrend::Falling);
			TestEqual(TEXT("D1 20:00 baja despacio"), Clock.TrendAt(At(1, 20.0)), EBarometerTrend::Falling);
			TestEqual(TEXT("Ojo estable"), Clock.TrendAt(At(2, 22.5)), EBarometerTrend::Steady);
			TestEqual(TEXT("D3 03:00 sube"), Clock.TrendAt(At(3, 3.0)), EBarometerTrend::Rising);
			TestEqual(TEXT("Antes de la curva"), Clock.TrendAt(At(1, 5.0)), EBarometerTrend::Steady);
			TestEqual(TEXT("Después de la curva"), Clock.TrendAt(At(4, 0.0)), EBarometerTrend::Steady);
		});

		It(TEXT("una hora no finita no rompe nada"), [this]()
		{
			const double NaN = std::numeric_limits<double>::quiet_NaN();
			const double Inf = std::numeric_limits<double>::infinity();
			TestEqual(TEXT("Presión"), Clock.PressureAt(NaN), 1014.0f);
			TestEqual(TEXT("Tendencia"), Clock.TrendAt(Inf), EBarometerTrend::Steady);
			TestEqual(TEXT("Estado"), Clock.StateAt(NaN), EStormState::Clear);
			TestFalse(TEXT("Balsa"), Clock.CanCrossByRaft(NaN));
			TestFalse(TEXT("Fuego"), Clock.ExtinguishesOutdoorFires(NaN));
			TestEqual(TEXT("Cargas"), Clock.BarrelChargesBetween(NaN, At(3, 0.0)), 0.0f);
		});
	});

	Describe(TEXT("estados"), [this]()
	{
		It(TEXT("siguen las horas del GDD, con los límites incluidos al empezar"), [this]()
		{
			const double Eps = 1.0 / 3600.0;
			const FStateCase Table[] = {{1, 7.0, EStormState::Clear}, {1, 15.0 - Eps, EStormState::Clear}, {1, 15.0, EStormState::Squall},
				{1, 17.0 - Eps, EStormState::Squall}, {1, 17.0, EStormState::Clear}, {2, 6.0 - Eps, EStormState::Clear},
				{2, 6.0, EStormState::HighClouds}, {2, 18.0 - Eps, EStormState::HighClouds}, {2, 18.0, EStormState::SeaClosed},
				{2, 19.0, EStormState::CycloneWall1}, {2, 22.0 - Eps, EStormState::CycloneWall1}, {2, 22.0, EStormState::CycloneEye},
				{2, 23.75, EStormState::CycloneWall2}, {3, 2.0 - Eps, EStormState::CycloneWall2}, {3, 2.0, EStormState::Clear},
				{3, 20.0, EStormState::Clear}};
			for (const FStateCase& C : Table)
			{
				TestEqual(FString::Printf(TEXT("D%d %.4f h"), C.Day, C.Hour), Clock.StateAt(At(C.Day, C.Hour)), C.State);
			}
		});

		It(TEXT("el ciclón dura 3 h de muro 1, 1,75 h de ojo y 2,25 h de muro 2"), [this]()
		{
			double Wall1 = 0.0;
			double Eye = 0.0;
			double Wall2 = 0.0;
			for (const FStormWindow& W : Clock.GetConfig().Windows)
			{
				const double Length = W.EndHour - W.StartHour;
				Wall1 += W.State == EStormState::CycloneWall1 ? Length : 0.0;
				Eye += W.State == EStormState::CycloneEye ? Length : 0.0;
				Wall2 += W.State == EStormState::CycloneWall2 ? Length : 0.0;
			}
			TestEqual(TEXT("Muro 1"), Wall1, 3.0);
			TestEqual(TEXT("Ojo"), Eye, 1.75);
			TestEqual(TEXT("Muro 2"), Wall2, 2.25);
			TestTrue(TEXT("Ojo es ciclón"), Clock.IsCyclone(At(2, 23.0)));
			TestFalse(TEXT("Mar cerrado aún no es ciclón"), Clock.IsCyclone(At(2, 18.5)));
		});
	});

	Describe(TEXT("fuego al aire libre"), [this]()
	{
		It(TEXT("solo lo apagan los muros del ciclón"), [this]()
		{
			TestTrue(TEXT("Muro 1"), Clock.ExtinguishesOutdoorFires(At(2, 20.0)));
			TestTrue(TEXT("Muro 2"), Clock.ExtinguishesOutdoorFires(At(3, 1.0)));
			TestFalse(TEXT("Ojo"), Clock.ExtinguishesOutdoorFires(At(2, 23.0)));
			TestFalse(TEXT("Chubasco"), Clock.ExtinguishesOutdoorFires(At(1, 16.0)));
			TestFalse(TEXT("Mar cerrado"), Clock.ExtinguishesOutdoorFires(At(2, 18.5)));
			TestFalse(TEXT("Despejado"), Clock.ExtinguishesOutdoorFires(At(3, 10.0)));
		});
	});

	Describe(TEXT("balsa"), [this]()
	{
		It(TEXT("se cruza hasta las 18:00 del día 2 y nunca después"), [this]()
		{
			TestTrue(TEXT("D1 10:00"), Clock.CanCrossByRaft(At(1, 10.0)));
			TestTrue(TEXT("D2 17:59"), Clock.CanCrossByRaft(At(2, 18.0 - 1.0 / 60.0)));
			TestFalse(TEXT("D2 18:00"), Clock.CanCrossByRaft(At(2, 18.0)));
			TestFalse(TEXT("Ciclón"), Clock.CanCrossByRaft(At(2, 20.0)));
			TestFalse(TEXT("D3 de día y en calma"), Clock.CanCrossByRaft(At(3, 10.0)));
		});

		It(TEXT("el fundido forzoso llega a las 18:45 del día 2"), [this]()
		{
			TestFalse(TEXT("18:44"), Clock.IsPastForcedReturn(At(2, 18.75 - 1.0 / 60.0)));
			TestTrue(TEXT("18:45"), Clock.IsPastForcedReturn(At(2, 18.75)));
		});
	});

	Describe(TEXT("lluvia y barril"), [this]()
	{
		It(TEXT("llueve en el chubasco y en los muros, no en el ojo"), [this]()
		{
			const FStormRain Squall = Clock.RainAt(At(1, 16.0));
			TestTrue(TEXT("Chubasco"), Squall.bRaining);
			TestEqual(TEXT("Intensidad del chubasco"), Squall.Intensity, 0.6f);
			const FStormRain Wall = Clock.RainAt(At(2, 20.0));
			TestTrue(TEXT("Muro"), Wall.bRaining);
			TestEqual(TEXT("Intensidad del muro"), Wall.Intensity, 1.0f);
			TestFalse(TEXT("Ojo"), Clock.RainAt(At(2, 23.0)).bRaining);
			TestFalse(TEXT("Nubes altas"), Clock.RainAt(At(2, 12.0)).bRaining);
			TestEqual(TEXT("Sin lluvia no llena"), Clock.RainAt(At(2, 12.0)).ChargesPerHour, 0.0f);
		});

		It(TEXT("el chubasco guiado llena 2 cargas"), [this]()
		{
			const float Charges = Clock.BarrelChargesBetween(At(1, 0.0), At(2, 0.0));
			TestEqual(TEXT("Cargas del día 1"), Charges, 2.0f, 1.0e-4f);
			TestEqual(TEXT("Barril"), FStormClockModel::AddRain(FStormBarrelState(), Charges).Charges, 2);
		});

		It(TEXT("el ciclón llena el barril hasta 4 y lo demás rebosa"), [this]()
		{
			const float Charges = Clock.BarrelChargesBetween(At(2, 19.0), At(3, 2.0));
			TestEqual(TEXT("Lluvia del ciclón (muros)"), Charges, 5.25f, 1.0e-4f);
			const FStormBarrelState Full = FStormClockModel::AddRain(FStormBarrelState(), Charges);
			TestEqual(TEXT("Lleno"), Full.Charges, FStormClockModel::BarrelCapacity);
			TestEqual(TEXT("Sin fracción"), Full.Partial, 0.0f);
		});

		It(TEXT("las fracciones se acumulan en pasos cortos"), [this]()
		{
			FStormBarrelState Barrel;
			double T = At(1, 15.0);
			for (int32 Step = 0; Step < 120; ++Step)
			{
				Barrel = FStormClockModel::AddRain(Barrel, Clock.BarrelChargesBetween(T, T + 1.0 / 60.0));
				T += 1.0 / 60.0;
			}
			TestEqual(TEXT("Dos cargas tras 2 h de chubasco a minutos"), Barrel.Charges + Barrel.Partial, 2.0f, 1.0e-3f);
		});

		It(TEXT("intervalos y cantidades no válidos no llenan"), [this]()
		{
			TestEqual(TEXT("Al revés"), Clock.BarrelChargesBetween(At(1, 17.0), At(1, 15.0)), 0.0f);
			FStormBarrelState Barrel;
			Barrel.Charges = 1;
			TestTrue(TEXT("Negativo"), FStormClockModel::AddRain(Barrel, -2.0f) == Barrel);
			TestTrue(TEXT("NaN"), FStormClockModel::AddRain(Barrel, std::numeric_limits<float>::quiet_NaN()) == Barrel);
		});

		It(TEXT("sacar una carga del barril vacío falla"), [this]()
		{
			bool bOk = true;
			const FStormBarrelState Empty;
			TestTrue(TEXT("Sigue vacío"), FStormClockModel::TakeCharge(Empty, bOk) == Empty);
			TestFalse(TEXT("Rechazado"), bOk);
			FStormBarrelState One;
			One.Charges = 1;
			TestEqual(TEXT("Queda 0"), FStormClockModel::TakeCharge(One, bOk).Charges, 0);
			TestTrue(TEXT("Aceptado"), bOk);
		});
	});

	Describe(TEXT("configuración"), [this]()
	{
		It(TEXT("la del GDD es válida"), [this]()
		{
			TestTrue(TEXT("Gdd"), FStormClockConfig::Gdd().IsValid());
			TestFalse(TEXT("Sin sustituir"), Clock.UsesFallback());
		});

		It(TEXT("una curva vacía, desordenada o con ventanas solapadas se sustituye por la del GDD"), [this]()
		{
			FStormClockConfig Empty = FStormClockConfig::Gdd();
			Empty.Points.Reset();
			FStormClockConfig Unsorted = FStormClockConfig::Gdd();
			Unsorted.Points[1].Hour = Unsorted.Points[0].Hour;
			FStormClockConfig Overlap = FStormClockConfig::Gdd();
			Overlap.Windows[0].EndHour = Overlap.Windows[1].StartHour + 1.0;
			Overlap.Windows[0].StartHour = Overlap.Windows[1].StartHour - 1.0;
			FStormClockConfig NegativeRate = FStormClockConfig::Gdd();
			NegativeRate.CycloneChargesPerHour = -1.0f;
			const FStormClockConfig Bad[] = {Empty, Unsorted, Overlap, NegativeRate};
			for (const FStormClockConfig& C : Bad)
			{
				TestFalse(TEXT("No válida"), C.IsValid());
				const FStormClockModel Model(C);
				TestTrue(TEXT("Sustituida"), Model.UsesFallback());
				TestEqual(TEXT("Curva del GDD"), Model.PressureAt(At(2, 22.0)), 965.0f);
			}
		});
	});
}

#endif
