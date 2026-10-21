#include "Misc/AutomationTest.h"

#include "WorldGen/IslandLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FIslandLayoutSpec, "Explored.WorldGen.IslandLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	FIslandLayout Layout;
END_DEFINE_SPEC(FIslandLayoutSpec)

void FIslandLayoutSpec::Define()
{
	BeforeEach([this]()
	{
		Layout = FIslandLayout::Provisional();
	});

	Describe("Provisional", [this]()
	{
		It("es coherente: ids únicos, radios positivos y zonas sobre tierra", [this]()
		{
			FString Error;
			TestTrue(TEXT("Validate"), Layout.Validate(Error));
			TestEqual(TEXT("sin error"), Error, FString());
		});

		It("tiene la isla principal de unos 330 × 260 m centrada en el origen", [this]()
		{
			const FIslandDesc* Main = Layout.FindIslandById(TEXT("isla_sal"));
			if (!TestNotNull(TEXT("isla_sal"), Main))
			{
				return;
			}
			TestEqual(TEXT("primera tierra"), Layout.Islands[0].Id, FName(TEXT("isla_sal")));
			TestEqual(TEXT("centrada"), Main->Center, FVector2D::ZeroVector);
			TestEqual(TEXT("330 m de este a oeste"), Main->RadiusAlong(FVector2D(0.0, 1.0)) * 2.0f, 330.0f, 1.0f);
			TestEqual(TEXT("260 m de norte a sur"), Main->RadiusAlong(FVector2D(1.0, 0.0)) * 2.0f, 260.0f, 1.0f);
		});

		It("pone el islote del carguero a unos 150 m al nordeste de Playa Norte", [this]()
		{
			const FIslandDesc* Islet = Layout.FindIslandById(TEXT("islote_carguero"));
			const FIslandZone* North = Layout.FindZone(TEXT("playa_norte"));
			if (!TestNotNull(TEXT("islote"), Islet) || !TestNotNull(TEXT("playa_norte"), North))
			{
				return;
			}
			const FVector2D Offset = Islet->Center - North->Center;
			TestTrue(TEXT("al nordeste"), Offset.X > 0.0 && Offset.Y > 0.0);
			TestEqual(TEXT("unos 150 m"), static_cast<float>(Offset.Size()), 150.0f, 15.0f);
			const FIslandDesc& Main = Layout.Islands[0];
			TestTrue(TEXT("separado por agua"), Main.DistanceToCoast(Islet->Center) - Islet->Radius > 60.0f);
		});

		It("cabe en el mapa de 400 × 400 m", [this]()
		{
			FVector2D Min(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
			FVector2D Max(-TNumericLimits<double>::Max(), -TNumericLimits<double>::Max());
			for (const FIslandDesc& Island : Layout.Islands)
			{
				Min.X = FMath::Min(Min.X, Island.Center.X - Island.Radius);
				Min.Y = FMath::Min(Min.Y, Island.Center.Y - Island.Radius);
				Max.X = FMath::Max(Max.X, Island.Center.X + Island.Radius);
				Max.Y = FMath::Max(Max.Y, Island.Center.Y + Island.Radius);
			}
			// Cota por el semieje mayor en ambos ejes: si así cabe, la elipse real también.
			TestTrue(TEXT("ancho norte-sur"), Max.X - Min.X <= 2.0 * FIslandLayout::WorldHalfExtent + 40.0);
			TestTrue(TEXT("ancho este-oeste"), Max.Y - Min.Y <= 2.0 * FIslandLayout::WorldHalfExtent);
		});

		It("trae las siete zonas del GDD §6 y los puntos de autor", [this]()
		{
			for (const TCHAR* Id : {TEXT("cala_varada"), TEXT("palmar"), TEXT("rocas_pescador"), TEXT("bosque_bambu"),
				TEXT("playa_norte"), TEXT("colina_faro"), TEXT("isla_carguero")})
			{
				TestNotNull(FString::Printf(TEXT("zona %s"), Id), Layout.FindZone(Id));
			}
			TestEqual(TEXT("siete zonas"), Layout.Zones.Num(), 7);
			for (const TCHAR* Id : {TEXT("player_start"), TEXT("raft_site"), TEXT("lighthouse")})
			{
				TestNotNull(FString::Printf(TEXT("punto %s"), Id), Layout.FindPoint(Id));
			}
		});

		It("coloca Cala Varada al sur y la Colina del Faro al norte de ella", [this]()
		{
			const FIslandZone* Cove = Layout.FindZone(TEXT("cala_varada"));
			const FIslandZone* Hill = Layout.FindZone(TEXT("colina_faro"));
			if (TestNotNull(TEXT("cala"), Cove) && TestNotNull(TEXT("colina"), Hill))
			{
				TestTrue(TEXT("la cala está al sur"), Cove->Center.X < -60.0);
				TestTrue(TEXT("la colina está más al norte"), Hill->Center.X > Cove->Center.X);
			}
		});
	});

	Describe("SampleColumn", [this]()
	{
		It("sube la Colina del Faro hasta unos 35 m", [this]()
		{
			const FIslandZone* Hill = Layout.FindZone(TEXT("colina_faro"));
			if (TestNotNull(TEXT("colina"), Hill))
			{
				TestEqual(TEXT("cota"), Layout.SampleColumn(Hill->Center.X, Hill->Center.Y).Height, 35.0f, 1.0f);
			}
		});

		It("da tierra en el centro de cada isla y mar abierto lejos de ellas", [this]()
		{
			for (int32 I = 0; I < Layout.Islands.Num(); ++I)
			{
				const FVector2D C = Layout.Islands[I].Center;
				const FIslandColumn Column = Layout.SampleColumn(C.X, C.Y);
				TestTrue(TEXT("sobre el mar"), Column.Height > FIslandLayout::SeaLevel);
				TestEqual(TEXT("isla dominante"), Column.IslandIndex, I);
			}
			const FIslandColumn Sea = Layout.SampleColumn(-600.0f, -600.0f);
			TestEqual(TEXT("sin isla"), Sea.IslandIndex, INDEX_NONE);
			TestEqual(TEXT("fondo oceánico"), Sea.Height, FIslandLayout::OceanFloor);
			TestEqual(TEXT("sin zona"), Sea.ZoneIndex, INDEX_NONE);
		});

		It("baja a cero en la costa nominal y por debajo fuera", [this]()
		{
			const FIslandDesc& Main = Layout.Islands[0];
			const FVector2D West(0.0, -1.0);
			const float Coast = Main.RadiusAlong(West);
			TestEqual(TEXT("costa"), Layout.SampleColumn(0.0f, -Coast).Height, 0.0f, 0.05f);
			TestTrue(TEXT("mar"), Layout.SampleColumn(0.0f, -Coast - 20.0f).Height < 0.0f);
			TestEqual(TEXT("normalizada en la costa"), Layout.SampleColumn(0.0f, -Coast).NormalizedDistance, 1.0f, 0.01f);
		});

		It("dice en qué zona está el punto", [this]()
		{
			const FIslandZone* North = Layout.FindZone(TEXT("playa_norte"));
			if (TestNotNull(TEXT("playa_norte"), North))
			{
				const int32 Zone = Layout.SampleColumn(North->Center.X, North->Center.Y).ZoneIndex;
				TestTrue(TEXT("índice válido"), Layout.Zones.IsValidIndex(Zone));
				TestEqual(TEXT("Playa Norte"), Layout.Zones.IsValidIndex(Zone) ? Layout.Zones[Zone].Id : NAME_None, North->Id);
			}
		});

		It("da el bioma de la zona y, fuera de zonas, el de la tierra", [this]()
		{
			const FIslandZone* Bamboo = Layout.FindZone(TEXT("bosque_bambu"));
			if (TestNotNull(TEXT("bambú"), Bamboo))
			{
				TestTrue(TEXT("selva"), Layout.BiomeAt(Layout.SampleColumn(Bamboo->Center.X, Bamboo->Center.Y)) == EIslandArchetype::Emerald);
			}
			FIslandColumn Bare;
			Bare.IslandIndex = 0;
			TestTrue(TEXT("tierra principal"), Layout.BiomeAt(Bare) == Layout.Islands[0].Archetype);
			TestTrue(TEXT("mar abierto"), Layout.BiomeAt(FIslandColumn()) == EIslandArchetype::Count);
		});

		It("es determinista", [this]()
		{
			const FIslandLayout Other = FIslandLayout::Provisional();
			for (float X = -150.0f; X <= 250.0f; X += 37.0f)
			{
				for (float Y = -170.0f; Y <= 190.0f; Y += 41.0f)
				{
					TestEqual(TEXT("misma altura"), Layout.SampleColumn(X, Y).Height, Other.SampleColumn(X, Y).Height);
				}
			}
		});
	});

	Describe("FIslandDesc", [this]()
	{
		It("mide la distancia a la costa como 0 dentro y positiva fuera", [this]()
		{
			const FIslandDesc& Main = Layout.Islands[0];
			TestEqual(TEXT("dentro"), Main.DistanceToCoast(FVector2D(10.0, 10.0)), 0.0f);
			TestEqual(TEXT("100 m al este de la costa"), Main.DistanceToCoast(FVector2D(0.0, 265.0)), 100.0f, 0.5f);
		});
	});

	Describe("Validate", [this]()
	{
		It("rechaza ids repetidos entre zonas y puntos", [this]()
		{
			FIslandPoint Clash;
			Clash.Id = TEXT("palmar");
			Layout.Points.Add(Clash);
			FString Error;
			TestFalse(TEXT("inválido"), Layout.Validate(Error));
			TestTrue(TEXT("con mensaje"), !Error.IsEmpty());
		});

		It("rechaza una zona en el agua", [this]()
		{
			FIslandZone Lost;
			Lost.Id = TEXT("perdida");
			Lost.Center = FVector2D(-190.0, -190.0);
			Layout.Zones.Add(Lost);
			FString Error;
			TestFalse(TEXT("inválido"), Layout.Validate(Error));
		});

		It("rechaza un layout sin tierras", [this]()
		{
			FString Error;
			TestFalse(TEXT("vacío"), FIslandLayout().Validate(Error));
		});
	});

	Describe("EIslandArchetype", [this]()
	{
		It("se lee por nombre sin distinguir mayúsculas", [this]()
		{
			EIslandArchetype Biome = EIslandArchetype::Count;
			TestTrue(TEXT("whitesands"), IslandArchetypeFromName(TEXT("whitesands"), Biome));
			TestTrue(TEXT("WhiteSands"), Biome == EIslandArchetype::WhiteSands);
			TestFalse(TEXT("desconocido"), IslandArchetypeFromName(TEXT("Atlantis"), Biome));
		});
	});
}

#endif
