#include "Misc/AutomationTest.h"

#include "Ruins/RuinsModel.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/PointsOfInterest.h"
#include "WorldGen/TerrainDensity.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace RuinsSpecDetail
{
	constexpr int32 SeedsToCheck = 200;

	/**
	 * Puntos de interés sintéticos (sin terreno) con la misma forma que los de FPoiLayout:
	 * brújula en el Humo, cascada en Esmeralda, tres petroglifos por isla y dos de cueva sin isla.
	 */
	TArray<FPointOfInterest> SyntheticPois(const FArchipelagoLayout& Layout)
	{
		TArray<FPointOfInterest> Pois;
		int32 Petro = 1;
		for (int32 I = 0; I < Layout.Islands.Num(); ++I)
		{
			const FIslandDesc& Island = Layout.Islands[I];
			const FVector Center(Island.Center.X, Island.Center.Y, 20.0);
			if (Island.Archetype == EIslandArchetype::Smoke)
			{
				FPointOfInterest Compass;
				Compass.Type = EPoiType::StarCompass;
				Compass.IslandIndex = I;
				Compass.Location = Center + FVector(0.0, 0.0, 300.0);
				Pois.Add(Compass);
			}
			if (Island.Archetype == EIslandArchetype::Emerald)
			{
				FPointOfInterest Waterfall;
				Waterfall.Type = EPoiType::Waterfall;
				Waterfall.IslandIndex = I;
				Waterfall.Location = Center + FVector(Island.Radius * 0.4, 0.0, 0.0);
				Pois.Add(Waterfall);
			}
			for (int32 K = 0; K < 3; ++K)
			{
				FPointOfInterest P;
				P.Type = EPoiType::Petroglyph;
				P.IslandIndex = I;
				P.ContentId = FName(*FString::Printf(TEXT("petro_%02d"), Petro++));
				P.Location = Center + FVector(Island.Radius * 0.2 * (K + 1), Island.Radius * 0.1 * K, 0.0);
				Pois.Add(P);
			}
		}
		// Petroglifos de cueva: sin isla, junto a la costa de la primera isla.
		for (int32 K = 0; K < 2; ++K)
		{
			const FIslandDesc& Island = Layout.Islands[0];
			FPointOfInterest P;
			P.Type = EPoiType::Petroglyph;
			P.ContentId = FName(*FString::Printf(TEXT("petro_%02d"), Petro++));
			P.Location = FVector(Island.Center.X + Island.Radius * 0.9, Island.Center.Y + K * 10.0, -5.0);
			Pois.Add(P);
		}
		return Pois;
	}

	FRuinsLayout SyntheticRuins(uint32 Seed)
	{
		const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(Seed);
		return FRuinsLayout::Generate(Layout, SyntheticPois(Layout));
	}

	/** Disposición y puntos de interés reales de la semilla oficial (se calculan una vez). */
	const TArray<FPointOfInterest>& OfficialPois()
	{
		static const TArray<FPointOfInterest> Pois = []()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
			return FPoiLayout::Generate(Density);
		}();
		return Pois;
	}

	/** Descubre lo mínimo para completar una ruina: obligatorios y los petroglifos exigidos. */
	FRuinDiscovery CompleteSite(FRuinsModel& Model, int32 SiteIndex)
	{
		FRuinDiscovery Last;
		const FRuinSite& Site = Model.GetLayout().Sites[SiteIndex];
		int32 Petroglyphs = 0;
		for (const FRuinElement& E : Site.Elements)
		{
			const bool bNeeded = E.bRequired || (E.Kind == ERuinElementKind::Petroglyph && Petroglyphs++ < Site.RequiredPetroglyphs);
			if (bNeeded)
			{
				const FRuinDiscovery D = Model.Discover(E.Id);
				if (D.bSiteCompleted)
				{
					Last = D;
				}
			}
		}
		return Last;
	}

	bool SameLayout(const FRuinsLayout& A, const FRuinsLayout& B)
	{
		if (A.Sites.Num() != B.Sites.Num() || !A.HiddenIslandCenter.Equals(B.HiddenIslandCenter, 0.01) || A.DepartureIsland != B.DepartureIsland)
		{
			return false;
		}
		for (int32 I = 0; I < A.Sites.Num(); ++I)
		{
			const FRuinSite& SA = A.Sites[I];
			const FRuinSite& SB = B.Sites[I];
			if (SA.Id != SB.Id || SA.Teaches != SB.Teaches || SA.StarPathTarget != SB.StarPathTarget || SA.Elements.Num() != SB.Elements.Num()
				|| SA.RequiredPetroglyphs != SB.RequiredPetroglyphs)
			{
				return false;
			}
			for (int32 E = 0; E < SA.Elements.Num(); ++E)
			{
				if (SA.Elements[E].Id != SB.Elements[E].Id || !SA.Elements[E].Location.Equals(SB.Elements[E].Location, 0.001))
				{
					return false;
				}
			}
		}
		return true;
	}
}

