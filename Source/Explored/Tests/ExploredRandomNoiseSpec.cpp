#include "Misc/AutomationTest.h"

#include "Core/ExploredNoise.h"
#include "Core/ExploredRandom.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FExploredRandomNoiseSpec, "Explored.Core.RandomNoise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FExploredRandomNoiseSpec)

void FExploredRandomNoiseSpec::Define()
{
	Describe("FExploredRandom", [this]()
	{
		It("produce la misma secuencia con la misma semilla", [this]()
		{
			FExploredRandom A(1234);
			FExploredRandom B(1234);
			for (int32 I = 0; I < 1000; ++I)
			{
				if (A.NextUInt32() != B.NextUInt32())
				{
					AddError(FString::Printf(TEXT("Divergencia en la iteración %d"), I));
					return;
				}
			}
		});

		It("produce secuencias distintas con semillas distintas", [this]()
		{
			FExploredRandom A(1);
			FExploredRandom B(2);
			int32 Equal = 0;
			for (int32 I = 0; I < 100; ++I)
			{
				Equal += A.NextUInt32() == B.NextUInt32() ? 1 : 0;
			}
			TestTrue(TEXT("Casi ningún valor coincide"), Equal < 3);
		});

		It("respeta los límites de RangeInt, incluidos los extremos", [this]()
		{
			FExploredRandom R(99);
			bool bSawMin = false;
			bool bSawMax = false;
			for (int32 I = 0; I < 5000; ++I)
			{
				const int32 V = R.RangeInt(-3, 3);
				if (V < -3 || V > 3)
				{
					AddError(FString::Printf(TEXT("Fuera de rango: %d"), V));
					return;
				}
				bSawMin |= V == -3;
				bSawMax |= V == 3;
			}
			TestTrue(TEXT("Aparece el mínimo"), bSawMin);
			TestTrue(TEXT("Aparece el máximo"), bSawMax);
		});

		It("genera NextFloat en [0, 1) con media cercana a 0.5", [this]()
		{
			FExploredRandom R(7);
			double Sum = 0.0;
			constexpr int32 N = 20000;
			for (int32 I = 0; I < N; ++I)
			{
				const float V = R.NextFloat();
				if (V < 0.0f || V >= 1.0f)
				{
					AddError(FString::Printf(TEXT("Fuera de rango: %f"), V));
					return;
				}
				Sum += V;
			}
			TestTrue(TEXT("Media ~0.5"), FMath::IsNearlyEqual(Sum / N, 0.5, 0.01));
		});
	});

	Describe("FExploredNoise", [this]()
	{
		It("es determinista para la misma semilla y coordenadas", [this]()
		{
			const FExploredNoise A(42);
			const FExploredNoise B(42);
			TestEqual(TEXT("2D"), A.Fbm2D(12.3f, -4.5f, 5), B.Fbm2D(12.3f, -4.5f, 5));
			TestEqual(TEXT("3D"), A.Fbm3D(1.1f, 2.2f, 3.3f, 4), B.Fbm3D(1.1f, 2.2f, 3.3f, 4));
		});

		It("cambia con la semilla", [this]()
		{
			const FExploredNoise A(1);
			const FExploredNoise B(2);
			int32 Different = 0;
			for (int32 I = 0; I < 50; ++I)
			{
				const float X = I * 0.37f + 0.13f;
				const float Y = I * 0.61f + 0.29f;
				Different += A.Gradient2D(X, Y) != B.Gradient2D(X, Y) ? 1 : 0;
			}
			TestTrue(TEXT("La mayoría de muestras difieren"), Different > 40);
		});

		It("mantiene los valores en [-1, 1] y el ridged en [0, 1]", [this]()
		{
			const FExploredNoise N(5);
			FExploredRandom R(11);
			for (int32 I = 0; I < 5000; ++I)
			{
				const float X = R.RangeFloat(-500.0f, 500.0f);
				const float Y = R.RangeFloat(-500.0f, 500.0f);
				const float Z = R.RangeFloat(-500.0f, 500.0f);
				const float G2 = N.Gradient2D(X, Y);
				const float G3 = N.Gradient3D(X, Y, Z);
				const float Rd = N.Ridged2D(X, Y, 4);
				if (G2 < -1.0f || G2 > 1.0f || G3 < -1.0f || G3 > 1.0f || Rd < 0.0f || Rd > 1.0f)
				{
					AddError(FString::Printf(TEXT("Fuera de rango en (%f, %f, %f)"), X, Y, Z));
					return;
				}
			}
		});

		It("es continuo: puntos muy cercanos dan valores cercanos", [this]()
		{
			const FExploredNoise N(3);
			FExploredRandom R(13);
			for (int32 I = 0; I < 1000; ++I)
			{
				const float X = R.RangeFloat(-100.0f, 100.0f);
				const float Y = R.RangeFloat(-100.0f, 100.0f);
				const float Delta = FMath::Abs(N.Gradient2D(X, Y) - N.Gradient2D(X + 0.001f, Y));
				if (Delta > 0.01f)
				{
					AddError(FString::Printf(TEXT("Salto de %f en (%f, %f)"), Delta, X, Y));
					return;
				}
			}
		});

		It("vale cero en los nodos enteros de la red", [this]()
		{
			const FExploredNoise N(8);
			TestTrue(TEXT("Nodo 2D"), FMath::IsNearlyZero(N.Gradient2D(3.0f, -7.0f)));
			TestTrue(TEXT("Nodo 3D"), FMath::IsNearlyZero(N.Gradient3D(1.0f, 2.0f, -4.0f)));
		});
	});
}

#endif
