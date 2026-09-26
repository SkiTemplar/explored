#pragma once

#include "CoreMinimal.h"
#include "Core/ExploredNoise.h"

/** Cómo se mueve el cartógrafo mientras dibuja (GDD §5.2). */
enum class ECartographyLocomotion : uint8
{
	Walking,
	Running,
	Swimming
};

/** Origen de una marca del mapa (GDD §5.4, §5.5). */
enum class EMapMarkSource : uint8
{
	/** Puesta a mano donde está el jugador: hereda la deriva del dibujo. */
	Hand,
	/** Vista con el catalejo: error de rumbo y de distancia proporcional a lo lejos que está. */
	Spyglass,
	/** Situada con el sextante: punto exacto, también en mar abierto. */
	Sextant
};

/** Muestra del jugador que el llamador pasa cada ~0,25–0,5 s. Distancias en metros. */
struct EXPLORED_API FCartographySample
{
	/** Posición real en el plano del mundo (X norte, Y este), en metros. */
	FVector2D WorldPosition = FVector2D::ZeroVector;
	float DeltaSeconds = 0.25f;
	ECartographyLocomotion Locomotion = ECartographyLocomotion::Walking;
	/** Lleva la brújula del Albatros: rectifica el rumbo general del dibujo. */
	bool bHasCompass = false;
	/** Distancia a la orilla que calcula el llamador (terreno o aproximación). */
	float DistanceToShore = TNumericLimits<float>::Max();
	/** Isla de esa orilla (índice del layout) o INDEX_NONE. */
	int32 IslandIndex = INDEX_NONE;
};

/** Agua que recibe el mapa en un intervalo (GDD §5.1). */
struct EXPLORED_API FCartographyExposure
{
	/** Lluvia 0–1 (FWeatherSample::Rain). */
	float Rain = 0.0f;
	/** El mapa está bajo el agua del mar (nadando o buceando con él encima). */
	bool bInSeaWater = false;
	/** Guardado en seco (funda o estuche): no se moja y se va secando. */
	bool bStoredDry = false;
};

/** Trazo de costa a mano alzada. Puntos en coordenadas de mapa normalizadas. */
struct EXPLORED_API FMapStroke
{
	int32 IslandIndex = INDEX_NONE;
	TArray<FVector2D> Points;
	/** Intensidad de la tinta: 1 recién dibujado; baja cuando la tinta corre. */
	float Ink = 1.0f;
	/** Cuánto se ha corrido la tinta (0 limpio – 1 muy corrido). */
	float Blur = 0.0f;
	/** Tolerancia de simplificación actual, en metros (crece al compactar y al correrse la tinta). */
	float ToleranceMeters = 1.0f;
};

/** Boceto tenue de un mirador (GDD §5.3): anillo cerrado, en coordenadas de mapa. */
struct EXPLORED_API FMapSketch
{
	int32 IslandIndex = INDEX_NONE;
	TArray<FVector2D> Points;
	/** 1 si ese tramo del boceto ya se ha recorrido de verdad (pasa a trazo firme). */
	TArray<uint8> Confirmed;
	float Ink = 1.0f;

	/** Fracción del contorno confirmada (0–1). */
	float ConfirmedFraction() const;
};

/** Marca o sello del jugador (GDD §5.4). */
struct EXPLORED_API FMapMark
{
	/** Id de sello de `story_es.json` → `map_marks`, o NAME_None para una nota solo de texto. */
	FName StampId;
	FVector2D Position = FVector2D::ZeroVector;
	FString Text;
	EMapMarkSource Source = EMapMarkSource::Hand;
	float Ink = 1.0f;
};

/** Receta anotada al margen del mapa (GDD §8.5). */
struct EXPLORED_API FMapRecipeNote
{
	FName RecipeId;
	/** Con boceto dibujado junto a la anotación. */
	bool bDoodle = false;
};

/**
 * Costa de referencia de una isla (anillo cerrado, coordenadas de mapa) y qué
 * puntos se han recorrido. Es el «hecho de haber estado allí»: el agua no lo borra.
 */
struct EXPLORED_API FMapIslandCoverage
{
	int32 IslandIndex = INDEX_NONE;
	TArray<FVector2D> Coast;
	TArray<uint8> Visited;

	float Fraction() const;
};

/** Estado completo del mapa en datos planos, listo para guardar (GDD §5.6). */
struct EXPLORED_API FCartographyState
{
	TArray<FMapStroke> Strokes;
	TArray<FMapSketch> Sketches;
	TArray<FMapMark> Marks;
	TArray<FMapRecipeNote> Recipes;
	TArray<FMapIslandCoverage> Coverage;

