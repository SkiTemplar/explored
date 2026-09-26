#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
		It("coloca las 18 notas de Inés una sola vez", [this]()
		{
			TSet<FName> Notes;
			for (const FPointOfInterest& P : Pois)
			{
				if (P.ContentId.ToString().StartsWith(TEXT("ines_")))
				{
					if (Notes.Contains(P.ContentId))
					{
						AddError(FString::Printf(TEXT("Nota repetida: %s"), *P.ContentId.ToString()));
					}
					Notes.Add(P.ContentId);
				}
			}
			TestEqual(TEXT("Notas"), Notes.Num(), FExploredProgress::TotalInesNotes);
		});

		It("coloca las 24 páginas Halden, 7 miradores y todos los lugares clave", [this]()
		{
			int32 Pages = 0;
			int32 Views = 0;
			TSet<EPoiType> Types;
			for (const FPointOfInterest& P : Pois)
			{
				Pages += P.Type == EPoiType::HaldenPage ? 1 : 0;
				Views += P.Type == EPoiType::Viewpoint ? 1 : 0;
				Types.Add(P.Type);
			}
			TestEqual(TEXT("Páginas"), Pages, FExploredProgress::TotalHaldenPages);
			TestEqual(TEXT("Miradores"), Views, FExploredProgress::TotalViewpoints);
			for (const EPoiType Key : {EPoiType::WreckFuselage, EPoiType::WreckWing, EPoiType::WreckTail, EPoiType::RadioStation,
				EPoiType::TideObservatory, EPoiType::Lighthouse, EPoiType::StarCompass, EPoiType::Waterfall, EPoiType::Shipwreck, EPoiType::BeaconSite})
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

	Describe("Historia", [this]()
	{
		It("tiene en story_es.json todo el contenido que colocan los puntos de interés", [this]()
		{
			FString Text;
			const FString Path = FPaths::ProjectContentDir() / TEXT("Data/story_es.json");
			TestTrue(TEXT("Existe el fichero"), FFileHelper::LoadFileToString(Text, *Path));
			TSharedPtr<FJsonObject> Root;
			TestTrue(TEXT("JSON válido"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid());
			if (!Root)
			{
				return;
			}
			TSet<FString> Ids;
			for (const TCHAR* Field : {TEXT("ines_notes"), TEXT("halden_pages"), TEXT("bottles"), TEXT("morse")})
			{
				for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(Field))
				{
					Ids.Add(V->AsObject()->GetStringField(TEXT("id")));
				}
			}
			TestEqual(TEXT("18 notas"), Root->GetArrayField(TEXT("ines_notes")).Num(), FExploredProgress::TotalInesNotes);
			TestEqual(TEXT("24 páginas"), Root->GetArrayField(TEXT("halden_pages")).Num(), FExploredProgress::TotalHaldenPages);
			TestEqual(TEXT("30 petroglifos"), Root->GetArrayField(TEXT("petroglyph_themes")).Num(), FExploredProgress::TotalPetroglyphs);
			for (const FPointOfInterest& P : Pois)
			{
				const FString Id = P.ContentId.ToString();
				if ((Id.StartsWith(TEXT("ines_")) || Id.StartsWith(TEXT("halden_")) || Id.StartsWith(TEXT("bottle_"))) && !Ids.Contains(Id))
				{
					AddError(FString::Printf(TEXT("Falta el texto de %s"), *Id));
				}
			}
		});
	});

	Describe("FExploredProgress", [this]()
	{
		It("exige las cuatro piezas para montar la baliza", [this]()
		{
			FExploredProgress P;
			P.AddBeaconPart(EBeaconPart::Radio);
			P.AddBeaconPart(EBeaconPart::Battery);
			P.AddBeaconPart(EBeaconPart::Antenna);
			TestFalse(TEXT("Faltan piezas"), P.CanBuildBeacon());
			P.AddBeaconPart(EBeaconPart::Flare);
			TestTrue(TEXT("Completa"), P.CanBuildBeacon());
			P.bBeaconBuilt = true;
			TestFalse(TEXT("No se monta dos veces"), P.CanBuildBeacon());
			TestFalse(TEXT("Sin encender no hay rescate"), P.CanTriggerRescue());
			P.bBeaconActivated = true;
			TestTrue(TEXT("Rescate"), P.CanTriggerRescue());
		});

		It("solo permite quedarse con el diario Halden completo", [this]()
		{
			FExploredProgress P;
			P.bBeaconBuilt = true;
			P.bBeaconActivated = true;
			TestFalse(TEXT("Sin páginas"), P.CanChooseStay());
			for (int32 I = 1; I <= FExploredProgress::TotalHaldenPages; ++I)
			{
				P.Discover(FName(*FString::Printf(TEXT("halden_%02d"), I)));
			}
			TestTrue(TEXT("Con todas"), P.CanChooseStay());
		});

		It("cuenta el diario y no duplica descubrimientos", [this]()
		{
			FExploredProgress P;
			TestTrue(TEXT("Nuevo"), P.Discover(TEXT("ines_01")));
			TestFalse(TEXT("Repetido"), P.Discover(TEXT("ines_01")));
			TestTrue(TEXT("Algo de progreso"), P.JournalCompletion() > 0.0f && P.JournalCompletion() < 2.0f);
			TestFalse(TEXT("No completo"), P.IsJournalComplete());
		});

		It("exige brújula, canoa e isla emergida para la Travesía", [this]()
		{
			FExploredProgress P;
			P.bStarCompassDeciphered = true;
			P.bOutriggerCanoeBuilt = true;
			TestFalse(TEXT("Falta la isla"), P.CanTriggerVoyage());
			P.bReachedEmergedIsland = true;
			TestTrue(TEXT("Travesía"), P.CanTriggerVoyage());
		});
	});
}

#endif
