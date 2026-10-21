#include "Sky/CloudFieldModel.h"

#include "Core/ExploredRandom.h"

namespace CloudFieldDetail
{
	/** Envuelve una coordenada en [-Half, Half). */
	double Wrap(double V, double Half)
	{
		const double Size = 2.0 * Half;
		double R = FMath::Fmod(V + Half, Size);
		if (R < 0.0)
		{
			R += Size;
		}
		return R - Half;
	}

	float Fade(float Edge0, float Edge1, float X)
	{
		return FMath::SmoothStep(Edge0, Edge1, X);
	}
}

FCloudFieldModel FCloudFieldModel::Make(uint32 Seed, int32 Count)
{
	FCloudFieldModel Field;
	FExploredRandom Rng(Seed);
	const int32 N = FMath::Clamp(Count, 0, 256);
	Field.Clouds.Reserve(N);
	for (int32 I = 0; I < N; ++I)
	{
		FCloudInstance C;
		C.Variant = Rng.RangeInt(0, NumVariants - 1);
		const float Angle = Rng.RangeFloat(0.0f, UE_TWO_PI);
		// Radio uniforme en área dentro del anillo.
		const float R2 = Rng.RangeFloat(InnerRadius * InnerRadius, OuterRadius * OuterRadius);
		const float Radius = FMath::Sqrt(R2);
		C.Position = FVector2D(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);
		C.Altitude = Rng.RangeFloat(MinAltitude, MaxAltitude);
		C.YawDeg = Rng.RangeFloat(0.0f, 360.0f);
		C.Scale = Rng.RangeFloat(1.4f, 2.6f);
		// Un tercio de las nubes se ve siempre; el resto entra con la nubosidad.
		C.CoverThreshold = (I % 3 == 0) ? 0.0f : Rng.RangeFloat(0.15f, 0.7f);
		Field.Clouds.Add(C);
	}
	return Field;
}

void FCloudFieldModel::Advance(const FVector2D& WindDir, float Wind, float DeltaSeconds)
{
	if (!FMath::IsFinite(Wind) || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return;
	}
	const FVector2D Dir = WindDir.GetSafeNormal();
	const double Speed = MaxDriftSpeed * FMath::Clamp(Wind, 0.05f, 1.0f);
	const FVector2D Step = Dir * (Speed * DeltaSeconds);
	for (FCloudInstance& C : Clouds)
	{
		C.Position.X = CloudFieldDetail::Wrap(C.Position.X + Step.X, OuterRadius);
		C.Position.Y = CloudFieldDetail::Wrap(C.Position.Y + Step.Y, OuterRadius);
	}
}

float FCloudFieldModel::Visibility(const FCloudInstance& Cloud, float CloudCover)
{
	const float Radius = static_cast<float>(Cloud.Position.Size());
	const float Ring = CloudFieldDetail::Fade(InnerRadius - 60.0f, InnerRadius, Radius)
		* (1.0f - CloudFieldDetail::Fade(OuterRadius - 80.0f, OuterRadius, Radius));
	const float Cover = FMath::IsFinite(CloudCover) ? CloudCover : 0.0f;
	const float Weather = CloudFieldDetail::Fade(Cloud.CoverThreshold - 0.05f, Cloud.CoverThreshold + 0.05f, Cover);
	return Cloud.CoverThreshold <= 0.0f ? Ring : Ring * Weather;
}