	/** Humedad del papel 0–1. */
	float Wetness = 0.0f;
	/** Progreso hacia la siguiente pasada de tinta corrida (0–1). */
	float InkRunProgress = 0.0f;
	/** Pasadas de tinta corrida desde la última copia en limpio. */
	int32 InkRuns = 0;
	/** Se ha dibujado alguna costa (para el logro «Sin mapa»). */
	bool bHasDrawnAnyCoast = false;

	/** Estado del dibujante: deriva acumulada (m), recorrido total (m) y contador de marcas. */
	FVector2D Drift = FVector2D::ZeroVector;
	double TravelDistance = 0.0;
	int32 MarkSerial = 0;
};

/**
 * El mapa dibujado a mano, sistema central del juego (GDD §5): sin minimapa ni
 * marcadores automáticos, solo lo que el jugador ha dibujado.
 *
 * - El llamador pasa muestras del jugador; cerca de la orilla se dibuja la costa
 *   con un temblor determinista (mayor al correr) y una deriva de rumbo que se
 *   acumula sin brújula y se corrige con ella. Nunca es un trazo de GPS.
 * - Los miradores añaden bocetos tenues que se confirman al recorrer esa costa.
 * - Marcas a mano, con catalejo (a distancia) y con sextante (punto exacto).
 * - El papel se moja y la tinta corre: se pierde detalle, no la cobertura.
 *
 * Puro (solo CoreMinimal) y determinista para una semilla y una secuencia de muestras.
 */
class EXPLORED_API FCartographyModel
{
public:
	/** Lado del mundo que cubre la hoja, en metros (≈ 6 × 6 km, centrado en el origen). */
	static constexpr double MapWorldSize = 6000.0;

	/** Distancia a la orilla a la que se empieza a dibujar y margen extra para dejar de hacerlo. */
	static constexpr float RecordDistance = 20.0f;
	static constexpr float RecordHysteresis = 6.0f;
	/** Salto mayor que esto entre dos muestras (teletransporte, carga) corta el trazo. */
	static constexpr float MaxStepMeters = 40.0f;
	/** Separación mínima entre puntos consecutivos de un trazo. */
	static constexpr float MinPointSpacing = 2.0f;
	/** Tolerancia base de simplificación y límites de memoria. */
	static constexpr float BaseToleranceMeters = 0.75f;
	static constexpr int32 MaxStrokePoints = 400;
	static constexpr int32 MaxTotalStrokePoints = 30000;

	/** Temblor (amplitud máxima en metros) según cómo se mueve. */
	static constexpr float TremorWalking = 1.2f;
	static constexpr float TremorSwimming = 2.0f;
	static constexpr float TremorRunning = 3.5f;

	/** Deriva: error de rumbo máximo (grados), efecto de la brújula y tope. */
	static constexpr float HeadingErrorWalking = 4.0f;
	static constexpr float HeadingErrorSwimming = 6.0f;
	static constexpr float HeadingErrorRunning = 7.0f;
	static constexpr float CompassHeadingFactor = 0.1f;
	/** Con brújula la deriva acumulada se reduce a la mitad cada ~17 m (e-folding de 25 m). */
	static constexpr float CompassCorrectionMeters = 25.0f;
	static constexpr float MaxDriftMeters = 60.0f;

	/** Cobertura y confirmación de bocetos: radio alrededor de la posición real. */
	static constexpr float CoverageRadius = 30.0f;
	static constexpr float CoverageSpacing = 10.0f;
	static constexpr float CompleteCoverage = 0.9f;
	static constexpr float SketchToleranceMeters = 12.0f;
	static constexpr float SketchSpacingMeters = 12.0f;
	static constexpr float SketchWobbleMeters = 8.0f;
	static constexpr int32 MaxSketchPoints = 600;

	/** Marcas. */
	static constexpr int32 MaxMarks = 256;
	static constexpr int32 MaxMarkTextLength = 32;
	static constexpr float SpyglassBearingErrorDegrees = 1.5f;
	static constexpr float SpyglassRangeError = 0.06f;

	/** Humedad (por segundo) y tinta corrida. */
	static constexpr float RainWetRate = 1.0f / 60.0f;
	static constexpr float SeaWetRate = 0.4f;
	static constexpr float DryRate = 1.0f / 300.0f;
	static constexpr float InkRunThreshold = 0.5f;
	static constexpr float InkRunSeconds = 30.0f;
	static constexpr float InkFadePerRun = 0.8f;
	static constexpr float InkFloor = 0.15f;
	static constexpr float LegibleInk = 0.35f;

	explicit FCartographyModel(uint32 InSeed = 0);

	/** Proyección mundo (m) → mapa normalizado [0, 1]²: norte (+X) arriba, este (+Y) a la derecha. */
	static FVector2D WorldToMap(const FVector2D& World);
	static FVector2D MapToWorld(const FVector2D& Map);
	static double MetersToMap(double Meters) { return Meters / MapWorldSize; }

