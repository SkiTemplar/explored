#include "Weather/RainStreaksModel.h"

namespace RainStreaksDetail
{
	double Hash01(uint32 Seed, uint32 Index, uint32 Salt)
	{
		uint32 X = Seed ^ (Index * 0x9E3779B9u) ^ (Salt * 0x85EBCA6Bu);
		X ^= X >> 16;
		X *= 0x7FEB352Du;
		X ^= X >> 15;
		X *= 0x846CA68Bu;
		X ^= X >> 16;
		return static_cast<double>(X & 0xFFFFFFu) / static_cast<double>(0x1000000u);
	}
}

FRainStreaksModel::FRainStreaksModel(const FRainStreaksTuning& InTuning, uint32 Seed)
	: Tuning(InTuning)
{
	Tuning.MaxStreaks = FMath::Max(Tuning.MaxStreaks, 0);
	Tuning.HalfExtentCm = FMath::Max(Tuning.HalfExtentCm, 1.0f);
	Tuning.HeightCm = FMath::Max(Tuning.HeightCm, 1.0f);
	Seeds.Reserve(Tuning.MaxStreaks);
	for (int32 Index = 0; Index < Tuning.MaxStreaks; ++Index)
	{
		Seeds.Add(FVector(RainStreaksDetail::Hash01(Seed, Index, 1), RainStreaksDetail::Hash01(Seed, Index, 2),
			RainStreaksDetail::Hash01(Seed, Index, 3)));
	}
}

int32 FRainStreaksModel::CountFor(float Intensity) const
{
	if (!FMath::IsFinite(Intensity) || Intensity < Tuning.MinIntensity)
	{
		return 0;
	}
	return FMath::Clamp(FMath::RoundToInt(Tuning.MaxStreaks * FMath::Min(Intensity, 1.0f)), 0, Tuning.MaxStreaks);
}

double FRainStreaksModel::Wrap(double Value, double Center, double Half)
{
	const double Span = 2.0 * Half;
	const double Local = FMath::Fmod(Value - (Center - Half), Span);
	return (Local < 0.0 ? Local + Span : Local) + (Center - Half);
}

FRainStreaksFrame FRainStreaksModel::Evaluate(double Time, float Intensity, float Wind, const FVector2D& WindDir, const FVector& Camera) const
{
	FRainStreaksFrame Frame;
	const int32 Count = CountFor(Intensity);
	if (Count == 0)
	{
		return Frame;
	}
	const float Strength = FMath::Clamp(FMath::IsFinite(Wind) ? Wind : 0.0f, 0.0f, 1.0f);
	const FVector2D Dir = WindDir.GetSafeNormal();
	const FVector2D Drift = Dir * (Tuning.MaxWindCmS * Strength);
	const FVector Velocity(Drift.X, Drift.Y, -Tuning.FallCmS);
	Frame.Direction = Velocity.GetSafeNormal();
	Frame.LengthCm = FMath::Lerp(Tuning.LengthLightCm, Tuning.LengthHeavyCm, FMath::Clamp(Intensity, 0.0f, 1.0f));

	const double Half = Tuning.HalfExtentCm;
	const double Height = Tuning.HeightCm;
	const double Floor = Camera.Z - Height / 3.0;
	Frame.Positions.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector& S = Seeds[Index];
		// Cada trazo cae a su ritmo (±20 %) para que no bajen en filas.
		const double Speed = 0.8 + 0.4 * S.Z;
		const double T = Time * Speed;
		const double X = S.X * 2.0 * Half + Velocity.X * T;
		const double Y = S.Y * 2.0 * Half + Velocity.Y * T;
		const double Z = S.Z * Height + Velocity.Z * T;
		FVector P(Wrap(X, Camera.X, Half), Wrap(Y, Camera.Y, Half), Wrap(Z, Floor + Height / 2.0, Height / 2.0));
		// Los que caen junto a la cámara se apartan hasta el borde del hueco.
		const FVector2D Away(P.X - Camera.X, P.Y - Camera.Y);
		const double Near = Away.Size();
		if (Near < Tuning.NearClearCm)
		{
			const FVector2D Push = Near > 1.0 ? Away / Near : FVector2D(1.0, 0.0);
			P.X = Camera.X + Push.X * Tuning.NearClearCm;
			P.Y = Camera.Y + Push.Y * Tuning.NearClearCm;
		}
		Frame.Positions.Add(P);
	}
	return Frame;
}
