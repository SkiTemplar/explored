#include "Ocean/OceanWaves.h"

FOceanWaves FOceanWaves::Make(float SeaState)
{
	const float S = FMath::Clamp(SeaState, 0.0f, 1.0f);
	FOceanWaves Result;
	// Mar de fondo del sureste más oleaje local; direcciones no alineadas para evitar patrones.
	const FVector2D Dirs[NumWaves] = {
		FVector2D(0.8f, 0.6f), FVector2D(0.6f, 0.8f), FVector2D(1.0f, -0.15f), FVector2D(-0.3f, 0.95f)};
	const float Lengths[NumWaves] = {6000.0f, 3100.0f, 1700.0f, 900.0f};
	const float CalmAmp[NumWaves] = {18.0f, 9.0f, 5.0f, 2.5f};
	const float RoughAmp[NumWaves] = {90.0f, 45.0f, 22.0f, 10.0f};
	for (int32 I = 0; I < NumWaves; ++I)
	{
		Result.Waves[I].Direction = Dirs[I].GetSafeNormal();
		Result.Waves[I].Wavelength = Lengths[I];
		Result.Waves[I].Amplitude = FMath::Lerp(CalmAmp[I], RoughAmp[I], S);
		Result.Waves[I].Steepness = FMath::Lerp(0.35f, 0.6f, S);
	}
	return Result;
}

FVector FOceanWaves::Displacement(const FVector2D& Position, float Time) const
{
	FVector Offset = FVector::ZeroVector;
	for (const FGerstnerWave& W : Waves)
	{
		const float K = UE_TWO_PI / W.Wavelength;
		const float Speed = FMath::Sqrt(Gravity / K);
		const float Phase = K * (FVector2D::DotProduct(W.Direction, Position) - Speed * Time);
		// Q normalizado por número de olas para que las crestas no se crucen.
		const float Q = W.Steepness / (K * W.Amplitude * NumWaves);
		Offset.X += Q * W.Amplitude * W.Direction.X * FMath::Cos(Phase);
		Offset.Y += Q * W.Amplitude * W.Direction.Y * FMath::Cos(Phase);
		Offset.Z += W.Amplitude * FMath::Sin(Phase);
	}
	return Offset;
}

float FOceanWaves::HeightAt(const FVector2D& Position, float Time) const
{
	// Busca el punto en reposo cuyo desplazamiento horizontal cae sobre Position.
	FVector2D Rest = Position;
	for (int32 I = 0; I < 3; ++I)
	{
		const FVector D = Displacement(Rest, Time);
		Rest = Position - FVector2D(D.X, D.Y);
	}
	return Displacement(Rest, Time).Z;
}

FVector FOceanWaves::NormalAt(const FVector2D& Position, float Time) const
{
	constexpr float Step = 25.0f; // cm; suficiente para captar la ola más corta (900 cm).
	const float HxMinus = HeightAt(Position - FVector2D(Step, 0.0f), Time);
	const float HxPlus = HeightAt(Position + FVector2D(Step, 0.0f), Time);
	const float HyMinus = HeightAt(Position - FVector2D(0.0f, Step), Time);
	const float HyPlus = HeightAt(Position + FVector2D(0.0f, Step), Time);
	const FVector Tangent(2.0f * Step, 0.0f, HxPlus - HxMinus);
	const FVector Bitangent(0.0f, 2.0f * Step, HyPlus - HyMinus);
	const FVector Normal = FVector::CrossProduct(Tangent, Bitangent).GetSafeNormal();
	// CrossProduct puede degenerar a cero solo si el paso es cero; con Step > 0 siempre da Z > 0.
	return Normal.IsNearlyZero() ? FVector::UpVector : Normal;
}
