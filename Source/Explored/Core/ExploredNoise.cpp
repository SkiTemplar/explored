#include "Core/ExploredNoise.h"

#include "Core/ExploredRandom.h"

namespace
{
	FORCEINLINE float Fade(float T)
	{
		return T * T * T * (T * (T * 6.0f - 15.0f) + 10.0f);
	}

	FORCEINLINE float Grad2(uint32 H, float X, float Y)
	{
		// 8 direcciones unitarias repartidas uniformemente.
		static constexpr float Diag = 0.70710678f;
		static constexpr float Dirs[8][2] = {
			{1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f},
			{Diag, Diag}, {-Diag, Diag}, {Diag, -Diag}, {-Diag, -Diag}};
		const float* D = Dirs[H & 7u];
		return D[0] * X + D[1] * Y;
	}

	FORCEINLINE float Grad3(uint32 H, float X, float Y, float Z)
	{
		// 12 aristas del cubo (Perlin mejorado).
		switch (H % 12u)
		{
		case 0: return X + Y;
		case 1: return -X + Y;
		case 2: return X - Y;
		case 3: return -X - Y;
		case 4: return X + Z;
		case 5: return -X + Z;
		case 6: return X - Z;
		case 7: return -X - Z;
		case 8: return Y + Z;
		case 9: return -Y + Z;
		case 10: return Y - Z;
		default: return -Y - Z;
		}
	}

	/**
	 * Gira y desplaza cada octava para que sus rejillas no coincidan: sin esto, los
	 * ceros de la rejilla del ruido de gradiente se alinean en todas las octavas y las
	 * costas salen con tramos rectos y esquinas en ángulo recto.
	 */
	FORCEINLINE FVector2D OctaveDomain(float X, float Y, int32 Octave)
	{
		// Rotación de 0,6435 rad (triángulo 3-4-5) acumulada por octava.
		float C = 1.0f;
		float S = 0.0f;
		for (int32 I = 0; I <= Octave; ++I)
		{
			const float NC = C * 0.8f - S * 0.6f;
			S = S * 0.8f + C * 0.6f;
			C = NC;
		}
		return FVector2D(X * C - Y * S + 31.416f * (Octave + 1), X * S + Y * C - 17.32f * (Octave + 1));
	}

	template <typename FSampler>
	float Fractal(FSampler&& Sample, int32 Octaves, float Lacunarity, float Gain)
	{
		float Sum = 0.0f;
		float Amplitude = 1.0f;
		float Frequency = 1.0f;
		float Norm = 0.0f;
		for (int32 I = 0; I < Octaves; ++I)
		{
			Sum += Sample(Frequency, I) * Amplitude;
			Norm += Amplitude;
			Amplitude *= Gain;
			Frequency *= Lacunarity;
		}
		return Norm > 0.0f ? Sum / Norm : 0.0f;
	}
}

float FExploredNoise::Gradient2D(float X, float Y) const
{
	const int32 X0 = FMath::FloorToInt32(X);
	const int32 Y0 = FMath::FloorToInt32(Y);
	const float Fx = X - X0;
	const float Fy = Y - Y0;

	const float N00 = Grad2(ExploredHash::Hash2D(Seed, X0, Y0), Fx, Fy);
	const float N10 = Grad2(ExploredHash::Hash2D(Seed, X0 + 1, Y0), Fx - 1.0f, Fy);
	const float N01 = Grad2(ExploredHash::Hash2D(Seed, X0, Y0 + 1), Fx, Fy - 1.0f);
	const float N11 = Grad2(ExploredHash::Hash2D(Seed, X0 + 1, Y0 + 1), Fx - 1.0f, Fy - 1.0f);

	const float U = Fade(Fx);
	const float V = Fade(Fy);
	const float Value = FMath::Lerp(FMath::Lerp(N00, N10, U), FMath::Lerp(N01, N11, U), V);
	// El máximo teórico en 2D es sqrt(2)/2; se reescala a [-1, 1].
	return FMath::Clamp(Value * 1.41421356f, -1.0f, 1.0f);
}

float FExploredNoise::Gradient3D(float X, float Y, float Z) const
{
	const int32 X0 = FMath::FloorToInt32(X);
	const int32 Y0 = FMath::FloorToInt32(Y);
	const int32 Z0 = FMath::FloorToInt32(Z);
	const float Fx = X - X0;
	const float Fy = Y - Y0;
	const float Fz = Z - Z0;

	auto Corner = [&](int32 Dx, int32 Dy, int32 Dz)
	{
		return Grad3(ExploredHash::Hash3D(Seed, X0 + Dx, Y0 + Dy, Z0 + Dz), Fx - Dx, Fy - Dy, Fz - Dz);
	};

	const float U = Fade(Fx);
	const float V = Fade(Fy);
	const float W = Fade(Fz);

	const float X00 = FMath::Lerp(Corner(0, 0, 0), Corner(1, 0, 0), U);
	const float X10 = FMath::Lerp(Corner(0, 1, 0), Corner(1, 1, 0), U);
	const float X01 = FMath::Lerp(Corner(0, 0, 1), Corner(1, 0, 1), U);
	const float X11 = FMath::Lerp(Corner(0, 1, 1), Corner(1, 1, 1), U);
	const float Value = FMath::Lerp(FMath::Lerp(X00, X10, V), FMath::Lerp(X01, X11, V), W);
	return FMath::Clamp(Value, -1.0f, 1.0f);
}

float FExploredNoise::Fbm2D(float X, float Y, int32 Octaves, float Lacunarity, float Gain) const
{
	return Fractal([&](float F, int32 I)
	{
		const FExploredNoise Octave(Seed + static_cast<uint32>(I) * 1013u);
		const FVector2D P = OctaveDomain(X * F, Y * F, I);
		return Octave.Gradient2D(P.X, P.Y);
	}, Octaves, Lacunarity, Gain);
}

float FExploredNoise::Fbm3D(float X, float Y, float Z, int32 Octaves, float Lacunarity, float Gain) const
{
	return Fractal([&](float F, int32 I)
	{
		const FExploredNoise Octave(Seed + static_cast<uint32>(I) * 1013u);
		return Octave.Gradient3D(X * F, Y * F, Z * F);
	}, Octaves, Lacunarity, Gain);
}

float FExploredNoise::Ridged2D(float X, float Y, int32 Octaves, float Lacunarity, float Gain) const
{
	return Fractal([&](float F, int32 I)
	{
		const FExploredNoise Octave(Seed + static_cast<uint32>(I) * 7919u);
		const FVector2D P = OctaveDomain(X * F, Y * F, I);
		const float R = 1.0f - FMath::Abs(Octave.Gradient2D(P.X, P.Y));
		return R * R;
	}, Octaves, Lacunarity, Gain);
}

FVector2D FExploredNoise::Warp2D(float X, float Y, float Strength, int32 Octaves) const
{
	const FExploredNoise NoiseA(Seed ^ 0xA511E9B3u);
	const FExploredNoise NoiseB(Seed ^ 0x63D83595u);
	const float Dx = NoiseA.Fbm2D(X + 17.3f, Y - 4.1f, Octaves);
	const float Dy = NoiseB.Fbm2D(X - 9.7f, Y + 31.9f, Octaves);
	return FVector2D(X + Dx * Strength, Y + Dy * Strength);
}
