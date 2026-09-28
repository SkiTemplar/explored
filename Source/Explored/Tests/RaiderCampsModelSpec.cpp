#include "Misc/AutomationTest.h"

#include "Raiders/RaiderCampsModel.h"
#include "WorldGen/PointsOfInterest.h"
#include "WorldGen/TerrainDensity.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace RaiderCampsSpecDetail
{
	constexpr uint32 OfficialSeed = FArchipelagoLayout::OfficialSeed;

	float HeightAt(const FTerrainDensity& Density, const FVector& P)
	{
		return Density.SampleColumn(static_cast<float>(P.X), static_cast<float>(P.Y)).Height;
	}

	double Dist2D(const FVector& A, const FVector& B)
	{
		return FVector2D::Distance(FVector2D(A.X, A.Y), FVector2D(B.X, B.Y));
	}

	bool IsFinite(const FVector& P)
	{
		return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && FMath::IsFinite(P.Z);
	}

	TArray<FVector> PoiLocations(const FTerrainDensity& Density)
	{
		TArray<FVector> Out;
		for (const FPointOfInterest& Poi : FPoiLayout::Generate(Density))
		{
			Out.Add(Poi.Location);
		}
		return Out;
	}
}

BEGIN_DEFINE_SPEC(FRaiderCampsModelSpec, "Explored.Raiders.Camps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FRaiderCampsModelSpec)

void FRaiderCampsModelSpec::Define()
{
	using namespace RaiderCampsSpecDetail;

	Describe("la tabla de la biblia", [this]()
	{
		It("Cala Rota en Los Dientes con piragua y Fondeadero Podrido en el Manglar con balandra", [this]()
		{
			TestEqual(TEXT("Cala Rota"), FRaiderCampsModel::IslandOf(ERaiderCampId::BrokenCove), EIslandArchetype::Teeth);
			TestEqual(TEXT("piragua"), FRaiderCampsModel::BoatOf(ERaiderCampId::BrokenCove), EPirateBoatType::RaidingCanoe);
			TestEqual(TEXT("Fondeadero"), FRaiderCampsModel::IslandOf(ERaiderCampId::RottenAnchorage), EIslandArchetype::Mangrove);
			TestEqual(TEXT("balandra"), FRaiderCampsModel::BoatOf(ERaiderCampId::RottenAnchorage), EPirateBoatType::BlackSloop);
			int32 Min = 0, Max = 0;
			FRaiderCampsModel::CrewRange(EPirateBoatType::RaidingCanoe, Min, Max);
			TestTrue(TEXT("2-3"), Min == 2 && Max == 3);
			FRaiderCampsModel::CrewRange(EPirateBoatType::BlackSloop, Min, Max);
			TestTrue(TEXT("5-6"), Min == 5 && Max == 6);
			TestEqual(TEXT("id"), FString(LexToString(ERaiderCampId::BrokenCove)), FString(TEXT("cala_rota")));
			TestEqual(TEXT("id"), FString(LexToString(ERaiderCampId::RottenAnchorage)), FString(TEXT("fondeadero_podrido")));
		});
	});

	Describe("el botín", [this]()
	{
		It("es fijo por semilla, con metal siempre y sin monedas, pólvora ni mapas", [this]()
		{
			const TSet<FName> Allowed = { FName(TEXT("chapa_fuselaje")), FName(TEXT("tubo_aluminio")), FName(TEXT("cable_electrico")),
				FName(TEXT("cuerda")), FName(TEXT("pasta_medicinal")) };
			for (uint32 Seed = 1; Seed < 300; ++Seed)
			{
				for (const ERaiderCampId Camp : { ERaiderCampId::BrokenCove, ERaiderCampId::RottenAnchorage })
				{
					const TArray<FRaiderLoot> A = FRaiderCampsModel::RollLoot(Seed, Camp);
					if (!(A == FRaiderCampsModel::RollLoot(Seed, Camp)))
					{
						AddError(FString::Printf(TEXT("semilla %u: botín distinto al repetir"), Seed));
					}
					bool bMetal = false;
					for (const FRaiderLoot& L : A)
					{
						if (!Allowed.Contains(L.ItemId) || L.Count <= 0 || L.Count > 4)
						{
							AddError(FString::Printf(TEXT("semilla %u: «%s» ×%d"), Seed, *L.ItemId.ToString(), L.Count));
						}
						bMetal |= L.ItemId == FName(TEXT("chapa_fuselaje"));
					}
					if (!bMetal)
					{
						AddError(FString::Printf(TEXT("semilla %u: cofre sin metal"), Seed));
					}
				}
			}
			TestFalse(TEXT("los dos cofres no son iguales"),
				FRaiderCampsModel::RollLoot(OfficialSeed, ERaiderCampId::BrokenCove) ==
				FRaiderCampsModel::RollLoot(OfficialSeed, ERaiderCampId::RottenAnchorage) &&
				FRaiderCampsModel::RollLoot(OfficialSeed + 1, ERaiderCampId::BrokenCove) ==
				FRaiderCampsModel::RollLoot(OfficialSeed + 1, ERaiderCampId::RottenAnchorage));
		});
	});

	Describe("con la semilla oficial", [this]()
	{
		It("los dos campamentos quedan en su isla, en la orilla y con su barco en el agua", [this]()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(OfficialSeed));
			const TArray<FRaiderCamp> Camps = FRaiderCampsModel::Generate(Density, PoiLocations(Density));
			if (!TestEqual(TEXT("dos"), Camps.Num(), 2))
			{
				return;
			}
			for (const FRaiderCamp& Camp : Camps)
			{
				const FString Name = LexToString(Camp.Id);
				const FIslandDesc& Island = Density.GetLayout().Islands[Camp.IslandIndex];
				TestEqual(Name + TEXT(": isla"), Island.Archetype, FRaiderCampsModel::IslandOf(Camp.Id));
				TestTrue(Name + TEXT(": en la orilla"), Camp.bOnBeach);
				const float H = HeightAt(Density, Camp.Location);
				TestTrue(Name + FString::Printf(TEXT(": altura de orilla (%.2f m)"), H),
					H > FRaiderCampsModel::BeachMinHeightM && H < FRaiderCampsModel::LedgeMaxHeightM);
				TestTrue(Name + TEXT(": cerca de su isla"), FVector2D::Distance(FVector2D(Camp.Location.X, Camp.Location.Y), Island.Center) <= Island.Radius * 1.4f);
				TestTrue(Name + TEXT(": barco a tiro"), Dist2D(Camp.BoatLocation, Camp.Location) <= FRaiderCampsModel::MaxBoatDistanceM);
				const float BoatH = HeightAt(Density, Camp.BoatLocation);
				if (Camp.Boat == EPirateBoatType::BlackSloop)
				{
					TestTrue(Name + FString::Printf(TEXT(": balandra con calado (%.2f m)"), BoatH), BoatH <= -FRaiderCampsModel::SloopDraftM);
					TestEqual(Name + TEXT(": balandra a flote"), static_cast<float>(Camp.BoatLocation.Z), FArchipelagoLayout::SeaLevel);
				}
				else
				{
					TestTrue(Name + FString::Printf(TEXT(": piragua en la línea de agua (%.2f m)"), BoatH), BoatH < 0.5f);
					TestTrue(Name + TEXT(": piragua nunca bajo el agua"), Camp.BoatLocation.Z >= FArchipelagoLayout::SeaLevel);
				}
				for (const FVector& Prop : { Camp.TentLocation, Camp.FireLocation, Camp.ChestLocation })
				{
					TestTrue(Name + TEXT(": objeto en tierra"), HeightAt(Density, Prop) >= 0.5f);
					TestTrue(Name + TEXT(": objeto junto al campamento"), Dist2D(Prop, Camp.Location) <= 12.0);
				}
				TestTrue(Name + TEXT(": cofre con botín"), Camp.Loot.Num() > 0);
			}
			TestTrue(TEXT("lejos el uno del otro"), Dist2D(Camps[0].Location, Camps[1].Location) > 500.0);
		});

		It("se separan de los puntos de interés", [this]()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(OfficialSeed));
			const TArray<FVector> Pois = PoiLocations(Density);
			for (const FRaiderCamp& Camp : FRaiderCampsModel::Generate(Density, Pois))
			{
				for (const FVector& P : Pois)
				{
					if (Dist2D(P, Camp.Location) < FRaiderCampsModel::MinClearanceM)
					{
						AddError(FString::Printf(TEXT("%s a %.0f m de un punto de interés"), LexToString(Camp.Id), Dist2D(P, Camp.Location)));
					}
				}
			}
		});

		It("salen iguales dos veces y se mueven si se les tapa el sitio", [this]()
		{
			const FTerrainDensity Density(FArchipelagoLayout::Generate(OfficialSeed));
			const TArray<FRaiderCamp> A = FRaiderCampsModel::Generate(Density);
			const TArray<FRaiderCamp> B = FRaiderCampsModel::Generate(Density);
			TestTrue(TEXT("idénticos"), A == B);
			if (A.Num() != 2)
			{
				return;
			}
			FRaiderCamp Moved;
			// El Manglar tiene playa de sobra: al tapar el sitio, el campamento se va a otra.
			TestTrue(TEXT("coloca"), FRaiderCampsModel::Place(Density, ERaiderCampId::RottenAnchorage, { A[1].Location }, Moved));
			TestTrue(TEXT("a más de 60 m del sitio tapado"), Dist2D(Moved.Location, A[1].Location) >= FRaiderCampsModel::MinClearanceM);
			TestTrue(TEXT("mismo botín"), Moved.Loot == A[1].Loot);
		});
	});

	Describe("con otras semillas", [this]()
	{
		It("siempre hay dos campamentos, finitos y en su isla", [this]()
		{
			for (const uint32 Seed : { 1u, 77u, 4242u, 99991u })
			{
				const FTerrainDensity Density(FArchipelagoLayout::Generate(Seed));
				const TArray<FRaiderCamp> Camps = FRaiderCampsModel::Generate(Density);
				TestEqual(FString::Printf(TEXT("semilla %u: dos"), Seed), Camps.Num(), 2);
				for (const FRaiderCamp& Camp : Camps)
				{
					const FIslandDesc& Island = Density.GetLayout().Islands[Camp.IslandIndex];
					TestTrue(FString::Printf(TEXT("semilla %u: %s finito"), Seed, LexToString(Camp.Id)),
						IsFinite(Camp.Location) && IsFinite(Camp.BoatLocation) && FMath::IsFinite(Camp.Yaw));
					TestTrue(FString::Printf(TEXT("semilla %u: %s en su isla"), Seed, LexToString(Camp.Id)),
						FVector2D::Distance(FVector2D(Camp.Location.X, Camp.Location.Y), Island.Center) <= Island.Radius * 1.4f);
				}
				TestFalse(FString::Printf(TEXT("semilla %u: distinta de la oficial"), Seed),
					Camps == FRaiderCampsModel::Generate(FTerrainDensity(FArchipelagoLayout::Generate(OfficialSeed))));
			}
		});

		It("un layout sin la isla no inventa el campamento", [this]()
		{
			FArchipelagoLayout Layout = FArchipelagoLayout::Generate(OfficialSeed);
			Layout.Islands.RemoveAll([](const FIslandDesc& I) { return I.Archetype == EIslandArchetype::Mangrove; });
			const FTerrainDensity Density(Layout);
			const TArray<FRaiderCamp> Camps = FRaiderCampsModel::Generate(Density);
			TestEqual(TEXT("solo Cala Rota"), Camps.Num(), 1);
			TestTrue(TEXT("es Cala Rota"), Camps.Num() == 1 && Camps[0].Id == ERaiderCampId::BrokenCove);
		});
	});
}

#endif
