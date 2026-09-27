#include "Misc/AutomationTest.h"

#include "Exploration/ExplorationContentModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ExplorationContentSpecDetail
{
	/** Un lugar válido de la isla del Amaraje, como el «landing_laguna_amaraje» de exploration.json. */
	FExplorationLandmark ValidLandmark(const TCHAR* Id)
	{
		FExplorationLandmark L;
		L.Id = FName(Id);
		L.IslandId = TEXT("landing");
		L.Kind = TEXT("naufragio");
		L.NameEs = TEXT("Laguna del Amaraje");
		L.NameEn = TEXT("Splashdown Lagoon");
		L.AngleDeg = 0.0f;
		L.DistanceFrac = 0.58f;
		L.LinkedPoi = TEXT("WreckFuselage");
		L.MapMark = TEXT("wreck");
		L.AccessEs = TEXT("Bucear en la laguna");
		L.RewardEs = TEXT("Piezas del Albatros");
		return L;
	}
}

BEGIN_DEFINE_SPEC(FExplorationContentSpec, "Explored.Exploration.Content",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FExplorationContentSpec)

void FExplorationContentSpec::Define()
{
	using namespace ExplorationContentSpecDetail;

	Describe("Ids conocidos", [this]()
	{
		It("acepta las siete islas jugables y rechaza la isla oculta", [this]()
		{
			TestTrue(TEXT("landing"), FExplorationCatalog::IsValidIslandId(TEXT("landing")));
			TestTrue(TEXT("mesa"), FExplorationCatalog::IsValidIslandId(TEXT("mesa")));
			TestFalse(TEXT("isla oculta"), FExplorationCatalog::IsValidIslandId(TEXT("isla_oculta")));
			TestFalse(TEXT("vacío"), FExplorationCatalog::IsValidIslandId(NAME_None));
		});

		It("acepta los tipos de lugar del documento de diseño y rechaza el resto", [this]()
		{
			TestTrue(TEXT("cueva"), FExplorationCatalog::IsValidKind(TEXT("cueva")));
			TestTrue(TEXT("arco_marino"), FExplorationCatalog::IsValidKind(TEXT("arco_marino")));
			TestFalse(TEXT("desconocido"), FExplorationCatalog::IsValidKind(TEXT("torre_vigia")));
		});

		It("un sello de mapa vacío es válido; uno desconocido no", [this]()
		{
			TestTrue(TEXT("vacío"), FExplorationCatalog::IsValidMapMark(NAME_None));
			TestTrue(TEXT("wreck"), FExplorationCatalog::IsValidMapMark(TEXT("wreck")));
			TestFalse(TEXT("tesoro"), FExplorationCatalog::IsValidMapMark(TEXT("tesoro")));
		});
	});

	Describe("FExplorationCatalog::Validate", [this]()
	{
		It("acepta un catálogo con todos los campos correctos", [this]()
		{
			FExplorationCatalog Catalog;
			Catalog.Landmarks.Add(ValidLandmark(TEXT("landing_laguna_amaraje")));
			Catalog.Landmarks.Add(ValidLandmark(TEXT("landing_playa_ala")));
			TArray<FString> Errors;
			TestTrue(TEXT("Sin errores"), Catalog.Validate(Errors));
			TestEqual(TEXT("Lista vacía"), Errors.Num(), 0);
		});

		It("rechaza ids repetidos", [this]()
		{
			FExplorationCatalog Catalog;
			Catalog.Landmarks.Add(ValidLandmark(TEXT("landing_laguna_amaraje")));
			Catalog.Landmarks.Add(ValidLandmark(TEXT("landing_laguna_amaraje")));
			TArray<FString> Errors;
			TestFalse(TEXT("Con errores"), Catalog.Validate(Errors));
			TestTrue(TEXT("Menciona el id repetido"), Errors.ContainsByPredicate([](const FString& E) { return E.Contains(TEXT("repetido")); }));
		});

		It("rechaza isla, tipo o sello desconocidos", [this]()
		{
			FExplorationCatalog Catalog;
			FExplorationLandmark BadIsland = ValidLandmark(TEXT("a"));
			BadIsland.IslandId = TEXT("atlantida");
			Catalog.Landmarks.Add(BadIsland);
			TArray<FString> Errors;
			TestFalse(TEXT("Isla desconocida"), Catalog.Validate(Errors));
		});

		It("rechaza nombres, acceso o recompensa vacíos", [this]()
		{
			FExplorationCatalog Catalog;
			FExplorationLandmark NoAccess = ValidLandmark(TEXT("a"));
			NoAccess.AccessEs.Reset();
			Catalog.Landmarks.Add(NoAccess);
			TArray<FString> Errors;
			TestFalse(TEXT("Sin acceso"), Catalog.Validate(Errors));
		});

		It("rechaza ángulo o distancia fuera de rango", [this]()
		{
			FExplorationCatalog Catalog;
			FExplorationLandmark BadAngle = ValidLandmark(TEXT("a"));
			BadAngle.AngleDeg = 400.0f;
			Catalog.Landmarks.Add(BadAngle);
			TArray<FString> Errors;
			TestFalse(TEXT("Ángulo fuera de rango"), Catalog.Validate(Errors));

			FExplorationCatalog Catalog2;
			FExplorationLandmark BadDistance = ValidLandmark(TEXT("b"));
			BadDistance.DistanceFrac = 1.5f;
			Catalog2.Landmarks.Add(BadDistance);
			TArray<FString> Errors2;
			TestFalse(TEXT("Distancia fuera de rango"), Catalog2.Validate(Errors2));
		});
	});

	Describe("Consultas", [this]()
	{
		It("cuenta los lugares de una isla y encuentra uno por id", [this]()
		{
			FExplorationCatalog Catalog;
			Catalog.Landmarks.Add(ValidLandmark(TEXT("landing_laguna_amaraje")));
			Catalog.Landmarks.Add(ValidLandmark(TEXT("landing_playa_ala")));
			FExplorationLandmark Other = ValidLandmark(TEXT("emerald_cascada"));
			Other.IslandId = TEXT("emerald");
			Catalog.Landmarks.Add(Other);

			TestEqual(TEXT("Landing"), Catalog.CountForIsland(TEXT("landing")), 2);
			TestEqual(TEXT("Emerald"), Catalog.CountForIsland(TEXT("emerald")), 1);
			TestEqual(TEXT("Isla sin lugares"), Catalog.CountForIsland(TEXT("teeth")), 0);
			TestNotNull(TEXT("Encuentra por id"), Catalog.FindLandmark(TEXT("landing_playa_ala")));
			TestNull(TEXT("No encuentra lo que no existe"), Catalog.FindLandmark(TEXT("no_existe")));
		});
	});
}

#endif
