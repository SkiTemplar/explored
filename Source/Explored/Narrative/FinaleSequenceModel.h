#pragma once

#include "CoreMinimal.h"

#include "Narrative/BeaconProgressModel.h"

/**
 * Final escenificado de Baliza Siete (GDD v3 §3 «Final», plan F6): tiempos, fases y epílogo.
 * Modelo puro: UBeaconSubsystem programa la bocina y el fundido con estos tiempos,
 * ALighthouseLamp pinta la llama y el cono del haz, y ExploredPlayerController enseña el
 * epílogo y los créditos.
 *
 * Secuencia (segundos reales):
 * 1. Encender: la llama crece durante IgniteSeconds (3 s); al terminar, la baliza queda
 *    encendida y el cono del haz se alarga en BeamGrowSeconds.
 * 2. Si es de noche, el Mistral aparece al encender (t = 0 de la llegada); si es de día, a las 21:00.
 *    Las luces se acercan durante MistralApproachSeconds; la cámara del final entra en
 *    CameraBlendSeconds.
 * 3. Bocina a HornAtApproachFraction de la llegada; fundido a negro HornToFadeSeconds después.
 * 4. Epílogo al terminar el fundido (FadeToBlackSeconds): la línea variable depende de las
 *    páginas leídas y solo cambia con 8 de 8.
 * 5. «Continuar» lleva a los créditos; a EndCreditsSeconds (o con «Volver»), al menú principal.
 */

/** Tiempos del final (s reales). Sanitize los deja siempre utilizables. */
struct EXPLORED_API FFinaleTiming
{
	float IgniteSeconds = 3.0f;
	float BeamGrowSeconds = 1.5f;
	float CameraBlendSeconds = 2.5f;
	float MistralApproachSeconds = 12.0f;
	/** Fracción de la llegada a la que suena la bocina, en [0, 1]. */
	float HornAtApproachFraction = 0.8f;
	float HornToFadeSeconds = 5.0f;
	float FadeToBlackSeconds = 2.0f;
	float FadeFromBlackSeconds = 1.5f;
	float EndCreditsSeconds = 60.0f;
};

/** Fase del final desde que aparecen las luces del Mistral. */
enum class EFinalePhase : uint8
{
	Approach,   // las luces se acercan
	Horn,       // ha sonado la bocina; el barco termina de llegar
	FadeOut,    // fundido a negro
	Epilogue,   // pantalla del epílogo (sale con «Continuar»)
};

/** Paso siguiente tras los créditos del final. */
enum class EFinaleCreditsStep : uint8
{
	Stay,
	MainMenu,
};

struct EXPLORED_API FFinaleSequenceModel
{
	/** Tope de cualquier tiempo del final: valores mayores se recortan. */
	static constexpr float MaxSeconds = 600.0f;
	/** Mínimo de cada tramo, para que ningún temporizador quede a 0. */
	static constexpr float MinStepSeconds = 0.1f;
	/** Mínimo de los créditos antes de volver solos al menú. */
	static constexpr float MinCreditsSeconds = 1.0f;

	/** Copia con valores finitos, positivos y dentro de los topes. */
	static FFinaleTiming Sanitize(const FFinaleTiming& Timing);

	/** Segundos desde que aparece el Mistral hasta la bocina, el fundido y el epílogo. */
	static float HornAtSeconds(const FFinaleTiming& Timing);
	static float FadeAtSeconds(const FFinaleTiming& Timing);
	static float EpilogueAtSeconds(const FFinaleTiming& Timing);

	/** Retraso de la bocina tras la aparición y del fundido tras la bocina (para los temporizadores). */
	static float HornDelaySeconds(const FFinaleTiming& Timing);
	static float FadeDelaySeconds(const FFinaleTiming& Timing);

	/** Fase a los segundos dados desde la aparición del Mistral (negativo o NaN = Approach). */
	static EFinalePhase PhaseAt(const FFinaleTiming& Timing, float SinceMistral);

	/** Fuerza de la llama (0-1) mientras prende: sube suave durante IgniteSeconds. */
	static float FlameStrength(const FFinaleTiming& Timing, float SinceIgnite);
	/** Largo relativo del cono del haz (0-1) desde que la baliza quedó encendida. */
	static float BeamGrow(const FFinaleTiming& Timing, float SinceLit);
	/** Parpadeo de la llama, siempre en [0,8; 1,2]. */
	static float Flicker(float Time);

	/** Epílogo según las páginas leídas: Complete solo con las 8 (GDD §2). */
	static EBeaconEpilogue EpilogueForPages(int32 PagesRead);

	/** Tras los créditos: al menú con «Volver» o al agotar EndCreditsSeconds. */
	static EFinaleCreditsStep CreditsStep(const FFinaleTiming& Timing, float CreditsElapsed, bool bBackPressed);
};
