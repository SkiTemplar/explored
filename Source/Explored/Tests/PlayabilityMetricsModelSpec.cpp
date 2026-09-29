#include "Misc/AutomationTest.h"

#include "WorldGen/PlayabilityMetricsModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PlayabilityMetricsTest
{
	/** Rejilla de W x H celdas con la altura Fn(X, Y) y toda la máscara a 1. */
	template <typename F>
	void MakeGrid(int32 W, int32 H, F&& Fn, TArray<float>& OutHeights, TArray<uint8>& OutMask)
	{
		OutHeights.SetNum(W * H);
		OutMask.Init(1, W * H);
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				OutHeights[Y * W + X] = Fn(X, Y);
			}
		}
	}
}

BEGIN_DEFINE_SPEC(FPlayabilityMetricsModelSpec, "Explored.WorldGen.PlayabilityMetrics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FPlayabilityMetricsModelSpec)

void FPlayabilityMetricsModelSpec::Define()
{
	using namespace PlayabilityMetricsTest;

	Describe("FlatPatches", [this]()
	{
		It("separa dos terrazas llanas por un escarpe y descarta la ladera", [this]()
		{
			// 40 x 20 celdas de 2 m: terraza baja (x < 15), escarpe de 30° y terraza alta (x > 25).
			TArray<float> Heights;
			TArray<uint8> Mask;
			MakeGrid(40, 20, [](int32 X, int32) { return X < 15 ? 1.0f : (X > 25 ? 1.0f + 10.0f * 2.0f * 0.577f : 1.0f + (X - 15) * 2.0f * 0.577f); },
				Heights, Mask);
			const FFlatPatchStats S = FPlayabilityMetricsModel::FlatPatches(Heights, 40, 20, 2.0f, Mask, 15.0f);
			TestEqual(TEXT("dos terrazas"), S.PatchAreas.Num(), 2);
			TestTrue(*FString::Printf(TEXT("llano %.2f"), S.FlatFraction), S.FlatFraction > 0.6f && S.FlatFraction < 0.8f);
			TestEqual(TEXT("parches de 400 m²"), S.CountAtLeast(400.0f), 2);
			TestEqual(TEXT("ninguno de 2.000 m²"), S.CountAtLeast(2000.0f), 0);
		});

		It("respeta la máscara y no revienta con entradas no válidas", [this]()
		{
			TArray<float> Heights;
			TArray<uint8> Mask;
			MakeGrid(10, 10, [](int32, int32) { return 2.0f; }, Heights, Mask);
			Mask.Init(0, 100);
			TestEqual(TEXT("sin tierra"), FPlayabilityMetricsModel::FlatPatches(Heights, 10, 10, 2.0f, Mask, 15.0f).LandCells, 0);
			TestEqual(TEXT("tamaño incoherente"), FPlayabilityMetricsModel::FlatPatches(Heights, 11, 10, 2.0f, Mask, 15.0f).LandCells, 0);
			TestEqual(TEXT("paso no finito"),
				FPlayabilityMetricsModel::FlatPatches(Heights, 10, 10, std::numeric_limits<float>::quiet_NaN(), Mask, 15.0f).LandCells, 0);
		});
	});

	Describe("ClarkEvans", [this]()
	{
		It("da ~2 en una malla regular y mucho menos de 1 en una cadena", [this]()
		{
			TArray<FVector2D> Grid;
			for (int32 Y = 0; Y < 5; ++Y)
			{
				for (int32 X = 0; X < 5; ++X)
				{
					Grid.Add(FVector2D(X * 1000.0f + 500.0f, Y * 1000.0f + 500.0f));
				}
			}
			const float Regular = FPlayabilityMetricsModel::ClarkEvans(Grid, 5000.0f * 5000.0f);
			TestTrue(*FString::Printf(TEXT("malla %.2f"), Regular), Regular > 1.8f);
			TArray<FVector2D> Chain;
			for (int32 I = 0; I < 6; ++I)
			{
				Chain.Add(FVector2D(1000.0f + I * 250.0f, 2000.0f + I * 60.0f));
			}
			const float Clustered = FPlayabilityMetricsModel::ClarkEvans(Chain, 5000.0f * 5000.0f);
			TestTrue(*FString::Printf(TEXT("cadena %.2f"), Clustered), Clustered < 0.5f);
			TestEqual(TEXT("un punto"), FPlayabilityMetricsModel::ClarkEvans({FVector2D::ZeroVector}, 100.0f), 0.0f);
		});
	});

	Describe("Variación a lo largo del perímetro", [this]()
	{
		It("distingue una variación amplia y suave de un borde en sierra", [this]()
		{
			TArray<float> Smooth;
			TArray<float> Saw;
			TArray<float> Constant;
			for (int32 I = 0; I < 360; ++I)
			{
				Smooth.Add(100.0f + 50.0f * FMath::Sin(UE_TWO_PI * 3.0f * I / 360.0f));
				Saw.Add(I % 2 == 0 ? 80.0f : 120.0f);
				Constant.Add(100.0f);
			}
			TestTrue(TEXT("cv suave"), FPlayabilityMetricsModel::CoefficientOfVariation(Smooth) > 0.3f);
			TestTrue(TEXT("cv constante"), FPlayabilityMetricsModel::CoefficientOfVariation(Constant) < 1.0e-4f);
			TestTrue(TEXT("dentado suave"), FPlayabilityMetricsModel::Jaggedness(Smooth) < 0.05f);
			TestTrue(TEXT("dentado sierra"), FPlayabilityMetricsModel::Jaggedness(Saw) > 0.3f);
		});

		It("mide la concentración de rumbos", [this]()
		{
			const TArray<float> Spread = {0.0f, UE_HALF_PI, UE_PI, -UE_HALF_PI};
			const TArray<float> Same = {0.3f, 0.35f, 0.25f};
			TestTrue(TEXT("repartidos"), FPlayabilityMetricsModel::MeanResultantLength(Spread, {}) < 1.0e-3f);
			TestTrue(TEXT("iguales"), FPlayabilityMetricsModel::MeanResultantLength(Same, {}) > 0.99f);
			TestTrue(TEXT("pesos"), FPlayabilityMetricsModel::MeanResultantLength(Spread, {10.0f, 0.0f, 0.0f, 0.0f}) > 0.99f);
		});
	});

	Describe("DrainagePattern", [this]()
	{
		It("ve la estrella en un cono y no en una ladera inclinada hacia un lado", [this]()
		{
			// Cono con surcos radiales (estrella) frente a plano inclinado con valles serpenteantes.
			constexpr int32 N = 121;
			TArray<float> Cone;
			TArray<float> Tilted;
			TArray<uint8> Mask;
			MakeGrid(N, N, [](int32 X, int32 Y)
			{
				const float Dx = X - 60.0f;
				const float Dy = Y - 60.0f;
				const float R = FMath::Sqrt(Dx * Dx + Dy * Dy);
				const float Gully = 0.6f * FMath::Cos(8.0f * FMath::Atan2(Dy, Dx));
				return R < 55.0f ? 20.0f * (1.0f - R / 55.0f) + Gully * R / 55.0f : -2.0f;
			}, Cone, Mask);
			MakeGrid(N, N, [](int32 X, int32 Y)
			{
				const bool bLand = X > 3 && Y > 3 && Y < N - 4;
				const float Meander = Y - 60.0f - 12.0f * FMath::Sin(X / 9.0f);
				return bLand ? 0.1f * X + 0.04f * FMath::Abs(Meander) : -2.0f;
			}, Tilted, Mask);
			const FDrainagePattern Star = FPlayabilityMetricsModel::DrainagePattern(Cone, N, N, FVector2D(60.0f), 150.0f, 0.0f);
			const FDrainagePattern Side = FPlayabilityMetricsModel::DrainagePattern(Tilted, N, N, FVector2D(60.0f), 150.0f, 0.0f);
			TestTrue(*FString::Printf(TEXT("cono: radial %.2f"), Star.Radiality), Star.Radiality > 0.85f);
			TestTrue(*FString::Printf(TEXT("cono: resultante %.2f"), Star.MouthResultant), Star.MouthResultant < 0.3f);
			TestTrue(*FString::Printf(TEXT("ladera: resultante %.2f"), Side.MouthResultant), Side.MouthResultant > 0.8f);
			TestTrue(*FString::Printf(TEXT("ladera: sinuosidad %.2f"), Side.Sinuosity), Side.Sinuosity > Star.Sinuosity + 0.1f);
		});

		It("aparta las alturas no finitas antes de ordenar el caudal y sigue viendo la estrella", [this]()
		{
			// Un NaN en la ordenación rompe el orden débil estricto del comparador (UB en el Sort).
			constexpr int32 N = 121;
			TArray<float> Cone;
			TArray<uint8> Mask;
			MakeGrid(N, N, [](int32 X, int32 Y)
			{
				const float Dx = X - 60.0f;
				const float Dy = Y - 60.0f;
				const float R = FMath::Sqrt(Dx * Dx + Dy * Dy);
				return R < 55.0f ? 20.0f * (1.0f - R / 55.0f) + 0.6f * FMath::Cos(8.0f * FMath::Atan2(Dy, Dx)) * R / 55.0f : -2.0f;
			}, Cone, Mask);
			const float Bad[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
				-std::numeric_limits<float>::infinity()};
			for (int32 I = 0; I < 90; ++I)
			{
				Cone[(I * 7919 + 1237) % Cone.Num()] = Bad[I % 3];
			}
			const FDrainagePattern P = FPlayabilityMetricsModel::DrainagePattern(Cone, N, N, FVector2D(60.0f), 150.0f, 0.0f);
			TestTrue(*FString::Printf(TEXT("radial %.2f"), P.Radiality), FMath::IsFinite(P.Radiality) && P.Radiality > 0.7f);
			TestTrue(*FString::Printf(TEXT("resultante %.2f"), P.MouthResultant), FMath::IsFinite(P.MouthResultant));
			TestTrue(*FString::Printf(TEXT("sinuosidad %.2f"), P.Sinuosity), FMath::IsFinite(P.Sinuosity));
			TestTrue(*FString::Printf(TEXT("desembocaduras %d"), P.Mouths), P.Mouths > 0);
		});
	});
}

#endif
