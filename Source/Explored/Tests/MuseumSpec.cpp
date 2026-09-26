#include "Misc/AutomationTest.h"

#include "Ruins/MuseumModel.h"
#include "Ruins/RuinsModel.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/PointsOfInterest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MuseumSpecDetail
{
	constexpr int32 ShelfKey = 100;
	constexpr int32 CaseKey = 200;
	constexpr int32 PanelKey = 300;

	FArtifactDef Artifact(const TCHAR* Id, EArtifactSize Size, EArtifactProvenance Provenance)
	{
		FArtifactDef A;
		A.Id = FName(Id);
		A.Kind = FName(TEXT("hook_bone"));
		A.NameEs = Id;
		A.NameEn = Id;
		A.Size = Size;
		A.Provenance = Provenance;
		return A;
	}

	/**
	 * Catálogo de prueba con la forma de artifacts.json: doce pequeños, dos medianos y dos
	 * grandes; estantería de 3 × 4 huecos, vitrina de uno y panel de pared para los grandes.
	 */
	FTreasureCatalog TestCatalog()
	{
		FTreasureCatalog C;
		for (int32 I = 0; I < 12; ++I)
		{
			const EArtifactProvenance P = I % 3 == 0 ? EArtifactProvenance::RitualCave : I % 3 == 1 ? EArtifactProvenance::Marae : EArtifactProvenance::SunkenRuin;
			C.Artifacts.Add(Artifact(*FString::Printf(TEXT("small_%02d"), I), EArtifactSize::Small, P));
		}
		C.Artifacts.Add(Artifact(TEXT("medium_a"), EArtifactSize::Medium, EArtifactProvenance::Shipwreck));
		C.Artifacts.Add(Artifact(TEXT("medium_b"), EArtifactSize::Medium, EArtifactProvenance::Marae));
		C.Artifacts.Add(Artifact(TEXT("large_a"), EArtifactSize::Large, EArtifactProvenance::Shipwreck));
		C.Artifacts.Add(Artifact(TEXT("large_b"), EArtifactSize::Large, EArtifactProvenance::RitualCave));

		FDisplayDef Shelf;
		Shelf.Id = FName(TEXT("estanteria_museo"));
		for (int32 Level = 0; Level < 4; ++Level)
		{
			for (int32 Column = 0; Column < 3; ++Column)
			{
				FDisplaySlotDef S;
				S.MaxSize = EArtifactSize::Medium;
				S.Offset = FVector((Column - 1) * 60.0, 0.0, 14.0 + Level * 45.0);
				Shelf.Slots.Add(S);
			}
		}
		FDisplayDef Case;
		Case.Id = FName(TEXT("vitrina_museo"));
		Case.Slots.Add({EArtifactSize::Medium, FVector(0.0, 0.0, 97.0)});
		FDisplayDef Panel;
		Panel.Id = FName(TEXT("panel_museo"));
		Panel.Slots.Add({EArtifactSize::Large, FVector(0.0, 5.0, 120.0)});
		C.Displays = {Shelf, Case, Panel};
		return C;
	}

	FMuseumModel FoundAll()
	{
		FMuseumModel M(TestCatalog());
		for (const FArtifactDef& A : M.GetCatalog().Artifacts)
		{
			M.MarkFound(A.Id, TEXT("ruin_emerald"), 1);
		}
		return M;
	}
}

