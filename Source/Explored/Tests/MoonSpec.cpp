#include "Misc/AutomationTest.h"

#include "Ocean/OceanCurrents.h"
#include "Sky/MoonModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FMoonSpec, "Explored.Moon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FMoonSpec)

void FMoonSpec::Define()
{
	Describe("FMoonModel", [this]()
	{
		It("tiene un ciclo de 12 días de juego", [this]()
		{
			TestEqual(TEXT("Días por ciclo"), FMoonModel::DaysPerCycle, 12);
			for (float T = 0.0f; T < 60.0f; T += 0.73f)
			{
				const float Delta = FMath::Abs(FMoonModel::Phase(T) - FMoonModel::Phase(T + 12.0f));
				// La fase es circular: 0.9999 y 0.0001 están juntas.
				TestTrue(TEXT("Periodo de 12 días"), FMath::Min(Delta, 1.0f - Delta) < 1e-4f);
			}
			TestTrue(TEXT("Nueva en el día 0"), FMoonModel::IlluminationAt(0.0f) < 0.001f);
			TestTrue(TEXT("Llena en el día 6"), FMoonModel::IlluminationAt(6.0f) > 0.999f);
			TestTrue(TEXT("Nueva otra vez en el día 12"), FMoonModel::IlluminationAt(12.0f) < 0.001f);
		});

		It("da la fase en [0, 1) también para instantes negativos", [this]()
		{
			for (float T = -30.0f; T < 30.0f; T += 0.37f)
			{
				const float Phase = FMoonModel::Phase(T);
				TestTrue(TEXT("En rango"), Phase >= 0.0f && Phase < 1.0f);
			}
			TestEqual(TEXT("Tres días antes de la nueva"), FMoonModel::Phase(-3.0f), 0.75f, 1e-5f);
		});

		It("nombra las ocho fases en su orden", [this]()
		{
			TestEqual(TEXT("Nueva"), FMoonModel::NamedPhase(FMoonModel::Phase(0.0f)), EMoonPhase::New);
			TestEqual(TEXT("Creciente"), FMoonModel::NamedPhase(FMoonModel::Phase(1.5f)), EMoonPhase::WaxingCrescent);
			TestEqual(TEXT("Cuarto creciente"), FMoonModel::NamedPhase(FMoonModel::Phase(3.0f)), EMoonPhase::FirstQuarter);
			TestEqual(TEXT("Gibosa creciente"), FMoonModel::NamedPhase(FMoonModel::Phase(4.5f)), EMoonPhase::WaxingGibbous);
			TestEqual(TEXT("Llena"), FMoonModel::NamedPhase(FMoonModel::Phase(6.0f)), EMoonPhase::Full);
			TestEqual(TEXT("Gibosa menguante"), FMoonModel::NamedPhase(FMoonModel::Phase(7.5f)), EMoonPhase::WaningGibbous);
			TestEqual(TEXT("Cuarto menguante"), FMoonModel::NamedPhase(FMoonModel::Phase(9.0f)), EMoonPhase::LastQuarter);
			TestEqual(TEXT("Menguante"), FMoonModel::NamedPhase(FMoonModel::Phase(10.5f)), EMoonPhase::WaningCrescent);
			TestEqual(TEXT("Casi nueva"), FMoonModel::NamedPhase(0.99f), EMoonPhase::New);
		});

		It("abre ventanas de luna llena y nueva alrededor del instante exacto", [this]()
		{
			TestTrue(TEXT("Llena exacta"), FMoonModel::IsFullMoonWindow(6.0f));
			TestTrue(TEXT("Llena al borde"), FMoonModel::IsFullMoonWindow(6.9f));
			TestFalse(TEXT("Fuera de la llena"), FMoonModel::IsFullMoonWindow(4.5f));
			TestTrue(TEXT("Nueva antes del ciclo"), FMoonModel::IsNewMoonWindow(11.5f));
			TestTrue(TEXT("Nueva después del ciclo"), FMoonModel::IsNewMoonWindow(12.5f));
			TestFalse(TEXT("Cuarto no es nueva"), FMoonModel::IsNewMoonWindow(3.0f));
			TestFalse(TEXT("Llena no es nueva"), FMoonModel::IsNewMoonWindow(6.0f));
			TestEqual(TEXT("Siguiente llena"), FMoonModel::NextFullMoon(5.9f), 6.0f);
			TestEqual(TEXT("Siguiente llena estricta"), FMoonModel::NextFullMoon(6.0f), 18.0f);
			TestEqual(TEXT("Siguiente nueva"), FMoonModel::NextNewMoon(0.0f), 12.0f);
			TestEqual(TEXT("Siguiente nueva a mitad de ciclo"), FMoonModel::NextNewMoon(30.0f), 36.0f);
		});

		It("hace la bioluminiscencia máxima en luna nueva y nula en luna llena", [this]()
		{
			TestEqual(TEXT("Nueva"), FMoonModel::Bioluminescence(0.0f), 1.0f, 1e-5f);
			TestEqual(TEXT("Llena"), FMoonModel::Bioluminescence(0.5f), 0.0f, 1e-5f);
			float Previous = 2.0f;
			for (float Phase = 0.0f; Phase <= 0.5f; Phase += 0.02f)
			{
				const float Value = FMoonModel::Bioluminescence(Phase);
				TestTrue(TEXT("Decrece hacia la llena"), Value <= Previous + 1e-6f);
				Previous = Value;
			}
		});
	});

	Describe("Coherencia con FOceanTide", [this]()
	{
		It("usa la misma fase lunar para las mareas vivas y muertas", [this]()
		{
			for (float T = 0.0f; T < 48.0f; T += 0.41f)
			{
				TestEqual(TEXT("SpringNeapFactorAt = SpringNeapFactor(fase)"),
					FOceanTide::SpringNeapFactorAt(T), FOceanTide::SpringNeapFactor(FMoonModel::Phase(T)), 1e-6f);
			}
			for (int32 Cycle = 0; Cycle < 6; ++Cycle)
			{
				TestTrue(TEXT("Viva en luna nueva"), FOceanTide::SpringNeapFactorAt(FMoonModel::NewMoonOfCycle(Cycle)) > 0.99f);
				TestTrue(TEXT("Viva en luna llena"), FOceanTide::SpringNeapFactorAt(FMoonModel::FullMoonOfCycle(Cycle)) > 0.99f);
				TestTrue(TEXT("Muerta en cuarto creciente"), FOceanTide::SpringNeapFactorAt(Cycle * 12.0f + 3.0f) < 0.45f);
				TestTrue(TEXT("Muerta en cuarto menguante"), FOceanTide::SpringNeapFactorAt(Cycle * 12.0f + 9.0f) < 0.45f);
			}
		});
	});
}

#endif
