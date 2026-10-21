#include "Misc/AutomationTest.h"

#include "Narrative/ExploredProgress.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FNarrativeSpec, "Explored.Narrative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FNarrativeSpec)

void FNarrativeSpec::Define()
{
	Describe("FExploredProgress", [this]()
	{
		It("exige las cuatro piezas del Albatros para construir el barco", [this]()
		{
			FExploredProgress P;
			P.AddShipPart(EShipPart::Fuselage);
			P.AddShipPart(EShipPart::Wing);
			P.AddShipPart(EShipPart::Tail);
			TestFalse(TEXT("Faltan piezas"), P.CanBuildShip());
			P.AddShipPart(EShipPart::Engine);
			TestTrue(TEXT("Completo"), P.CanBuildShip());
			P.bShipBuilt = true;
			TestFalse(TEXT("No se construye dos veces"), P.CanBuildShip());
			TestTrue(TEXT("Puede zarpar"), P.CanDepart());
			TestTrue(TEXT("Puede quedarse"), P.CanChooseStay());
		});

		It("avanza el viaje: piezas, grada y zarpar solo de noche", [this]()
		{
			FExploredProgress P;
			TestTrue(TEXT("Empieza rescatando"), P.GetVoyageStage() == EVoyageStage::Salvage);
			TestFalse(TEXT("Sin piezas no se construye"), P.BuildShip());
			P.AddShipPart(EShipPart::Wing);
			P.AddShipPart(EShipPart::Wing);
			TestEqual(TEXT("Una pieza repetida cuenta una vez"), P.CountShipParts(), 1);
			for (const EShipPart Part : {EShipPart::Fuselage, EShipPart::Tail, EShipPart::Engine})
			{
				P.AddShipPart(Part);
			}
			TestEqual(TEXT("Cuatro piezas"), P.CountShipParts(), 4);
			TestTrue(TEXT("Toca construir"), P.GetVoyageStage() == EVoyageStage::Build);
			TestFalse(TEXT("No zarpa sin barco"), P.Depart(22.0f));
			TestTrue(TEXT("Se construye"), P.BuildShip());
			TestFalse(TEXT("No se construye dos veces"), P.BuildShip());
			TestTrue(TEXT("Toca zarpar"), P.GetVoyageStage() == EVoyageStage::SetSail);
			TestFalse(TEXT("De día no zarpa"), P.Depart(13.0f));
			TestTrue(TEXT("De noche zarpa"), P.Depart(23.5f));
			TestFalse(TEXT("No zarpa dos veces"), P.Depart(23.6f));
			TestTrue(TEXT("Final visto"), P.GetVoyageStage() == EVoyageStage::Departed);
		});

		It("considera noche de 20:00 a 05:00, también con horas acumuladas", [this]()
		{
			TestTrue(TEXT("20:00"), FExploredProgress::IsStarNight(20.0f));
			TestTrue(TEXT("04:59"), FExploredProgress::IsStarNight(4.98f));
			TestFalse(TEXT("05:00"), FExploredProgress::IsStarNight(5.0f));
			TestFalse(TEXT("19:59"), FExploredProgress::IsStarNight(19.98f));
			TestTrue(TEXT("Día 3 a las 22:00"), FExploredProgress::IsStarNight(70.0f));
			TestTrue(TEXT("Negativas"), FExploredProgress::IsStarNight(-1.0f));
		});

		It("cuenta el mapa y no duplica descubrimientos", [this]()
		{
			FExploredProgress P;
			TestTrue(TEXT("Nuevo"), P.Discover(TEXT("petro_01")));
			TestFalse(TEXT("Repetido"), P.Discover(TEXT("petro_01")));
			TestTrue(TEXT("Algo de progreso"), P.MapCompletion() > 0.0f && P.MapCompletion() < 3.0f);
			TestFalse(TEXT("No completo"), P.IsMapComplete());
		});
	});
}

#endif
