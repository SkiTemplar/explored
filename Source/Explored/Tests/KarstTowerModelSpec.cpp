#include "Misc/AutomationTest.h"

#include "WorldGen/KarstTowerModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FKarstTowerModelSpec, "Explored.WorldGen.KarstTowers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FKarstTowerModelSpec)

void FKarstTowerModelSpec::Define()
{
	Describe("Generate", [this]()
	{
		It("reparte entre 9 y 18 torres dentro del macizo, sin montarse del todo, y alguna laguna", [this]()
		{
			for (uint32 Seed : {1u, 77u, 20260926u, 424242u})
			{
				const FKarstLayout Layout = FKarstTowerModel::Generate(Seed);
				const int32 N = Layout.Towers.Num();
				TestTrue(*FString::Printf(TEXT("semilla %u: %d torres"), Seed, N), N >= FKarstTowerModel::MinTowers && N <= FKarstTowerModel::MaxTowers);
				TestTrue(TEXT("hay laguna"), Layout.Lagoons.Num() >= 1);
				for (const FKarstTower& T : Layout.Towers)
				{
					TestTrue(TEXT("dentro de la isla"), T.Center.Size() + T.Radius < 1.05f);
					TestTrue(TEXT("altura razonable"), T.Height >= 0.35f && T.Height <= 1.0f);
				}
			}
		});

		It("es determinista", [this]()
		{
			const FKarstLayout A = FKarstTowerModel::Generate(99);
			const FKarstLayout B = FKarstTowerModel::Generate(99);
			TestEqual(TEXT("torres"), A.Towers.Num(), B.Towers.Num());
			for (int32 I = 0; I < FMath::Min(A.Towers.Num(), B.Towers.Num()); ++I)
			{
				TestTrue(TEXT("misma torre"), A.Towers[I].Center == B.Towers[I].Center && A.Towers[I].Height == B.Towers[I].Height);
			}
		});
	});

	Describe("TowerHeight", [this]()
	{
		It("levanta paredes casi a plomo y cimas anchas, no conos", [this]()
		{
			const FKarstLayout Layout = FKarstTowerModel::Generate(20260926);
			int32 Vertical = 0;
			int32 Checked = 0;
			for (const FKarstTower& Tower : Layout.Towers)
			{
				// Perfil radial desde el centro: dónde cae del 60 % al 10 % de la cima.
				const FVector2D Dir(FMath::Cos(Tower.Angle + 0.3f), FMath::Sin(Tower.Angle + 0.3f));
				const int32 Steps = 400;
				float High = -1.0f, Low = -1.0f;
				const float Top = FKarstTowerModel::TowerHeight(Layout, Tower.Center.X, Tower.Center.Y);
				int32 AboveThreeQuarters = 0;
				for (int32 S = 0; S <= Steps; ++S)
				{
					const float R = 2.0f * Tower.Radius * S / Steps;
					const FVector2D P = Tower.Center + Dir * R;
					const float H = FKarstTowerModel::TowerHeight(Layout, P.X, P.Y);
					AboveThreeQuarters += (R < Tower.Radius * 0.6f && H > 0.75f * Top) ? 1 : 0;
					High = (H >= 0.6f * Top) ? R : High;
					Low = (Low < 0.0f && R > High && High >= 0.0f && H <= 0.1f * Top) ? R : Low;
				}
				if (High < 0.0f || Low < 0.0f)
				{
					continue; // perfil tapado por una torre vecina
				}
				++Checked;
				// Horizontal en metros para una isla de 600 m frente a la caída en fracción de altura.
				const float Run = (Low - High) * 600.0f;
				const float Drop = 0.5f * Tower.Height * 250.0f;
				Vertical += Drop / FMath::Max(Run, 0.01f) > 5.0f ? 1 : 0; // más de ~79°
				TestTrue(TEXT("cima ancha"), AboveThreeQuarters > Steps * 0.6f * 0.5f * 0.5f);
			}
			TestTrue(*FString::Printf(TEXT("paredes a plomo en %d de %d torres"), Vertical, Checked), Checked >= 6 && Vertical >= Checked * 8 / 10);
		});

		It("vale cero lejos de las torres y marca su planta", [this]()
		{
			const FKarstLayout Layout = FKarstTowerModel::Generate(5);
			float Core = -1.0f;
			TestEqual(TEXT("fuera"), FKarstTowerModel::TowerHeight(Layout, 3.0f, 3.0f, &Core), 0.0f);
			TestEqual(TEXT("sin planta"), Core, 0.0f);
			const FKarstTower& T = Layout.Towers[0];
			FKarstTowerModel::TowerHeight(Layout, T.Center.X, T.Center.Y, &Core);
			TestEqual(TEXT("planta"), Core, 1.0f);
		});
	});

	Describe("ApplyTowers", [this]()
	{
		It("levanta las torres y no toca lo que queda fuera, aunque esté bajo el mar", [this]()
		{
			const FKarstLayout Layout = FKarstTowerModel::Generate(20260926);
			const FKarstTower& T = Layout.Towers[0];
			TestEqual(TEXT("fondo lejos de las torres"), FKarstTowerModel::ApplyTowers(Layout, 3.0f, 3.0f, -3.0f, 250.0f), -3.0f);
			TestTrue(TEXT("cima de la torre"), FKarstTowerModel::ApplyTowers(Layout, T.Center.X, T.Center.Y, -3.0f, 250.0f) > 0.5f * T.Height * 250.0f);
			for (const FKarstLagoon& L : Layout.Lagoons)
			{
				TestEqual(TEXT("la laguna sigue bajo el mar"), FKarstTowerModel::ApplyTowers(Layout, L.Center.X, L.Center.Y, -3.0f, 250.0f), -3.0f);
			}
		});
	});

	Describe("LowlandHeight", [this]()
	{
		It("hunde las lagunas bajo el mar y deja el llano bajo", [this]()
		{
			const FKarstLayout Layout = FKarstTowerModel::Generate(20260926);
			const FExploredNoise Noise(3);
			for (const FKarstLagoon& L : Layout.Lagoons)
			{
				const float U = 1.0f - L.Center.Size();
				const float Lowland = FKarstTowerModel::LowlandHeight(Layout, Noise, L.Center.X, L.Center.Y, U, 250.0f);
				TestTrue(TEXT("fondo de laguna"), FKarstTowerModel::ApplyLagoons(Layout, L.Center.X, L.Center.Y, Lowland) < -1.5f);
				const FVector2D Far = L.Center + FVector2D(L.Radius * 1.5f, 0.0f);
				TestEqual(TEXT("fuera de la laguna no toca nada"), FKarstTowerModel::ApplyLagoons(Layout, Far.X, Far.Y, 7.0f), 7.0f);
			}
			float Highest = -1000.0f;
			for (float Q = -0.9f; Q <= 0.9f; Q += 0.01f)
			{
				float Core = 0.0f;
				FKarstTowerModel::TowerHeight(Layout, Q, 0.1f, &Core);
				if (Core == 0.0f)
				{
					Highest = FMath::Max(Highest, FKarstTowerModel::LowlandHeight(Layout, Noise, Q, 0.1f, 1.0f - FMath::Abs(Q), 250.0f));
				}
			}
			TestTrue(*FString::Printf(TEXT("llano bajo entre torres (%.1f m)"), Highest), Highest < 0.25f * 250.0f);
		});
	});
}

#endif
