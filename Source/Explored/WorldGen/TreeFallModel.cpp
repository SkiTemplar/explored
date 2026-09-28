#include "WorldGen/TreeFallModel.h"

namespace
{
	bool IsFinite2D(const FVector2D& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y);
	}

	/** Por debajo de esto el viento va de cara o de espaldas: no hay lado hacia el que desviar. */
	constexpr double MinCrossForSide = 1.0e-9;
}

FVector2D FTreeFallModel::ApplyWind(const FVector2D& Direction, const FTreeFallWind& Wind, double MaxDeviationDeg)
{
	const FVector2D Dir = IsFinite2D(Direction) ? Direction.GetSafeNormal() : FVector2D::ZeroVector;
	const FVector2D WindDir = IsFinite2D(Wind.Direction) ? Wind.Direction.GetSafeNormal() : FVector2D::ZeroVector;
	const double Strength = FMath::IsFinite(Wind.Strength) ? FMath::Clamp((double)Wind.Strength, 0.0, 1.0) : 0.0;
	const double MaxRad = FMath::IsFinite(MaxDeviationDeg) ? FMath::DegreesToRadians(FMath::Max(0.0, MaxDeviationDeg)) : 0.0;
	if (Dir.IsZero() || WindDir.IsZero() || Strength <= 0.0 || MaxRad <= 0.0)
	{
		return Dir;
	}
	const double Cross = Dir.X * WindDir.Y - Dir.Y * WindDir.X;
	const double Dot = Dir.X * WindDir.X + Dir.Y * WindDir.Y;
	if (FMath::Abs(Cross) < MinCrossForSide)
	{
		return Dir;
	}
	const double Between = FMath::Atan2(Cross, Dot);
	const double Turn = FMath::Sign(Between) * FMath::Min(FMath::Abs(Between), MaxRad * Strength);
	const double C = FMath::Cos(Turn);
	const double S = FMath::Sin(Turn);
	return FVector2D(Dir.X * C - Dir.Y * S, Dir.X * S + Dir.Y * C).GetSafeNormal();
}

FVector2D FTreeFallModel::ResolveDirection(const FFellingProfile& Profile, const FFellingProgress& Progress,
	const FVector2D& Downhill, const FTreeFallWind& Wind, uint32 InstanceSeed)
{
	const FVector2D Base = FFellingModel::ResolveFallDirection(Progress, Downhill, InstanceSeed);
	return ApplyWind(Base, Wind, Profile.WindDeviationDeg);
}

FTreeFallResult FTreeFallModel::Resolve(const FFellingProfile& Profile, const FVector2D& Base, const FVector2D& Direction,
	const TArray<FTreeFallObstacle>& Obstacles)
{
	FTreeFallResult Result;
	const FVector2D Dir = IsFinite2D(Direction) ? Direction.GetSafeNormal() : FVector2D::ZeroVector;
	Result.Direction = Dir.IsZero() ? FVector2D(1.0, 0.0) : Dir;
	const double HeightCm = FMath::IsFinite(Profile.HeightMeters) ? FMath::Max(0.0f, Profile.HeightMeters) * 100.0 : 0.0;
	if (HeightCm <= 0.0 || !IsFinite2D(Base))
	{
		// El arbusto no cae: se desbroza en el sitio, sin barrer nada.
		Result.RestAngleDeg = 0.0;
		Result.ReachFraction = 0.0;
		return Result;
	}

	struct FContact
	{
		int32 Index;
		double AngleRad;
	};
	TArray<FContact> Contacts;
	for (int32 i = 0; i < Obstacles.Num(); ++i)
	{
		const FTreeFallObstacle& O = Obstacles[i];
		if (!IsFinite2D(O.Center) || !FMath::IsFinite(O.RadiusCm) || !FMath::IsFinite(O.TopCm) || O.TopCm <= 0.0)
		{
			continue;
		}
		const FVector2D Rel = O.Center - Base;
		const double Along = Rel.X * Result.Direction.X + Rel.Y * Result.Direction.Y;
		const double Across = FMath::Abs(Rel.X * Result.Direction.Y - Rel.Y * Result.Direction.X);
		const double Reach = FMath::Max(0.0, O.RadiusCm) + TrunkRadiusCm;
		if (Across >= Reach)
		{
			continue;
		}
		const double HalfChord = FMath::Sqrt(Reach * Reach - Across * Across);
		if (Along + HalfChord <= 0.0)
		{
			// Detrás de la base: el tronco cae hacia el otro lado.
			continue;
		}
		// Borde más cercano de la huella sobre la línea de caída (0 si la base está dentro).
		const double Near = FMath::Max(0.0, Along - HalfChord);
		// El tronco toca la esquina (Near, Top) si llega a ella: barre el cuarto de círculo de radio la altura.
		if (Near * Near + O.TopCm * O.TopCm > HeightCm * HeightCm)
		{
			continue;
		}
		Contacts.Add({ i, FMath::Atan2(Near, O.TopCm) });
	}
	Contacts.Sort([](const FContact& A, const FContact& B)
	{
		return A.AngleRad != B.AngleRad ? A.AngleRad < B.AngleRad : A.Index < B.Index;
	});

	for (const FContact& Contact : Contacts)
	{
		const FTreeFallObstacle& O = Obstacles[Contact.Index];
		FTreeFallHit Hit;
		Hit.ObstacleIndex = Contact.Index;
		Hit.ContactAngleDeg = FMath::RadiansToDegrees(Contact.AngleRad);
		if (O.Kind == ETreeFallObstacleKind::Building && O.TierOrder <= MaxCrushedTierOrder)
		{
			Hit.Damage = FMath::IsFinite(O.MaxIntegrity) ? FMath::Max(0.0f, O.MaxIntegrity) * CrushDamageFraction : 0.0f;
			Result.Crushed.Add(Hit);
			continue;
		}
		Result.StoppedBy = Hit;
		Result.RestAngleDeg = Hit.ContactAngleDeg;
		Result.ReachFraction = FMath::Sin(Contact.AngleRad);
		return Result;
	}
	return Result;
}
