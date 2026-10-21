#include "Misc/AutomationTest.h"

#include "Narrative/ExploredProgress.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/PointsOfInterest.h"
#include "WorldGen/TerrainDensity.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FNarrativeSpec, "Explored.Narrative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	TUniquePtr<FTerrainDensity> Density;
	TArray<FPointOfInterest> Pois;
END_DEFINE_SPEC(FNarrativeSpec)

void FNarrativeSpec::Define()
{
	BeforeEach([this]()
	{
		if (!Density)
		{
			Density = MakeUnique<FTerrainDensity>(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
			Pois = FPoiLayout::Generate(*Density);
		}
	});

	Describe("FPoiLayout", [this]()
	{
		It("coloca los 30 petroglifos sin repetir id", [this]()
		{
			TSet<FName> Petroglyphs;
			for (const FPointOfInterest& P : Pois)
			{
				if (P.Type == EPoiType::Petroglyph)
				{
					if (Petroglyphs.Contains(P.ContentId))
					{
						AddError(FString::Printf(TEXT("Petroglifo repetido: %s"), *P.ContentId.ToString()));
					}
					Petroglyphs.Add(P.ContentId);
				}
			}
			TestEqual(TEXT("Petroglifos"), Petroglyphs.Num(), FExploredProgress::TotalPetroglyphs);
		});

		It("coloca 7 miradores y todos los lugares clave", [this]()
		{
			int32 Views = 0;
			TSet<EPoiType> Types;
			for (const FPointOfInterest& P : Pois)
			{
				Views += P.Type == EPoiType::Viewpoint ? 1 : 0;
				Types.Add(P.Type);
			}
			TestEqual(TEXT("Miradores"), Views, FExploredProgress::TotalViewpoints);
			for (const EPoiType Key : {EPoiType::WreckFuselage, EPoiType::WreckWing, EPoiType::WreckTail, EPoiType::RadioStation,
				EPoiType::TideObservatory, EPoiType::Lighthouse, EPoiType::StarCompass, EPoiType::Waterfall, EPoiType::Shipwreck})
			{
				TestTrue(FString::Printf(TEXT("Existe %s"), LexToString(Key)), Types.Contains(Key));
			}
		});

		It("pone en tierra firme lo que no es submarino", [this]()
		{
			for (const FPointOfInterest& P : Pois)
			{
				if (!P.bUnderwater && P.Type != EPoiType::Petroglyph && P.Location.Z < 0.0f)
				{
					AddError(FString::Printf(TEXT("%s %s bajo el agua (%.1f m)"), LexToString(P.Type), *P.ContentId.ToString(), P.Location.Z));
				}
			}
		});

		It("es determinista", [this]()
		{
			const TArray<FPointOfInterest> Again = FPoiLayout::Generate(*Density);
			TestEqual(TEXT("Mismo número"), Again.Num(), Pois.Num());
			for (int32 I = 0; I < FMath::Min(Again.Num(), Pois.Num()); ++I)
			{
				if (!Again[I].Location.Equals(Pois[I].Location, 0.01))
				{
					AddError(TEXT("Posición distinta"));
					return;
				}
			}
		});
	});

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
