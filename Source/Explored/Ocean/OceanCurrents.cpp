#include "Ocean/OceanCurrents.h"

#include "WorldGen/ArchipelagoLayout.h"

namespace
{
	// WorldGen trabaja en metros (ver TerrainChunkBuilder::Build); el resto del
	// juego (Ocean, Player) en centímetros.
	constexpr float MetersToCm = 100.0f;
}

float FOceanTide::Level(float TotalDays)
{
	return FMath::Sin(TotalDays * CyclesPerDay * UE_TWO_PI);
}

float FOceanTide::Flow(float TotalDays)
{
	// Derivada de Level: máxima a media marea, cero en el cambio de marea.
	return FMath::Cos(TotalDays * CyclesPerDay * UE_TWO_PI);
}

float FOceanTide::SpringNeapFactor(float MoonPhase01)
{
	// Periodo de medio ciclo lunar: pico en 0 (nueva) y 0.5 (llena), valle en 0.25 y 0.75 (cuartos).
	return 0.7f + 0.3f * FMath::Cos(4.0f * UE_PI * MoonPhase01);
}

TArray<FOceanStrait> FOceanCurrents::BuildStraits(const FArchipelagoLayout& Layout)
{
	TArray<FOceanStrait> Straits;
	Straits.Reserve(FMath::Max(0, Layout.Islands.Num() - 1));
	for (int32 I = 0; I + 1 < Layout.Islands.Num(); ++I)
	{
		const FIslandDesc& A = Layout.Islands[I];
		const FIslandDesc& B = Layout.Islands[I + 1];
		const FVector2D CenterA = A.Center * MetersToCm;
		const FVector2D CenterB = B.Center * MetersToCm;
		const FVector2D Dir = (CenterB - CenterA).GetSafeNormal();
		if (Dir.IsNearlyZero())
		{
			continue;
		}
		FOceanStrait Strait;
		Strait.CoastA = CenterA + Dir * (A.Radius * MetersToCm);
		Strait.CoastB = CenterB - Dir * (B.Radius * MetersToCm);
		// Medio ancho del canal real entre las dos costas; nunca cero para evitar dividir por él.
		Strait.HalfWidthCm = FMath::Max(50.0f, FVector2D::Distance(Strait.CoastA, Strait.CoastB) * 0.5f);
		Straits.Add(Strait);
	}
	return Straits;
}

FVector2D FOceanCurrents::CurrentAt(const TArray<FOceanStrait>& Straits, const FVector2D& PositionCm,
	float FlowSign, float TideStrength01, float Wind01)
{
	FVector2D Result = FVector2D::ZeroVector;
	for (const FOceanStrait& Strait : Straits)
	{
		const FVector2D Segment = Strait.CoastB - Strait.CoastA;
		const float LengthSq = Segment.SizeSquared();
		if (LengthSq < KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const float Alpha = FMath::Clamp(FVector2D::DotProduct(PositionCm - Strait.CoastA, Segment) / LengthSq, 0.0f, 1.0f);
		const FVector2D Closest = Strait.CoastA + Segment * Alpha;
		const float PerpDist = FVector2D::Distance(PositionCm, Closest);
		if (PerpDist >= Strait.HalfWidthCm)
		{
			continue;
		}
		// Se calma cerca de las dos costas, máxima en el centro del canal.
		const float EdgeFalloff = FMath::Square(1.0f - PerpDist / Strait.HalfWidthCm);
		const FVector2D Dir = Segment.GetSafeNormal();
		const float Magnitude = MaxSpeedCmS * FMath::Clamp(TideStrength01, 0.0f, 1.0f)
			* (0.6f + 0.4f * FMath::Clamp(Wind01, 0.0f, 1.0f)) * EdgeFalloff * FMath::Clamp(FlowSign, -1.0f, 1.0f);
		Result += Dir * Magnitude;
	}
	return Result;
}
