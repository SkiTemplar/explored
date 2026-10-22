#include "Sky/SkyPaletteModel.h"

namespace SkyPaletteDetail
{
	struct FNamedColor
	{
		const TCHAR* Name;
		uint8 R, G, B;
	};

	// Copia de Tools/Style/paleta.json (solo los tonos que usa el cielo); SkyPaletteSpec la
	// compara con los hex del JSON para que no se desvíe.
	const FNamedColor Colors[] = {
		{ TEXT("cristal"), 0x9F, 0xD6, 0xE0 },
		{ TEXT("cielo_dia"), 0x9C, 0xCB, 0xE8 },
		{ TEXT("cielo_ocaso"), 0xF2, 0xB4, 0x7E },
		{ TEXT("cielo_noche"), 0x2A, 0x38, 0x58 },
		{ TEXT("fuego"), 0xF0, 0x8C, 0x2E },
		{ TEXT("papel"), 0xF6, 0xEE, 0xDB },
		{ TEXT("agua_media"), 0x2E, 0x8F, 0xA6 },
		{ TEXT("blanco_nube"), 0xFA, 0xF7, 0xF2 },
		{ TEXT("gris_lila"), 0xC7, 0xC3, 0xD6 },
		{ TEXT("cielo_cenit"), 0x52, 0x88, 0xC2 },
		{ TEXT("malva_ocaso"), 0xB4, 0x8A, 0xA2 },
		{ TEXT("gris_tormenta"), 0x5A, 0x61, 0x6C },
		{ TEXT("noche_cenit"), 0x18, 0x20, 0x3A },
	};

	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float T)
	{
		return A + (B - A) * FMath::Clamp(T, 0.0f, 1.0f);
	}

	FLinearColor Blend3(const FSkyPaletteModel::FWeights& W, const FLinearColor& Night, const FLinearColor& Twilight, const FLinearColor& Day)
	{
		return Night * W.Night + Twilight * W.Twilight + Day * W.Day;
	}

	float Blend3(const FSkyPaletteModel::FWeights& W, float Night, float Twilight, float Day)
	{
		return Night * W.Night + Twilight * W.Twilight + Day * W.Day;
	}

	FLinearColor P(const TCHAR* Name)
	{
		return FSkyPaletteModel::PaletteColor(Name);
	}
}

FLinearColor FSkyPaletteModel::PaletteColor(const TCHAR* Name)
{
	for (const SkyPaletteDetail::FNamedColor& C : SkyPaletteDetail::Colors)
	{
		if (FString(C.Name) == FString(Name))
		{
			return FLinearColor(FColor(C.R, C.G, C.B));
		}
	}
	return FLinearColor::Black;
}

FSkyPaletteModel::FWeights FSkyPaletteModel::MomentWeights(float SunZ)
{
	const float Z = FMath::IsFinite(SunZ) ? SunZ : 0.0f;
	FWeights W;
	W.Day = FMath::SmoothStep(0.06f, 0.32f, Z);
	W.Night = 1.0f - FMath::SmoothStep(-0.22f, -0.03f, Z);
	W.Twilight = FMath::Max(0.0f, 1.0f - W.Day - W.Night);
	return W;
}

float FSkyPaletteModel::StormFromCloudCover(float CloudCover)
{
	return FMath::IsFinite(CloudCover) ? FMath::SmoothStep(0.4f, 0.85f, CloudCover) : 0.0f;
}

bool FSkyPaletteModel::ShouldRotateLight(const FVector& Applied, const FVector& Wanted)
{
	const FVector A = Applied.GetSafeNormal();
	const FVector B = Wanted.GetSafeNormal();
	if (A.IsZero() || B.IsZero())
	{
		return !B.IsZero();
	}
	const double CosStep = FMath::Cos(FMath::DegreesToRadians(static_cast<double>(LightRotationStepDeg)));
	return FVector::DotProduct(A, B) < CosStep;
}

bool FSkyPaletteModel::MoonCastsShadows(float SunUp)
{
	return !(SunUp > 0.001f);
}

float FSkyPaletteModel::SunUp(float SunZ)
{
	return FMath::SmoothStep(-0.08f, 0.04f, SunZ);
}

float FSkyPaletteModel::MoonLightFade(float SunZ)
{
	// Sale de 0 justo donde SunUp llega a 0 (SunZ = -0,08): nunca alumbra con el sol encendido.
	return 1.0f - FMath::SmoothStep(-0.12f, -0.08f, SunZ);
}

FSkyLook FSkyPaletteModel::Evaluate(float SunZ, float Storm)
{
	using namespace SkyPaletteDetail;
	const FWeights W = MomentWeights(SunZ);
	const float S = FMath::IsFinite(Storm) ? FMath::Clamp(Storm, 0.0f, 1.0f) : 0.0f;

	FSkyLook L;
	L.Horizon = Blend3(W, P(TEXT("cielo_noche")), P(TEXT("cielo_ocaso")), P(TEXT("cielo_dia")));
	L.Middle = Blend3(W, P(TEXT("cielo_noche")), P(TEXT("malva_ocaso")), Mix(P(TEXT("cielo_dia")), P(TEXT("cielo_cenit")), 0.5f));
	L.Zenith = Blend3(W, P(TEXT("noche_cenit")), P(TEXT("cielo_noche")), P(TEXT("cielo_cenit")));
	L.Glow = P(TEXT("fuego"));
	L.GlowStrength = W.Twilight * (1.0f - S);
	L.SkyBrightness = Blend3(W, 0.55f, 1.0f, 1.0f);

	L.SunColor = Blend3(W, P(TEXT("cielo_ocaso")), P(TEXT("cielo_ocaso")), Mix(P(TEXT("papel")), P(TEXT("cielo_ocaso")), 0.25f));
	L.SunIntensity = (1.0f - W.Night) * Blend3(W, 0.0f, 0.55f, 1.0f) * (1.0f - 0.75f * S);
	// Sombras azuladas de día, malva al atardecer y azul noche de noche: la luz de cielo nunca es gris neutro.
	L.AmbientTint = Blend3(W, P(TEXT("cielo_noche")), P(TEXT("malva_ocaso")), P(TEXT("cielo_dia")));
	// Las nubes son de material iluminado: la luz ya las tiñe; el tinte solo las oscurece con el ciclón.
	L.CloudTint = FLinearColor::White;
	L.FogColor = L.Horizon;

	if (S > 0.0f)
	{
		const FLinearColor StormSky = P(TEXT("gris_tormenta"));
		L.Horizon = Mix(L.Horizon, P(TEXT("gris_lila")) * Blend3(W, 0.25f, 0.6f, 0.75f), S);
		L.Middle = Mix(L.Middle, StormSky * Blend3(W, 0.35f, 0.8f, 1.0f), S);
		L.Zenith = Mix(L.Zenith, StormSky * Blend3(W, 0.25f, 0.6f, 0.8f), S);
		L.AmbientTint = Mix(L.AmbientTint, P(TEXT("gris_lila")), S);
		L.CloudTint = Mix(L.CloudTint, StormSky * 1.4f, S);
		L.FogColor = Mix(L.FogColor, L.Horizon, S);
	}
	return L;
}
