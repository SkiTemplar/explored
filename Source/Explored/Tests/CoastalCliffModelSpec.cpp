#include "Misc/AutomationTest.h"

#include "WorldGen/CoastalCliffModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CoastalCliffTest
{
	constexpr float Radius = 600.0f;

	float PlainFraction(const FCoastalCliffs& Cliffs)
	{
		int32 Cliff = 0;
		for (int32 I = 0; I < 720; ++I)
		{
			Cliff += Cliffs.Amount(UE_TWO_PI * I / 720.0f - UE_PI) > 0.5f ? 1 : 0;
		}
		return Cliff / 720.0f;
	}

	/** Rumbo con acantilado pleno (o -10 si no hay ninguno). */
	float FullCliffAngle(const FCoastalCliffs& Cliffs)
	{
		for (int32 I = 0; I < 720; ++I)
		{
			const float Angle = UE_TWO_PI * I / 720.0f - UE_PI;
			if (Cliffs.Amount(Angle) > 0.999f)
			{
				return Angle;
			}
		}
		return -10.0f;
	}
}

BEGIN_DEFINE_SPEC(FCoastalCliffModelSpec, "Explored.WorldGen.CoastalCliffs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FCoastalCliffModelSpec)

void FCoastalCliffModelSpec::Define()
{
	using namespace CoastalCliffTest;

	Describe("StyleFor y Build", [this]()
	{
		It("da acantilados a Smoke, Emerald y Landing y a ninguna otra isla", [this]()
		{
			FCliffStyle Style;
			TestTrue(TEXT("Smoke"), FCoastalCliffModel::StyleFor(EIslandArchetype::Smoke, Style));
			TestTrue(TEXT("Emerald"), FCoastalCliffModel::StyleFor(EIslandArchetype::Emerald, Style));
			TestTrue(TEXT("Landing"), FCoastalCliffModel::StyleFor(EIslandArchetype::Landing, Style));
			TestFalse(TEXT("Mangrove"), FCoastalCliffModel::StyleFor(EIslandArchetype::Mangrove, Style));
			TestFalse(TEXT("WhiteSands"), FCoastalCliffModel::StyleFor(EIslandArchetype::WhiteSands, Style));
			TestTrue(TEXT("sin estilo, sin tablas"), FCoastalCliffModel::Build(3u, FCliffStyle()).IsEmpty());
		});

		It("reparte por ruido la fracción de costa pedida, en tramos y de forma determinista", [this]()
		{
			FCliffStyle Style;
			Style.Fraction = 0.25f;
			const FCoastalCliffs A = FCoastalCliffModel::Build(42u, Style);
			const FCoastalCliffs B = FCoastalCliffModel::Build(42u, Style);
			const float Fraction = PlainFraction(A);
			TestTrue(*FString::Printf(TEXT("fracción %.3f"), Fraction), FMath::Abs(Fraction - 0.25f) < 0.03f);
			int32 Changes = 0;
			for (int32 I = 0; I < 720; ++I)
			{
				const float Here = UE_TWO_PI * I / 720.0f - UE_PI;
				const float Next = UE_TWO_PI * (I + 1) / 720.0f - UE_PI;
				Changes += (A.Amount(Here) > 0.5f) != (A.Amount(Next) > 0.5f) ? 1 : 0;
				TestEqual(TEXT("determinista"), A.Height(Here), B.Height(Here));
			}
			TestTrue(*FString::Printf(TEXT("tramos: %d cambios"), Changes), Changes >= 4 && Changes <= 16);
		});

		It("pone los tramos fijos donde se piden y en ningún otro sitio", [this]()
		{
			FCliffStyle Style;
			Style.Sectors.Add(FVector2D(-UE_HALF_PI, 0.36f));
			const FCoastalCliffs Cliffs = FCoastalCliffModel::Build(7u, Style);
			TestEqual(TEXT("en el centro del tramo"), Cliffs.Amount(-UE_HALF_PI), 1.0f, 1.0e-3f);
			TestEqual(TEXT("en la bahía (+X)"), Cliffs.Amount(0.0f), 0.0f);
			TestEqual(TEXT("en el spawn (-X)"), Cliffs.Amount(UE_PI), 0.0f);
		});
	});

	Describe("Uplift", [this]()
	{
		It("vale 0 en la costa y en el mar, levanta una pared de más de 60° y se apaga tierra adentro", [this]()
		{
			FCliffStyle Style;
			Style.Fraction = 0.3f;
			Style.MinHeight = 20.0f;
			Style.MaxHeight = 40.0f;
			const FCoastalCliffs Cliffs = FCoastalCliffModel::Build(11u, Style);
			const float Angle = FullCliffAngle(Cliffs);
			TestTrue(TEXT("hay un tramo pleno"), Angle > -5.0f);
			const float Height = Cliffs.Height(Angle);
			const float MetersToU = 1.0f / Radius;
			TestEqual(TEXT("en la línea de costa"), FCoastalCliffModel::Uplift(Cliffs, Angle, 0.0f, Radius), 0.0f);
			TestEqual(TEXT("en el mar"), FCoastalCliffModel::Uplift(Cliffs, Angle, -0.01f, Radius), 0.0f);
			const float Top = FCoastalCliffModel::Uplift(Cliffs, Angle, FCoastalCliffModel::FaceWidth * MetersToU, Radius);
			TestTrue(*FString::Printf(TEXT("pared de %.1f m de %.1f"), Top, Height), Top > Height * 0.9f);
			TestTrue(TEXT("más de 60°"), Top / FCoastalCliffModel::FaceWidth > FMath::Tan(FMath::DegreesToRadians(60.0f)));
			const float Far = FCoastalCliffModel::Uplift(Cliffs, Angle, Height * FCoastalCliffModel::BackSlopeRun * 1.01f * MetersToU, Radius);
			TestEqual(TEXT("se apaga tierra adentro"), Far, 0.0f, 1.0e-3f);
			float Previous = Top;
			for (float Meters = FCoastalCliffModel::FaceWidth; Meters < Height * FCoastalCliffModel::BackSlopeRun; Meters += 1.0f)
			{
				const float H = FCoastalCliffModel::Uplift(Cliffs, Angle, Meters * MetersToU, Radius);
				TestTrue(TEXT("rellano que baja suave (< 20°)"), Previous - H < FMath::Tan(FMath::DegreesToRadians(20.0f)) + 1.0e-3f);
				Previous = H;
			}
		});

		It("no devuelve alturas no finitas con entradas no finitas", [this]()
		{
			FCliffStyle Style;
			Style.Fraction = 0.3f;
			const FCoastalCliffs Cliffs = FCoastalCliffModel::Build(13u, Style);
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			TestTrue(TEXT("rumbo NaN"), FMath::IsFinite(FCoastalCliffModel::Uplift(Cliffs, NaN, 0.01f, Radius)));
			TestTrue(TEXT("U NaN"), FMath::IsFinite(FCoastalCliffModel::Uplift(Cliffs, 0.3f, NaN, Radius)));
			TestTrue(TEXT("radio NaN"), FMath::IsFinite(FCoastalCliffModel::Uplift(Cliffs, 0.3f, 0.01f, NaN)));
		});
	});
}

#endif
