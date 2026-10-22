#include "Misc/AutomationTest.h"

#include "Sky/CloudFieldModel.h"
#include "Sky/SkyPaletteModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace SkyPaletteTest
{
	bool Same(const FLinearColor& A, const FLinearColor& B)
	{
		return A.Equals(B, 1.0e-4f);
	}

	float Sum(const FSkyPaletteModel::FWeights& W)
	{
		return W.Night + W.Twilight + W.Day;
	}
}

BEGIN_DEFINE_SPEC(FSkyPaletteSpec, "Explored.SkyPalette",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FSkyPaletteSpec)

void FSkyPaletteSpec::Define()
{
	using namespace SkyPaletteTest;

	Describe(TEXT("momentos del día"), [this]()
	{
		It(TEXT("los pesos suman 1 y cada momento domina en su franja"), [this]()
		{
			for (const float Z : { -1.0f, -0.3f, -0.1f, 0.0f, 0.05f, 0.15f, 0.3f, 1.0f })
			{
				TestEqual(FString::Printf(TEXT("suma en %.2f"), Z), Sum(FSkyPaletteModel::MomentWeights(Z)), 1.0f, 1.0e-4f);
			}
			TestEqual(TEXT("noche cerrada"), FSkyPaletteModel::MomentWeights(-0.5f).Night, 1.0f);
			TestEqual(TEXT("mediodía"), FSkyPaletteModel::MomentWeights(0.8f).Day, 1.0f);
			TestTrue(TEXT("ocaso"), FSkyPaletteModel::MomentWeights(0.0f).Twilight > 0.9f);
		});

		It(TEXT("una altura no finita cuenta como horizonte"), [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			TestEqual(TEXT("suma"), Sum(FSkyPaletteModel::MomentWeights(NaN)), 1.0f, 1.0e-4f);
			const FSkyLook Look = FSkyPaletteModel::Evaluate(NaN, NaN);
			TestTrue(TEXT("horizonte finito"), FMath::IsFinite(Look.Horizon.R));
		});
	});

	Describe(TEXT("colores de paleta"), [this]()
	{
		It(TEXT("de día el degradado va de cielo_dia a cielo_cenit"), [this]()
		{
			const FSkyLook Day = FSkyPaletteModel::Evaluate(0.8f, 0.0f);
			TestTrue(TEXT("horizonte"), Same(Day.Horizon, FSkyPaletteModel::PaletteColor(TEXT("cielo_dia"))));
			TestTrue(TEXT("cénit"), Same(Day.Zenith, FSkyPaletteModel::PaletteColor(TEXT("cielo_cenit"))));
			TestEqual(TEXT("sin halo"), Day.GlowStrength, 0.0f);
			TestEqual(TEXT("sol pleno"), Day.SunIntensity, 1.0f);
		});

		It(TEXT("al atardecer el horizonte es cálido, con halo, y el sol se apaga a medias"), [this]()
		{
			const FSkyLook Dusk = FSkyPaletteModel::Evaluate(0.0f, 0.0f);
			const FSkyPaletteModel::FWeights W = FSkyPaletteModel::MomentWeights(0.0f);
			TestTrue(TEXT("horizonte cálido"), Dusk.Horizon.R > Dusk.Horizon.B);
			TestEqual(TEXT("halo"), Dusk.GlowStrength, W.Twilight, 1.0e-4f);
			TestTrue(TEXT("sol bajo"), Dusk.SunIntensity < 0.6f && Dusk.SunIntensity > 0.0f);
		});

		It(TEXT("de noche cielo_noche y noche_cenit, sin sol, y la sombra no es negra"), [this]()
		{
			const FSkyLook Night = FSkyPaletteModel::Evaluate(-0.6f, 0.0f);
			TestTrue(TEXT("horizonte"), Same(Night.Horizon, FSkyPaletteModel::PaletteColor(TEXT("cielo_noche"))));
			TestTrue(TEXT("cénit"), Same(Night.Zenith, FSkyPaletteModel::PaletteColor(TEXT("noche_cenit"))));
			TestEqual(TEXT("sin sol"), Night.SunIntensity, 0.0f);
			TestTrue(TEXT("ambiente tintado"), Night.AmbientTint.B > 0.02f && Night.AmbientTint.B > Night.AmbientTint.R);
		});

		It(TEXT("el ciclón vira a gris tormenta, apaga el sol y quita el halo"), [this]()
		{
			const FSkyLook Clear = FSkyPaletteModel::Evaluate(0.5f, 0.0f);
			const FSkyLook Storm = FSkyPaletteModel::Evaluate(0.5f, 1.0f);
			TestTrue(TEXT("cénit gris"), Same(Storm.Zenith, FSkyPaletteModel::PaletteColor(TEXT("gris_tormenta")) * 0.8f));
			TestTrue(TEXT("más oscuro"), Storm.Zenith.GetLuminance() < Clear.Zenith.GetLuminance());
			TestEqual(TEXT("sol"), Storm.SunIntensity, 0.25f, 1.0e-4f);
			TestEqual(TEXT("halo"), FSkyPaletteModel::Evaluate(0.0f, 1.0f).GlowStrength, 0.0f);
		});

		It(TEXT("un nombre que no está en la paleta da negro"), [this]()
		{
			TestTrue(TEXT("fucsia"), Same(FSkyPaletteModel::PaletteColor(TEXT("fucsia")), FLinearColor::Black));
		});

		It(TEXT("la tormenta visual sale de la nubosidad"), [this]()
		{
			TestEqual(TEXT("despejado"), FSkyPaletteModel::StormFromCloudCover(0.3f), 0.0f);
			TestEqual(TEXT("muro"), FSkyPaletteModel::StormFromCloudCover(1.0f), 1.0f);
			TestEqual(TEXT("NaN"), FSkyPaletteModel::StormFromCloudCover(std::numeric_limits<float>::quiet_NaN()), 0.0f);
		});
	});

	Describe(TEXT("luces direccionales y sombras virtuales"), [this]()
	{
		It(TEXT("el sol solo gira cuando se ha movido más del umbral (la caché de VSM no se invalida cada fotograma)"), [this]()
		{
			const FVector Applied = FRotator(-30.0, 120.0, 0.0).Vector();
			// Un fotograma de un día de 26 min mueve el sol unos 0,004°.
			const FVector OneFrame = FRotator(-30.004, 120.0, 0.0).Vector();
			const FVector Later = FRotator(-30.0 - 2.0 * FSkyPaletteModel::LightRotationStepDeg, 120.0, 0.0).Vector();
			TestFalse(TEXT("un fotograma no gira la luz"), FSkyPaletteModel::ShouldRotateLight(Applied, OneFrame));
			TestTrue(TEXT("pasado el umbral, sí"), FSkyPaletteModel::ShouldRotateLight(Applied, Later));
			TestTrue(TEXT("sin dirección aplicada, siempre"), FSkyPaletteModel::ShouldRotateLight(FVector::ZeroVector, OneFrame));
			TestTrue(TEXT("un salto de hora (dormir, cargar), siempre"), FSkyPaletteModel::ShouldRotateLight(Applied, -Applied));
		});

		It(TEXT("la luna no proyecta sombra mientras el sol está fuera"), [this]()
		{
			TestFalse(TEXT("de día"), FSkyPaletteModel::MoonCastsShadows(1.0f));
			TestFalse(TEXT("crepúsculo"), FSkyPaletteModel::MoonCastsShadows(0.2f));
			TestTrue(TEXT("de noche"), FSkyPaletteModel::MoonCastsShadows(0.0f));
		});

		It(TEXT("la luna solo alumbra cuando puede proyectar sombra (sin Lumen se colaría en los interiores)"), [this]()
		{
			bool bLeaks = false;
			for (int32 Step = 0; Step <= 400; ++Step)
			{
				const float SunZ = -0.3f + 0.4f * static_cast<float>(Step) / 400.0f;
				const float SunUp = FSkyPaletteModel::SunUp(SunZ);
				if (FSkyPaletteModel::MoonLightFade(SunZ) > 0.0f && !FSkyPaletteModel::MoonCastsShadows(SunUp))
				{
					bLeaks = true;
				}
			}
			TestFalse(TEXT("ninguna altura del sol enciende la luna sin sombra"), bLeaks);
			TestEqual(TEXT("crepúsculo (sol aún alumbrando): luna apagada"), FSkyPaletteModel::MoonLightFade(-0.05f), 0.0f);
			TestEqual(TEXT("noche cerrada: luna entera"), FSkyPaletteModel::MoonLightFade(-0.3f), 1.0f);
			const float Mid = FSkyPaletteModel::MoonLightFade(-0.1f);
			TestTrue(TEXT("entra con fundido, no de golpe"), Mid > 0.0f && Mid < 1.0f);
		});
	});

	Describe(TEXT("campo de nubes"), [this]()
	{
		It(TEXT("genera las nubes en el anillo de 300-900 m, deterministas"), [this]()
		{
			const FCloudFieldModel A = FCloudFieldModel::Make(7u, 34);
			const FCloudFieldModel B = FCloudFieldModel::Make(7u, 34);
			TestEqual(TEXT("número"), A.Clouds.Num(), 34);
			int32 Always = 0;
			for (int32 I = 0; I < A.Clouds.Num(); ++I)
			{
				const FCloudInstance& C = A.Clouds[I];
				const double R = C.Position.Size();
				TestTrue(TEXT("anillo"), R >= FCloudFieldModel::InnerRadius - 0.01 && R <= FCloudFieldModel::OuterRadius + 0.01);
				TestTrue(TEXT("altura"), C.Altitude >= FCloudFieldModel::MinAltitude && C.Altitude <= FCloudFieldModel::MaxAltitude);
				TestTrue(TEXT("sobre la linterna con tormenta"), C.Altitude * FCloudFieldModel::StormCeilingFactor >= FCloudFieldModel::LanternClearanceM);
				TestTrue(TEXT("variante"), C.Variant >= 0 && C.Variant < FCloudFieldModel::NumVariants);
				TestEqual(TEXT("determinista"), C.Position.X, B.Clouds[I].Position.X);
				Always += C.CoverThreshold <= 0.0f ? 1 : 0;
			}
			TestEqual(TEXT("un tercio siempre visible"), Always, 12);
			TestEqual(TEXT("tope"), FCloudFieldModel::Make(1u, 5000).Clouds.Num(), 256);
		});

		It(TEXT("derivan con el viento y vuelven a entrar por el otro lado"), [this]()
		{
			FCloudFieldModel Field;
			FCloudInstance C;
			C.Position = FVector2D(890.0, 0.0);
			Field.Clouds.Add(C);
			Field.Advance(FVector2D(1.0, 0.0), 1.0f, 1.0f);
			TestEqual(TEXT("envuelta"), Field.Clouds[0].Position.X, -898.0, 1.0e-3);
			Field.Advance(FVector2D(1.0, 0.0), 1.0f, -1.0f);
			Field.Advance(FVector2D(1.0, 0.0), std::numeric_limits<float>::quiet_NaN(), 1.0f);
			TestEqual(TEXT("sin pasos no válidos"), Field.Clouds[0].Position.X, -898.0, 1.0e-3);
		});

		It(TEXT("aparecen y se disuelven en los bordes y con la nubosidad"), [this]()
		{
			FCloudInstance C;
			C.Position = FVector2D(600.0, 0.0);
			TestEqual(TEXT("dentro"), FCloudFieldModel::Visibility(C, 0.0f), 1.0f);
			C.Position = FVector2D(100.0, 0.0);
			TestEqual(TEXT("sobre la isla"), FCloudFieldModel::Visibility(C, 0.0f), 0.0f);
			C.Position = FVector2D(600.0, 0.0);
			C.CoverThreshold = 0.5f;
			TestEqual(TEXT("despejado"), FCloudFieldModel::Visibility(C, 0.2f), 0.0f);
			TestEqual(TEXT("cubierto"), FCloudFieldModel::Visibility(C, 0.8f), 1.0f);
		});
	});
}

#endif