BEGIN_DEFINE_SPEC(FRuinsSpec, "Explored.Ruins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FRuinsSpec)

void FRuinsSpec::Define()
{
	using namespace RuinsSpecDetail;

	Describe("FRuinsLayout", [this]()
	{
		It("crea una ruina por isla y la brújula estelar con los puntos de interés reales", [this]()
		{
			const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed);
			const TArray<FPointOfInterest>& Pois = OfficialPois();
			const FRuinsLayout Ruins = FRuinsLayout::Generate(Layout, Pois);
			TestEqual(TEXT("Ruinas"), Ruins.Sites.Num(), Layout.Islands.Num() + 1);
			TestTrue(TEXT("Brújula estelar"), Ruins.FindSite(TEXT("ruin_compass")) != INDEX_NONE);

			// Cada petroglifo pertenece a una sola ruina.
			int32 Petroglyphs = 0;
			for (const FPointOfInterest& P : Pois)
			{
				if (P.Type != EPoiType::Petroglyph)
				{
					continue;
				}
				++Petroglyphs;
				int32 Owners = 0;
				for (const FRuinSite& Site : Ruins.Sites)
				{
					Owners += Site.FindElement(P.ContentId) != INDEX_NONE ? 1 : 0;
				}
				TestEqual(FString::Printf(TEXT("Dueños de %s"), *P.ContentId.ToString()), Owners, 1);
			}
			int32 InSites = 0;
			for (const FRuinSite& Site : Ruins.Sites)
			{
				InSites += Site.CountPetroglyphs();
				TestTrue(FString::Printf(TEXT("%s pide al menos un elemento"), *Site.Id.ToString()), Site.Elements.Num() >= 2);
			}
			TestEqual(TEXT("Todos los petroglifos en alguna ruina"), InSites, Petroglyphs);
			TestTrue(TEXT("Se puede aprender todo"), Ruins.CountStarPathSites() >= FRuinsLayout::RequiredStarPaths);
		});

		It("permite aprender las cinco técnicas y reunir los caminos de estrellas con cualquier semilla", [this]()
		{
			for (uint32 Seed = 1; Seed <= SeedsToCheck; ++Seed)
			{
				for (const bool bWithPois : {true, false})
				{
					const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(Seed);
					const FRuinsLayout Ruins = FRuinsLayout::Generate(Layout, bWithPois ? SyntheticPois(Layout) : TArray<FPointOfInterest>());
					for (uint8 T = 0; T < static_cast<uint8>(EWayfindingTechnique::Count); ++T)
					{
						if (Ruins.CountSitesTeaching(static_cast<EWayfindingTechnique>(T)) < 1)
						{
							AddError(FString::Printf(TEXT("Semilla %u: nadie enseña %s"), Seed, LexToString(static_cast<EWayfindingTechnique>(T))));
							return;
						}
					}
					if (Ruins.CountStarPathSites() < FRuinsLayout::RequiredStarPaths)
					{
						AddError(FString::Printf(TEXT("Semilla %u: solo %d caminos de estrellas"), Seed, Ruins.CountStarPathSites()));
						return;
					}
					// Los destinos de los caminos son distintos (si no, no sumarían) y nunca la propia isla.
					TArray<int32> Targets;
					for (const FRuinSite& Site : Ruins.Sites)
					{
						if (Site.Teaches != EWayfindingTechnique::StarPath)
						{
							TestEqual(TEXT("Sin destino"), Site.StarPathTarget, INDEX_NONE);
							continue;
						}
						const bool bValid = Site.StarPathTarget == FRuinsLayout::HiddenIslandIndex
							|| (Layout.Islands.IsValidIndex(Site.StarPathTarget) && Site.StarPathTarget != Site.IslandIndex);
						if (!bValid || Targets.Contains(Site.StarPathTarget))
						{
							AddError(FString::Printf(TEXT("Semilla %u: destino %d inválido o repetido en %s"), Seed, Site.StarPathTarget, *Site.Id.ToString()));
							return;
						}
						Targets.Add(Site.StarPathTarget);
					}
				}
			}
		});

		It("esconde la isla oculta fuera del mapa y lejos de todas las costas", [this]()
		{
			for (uint32 Seed = 1; Seed <= SeedsToCheck; ++Seed)
			{
				const FRuinsLayout Ruins = SyntheticRuins(Seed);
				const FVector2D H = Ruins.HiddenIslandCenter;
				if (FMath::Max(FMath::Abs(H.X), FMath::Abs(H.Y)) <= FArchipelagoLayout::WorldHalfExtent)
				{
					AddError(FString::Printf(TEXT("Semilla %u: la isla oculta está dentro del mapa"), Seed));
					return;
				}
				for (const FIslandDesc& Island : Ruins.Islands)
				{
					if (FVector2D::Distance(H, Island.Center) - Island.Radius < FArchipelagoLayout::MinChannel * 2.0f)
					{
						AddError(FString::Printf(TEXT("Semilla %u: la isla oculta roza %s"), Seed, LexToString(Island.Archetype)));
						return;
					}
				}
				TestTrue(TEXT("Isla de salida"), Ruins.Islands.IsValidIndex(Ruins.DepartureIsland));
				const int32 Compass = Ruins.FindSite(TEXT("ruin_compass"));
				if (TestTrue(TEXT("Hay brújula"), Compass != INDEX_NONE))
				{
					TestEqual(TEXT("La brújula apunta a la isla oculta"), Ruins.Sites[Compass].StarPathTarget, FRuinsLayout::HiddenIslandIndex);
				}
			}
		});

		It("es determinista y la asignación cambia con la semilla", [this]()
		{
			TSet<FString> Assignments;
			for (uint32 Seed = 1; Seed <= 40; ++Seed)
			{
				const FRuinsLayout A = SyntheticRuins(Seed);
				const FRuinsLayout B = SyntheticRuins(Seed);
				if (!SameLayout(A, B))
				{
					AddError(FString::Printf(TEXT("Semilla %u no es determinista"), Seed));
					return;
				}
				FString Key;
				for (const FRuinSite& Site : A.Sites)
				{
					Key += LexToString(Site.Teaches);
				}
				Assignments.Add(Key);
			}
			TestTrue(TEXT("Variedad de asignaciones"), Assignments.Num() > 5);

			const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed);
			TestTrue(TEXT("Oficial determinista"), SameLayout(FRuinsLayout::Generate(Layout, OfficialPois()), FRuinsLayout::Generate(Layout, OfficialPois())));
		});
	});

	Describe("FRuinsModel", [this]()
	{
		It("completa una ruina solo con sus obligatorios y los petroglifos exigidos", [this]()
		{
			FRuinsModel Model(SyntheticRuins(7));
			const int32 SiteIndex = Model.GetLayout().FindSite(TEXT("ruin_emerald"));
			if (!TestTrue(TEXT("Existe Esmeralda"), SiteIndex != INDEX_NONE))
			{
				return;
			}
			const FRuinSite& Site = Model.GetLayout().Sites[SiteIndex];
			TestEqual(TEXT("Petroglifos exigidos"), Site.RequiredPetroglyphs, FRuinsLayout::PetroglyphsPerSite);
			TestTrue(TEXT("Esmeralda tiene cueva ritual"), Site.Elements.ContainsByPredicate([](const FRuinElement& E) { return E.Kind == ERuinElementKind::RitualCave; }));

			// Todos los obligatorios sin petroglifos: aún no.
			for (const FRuinElement& E : Site.Elements)
			{
				if (E.bRequired)
				{
					TestFalse(TEXT("No completa sin petroglifos"), Model.Discover(E.Id).bSiteCompleted);
				}
			}
			TestFalse(TEXT("Incompleta"), Model.IsSiteComplete(SiteIndex));
			int32 Done = 0;
			int32 Required = 0;
			Model.GetSiteProgress(SiteIndex, Done, Required);
			TestEqual(TEXT("Faltan los petroglifos"), Required - Done, Site.RequiredPetroglyphs);

			// Cualquier par de petroglifos de la ruina vale: se usan los dos últimos.
			TArray<FName> Petroglyphs;
			for (const FRuinElement& E : Site.Elements)
			{
				if (E.Kind == ERuinElementKind::Petroglyph)
				{
					Petroglyphs.Add(E.Id);
				}
			}
			TestFalse(TEXT("Uno no basta"), Model.Discover(Petroglyphs.Last()).bSiteCompleted);
			const FRuinDiscovery D = Model.Discover(Petroglyphs.Last(1));
			TestTrue(TEXT("Completa"), D.bSiteCompleted);
			TestTrue(TEXT("Aprende la técnica"), D.bTechniqueLearned);
			TestTrue(TEXT("Técnica de la ruina"), D.Technique == Site.Teaches);
			TestTrue(TEXT("Conoce la técnica"), Model.KnowsTechnique(Site.Teaches));
			TestTrue(TEXT("Marcada"), Model.IsSiteComplete(SiteIndex));

			// Un petroglifo más no vuelve a completarla.
			const FRuinDiscovery Extra = Model.Discover(Petroglyphs[0]);
			TestTrue(TEXT("Descubrimiento nuevo"), Extra.bNew);
			TestFalse(TEXT("No se completa dos veces"), Extra.bSiteCompleted);
		});

		It("ignora ids desconocidos y repetidos", [this]()
		{
			FRuinsModel Model(SyntheticRuins(3));
			TestFalse(TEXT("Desconocido"), Model.Discover(TEXT("petro_99")).bNew);
			TestFalse(TEXT("Vacío"), Model.Discover(NAME_None).bNew);
			const FName Statue = Model.GetLayout().Sites[0].Elements[0].Id;
			TestTrue(TEXT("Primero"), Model.Discover(Statue).bNew);
			TestFalse(TEXT("Repetido"), Model.Discover(Statue).bNew);
			TestEqual(TEXT("Un descubrimiento"), Model.GetState().DiscoveredElements.Num(), 1);
		});

		It("solo deja zarpar a la isla oculta con suficientes caminos de estrellas", [this]()
		{
			FRuinsModel Model(SyntheticRuins(11));
			const FRuinsLayout& Ruins = Model.GetLayout();
			// Primero las ruinas que no enseñan caminos: no acercan a la isla oculta.
			for (int32 I = 0; I < Ruins.Sites.Num(); ++I)
			{
				if (Ruins.Sites[I].Teaches != EWayfindingTechnique::StarPath)
				{
					CompleteSite(Model, I);
				}
			}
			TestEqual(TEXT("Cero caminos"), Model.CountStarPaths(), 0);
			TestFalse(TEXT("No puede zarpar"), Model.CanSailToHiddenIsland());
			int32 Paths = 0;
			for (int32 I = 0; I < Ruins.Sites.Num(); ++I)
			{
				if (Ruins.Sites[I].Teaches != EWayfindingTechnique::StarPath)
				{
					continue;
				}
				TestEqual(TEXT("Antes de completarla"), Model.CanSailToHiddenIsland(), Paths >= FRuinsLayout::RequiredStarPaths);
				const FRuinDiscovery D = CompleteSite(Model, I);
				++Paths;
				TestEqual(TEXT("Destino del camino"), D.StarPathTarget, Ruins.Sites[I].StarPathTarget);
				TestTrue(TEXT("Conoce el camino"), Model.HasStarPathTo(Ruins.Sites[I].StarPathTarget));
				TestEqual(TEXT("Caminos"), Model.CountStarPaths(), Paths);
				TestEqual(TEXT("Umbral"), Model.CanSailToHiddenIsland(), Paths >= FRuinsLayout::RequiredStarPaths);
				// Solo el primer camino de estrellas enseña la técnica.
				TestEqual(TEXT("Técnica nueva solo la primera vez"), D.bTechniqueLearned, Paths == 1);
			}
			TestTrue(TEXT("Wayfinder"), Model.KnowsAllTechniques());
		});

		It("anota en el mapa lo que enseña cada técnica, con rumbos coherentes", [this]()
		{
			FRuinsModel Model(SyntheticRuins(5));
			const FRuinsLayout& Ruins = Model.GetLayout();
			TestEqual(TEXT("Sin técnicas no hay anotaciones"), Model.BuildAnnotations().Num(), 0);
			for (int32 I = 0; I < Ruins.Sites.Num(); ++I)
			{
				CompleteSite(Model, I);
			}
			const TArray<FWayfindingAnnotation> All = Model.BuildAnnotations();
			for (uint8 T = 0; T < static_cast<uint8>(EWayfindingTechnique::Count); ++T)
			{
				const EWayfindingTechnique Technique = static_cast<EWayfindingTechnique>(T);
				TestTrue(FString::Printf(TEXT("Anota %s"), LexToString(Technique)),
					All.ContainsByPredicate([Technique](const FWayfindingAnnotation& A) { return A.Technique == Technique; }));
			}
			int32 StarPaths = 0;
			for (const FWayfindingAnnotation& A : All)
			{
				if (A.BearingDegrees < 0.0f || A.BearingDegrees >= 360.0f || A.DistanceMeters < 0.0f)
				{
					AddError(TEXT("Rumbo o distancia fuera de rango"));
					return;
				}
				if (A.Technique == EWayfindingTechnique::StarPath)
				{
					++StarPaths;
					TestTrue(TEXT("Llega al destino"), A.To.Equals(Ruins.TargetCenter(A.TargetIsland), 0.01));
					TestEqual(TEXT("Rumbo"), A.BearingDegrees, FRuinsModel::BearingDegrees(A.From, A.To), 0.001f);
					TestEqual(TEXT("Distancia"), A.DistanceMeters, static_cast<float>(FVector2D::Distance(A.From, A.To)), 0.01f);
				}
			}
			TestEqual(TEXT("Un camino por ruina que lo enseña"), StarPaths, Ruins.CountStarPathSites());
			TestTrue(TEXT("Nubes sobre el Humo"), All.ContainsByPredicate([&Ruins](const FWayfindingAnnotation& A)
			{
				return A.Technique == EWayfindingTechnique::FixedClouds && Ruins.Islands[A.TargetIsland].Archetype == EIslandArchetype::Smoke;
			}));
			TestEqual(TEXT("Deterministas"), Model.BuildAnnotations().Num(), All.Num());
		});

		It("mide los rumbos como el yaw de Unreal", [this]()
		{
			TestEqual(TEXT("+X"), FRuinsModel::BearingDegrees(FVector2D(0.0, 0.0), FVector2D(10.0, 0.0)), 0.0f, 0.001f);
			TestEqual(TEXT("+Y"), FRuinsModel::BearingDegrees(FVector2D(0.0, 0.0), FVector2D(0.0, 10.0)), 90.0f, 0.001f);
			TestEqual(TEXT("-X"), FRuinsModel::BearingDegrees(FVector2D(0.0, 0.0), FVector2D(-10.0, 0.0)), 180.0f, 0.001f);
			TestEqual(TEXT("-Y"), FRuinsModel::BearingDegrees(FVector2D(0.0, 0.0), FVector2D(0.0, -10.0)), 270.0f, 0.001f);
			TestEqual(TEXT("Mismo punto"), FRuinsModel::BearingDegrees(FVector2D(3.0, 3.0), FVector2D(3.0, 3.0)), 0.0f);
		});

		It("guarda y recupera el estado como datos planos", [this]()
		{
			const FRuinsLayout Ruins = SyntheticRuins(21);
			FRuinsModel Model(Ruins);
			CompleteSite(Model, 2);
			CompleteSite(Model, 0);
			Model.Discover(Ruins.Sites[4].Elements[0].Id);

			FRuinsModel Loaded(Ruins);
			Loaded.LoadState(Model.GetState());
			TestTrue(TEXT("Mismo estado"), Loaded.GetState() == Model.GetState());
			TestEqual(TEXT("Mismo orden de ruinas"), Loaded.GetState().CompletedSites[0], Ruins.Sites[2].Id);
			TestEqual(TEXT("Mismas técnicas"), Loaded.CountKnownTechniques(), Model.CountKnownTechniques());

			// Un guardado con basura: ids desconocidos, repetidos y una ruina que no está completa.
			FRuinsState Dirty = Model.GetState();
			Dirty.DiscoveredElements.Add(TEXT("petro_99"));
			Dirty.DiscoveredElements.Add(Dirty.DiscoveredElements[0]);
			Dirty.CompletedSites.Add(Ruins.Sites[4].Id);
			Dirty.CompletedSites.Add(TEXT("ruin_atlantis"));
			FRuinsModel Clean(Ruins);
			Clean.LoadState(Dirty);
			TestTrue(TEXT("Descarta la basura"), Clean.GetState() == Model.GetState());

			// Las ruinas completas se recalculan aunque el guardado no las liste.
			FRuinsState NoSites = Model.GetState();
			NoSites.CompletedSites.Reset();
			FRuinsModel Recomputed(Ruins);
			Recomputed.LoadState(NoSites);
			TestTrue(TEXT("Recalcula"), Recomputed.IsSiteComplete(0) && Recomputed.IsSiteComplete(2));
		});
	});
}

#endif