	/** Procesa una muestra del jugador: deriva, cobertura, bocetos y trazo de costa. */
	void Sample(const FCartographySample& InSample);

	/** Cierra el trazo en curso (se simplifica y se descarta si tiene un solo punto). */
	void EndStroke();

	bool IsRecording() const { return ActiveStroke != INDEX_NONE; }

	/** Desplazamiento del temblor de la mano para un recorrido dado; nunca supera TremorAmplitude. */
	FVector2D TremorOffset(double TravelMeters, ECartographyLocomotion Locomotion) const;
	static float TremorAmplitude(ECartographyLocomotion Locomotion);

	/**
	 * Boceto de mirador: contorno aproximado (anillo, metros) que se guarda tenue y
	 * deformado. Si la isla ya tiene boceto, se conserva el primero y se devuelve su índice.
	 */
	int32 AddSketch(int32 IslandIndex, const TArray<FVector2D>& WorldOutline);

	/** Marca a mano donde está el jugador. INDEX_NONE si el sello no existe o no caben más. */
	int32 AddMark(FName StampId, const FVector2D& WorldPosition, const FString& Text);
	/** Marca con catalejo desde Observer hacia Target, sin haber llegado. */
	int32 AddSpyglassMark(FName StampId, const FVector2D& ObserverWorld, const FVector2D& TargetWorld, const FString& Text);
	/** Marca con sextante: punto exacto (sin deriva ni error), también en mar abierto. */
	int32 AddSextantMark(FName StampId, const FVector2D& WorldPosition, const FString& Text);

	/** Anota una receta; true si es nueva o si gana su boceto. */
	bool NoteRecipe(FName RecipeId, bool bDoodle);

	/** Moja o seca el papel; si la humedad pasa del umbral, la tinta corre por pasadas. */
	void TickWetness(const FCartographyExposure& Exposure, float DeltaSeconds);
	/** Una pasada de tinta corrida: suaviza y diezma los trazos y apaga la tinta. */
	void ApplyInkRun();
	/** Copia en limpio en la mesa de cartografía: lo legible vuelve a tinta plena y el papel está seco. */
	int32 CopyToCleanSheet();

	/** Costa de referencia de una isla (anillo en metros) para medir la cobertura. */
	void RegisterIslandCoast(int32 IslandIndex, const TArray<FVector2D>& WorldCoast);
	/** Fracción de la costa recorrida (0–1); 0 si la isla no tiene costa de referencia. */
	float GetIslandCoverage(int32 IslandIndex) const;
	/** Isla cartografiada (logro «Cartógrafo»). */
	bool IsIslandCharted(int32 IslandIndex) const { return GetIslandCoverage(IslandIndex) >= CompleteCoverage; }
	bool HasDrawnAnyCoast() const { return State.bHasDrawnAnyCoast; }

	/** Distancia (m) a la costa de referencia más cercana y su isla; aproximación barata para el componente. */
	float DistanceToReferenceCoast(const FVector2D& WorldPosition, int32& OutIslandIndex) const;

	static const TArray<FName>& KnownStamps();
	static bool IsKnownStamp(FName StampId);
	/** Recorta los espacios y limita la longitud del texto de una marca. */
	static FString CapMarkText(const FString& Text);

	const FCartographyState& GetState() const { return State; }
	/** Restaura un estado guardado; el trazo en curso queda cerrado. */
	void LoadState(const FCartographyState& InState);

	int32 NumStrokePoints() const;
	FVector2D GetDrift() const { return State.Drift; }
	float GetWetness() const { return State.Wetness; }
	uint32 GetSeed() const { return Seed; }

private:
	void UpdateDrift(const FVector2D& Step, double StepLength, const FCartographySample& InSample);
	void MarkVisited(const FVector2D& WorldPosition, int32 IslandIndex);
	void ConfirmSketches(const FVector2D& WorldPosition);
	void CompactStroke(FMapStroke& Stroke, int32 MaxPoints);
	void EnforceTotalBudget();
	int32 AddMarkAt(FName StampId, const FVector2D& MapPosition, const FString& Text, EMapMarkSource Source);

	uint32 Seed = 0;
	FExploredNoise TremorNoise;
	FExploredNoise HeadingNoise;
	FExploredNoise SketchNoise;
	/** Signo del sesgo de rumbo propio de este cartógrafo (+1 o −1). */
	float HeadingBiasSign = 1.0f;

	FCartographyState State;
	/** Trazo en curso (no se guarda: cargar cierra el trazo). */
	int32 ActiveStroke = INDEX_NONE;
	FVector2D LastPosition = FVector2D::ZeroVector;
	bool bHasLastPosition = false;
};
