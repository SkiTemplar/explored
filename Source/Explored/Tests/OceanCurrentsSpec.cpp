#include "Misc/AutomationTest.h"

#include "Ocean/OceanCurrents.h"
#include "Ocean/OceanWaves.h"
#include "WorldGen/ArchipelagoLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FOceanCurrentsSpec, "Explored.Ocean.CorrientesYMareas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FOceanCurrentsSpec)

void FOceanCurrentsSpec::Define()
{
	Describe("FOceanWaves::NormalAt", [this]()
	{
		It("devuelve normales unitarias apuntando sobre todo hacia arriba con mar en calma", [this]()
		{
			const FOceanWaves Waves = FOceanWaves::Make(0.05f);
			for (int32 I = 0; I < 50; ++I)
			{
				const FVector Normal = Waves.NormalAt(FVector2D(I * 311.0f, I * -197.0f), I * 0.53f);
				TestTrue(TEXT("Unitaria"), FMath::IsNearlyEqual(Normal.Size(), 1.0, 1e-3));
				TestTrue(TEXT("Mayormente hacia arriba"), Normal.Z > 0.8f);
			}
		});

		It("se inclina más con mar más fuerte", [this]()
		{
			const FOceanWaves Calm = FOceanWaves::Make(0.05f);
			const FOceanWaves Rough = FOceanWaves::Make(1.0f);
			float CalmTiltSum = 0.0f;
			float RoughTiltSum = 0.0f;
			for (int32 I = 0; I < 30; ++I)
			{
				const FVector2D P(I * 173.0f, I * 91.0f);
				CalmTiltSum += 1.0f - Calm.NormalAt(P, 4.0f).Z;
				RoughTiltSum += 1.0f - Rough.NormalAt(P, 4.0f).Z;
			}
			TestTrue(TEXT("Más pendiente con temporal"), RoughTiltSum > CalmTiltSum * 2.0f);
		});
	});

	Describe("FOceanTide", [this]()
	{
		It("hace dos pleamares y dos bajamares al día", [this]()
		{
			// Level(T) = sin(4*pi*T): periodo 0.5 día, pleamares en T = 0.125 y 0.625.
			TestTrue(TEXT("Pleamar en 0.125"), FOceanTide::Level(0.125f) > 0.999f);
			TestTrue(TEXT("Pleamar en 0.625"), FOceanTide::Level(0.625f) > 0.999f);
			TestTrue(TEXT("Bajamar en 0.375"), FOceanTide::Level(0.375f) < -0.999f);
			TestTrue(TEXT("Bajamar en 0.875"), FOceanTide::Level(0.875f) < -0.999f);
			TestTrue(TEXT("Ni pleamar ni bajamar a mitad de subida"), FMath::Abs(FOceanTide::Level(0.0f)) < 0.01f);
		});

		It("la corriente es nula en el cambio de marea y máxima a media marea", [this]()
		{
			// Level(T) = sin(4*pi*T): pleamar (nivel = 1) en T = 0.125.
			TestTrue(TEXT("Pleamar"), FOceanTide::Level(0.125f) > 0.99f);
			TestTrue(TEXT("Slack en pleamar"), FMath::Abs(FOceanTide::Flow(0.125f)) < 0.05f);
			TestTrue(TEXT("Máxima corriente a media marea"), FMath::Abs(FOceanTide::Flow(0.25f)) > 0.95f);
		});

		It("da marea viva en luna nueva y llena, y marea muerta en los cuartos", [this]()
		{
			const float New = FOceanTide::SpringNeapFactor(0.0f);
			const float Full = FOceanTide::SpringNeapFactor(0.5f);
			const float FirstQuarter = FOceanTide::SpringNeapFactor(0.25f);
			const float LastQuarter = FOceanTide::SpringNeapFactor(0.75f);
			TestTrue(TEXT("Nueva viva"), New > 0.95f);
			TestTrue(TEXT("Llena viva"), Full > 0.95f);
			TestTrue(TEXT("Creciente muerta"), FirstQuarter < 0.45f);
			TestTrue(TEXT("Menguante muerta"), LastQuarter < 0.45f);
		});
	});

	Describe("FOceanCurrents", [this]()
	{
		It("construye un estrecho por cada par de islas consecutivas de la cadena", [this]()
		{
			const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed);
			const TArray<FOceanStrait> Straits = FOceanCurrents::BuildStraits(Layout);
			TestEqual(TEXT("N - 1 estrechos"), Straits.Num(), FMath::Max(0, Layout.Islands.Num() - 1));
		});

		It("empuja a lo largo del canal, calmada en las costas y sin sobrepasar la velocidad máxima", [this]()
		{
			TArray<FOceanStrait> Straits;
			FOceanStrait Strait;
			Strait.CoastA = FVector2D(0.0f, 0.0f);
			Strait.CoastB = FVector2D(20000.0f, 0.0f);
			Strait.HalfWidthCm = 5000.0f;
			Straits.Add(Strait);

			const FVector2D Center = FOceanCurrents::CurrentAt(Straits, FVector2D(10000.0f, 0.0f), 1.0f, 1.0f, 1.0f);
			const FVector2D Edge = FOceanCurrents::CurrentAt(Straits, FVector2D(10000.0f, 4999.0f), 1.0f, 1.0f, 1.0f);
			const FVector2D Outside = FOceanCurrents::CurrentAt(Straits, FVector2D(10000.0f, 6000.0f), 1.0f, 1.0f, 1.0f);

			TestTrue(TEXT("Máxima en el centro"), Center.Size() > Edge.Size());
			TestTrue(TEXT("Nula fuera del canal"), Outside.IsNearlyZero());
			TestTrue(TEXT("No supera la velocidad máxima"), Center.Size() <= FOceanCurrents::MaxSpeedCmS + 1.0f);
			TestTrue(TEXT("Sigue la dirección del canal"), FMath::Abs(Center.Y) < 1.0f && Center.X > 0.0f);
		});

		It("invierte el sentido con la vaciante", [this]()
		{
			TArray<FOceanStrait> Straits;
			FOceanStrait Strait;
			Strait.CoastA = FVector2D(0.0f, 0.0f);
			Strait.CoastB = FVector2D(20000.0f, 0.0f);
			Strait.HalfWidthCm = 5000.0f;
			Straits.Add(Strait);

			const FVector2D Flood = FOceanCurrents::CurrentAt(Straits, FVector2D(10000.0f, 0.0f), 1.0f, 1.0f, 0.5f);
			const FVector2D Ebb = FOceanCurrents::CurrentAt(Straits, FVector2D(10000.0f, 0.0f), -1.0f, 1.0f, 0.5f);
			TestTrue(TEXT("Sentido opuesto"), Flood.X > 0.0f && Ebb.X < 0.0f);
		});

		It("es más fuerte en marea viva y con viento que en marea muerta y calma", [this]()
		{
			TArray<FOceanStrait> Straits;
			FOceanStrait Strait;
			Strait.CoastA = FVector2D(0.0f, 0.0f);
			Strait.CoastB = FVector2D(20000.0f, 0.0f);
			Strait.HalfWidthCm = 5000.0f;
			Straits.Add(Strait);

			const FVector2D Strong = FOceanCurrents::CurrentAt(Straits, FVector2D(10000.0f, 0.0f), 1.0f, 1.0f, 1.0f);
			const FVector2D Weak = FOceanCurrents::CurrentAt(Straits, FVector2D(10000.0f, 0.0f), 1.0f, 0.4f, 0.0f);
			TestTrue(TEXT("Marea viva y viento empujan más"), Strong.Size() > Weak.Size() * 1.5f);
		});
	});
}

#endif
