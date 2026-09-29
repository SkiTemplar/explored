#include "WorldGen/IslandShapeModel.h"

#include "Core/ExploredRandom.h"
#include "WorldGen/KarstTowerModel.h"

namespace
{
	FORCEINLINE float SmoothStep(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** Mínimo suave polinómico: une dos siluetas sin arista en la junta. */
	FORCEINLINE float SmoothMin(float A, float B, float K)
	{
		const float H = FMath::Max(K - FMath::Abs(A - B), 0.0f) / K;
		return FMath::Min(A, B) - H * H * K * 0.25f;
	}

	FVector2D ToLocal(const FIslandDesc& Island, float X, float Y)
	{
		const FVector2D D = (FVector2D(X, Y) - Island.Center) / Island.Radius;
		const float C = FMath::Cos(-Island.Rotation);
		const float S = FMath::Sin(-Island.Rotation);
		return FVector2D(D.X * C - D.Y * S, D.X * S + D.Y * C);
	}

	float SeedUnit(const FIslandDesc& Island, uint32 Salt)
	{
		return ExploredHash::ToUnitFloat(ExploredHash::Hash32(Island.Seed ^ Salt));
	}

	float LandingLand(const FExploredNoise& N, const FVector2D& Q, float U, float Hmax)
	{
		const float Rise = FMath::Pow(SmoothStep(0.06f, 1.0f, U), 1.6f);
		const float Hills = 0.7f + 0.3f * N.Fbm2D(Q.X * 3.0f, Q.Y * 3.0f, 4);
		// Lomas pequeñas en el interior: sin ellas el palmeral era una cúpula lisa.
		const float Knolls = 1.5f * N.Fbm2D(Q.X * 14.0f - 9.0f, Q.Y * 14.0f, 3) * SmoothStep(0.1f, 0.4f, U);
		float Land = 1.8f * SmoothStep(-0.02f, 0.08f, U) + (Hmax - 2.0f) * Rise * Hills + Knolls;
		// Laguna protegida (lugar del amaraje) con bocana hacia el mar.
		const float Lagoon = FVector2D::Distance(Q, FVector2D(0.58f, 0.0f)) / 0.26f;
		const float Inlet = FMath::Abs(Q.Y) / 0.07f + FMath::Max(0.0f, 0.62f - Q.X) * 10.0f;
		const float Water = FMath::Min(Lagoon, Inlet);
		if (Water < 1.3f)
		{
			const float LagoonFloor = -5.0f + 3.5f * SmoothStep(0.3f, 1.0f, Water);
			Land = FMath::Lerp(LagoonFloor, Land, SmoothStep(0.85f, 1.3f, Water));
		}
		return Land;
	}

	/**
	 * Volcán con delantales de lava junto al mar: en parte del perímetro el cono arranca más
	 * adentro y deja un llano costero de coladas que baja suave al agua (sitio para construir
	 * sin quitarle el carácter al cono, que conserva su altura).
	 */
	float SmokeLand(const FIslandDesc& Island, const FExploredNoise& N, const FVector2D& Q, float T, float Hmax)
	{
		const float U = 1.0f - T;
		const float Angle = FMath::Atan2(static_cast<float>(Q.Y), static_cast<float>(Q.X));
		const float Apron = 0.03f + 0.24f * SmoothStep(0.0f, 0.4f, FIslandShapeModel::AroundCoast(N, Angle, 1.3f, 7.0f + 10.0f * SeedUnit(Island, 0x5A9Fu), 2));
		const float ConeU = FMath::Max(U - Apron, 0.0f) / (1.0f - Apron);
		const float Cone = FMath::Pow(ConeU, 1.35f);
		const float Flows = N.Ridged2D(Q.X * 6.0f, Q.Y * 6.0f, 3);
		float Land = 1.2f * SmoothStep(-0.02f, 0.05f, U) + 4.0f * SmoothStep(0.0f, Apron + 0.02f, U) + Hmax * Cone * (0.92f + 0.08f * Flows);
		// Cráter con borde marcado.
		const float Crater = 0.14f;
		if (T < Crater * 1.3f)
		{
			Land -= (1.0f - SmoothStep(0.0f, Crater, T)) * Hmax * 0.28f;
		}
		return Land;
	}

	/**
	 * Llano de manglar que drena hacia un lado: un estuario serpenteante cruza la isla desde
	 * una cabecera interior hasta su boca en la costa, y el resto del llano cae hacia él, así
	 * que los arroyos se juntan en árbol en lugar de salir en estrella desde el centro (lo que
	 * hacía la cúpula de antes). Relieve menudo a tres escalas encima.
	 */
	float MangroveLand(const FIslandDesc& Island, const FExploredNoise& N, const FVector2D& Q, float U)
	{
		const float Axis = UE_TWO_PI * SeedUnit(Island, 0x3A7Eu);
		const FVector2D Along(FMath::Cos(Axis), FMath::Sin(Axis));
		const FVector2D Side(-Along.Y, Along.X);
		const float S = static_cast<float>(FVector2D::DotProduct(Q, Along));
		const float Lateral = static_cast<float>(FVector2D::DotProduct(Q, Side));
		const float Phase = UE_TWO_PI * SeedUnit(Island, 0x7E11u);
		const float Meander = 0.21f * FMath::Sin(S * UE_TWO_PI / 0.4f + Phase) + 0.05f * N.Fbm2D(S * 3.0f, 7.0f, 2);
		const float Distance = FMath::Abs(Lateral - Meander);
		const float Off = FMath::Min(Distance / 0.7f, 1.0f);
		// Sección en V abierta hacia el cauce, que se apaga más allá de la cabecera, y un canal
		// de marea marcado en la curva: sin él, el relieve menudo desviaba el agua y el cauce
		// principal cortaba las curvas.
		const float Reach = SmoothStep(-0.8f, -0.4f, S);
		const float Valley = (1.0f - FMath::Square(1.0f - Off)) * Reach + (1.0f - Reach);
		const float Channel = (1.0f - SmoothStep(0.0f, 0.04f, Distance)) * Reach;
		const float Head = SmoothStep(1.0f, -0.7f, S);
		const float Relief = 0.5f * N.Fbm2D(Q.X * 9.0f, Q.Y * 9.0f, 3)
			+ 0.35f * N.Fbm2D(Q.X * 21.0f + 30.0f, Q.Y * 21.0f, 4)
			+ 0.15f * N.Fbm2D(Q.X * 53.0f - 70.0f, Q.Y * 53.0f, 3);
		const float Rim = SmoothStep(0.0f, 0.2f, U);
		return 0.4f + Rim * (0.8f + 1.4f * Head + 3.0f * Valley - 1.8f * Channel) + 1.2f * Relief * Rim;
	}

	float WhiteSandsLand(const FExploredNoise& N, const FVector2D& Q, float T, float Hmax)
	{
		// Anillo de arena alrededor de una laguna somera.
		const float Ring = FMath::Abs(T - 0.78f) / 0.14f;
		const float RingLand = 0.6f + Hmax * FMath::Square(FMath::Max(0.0f, 1.0f - Ring));
		const float LagoonFloor = -3.5f - 1.5f * SmoothStep(0.64f, 0.2f, T);
		float Land = Ring < 1.0f ? RingLand : (T < 0.78f ? FMath::Lerp(RingLand, LagoonFloor, SmoothStep(1.0f, 1.8f, Ring)) : -1000.0f);
		// Pasos entre el anillo y la laguna (motus). Solo donde hay anillo o laguna: sobre el
		// centinela -1000 el Lerp dejaba pozos de cientos de metros en el borde del atolón.
		const float Gap = N.Fbm2D(Q.X * 4.0f, Q.Y * 4.0f, 2);
		if (Ring < 1.2f && Gap > 0.35f && Land > -500.0f)
		{
			Land = FMath::Lerp(Land, -0.8f, SmoothStep(0.35f, 0.5f, Gap));
		}
		return Land;
	}

	/** Islotes de Los Dientes sobre un fondo rocoso somero que nunca asoma en bajíos sueltos. */
	float TeethLand(const FIslandDesc& Island, const FExploredNoise& N, const FVector2D& Q)
	{
		float Best = -1000.0f;
		for (int32 I = 0; I < Island.Islets.Num(); ++I)
		{
			const FVector2D Center = Island.Islets[I] / Island.Radius;
			const float R = 0.13f + 0.07f * ExploredHash::ToUnitFloat(ExploredHash::Hash2D(Island.Seed, I, 0));
			const float D = FVector2D::Distance(Q, Center) / R;
			if (D < 2.2f)
			{
				const float Stack = FMath::Pow(FMath::Max(0.0f, 1.0f - D), 0.45f);
				const float Height = Island.MaxHeight * (0.5f + 0.5f * ExploredHash::ToUnitFloat(ExploredHash::Hash2D(Island.Seed, I, 1)));
				// El pie del islote baja sin cortes hasta el fondo rocoso (antes, dos escalones).
				Best = FMath::Max(Best, D < 1.0f ? 1.0f + Height * Stack : FMath::Lerp(1.0f, -5.0f, SmoothStep(1.0f, 2.2f, D)));
			}
		}
		return FMath::Max(Best, -5.4f + 1.8f * N.Fbm2D(Q.X * 5.0f, Q.Y * 5.0f, 3));
	}
}

FVector2D FIslandShapeModel::WarpedLocal(const FIslandDesc& Island, const FExploredNoise& N, float X, float Y, bool bFineDetail)
{
	FVector2D Q = ToLocal(Island, X, Y);
	// Forma alargada propia de cada isla (las circulares no parecen naturales).
	const float Aspect = 0.62f + 0.3f * SeedUnit(Island, 0xA5u);
	Q.Y /= Aspect;
	// Costa irregular: deformación del dominio a dos escalas. El fBm va normalizado por la suma
	// de amplitudes (1,875 con cuatro octavas, 1,5 con dos): con dos octavas y 0,8 veces la
	// fuerza, sus octavas bajas coinciden con las de la deformación completa.
	const FVector2D Warped = bFineDetail ? N.Warp2D(Q.X * 1.6f, Q.Y * 1.6f, 0.55f, 4) / 1.6f
		: N.Warp2D(Q.X * 1.6f, Q.Y * 1.6f, 0.55f * 0.8f, 2) / 1.6f;
	return FMath::Lerp(Q, Warped, 0.85f);
}

float FIslandShapeModel::CoastT(const FIslandDesc& Island, const FExploredNoise& N, const FVector2D& Q, bool bFineDetail)
{
	const float Len = Q.Size();
	const FVector2D Dir = Len > KINDA_SMALL_NUMBER ? Q / Len : FVector2D(1.0f, 0.0f);
	const float Lobes = N.Fbm2D(Dir.X * 1.4f + 5.0f, Dir.Y * 1.4f - 3.0f, 3);
	const float Fine = bFineDetail ? 0.1f * N.Fbm2D(Q.X * 6.0f + 11.0f, Q.Y * 6.0f - 7.0f, 3) : 0.0f;
	const float Coast = 1.0f + 0.32f * Lobes + Fine;
	float T = Len / FMath::Max(Coast, 0.45f);
	for (const FIslandLobe& Lobe : Island.Lobes)
	{
		const FVector2D D = Q - Lobe.Offset;
		const float C = FMath::Cos(-Lobe.Angle);
		const float S = FMath::Sin(-Lobe.Angle);
		const FVector2D L(D.X * C - D.Y * S, (D.X * S + D.Y * C) / Lobe.Aspect);
		T = SmoothMin(T, L.Size() / (Lobe.Radius * FMath::Max(Coast, 0.45f)), 0.18f);
	}
	return T;
}

float FIslandShapeModel::BaseLand(const FIslandDesc& Island, const FExploredNoise& N, const FVector2D& Q, float T, const FKarstLayout* Karst)
{
	const float U = 1.0f - T;
	const float Hmax = Island.MaxHeight;
	switch (Island.Archetype)
	{
	case EIslandArchetype::Landing: return LandingLand(N, Q, U, Hmax);
	case EIslandArchetype::Emerald:
	{
		const float Rise = FMath::Pow(SmoothStep(0.04f, 1.0f, U), 1.15f);
		const float Ridges = N.Ridged2D(Q.X * 2.4f + 3.0f, Q.Y * 2.4f, 5);
		return 1.5f * SmoothStep(-0.02f, 0.06f, U) + Hmax * Rise * (0.45f + 0.55f * Ridges);
	}
	case EIslandArchetype::Smoke: return SmokeLand(Island, N, Q, T, Hmax);
	case EIslandArchetype::Mesa:
		return Karst ? FKarstTowerModel::LowlandHeight(*Karst, N, Q.X, Q.Y, U, Hmax) : 1.8f * SmoothStep(-0.02f, 0.06f, U);
	case EIslandArchetype::Mangrove: return MangroveLand(Island, N, Q, U);
	case EIslandArchetype::WhiteSands: return WhiteSandsLand(N, Q, T, Hmax);
	case EIslandArchetype::Teeth: return TeethLand(Island, N, Q);
	default: return -1000.0f;
	}
}

float FIslandShapeModel::AroundCoast(const FExploredNoise& N, float Angle, float Frequency, float Offset, int32 Octaves)
{
	// Sobre un círculo del plano de ruido: periódico en el ángulo, sin costura en ±π.
	return N.Fbm2D(FMath::Cos(Angle) * Frequency + Offset, FMath::Sin(Angle) * Frequency - Offset, Octaves);
}
