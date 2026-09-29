#include "WorldGen/CoastalCliffModel.h"

#include "Core/ExploredNoise.h"
#include "WorldGen/IslandShapeModel.h"

namespace
{
	FORCEINLINE float SmoothStep(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** Transición (en unidades del ruido) entre costa normal y acantilado pleno. */
	constexpr float EdgeSoftness = 0.04f;
	/** Semiancho (celdas de 0,5°) de la rampa con que acaba cada tramo a lo largo de la costa. */
	constexpr int32 EndRampBins = 5;

	/** Umbral que deja por encima la fracción Fraction de los valores. */
	float ThresholdFor(TArray<float> Values, float Fraction)
	{
		Values.Sort();
		const int32 Index = FMath::Clamp(FMath::RoundToInt32((1.0f - Fraction) * Values.Num()), 0, Values.Num() - 1);
		return Values[Index];
	}

	/**
	 * Media móvil circular (dos pasadas de caja de ±Radius celdas): los extremos de cada tramo
	 * bajan en rampa a lo largo de la costa. Con el corte seco del umbral quedaba una pared
	 * lateral de toda la altura del acantilado que se metía tierra adentro.
	 */
	TArray<float> SmoothCircular(const TArray<float>& Values, int32 Radius)
	{
		TArray<float> Current = Values;
		const int32 Num = Current.Num();
		for (int32 Pass = 0; Pass < 2 && Num > 0; ++Pass)
		{
			TArray<float> Next;
			Next.SetNum(Num);
			for (int32 I = 0; I < Num; ++I)
			{
				float Sum = 0.0f;
				for (int32 K = -Radius; K <= Radius; ++K)
				{
					Sum += Current[((I + K) % Num + Num) % Num];
				}
				Next[I] = Sum / (2 * Radius + 1);
			}
			Current = MoveTemp(Next);
		}
		return Current;
	}

	float SectorAmount(const TArray<FVector2D>& Sectors, float Angle)
	{
		float Amount = 0.0f;
		for (const FVector2D& Sector : Sectors)
		{
			const float Half = FMath::Max(static_cast<float>(Sector.Y), 0.05f);
			const float Delta = FMath::Abs(FMath::FindDeltaAngleRadians(Angle, static_cast<float>(Sector.X)));
			Amount = FMath::Max(Amount, SmoothStep(Half, Half * 0.6f, Delta));
		}
		return Amount;
	}
}

float FCoastalCliffs::Sample(const TArray<float>& Table, float Angle) const
{
	if (Table.Num() != Bins || !FMath::IsFinite(Angle))
	{
		return 0.0f;
	}
	const float Pos = FMath::Frac((Angle + UE_PI) / UE_TWO_PI) * Bins;
	const int32 I0 = FMath::Clamp(FMath::FloorToInt32(Pos), 0, Bins - 1);
	return FMath::Lerp(Table[I0], Table[(I0 + 1) % Bins], Pos - I0);
}

bool FCoastalCliffModel::StyleFor(EIslandArchetype Archetype, FCliffStyle& OutStyle)
{
	FCliffStyle Style;
	switch (Archetype)
	{
	case EIslandArchetype::Smoke:
		// Frentes de colada cortados por el mar: paredes altas de basalto.
		Style.Fraction = 0.17f;
		Style.MinHeight = 18.0f;
		Style.MaxHeight = 48.0f;
		break;
	case EIslandArchetype::Emerald:
		Style.Fraction = 0.16f;
		Style.MinHeight = 14.0f;
		Style.MaxHeight = 40.0f;
		break;
	case EIslandArchetype::Landing:
		// Un solo tramo en -Y local: lejos de la bahía (+X) y de la playa del spawn (-X).
		Style.MinHeight = 12.0f;
		Style.MaxHeight = 22.0f;
		Style.Sectors.Add(FVector2D(-UE_HALF_PI, 0.36f));
		break;
	default:
		return false;
	}
	OutStyle = Style;
	return true;
}

FCoastalCliffs FCoastalCliffModel::Build(uint32 Seed, const FCliffStyle& Style)
{
	FCoastalCliffs Cliffs;
	if (Style.Fraction <= 0.0f && Style.Sectors.IsEmpty())
	{
		return Cliffs;
	}
	const FExploredNoise N(Seed ^ 0xC1FFu);
	TArray<float> Raw;
	Raw.SetNum(FCoastalCliffs::Bins);
	for (int32 B = 0; B < FCoastalCliffs::Bins; ++B)
	{
		Raw[B] = FIslandShapeModel::AroundCoast(N, -UE_PI + UE_TWO_PI * B / FCoastalCliffs::Bins, 1.7f, 4.0f, 3);
	}
	const float Threshold = ThresholdFor(Raw, FMath::Clamp(Style.Fraction, 0.0f, 1.0f));
	TArray<float> Amounts;
	Amounts.SetNum(FCoastalCliffs::Bins);
	Cliffs.Heights.SetNum(FCoastalCliffs::Bins);
	for (int32 B = 0; B < FCoastalCliffs::Bins; ++B)
	{
		const float Angle = -UE_PI + UE_TWO_PI * B / FCoastalCliffs::Bins;
		Amounts[B] = Style.Sectors.IsEmpty() ? SmoothStep(Threshold - EdgeSoftness, Threshold + EdgeSoftness, Raw[B])
			: SectorAmount(Style.Sectors, Angle);
		const float Variation = SmoothStep(-0.4f, 0.4f, FIslandShapeModel::AroundCoast(N, Angle, 2.3f, -13.0f, 2));
		Cliffs.Heights[B] = FMath::Lerp(Style.MinHeight, Style.MaxHeight, Variation);
	}
	Cliffs.Amounts = Style.Sectors.IsEmpty() ? SmoothCircular(Amounts, EndRampBins) : MoveTemp(Amounts);
	return Cliffs;
}

float FCoastalCliffModel::Uplift(const FCoastalCliffs& Cliffs, float Angle, float U, float Radius)
{
	// Comprobación por bits: con matemáticas rápidas, !(U > 0) no descarta un NaN.
	if (!FMath::IsFinite(U) || !FMath::IsFinite(Radius) || U <= 0.0f || Radius <= 1.0f)
	{
		return 0.0f;
	}
	const float Amount = Cliffs.Amount(Angle);
	if (Amount <= 0.0f)
	{
		return 0.0f;
	}
	const float Height = Cliffs.Height(Angle) * Amount;
	const float Run = FMath::Max(Height * BackSlopeRun, 40.0f) / Radius;
	// Pared de FaceWidth metros desde la línea de costa y rellano que baja suave tierra adentro.
	return Height * SmoothStep(0.0f, FaceWidth / Radius, U) * (1.0f - SmoothStep(0.0f, Run, U));
}
