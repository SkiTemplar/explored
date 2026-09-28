#include "WorldGen/KarstTowerModel.h"

#include "Core/ExploredRandom.h"

namespace
{
	/** Distancia normalizada a la planta de la torre (superelipse; 1 = pie de la pared). */
	float TowerDistance(const FKarstTower& Tower, float Qx, float Qy, FVector2D& OutLocal)
	{
		const FVector2D D = FVector2D(Qx, Qy) - Tower.Center;
		const float C = FMath::Cos(-Tower.Angle);
		const float S = FMath::Sin(-Tower.Angle);
		OutLocal = FVector2D(D.X * C - D.Y * S, (D.X * S + D.Y * C) / Tower.Aspect);
		const float Ax = FMath::Abs(static_cast<float>(OutLocal.X)) / Tower.Radius;
		const float Ay = FMath::Abs(static_cast<float>(OutLocal.Y)) / Tower.Radius;
		return FMath::Pow(FMath::Pow(Ax, Tower.Squareness) + FMath::Pow(Ay, Tower.Squareness), 1.0f / Tower.Squareness);
	}

	/** Descarte rápido: fuera de este radio (Q) la torre no aporta nada. */
	float TowerBound(const FKarstTower& Tower, float Scale)
	{
		return Tower.Radius * Scale * 1.45f / FMath::Min(Tower.Aspect, 1.0f);
	}

	bool Overlaps(const FKarstLayout& Layout, const FVector2D& Center, float Radius)
	{
		for (const FKarstTower& T : Layout.Towers)
		{
			// Se pueden solapar algo: varias torres juntas forman un macizo de paredes quebradas.
			if (FVector2D::Distance(T.Center, Center) < 0.6f * (T.Radius + Radius))
			{
				return true;
			}
		}
		for (const FKarstLagoon& L : Layout.Lagoons)
		{
			if (FVector2D::Distance(L.Center, Center) < L.Radius * 1.15f + Radius)
			{
				return true;
			}
		}
		return false;
	}

	void AddTower(FKarstLayout& Layout, FExploredRandom& Rng, const FVector2D& Center, float Radius, float Height, float Angle)
	{
		if (Center.Size() + Radius >= 1.0f || Overlaps(Layout, Center, Radius))
		{
			return;
		}
		FKarstTower Tower;
		Tower.Center = Center;
		Tower.Radius = Radius;
		// Las de la orilla son farallones más bajos que las del centro del macizo.
		Tower.Height = FMath::Clamp(Height * FMath::Lerp(1.0f, 0.7f, FMath::SmoothStep(0.55f, 0.95f, static_cast<float>(Center.Size()))), 0.35f, 1.0f);
		Tower.Aspect = Rng.RangeFloat(0.6f, 1.0f);
		Tower.Angle = Angle + Rng.RangeFloat(-0.5f, 0.5f);
		Tower.Squareness = Rng.RangeFloat(2.4f, 4.0f);
		Tower.Seed = Rng.NextUInt32();
		Layout.Towers.Add(Tower);
	}

	/** Laguna interior y un anillo de torres que la encierra. */
	void AddLagoon(FKarstLayout& Layout, FExploredRandom& Rng, float RidgeAngle)
	{
		const float A = Rng.RangeFloat(0.0f, UE_TWO_PI);
		FKarstLagoon Lagoon;
		Lagoon.Center = FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rng.RangeFloat(0.1f, 0.45f);
		Lagoon.Radius = Rng.RangeFloat(0.08f, 0.13f);
		Lagoon.Floor = Rng.RangeFloat(-4.0f, -2.5f);
		Layout.Lagoons.Add(Lagoon);
		const int32 Rim = Rng.RangeInt(4, 6);
		for (int32 I = 0; I < Rim; ++I)
		{
			const float Around = A + UE_TWO_PI * (I + Rng.RangeFloat(-0.2f, 0.2f)) / Rim;
			const float Radius = Rng.RangeFloat(0.08f, 0.14f);
			const FVector2D Dir(FMath::Cos(Around), FMath::Sin(Around));
			AddTower(Layout, Rng, Lagoon.Center + Dir * (Lagoon.Radius * 1.2f + Radius), Radius, Rng.RangeFloat(0.55f, 0.95f), RidgeAngle);
		}
	}
}

FKarstLayout FKarstTowerModel::Generate(uint32 IslandSeed)
{
	FKarstLayout Layout;
	FExploredRandom Rng(static_cast<uint64>(IslandSeed) ^ 0x4B41525354ULL);
	const float RidgeAngle = Rng.RangeFloat(0.0f, UE_PI);
	const FVector2D Axis(FMath::Cos(RidgeAngle), FMath::Sin(RidgeAngle));
	const FVector2D Across(-Axis.Y, Axis.X);
	AddLagoon(Layout, Rng, RidgeAngle);

	const int32 Wanted = Rng.RangeInt(MinTowers, MaxTowers);
	for (int32 Attempt = 0; Attempt < 800 && Layout.Towers.Num() < Wanted; ++Attempt)
	{
		// Seis de cada diez, a lo largo de una cresta principal (más altas hacia el centro);
		// el resto sueltas por el macizo y la orilla.
		// Las torres son anchas (100-200 m de radio en una isla de 600 m) y se funden en
		// macizos: en El Nido la caliza ocupa casi toda la isla y el llano son bolsas entre ellas.
		if (Rng.Chance(0.6f))
		{
			const float Along = Rng.RangeFloat(-0.72f, 0.72f);
			const FVector2D P = Axis * Along + Across * Rng.RangeFloat(-0.28f, 0.28f);
			AddTower(Layout, Rng, P, Rng.RangeFloat(0.14f, 0.27f), Rng.RangeFloat(0.55f, 1.0f) * (1.0f - 0.3f * FMath::Abs(Along)), RidgeAngle);
		}
		else
		{
			const float A = Rng.RangeFloat(0.0f, UE_TWO_PI);
			const FVector2D P = FVector2D(FMath::Cos(A), FMath::Sin(A)) * FMath::Sqrt(Rng.NextFloat()) * 0.85f;
			AddTower(Layout, Rng, P, Rng.RangeFloat(0.08f, 0.17f), Rng.RangeFloat(0.35f, 0.75f), RidgeAngle);
		}
	}
	return Layout;
}