BEGIN_DEFINE_SPEC(FMuseumSpec, "Explored.Museum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FMuseumSpec)

void FMuseumSpec::Define()
{
	using namespace MuseumSpecDetail;

	Describe("Catálogo", [this]()
	{
		It("registra hallazgos con su procedencia y fotos sin duplicar", [this]()
		{
			FMuseumModel M(TestCatalog());
			TestFalse(TEXT("Desconocido"), M.MarkFound(TEXT("tesoro_falso"), TEXT("ruin_mesa"), 6));
			TestTrue(TEXT("Nuevo"), M.MarkFound(TEXT("small_00"), TEXT("ruin_mesa"), 6));
			TestFalse(TEXT("Repetido"), M.MarkFound(TEXT("small_00"), TEXT("shipwreck"), 5));
			const FArtifactRecord* R = M.FindRecord(TEXT("small_00"));
			if (TestNotNull(TEXT("Registro"), R))
			{
				TestEqual(TEXT("Procedencia"), R->FoundAt, FName(TEXT("ruin_mesa")));
				TestEqual(TEXT("Isla"), R->FoundIsland, 6);
			}

			// La cámara desechable registra un tesoro sin recogerlo (GDD §8.11).
			TestTrue(TEXT("Foto nueva"), M.MarkPhotographed(TEXT("large_a")));
			TestFalse(TEXT("Foto repetida"), M.MarkPhotographed(TEXT("large_a")));
			TestTrue(TEXT("Fotografiado"), M.IsPhotographed(TEXT("large_a")));
			TestFalse(TEXT("No hallado"), M.IsFound(TEXT("large_a")));
			TestTrue(TEXT("Registrado"), M.IsRegistered(TEXT("large_a")));
			TestFalse(TEXT("Foto de algo inexistente"), M.MarkPhotographed(TEXT("tesoro_falso")));
			TestEqual(TEXT("Hallados"), M.CountFound(), 1);
			TestEqual(TEXT("Registrados"), M.CountRegistered(), 2);
			TestEqual(TEXT("Completado"), M.CatalogCompletion(), 2.0f / M.GetCatalog().Artifacts.Num(), 0.0001f);
			TestFalse(TEXT("Incompleto"), M.IsCatalogComplete());
		});

		It("se completa con hallazgos o fotos de todos los tesoros", [this]()
		{
			FMuseumModel M(TestCatalog());
			const TArray<FArtifactDef>& All = M.GetCatalog().Artifacts;
			for (int32 I = 0; I < All.Num(); ++I)
			{
				if (I % 2 == 0)
				{
					M.MarkFound(All[I].Id, TEXT("ruin_teeth"), 3);
				}
				else
				{
					M.MarkPhotographed(All[I].Id);
				}
			}
			TestTrue(TEXT("Completo"), M.IsCatalogComplete());
			TestEqual(TEXT("100 %"), M.CatalogCompletion(), 1.0f, 0.0001f);
		});

		It("todo tesoro cabe en algún mueble", [this]()
		{
			const FTreasureCatalog C = TestCatalog();
			for (const FArtifactDef& A : C.Artifacts)
			{
				TestTrue(FString::Printf(TEXT("%s expuesto"), *A.Id.ToString()), C.CanEverDisplay(A));
			}
		});

		It("lee los ids del JSON sin distinguir mayúsculas", [this]()
		{
			EArtifactSize Size = EArtifactSize::Small;
			TestTrue(TEXT("Grande"), FTreasureCatalog::ParseSize(TEXT("grande"), Size) && Size == EArtifactSize::Large);
			EArtifactProvenance P = EArtifactProvenance::Marae;
			TestTrue(TEXT("Cueva"), FTreasureCatalog::ParseProvenance(TEXT("cueva_ritual"), P) && P == EArtifactProvenance::RitualCave);
			EArtifactRarity R = EArtifactRarity::Common;
			TestTrue(TEXT("Único"), FTreasureCatalog::ParseRarity(TEXT("unico"), R) && R == EArtifactRarity::Unique);
			TestFalse(TEXT("Basura"), FTreasureCatalog::ParseSize(TEXT("enorme"), Size));
		});
	});

	Describe("Museo", [this]()
	{
		It("aplica las reglas de colocación", [this]()
		{
			FMuseumModel M(TestCatalog());
			TestTrue(TEXT("Estantería"), M.AddDisplay(ShelfKey, TEXT("estanteria_museo")) == EMuseumResult::Ok);
			TestTrue(TEXT("Clave repetida"), M.AddDisplay(ShelfKey, TEXT("vitrina_museo")) == EMuseumResult::DuplicateDisplay);
			TestTrue(TEXT("Mueble desconocido"), M.AddDisplay(999, TEXT("altar_de_oro")) == EMuseumResult::UnknownDisplay);
			TestTrue(TEXT("Panel"), M.AddDisplay(PanelKey, TEXT("panel_museo")) == EMuseumResult::Ok);

			TestTrue(TEXT("Sin hallar"), M.Place(TEXT("small_01"), ShelfKey, 0) == EMuseumResult::NotFound);
			TestTrue(TEXT("Desconocido"), M.Place(TEXT("tesoro_falso"), ShelfKey, 0) == EMuseumResult::UnknownArtifact);
			M.MarkFound(TEXT("small_01"), TEXT("ruin_landing"), 3);
			M.MarkFound(TEXT("small_02"), TEXT("ruin_landing"), 3);
			M.MarkFound(TEXT("large_a"), TEXT("shipwreck"), 5);
			TestTrue(TEXT("Hueco inválido"), M.Place(TEXT("small_01"), ShelfKey, 12) == EMuseumResult::InvalidSlot);
			TestTrue(TEXT("Mueble inexistente"), M.Place(TEXT("small_01"), 12345, 0) == EMuseumResult::UnknownDisplay);
			TestTrue(TEXT("Coloca"), M.Place(TEXT("small_01"), ShelfKey, 0) == EMuseumResult::Ok);
			TestTrue(TEXT("Ya expuesto"), M.Place(TEXT("small_01"), ShelfKey, 1) == EMuseumResult::AlreadyExhibited);
			TestTrue(TEXT("Ocupado"), M.Place(TEXT("small_02"), ShelfKey, 0) == EMuseumResult::SlotOccupied);
			TestTrue(TEXT("El remo no cabe en la estantería"), M.Place(TEXT("large_a"), ShelfKey, 5) == EMuseumResult::TooLarge);
			TestTrue(TEXT("El remo va al panel"), M.Place(TEXT("large_a"), PanelKey, 0) == EMuseumResult::Ok);
			TestTrue(TEXT("Expuesto"), M.IsExhibited(TEXT("small_01")));
			TestEqual(TEXT("Dos expuestos"), M.CountExhibited(), 2);

			FName Removed;
			TestTrue(TEXT("Retira"), M.Remove(ShelfKey, 0, Removed) == EMuseumResult::Ok);
			TestEqual(TEXT("Devuelve el tesoro"), Removed, FName(TEXT("small_01")));
			TestTrue(TEXT("Hueco vacío"), M.Remove(ShelfKey, 0, Removed) == EMuseumResult::SlotEmpty);
			TestFalse(TEXT("Ya no expuesto"), M.IsExhibited(TEXT("small_01")));
			TestTrue(TEXT("Se puede volver a colocar en otro hueco"), M.Place(TEXT("small_01"), ShelfKey, 7) == EMuseumResult::Ok);

			TArray<FName> Returned;
			TestTrue(TEXT("Desmonta la estantería"), M.RemoveDisplay(ShelfKey, Returned) == EMuseumResult::Ok);
			TestEqual(TEXT("Devuelve lo expuesto"), Returned.Num(), 1);
			TestEqual(TEXT("Queda el panel"), M.CountExhibited(), 1);
			TestTrue(TEXT("Ya no existe"), M.RemoveDisplay(ShelfKey, Returned) == EMuseumResult::UnknownDisplay);
		});

		It("busca el primer hueco libre donde cabe cada tesoro", [this]()
		{
			FMuseumModel M = FoundAll();
			M.AddDisplay(CaseKey, TEXT("vitrina_museo"));
			M.AddDisplay(PanelKey, TEXT("panel_museo"));
			int32 Key = INDEX_NONE;
			int32 Slot = INDEX_NONE;
			TestTrue(TEXT("Mediano en la vitrina"), M.FindFreeSlot(TEXT("medium_a"), Key, Slot) && Key == CaseKey && Slot == 0);
			M.Place(TEXT("medium_a"), Key, Slot);
			TestTrue(TEXT("El otro mediano va al panel"), M.FindFreeSlot(TEXT("medium_b"), Key, Slot) && Key == PanelKey);
			M.Place(TEXT("large_a"), PanelKey, 0);
			TestFalse(TEXT("No queda sitio para el otro grande"), M.FindFreeSlot(TEXT("large_b"), Key, Slot));
		});

		It("cuenta los expuestos y concede «Coleccionista» a los diez", [this]()
		{
			FMuseumModel M = FoundAll();
			M.AddDisplay(ShelfKey, TEXT("estanteria_museo"));
			const TArray<FArtifactDef>& All = M.GetCatalog().Artifacts;
			int32 Placed = 0;
			for (const FArtifactDef& A : All)
			{
				int32 Key = INDEX_NONE;
				int32 Slot = INDEX_NONE;
				if (Placed < FMuseumModel::CollectorThreshold && M.FindFreeSlot(A.Id, Key, Slot))
				{
					TestFalse(TEXT("Aún no"), M.HasCollectorAchievement());
					TestTrue(TEXT("Coloca"), M.Place(A.Id, Key, Slot) == EMuseumResult::Ok);
					++Placed;
				}
			}
			TestEqual(TEXT("Diez expuestos"), M.CountExhibited(), FMuseumModel::CollectorThreshold);
			TestTrue(TEXT("Coleccionista"), M.HasCollectorAchievement());
			FName Removed;
			M.Remove(ShelfKey, 0, Removed);
			TestFalse(TEXT("Retirar uno lo baja de diez"), M.HasCollectorAchievement());
		});

		It("guarda y recupera el museo descartando lo incoherente", [this]()
		{
			FMuseumModel M = FoundAll();
			M.MarkPhotographed(TEXT("large_b"));
			M.AddDisplay(ShelfKey, TEXT("estanteria_museo"));
			M.AddDisplay(CaseKey, TEXT("vitrina_museo"));
			M.Place(TEXT("small_03"), ShelfKey, 4);
			M.Place(TEXT("medium_b"), CaseKey, 0);

			FMuseumModel Loaded(TestCatalog());
			Loaded.LoadState(M.GetState());
			TestTrue(TEXT("Mismo estado"), Loaded.GetState() == M.GetState());
			TestEqual(TEXT("Mismos expuestos"), Loaded.CountExhibited(), 2);

			FMuseumState Dirty = M.GetState();
			Dirty.Displays[0].Slots[5] = FName(TEXT("small_03"));  // Duplicado.
			Dirty.Displays[0].Slots[6] = FName(TEXT("tesoro_falso"));
			Dirty.Displays[1].Slots.Add(FName(TEXT("small_04")));  // Hueco que no existe.
			FDisplayState Unknown;
			Unknown.Key = 999;
			Unknown.DisplayId = FName(TEXT("altar_de_oro"));
			Dirty.Displays.Add(Unknown);
			FArtifactRecord Ghost;
			Ghost.ArtifactId = FName(TEXT("tesoro_falso"));
			Ghost.bFound = true;
			Dirty.Records.Add(Ghost);
			FMuseumModel Clean(TestCatalog());
			Clean.LoadState(Dirty);
			TestTrue(TEXT("Descarta lo incoherente"), Clean.GetState() == M.GetState());

			// Un tesoro expuesto que el guardado no da por hallado no se expone.
			FMuseumState NotFound = M.GetState();
			NotFound.Records.RemoveAll([](const FArtifactRecord& R) { return R.ArtifactId == FName(TEXT("small_03")); });
			FMuseumModel Strict(TestCatalog());
			Strict.LoadState(NotFound);
			TestFalse(TEXT("No expone lo no hallado"), Strict.IsExhibited(TEXT("small_03")));
		});
	});

	Describe("FTreasurePlacement", [this]()
	{
		It("reparte cada tesoro en un lugar de su procedencia, de forma determinista", [this]()
		{
			const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed);
			TArray<FPointOfInterest> Pois;
			for (int32 I = 0; I < Layout.Islands.Num(); ++I)
			{
				FPointOfInterest P;
				P.Type = EPoiType::Petroglyph;
				P.IslandIndex = I;
				P.ContentId = FName(*FString::Printf(TEXT("petro_%02d"), I + 1));
				P.Location = FVector(Layout.Islands[I].Center.X, Layout.Islands[I].Center.Y, 15.0);
				Pois.Add(P);
			}
			FPointOfInterest Wreck;
			Wreck.Type = EPoiType::Shipwreck;
			Wreck.IslandIndex = 6;
			Wreck.Location = FVector(100.0, 200.0, -4.0);
			Wreck.bUnderwater = true;
			Pois.Add(Wreck);

			const FRuinsLayout Ruins = FRuinsLayout::Generate(Layout, Pois);
			const FTreasureCatalog Catalog = TestCatalog();
			const TArray<FArtifactPlacement> A = FTreasurePlacement::Generate(Ruins, Pois, Catalog);
			const TArray<FArtifactPlacement> B = FTreasurePlacement::Generate(Ruins, Pois, Catalog);
			TestEqual(TEXT("Uno por tesoro"), A.Num(), Catalog.Artifacts.Num());
			for (int32 I = 0; I < A.Num(); ++I)
			{
				const FArtifactDef& Def = Catalog.Artifacts[I];
				TestEqual(TEXT("Mismo orden"), A[I].ArtifactId, Def.Id);
				TestTrue(TEXT("Determinista"), A[I].Location.Equals(B[I].Location, 0.001) && A[I].PlaceId == B[I].PlaceId);
				TestFalse(TEXT("Tiene lugar"), A[I].PlaceId.IsNone());
				switch (Def.Provenance)
				{
				case EArtifactProvenance::Shipwreck:
					TestEqual(TEXT("En el pecio"), A[I].PlaceId, FName(TEXT("shipwreck")));
					TestTrue(TEXT("Bajo el agua"), A[I].bUnderwater);
					break;
				case EArtifactProvenance::RitualCave:
				{
					const int32 Site = Ruins.FindSite(A[I].PlaceId);
					TestTrue(TEXT("En una ruina con cueva"), Site != INDEX_NONE && Ruins.Sites[Site].Elements.ContainsByPredicate(
						[](const FRuinElement& E) { return E.Kind == ERuinElementKind::RitualCave; }));
					break;
				}
				case EArtifactProvenance::SunkenRuin:
					TestTrue(TEXT("Sumergida con marea viva"), A[I].bUnderwater && A[I].bNeedsSpringTide);
					break;
				default:
					TestTrue(TEXT("En un marae de isla"), Ruins.FindSite(A[I].PlaceId) != INDEX_NONE && A[I].PlaceId != FName(TEXT("ruin_compass")));
					break;
				}
			}
			// Dos tesoros no comparten punto exacto.
			for (int32 I = 0; I < A.Num(); ++I)
			{
				for (int32 J = I + 1; J < A.Num(); ++J)
				{
					if (A[I].Location.Equals(A[J].Location, 0.01))
					{
						AddError(FString::Printf(TEXT("%s y %s en el mismo punto"), *A[I].ArtifactId.ToString(), *A[J].ArtifactId.ToString()));
					}
				}
			}

			// Sin pecio, los tesoros del pecio quedan en un marae.
			Pois.Pop();
			const TArray<FArtifactPlacement> NoWreck = FTreasurePlacement::Generate(Ruins, Pois, Catalog);
			for (const FArtifactPlacement& P : NoWreck)
			{
				TestNotEqual(TEXT("Sin pecio"), P.PlaceId, FName(TEXT("shipwreck")));
			}
		});
	});
}

#endif