namespace
{
	/**
	 * Cima (fracción de la altura máxima, sin la pared) y máscara de pared de una torre en Q.
	 * false si Q queda fuera de su planta.
	 */
	bool EvaluateTower(const FKarstTower& Tower, float Qx, float Qy, float& OutTop, float& OutWall)
	{
		if (FVector2D::DistSquared(Tower.Center, FVector2D(Qx, Qy)) > FMath::Square(TowerBound(Tower, 1.0f)))
		{
			return false;
		}
		FVector2D Local;
		const FExploredNoise N(Tower.Seed);
		// Acanaladuras verticales (lapiaz) en la pared: la planta no es una curva limpia.
		const float Scale = Tower.Radius * 0.3f;
		const float D = TowerDistance(Tower, Qx, Qy, Local) * (1.0f + 0.09f * N.Fbm2D(Local.X / Scale, Local.Y / Scale, 2));
		if (D >= 1.0f)
		{
			return false;
		}
		OutWall = 1.0f - FMath::SmoothStep(1.0f - FKarstTowerModel::WallSoftness, 1.0f, D);
		const float Dome = 0.82f + 0.18f * (1.0f - D * D);
		const float Crag = 0.12f * (N.Ridged2D(Local.X / (Tower.Radius * 0.45f) + 7.0f, Local.Y / (Tower.Radius * 0.45f), 3) - 0.5f);
		OutTop = Tower.Height * (Dome + Crag);
		return true;
	}
}

float FKarstTowerModel::TowerHeight(const FKarstLayout& Layout, float Qx, float Qy, float* OutCore)
{
	float Best = 0.0f;
	float Core = 0.0f;
	for (const FKarstTower& Tower : Layout.Towers)
	{
		float Top = 0.0f;
		float Wall = 0.0f;
		if (EvaluateTower(Tower, Qx, Qy, Top, Wall))
		{
			Best = FMath::Max(Best, Top * Wall);
			Core = FMath::Max(Core, Wall);
		}
	}
	if (OutCore)
	{
		*OutCore = Core;
	}
	return Best;
}

float FKarstTowerModel::ApplyTowers(const FKarstLayout& Layout, float Qx, float Qy, float Land, float MaxHeight)
{
	for (const FKarstTower& Tower : Layout.Towers)
	{
		float Top = 0.0f;
		float Wall = 0.0f;
		if (EvaluateTower(Tower, Qx, Qy, Top, Wall))
		{
			Land = FMath::Lerp(Land, FMath::Max(Land, Top * MaxHeight), Wall);
		}
	}
	return Land;
}


float FKarstTowerModel::LowlandHeight(const FKarstLayout& Layout, const FExploredNoise& Noise, float Qx, float Qy, float U, float MaxHeight)
{
	// Llano de selva y arena: montículos de unos pocos metros que arrancan tras la playa.
	float Land = 1.8f * FMath::SmoothStep(-0.02f, 0.06f, U)
		+ (2.5f + 3.5f * Noise.Fbm2D(Qx * 6.0f + 3.0f, Qy * 6.0f, 3)) * FMath::SmoothStep(0.02f, 0.3f, U);

	// Arranque de cada torre: montículo de derrubios que la erosión reparte al pie de la pared.
	float Talus = 0.0f;
	for (const FKarstTower& Tower : Layout.Towers)
	{
		if (FVector2D::DistSquared(Tower.Center, FVector2D(Qx, Qy)) > FMath::Square(TowerBound(Tower, 1.6f)))
		{
			continue;
		}
		FVector2D Local;
		const float D = TowerDistance(Tower, Qx, Qy, Local);
		Talus = FMath::Max(Talus, TalusFraction * Tower.Height * MaxHeight * (1.0f - FMath::SmoothStep(0.6f, 1.6f, D)));
	}
	return Land + Talus * FMath::SmoothStep(-0.05f, 0.1f, U);
}

float FKarstTowerModel::ApplyLagoons(const FKarstLayout& Layout, float Qx, float Qy, float Land)
{
	for (const FKarstLagoon& Lagoon : Layout.Lagoons)
	{
		const float R = FVector2D::Distance(FVector2D(Qx, Qy), Lagoon.Center) / Lagoon.Radius;
		if (R < 1.4f)
		{
			Land = FMath::Lerp(Lagoon.Floor, Land, FMath::SmoothStep(0.75f, 1.4f, R));
		}
	}
	return Land;
}
