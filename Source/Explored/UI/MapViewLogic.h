#pragma once

#include "CoreMinimal.h"
#include "Cartography/CartographyModel.h"
#include "Ruins/RuinsModel.h"

/**
 * Lógica pura del mapa en las manos (GDD §5.6), sin Slate ni UObject: zona
 * visible (desplazar y acercar con límites), transformación hoja ↔ pantalla,
 * selección de marcas con el puntero, reglas del texto corto de las marcas,
 * teclas que guardan el mapa y los trazos generados por código de la hoja
 * (manchas de agua, anotaciones de wayfinding y bocetos de recetas).
 *
 * SExploredMapSheet y SExploredMapInHands solo llaman a estas funciones, así
 * que MapViewLogicSpec las prueba en el editor y en Tools/HostTests.
 *
 * Coordenadas de mapa: [0, 1]² como FCartographyModel::WorldToMap (norte
 * arriba). Coordenadas locales: píxeles de la geometría del widget.
 */
namespace ExploredMapView
{
	// --- Zona visible ------------------------------------------------------------

	/** Lado mínimo visible (≈ 480 m de los 6 km de la hoja) y máximo (la hoja entera). */
	inline constexpr double MinViewSize = 0.08;
	inline constexpr double MaxViewSize = 1.0;
	/** Cuánto acerca o aleja un paso de rueda, gatillo o tecla. */
	inline constexpr double ZoomStep = 1.25;
	/** Fracción de la zona visible que recorre un paso de flecha o cruceta. */
	inline constexpr double PanStepFraction = 0.1;

	/** Zona visible de la hoja: centro y lado, en coordenadas de mapa. */
	struct FMapView
	{
		FVector2D Center = FVector2D(0.5, 0.5);
		double Size = 1.0;
	};

	/**
	 * Lado dentro de [MinViewSize, MaxViewSize] y centro de modo que la zona
	 * visible no se salga de la hoja. Valores no finitos vuelven a la hoja entera.
	 */
	EXPLORED_API FMapView ClampView(const FMapView& View);

	/**
	 * Acerca (Factor > 1) o aleja (Factor < 1) manteniendo AnchorMap bajo el
	 * mismo punto de la pantalla (el cursor al usar la rueda). Recortado.
	 */
	EXPLORED_API FMapView ZoomAt(const FMapView& View, double Factor, const FVector2D& AnchorMap);

	/** Arrastre: la hoja sigue al puntero DeltaPixels sobre un cuadrado de SidePixels. Recortado. */
	EXPLORED_API FMapView PanByPixels(const FMapView& View, const FVector2D& DeltaPixels, double SidePixels);

	/** Flechas y cruceta: mueve la vista Fraction de su lado en Direction (−1…1 por eje). Recortado. */
	EXPLORED_API FMapView PanByFraction(const FMapView& View, const FVector2D& Direction, double Fraction = PanStepFraction);

	/**
	 * Transformación de la zona visible a la geometría local: la hoja se
	 * dibuja en el mayor cuadrado centrado que cabe en LocalSize.
	 */
	struct EXPLORED_API FSheetTransform
	{
		/** Esquina superior izquierda de la zona visible, en coordenadas de mapa. */
		FVector2D Origin = FVector2D::ZeroVector;
		/** Píxeles por unidad de mapa. */
		double Scale = 1.0;
		/** Margen del cuadrado dentro de la geometría. */
		FVector2D Offset = FVector2D::ZeroVector;
		/** Lado del cuadrado en píxeles. */
		double Side = 0.0;

		static FSheetTransform Make(const FMapView& View, const FVector2D& LocalSize);

		FVector2D MapToLocal(const FVector2D& Map) const;
		FVector2D LocalToMap(const FVector2D& Local) const;
		/** El punto local cae dentro del cuadrado de la hoja. */
		bool IsInsideSheet(const FVector2D& Local) const;
	};

	/** Radio en píxeles alrededor de una marca que cuenta como pulsarla. */
	inline constexpr double MarkHitRadiusPixels = 12.0;

	/** Marca más cercana a LocalPoint dentro de RadiusPixels, o INDEX_NONE. */
	EXPLORED_API int32 HitTestMarks(const TArray<FMapMark>& Marks, const FSheetTransform& Transform, const FVector2D& LocalPoint,
		double RadiusPixels = MarkHitRadiusPixels);

	// --- Teclado y mando sobre la hoja ---------------------------------------------

	/**
	 * Dirección de desplazamiento de una tecla (flechas y cruceta), en ejes de
	 * pantalla (+Y hacia abajo = sur); cero si la tecla no desplaza.
	 */
	EXPLORED_API FVector2D PanDirectionForKey(FName KeyName);

	/** Factor de zoom de una tecla: ZoomStep (RePág, +, RB, RT), 1/ZoomStep (AvPág, −, LB, LT) o 1. */
	EXPLORED_API double ZoomFactorForKey(FName KeyName);

	/** Aceptar sobre la hoja (Intro, espacio, A del mando): pulsa en el centro de la vista. */
	EXPLORED_API bool IsSheetAcceptKey(FName KeyName);

	/** Zona muerta del stick y fracción de la vista que recorre por evento a fondo. */
	inline constexpr float StickDeadZone = 0.25f;
	inline constexpr double StickPanFraction = 0.03;

	/** Desplazamiento de un eje del stick (valor −1…1) con zona muerta; 0 dentro de ella. */
	EXPLORED_API double StickPanAmount(float AxisValue);

	/** Y del mando: pasa el foco de la hoja al cuaderno del margen y viceversa (las flechas desplazan la hoja). */
	EXPLORED_API bool IsMapFocusSwitchKey(FName KeyName);

	// --- Sellos (story_es.json → map_marks) ----------------------------------------

	/** Sello con su etiqueta en los dos idiomas. */
	struct FMapStampLabel
	{
		FName Id;
		FString LabelEs;
		FString LabelEn;
	};

	/**
	 * Lee `map_marks` de story_es.json (id, label, labelEn) con el lector JSON
	 * puro del guardado. Solo conserva los sellos que conoce el modelo
	 * (FCartographyModel::KnownStamps), en el orden del fichero. Devuelve false
	 * con OutError si el texto no es JSON o no trae `map_marks`.
	 */
	EXPLORED_API bool ParseMapStampLabels(const FString& JsonText, TArray<FMapStampLabel>& OutStamps, FString& OutError);

	/** Sellos de reserva si no se puede leer el fichero: los ids del modelo como etiqueta. */
	EXPLORED_API TArray<FMapStampLabel> FallbackStampLabels();

	// --- Texto de las marcas -----------------------------------------------------

	/**
	 * Mientras se escribe: los saltos de línea y tabuladores pasan a espacios y
	 * se corta en FCartographyModel::MaxMarkTextLength. No recorta espacios
	 * (se podría estar escribiendo la segunda palabra); eso lo hace el modelo
	 * al guardar (CapMarkText).
	 */
	EXPLORED_API FString LimitMarkTextWhileTyping(const FString& Text);

	/** Caracteres que aún caben en el texto de la marca (nunca negativo). */
	EXPLORED_API int32 RemainingMarkChars(const FString& Text);

	/** Se puede poner la marca: un sello conocido, o una nota sin sello con texto. */
	EXPLORED_API bool CanCommitMark(FName StampId, const FString& Text);

	/**
	 * Si una tecla guarda el mapa. Volver (Escape, B) y la pausa (Start)
	 * siempre; M y View solo si no se está escribiendo el texto de una marca
	 * (con el cuadro de texto enfocado, la «m» es una letra).
	 */
	EXPLORED_API bool ShouldCloseMapOnKey(FName KeyName, bool bTypingText);

	// --- Trazos generados de la hoja --------------------------------------------

	/** Mancha de agua sobre el papel, en coordenadas de hoja [0, 1]². */
	struct FWetStain
	{
		FVector2D Center = FVector2D::ZeroVector;
		double Radius = 0.0;
		/** Opacidad de la mancha (0–1). */
		float Alpha = 0.0f;
		/** Mancha ya seca de una pasada de tinta corrida (cerco tenue). */
		bool bDried = false;
	};

	/** Manchas por humedad (hasta 8) y cercos secos por pasadas de tinta corrida (hasta 6). */
	inline constexpr int32 MaxWetStains = 8;
	inline constexpr int32 MaxDriedStains = 6;

	/**
	 * Manchas deterministas para una semilla: más cuanto más mojado está el
	 * papel y un cerco seco por cada pasada de tinta corrida. Seco y sin
	 * pasadas, ninguna.
	 */
	EXPLORED_API TArray<FWetStain> WetStains(uint32 Seed, float Wetness, int32 InkRuns);

	/** Polilínea en coordenadas de mapa. */
	using FPolyline = TArray<FVector2D>;

	/** Radio mínimo (unidades de mapa) de las anotaciones en anillo, para que se vean. */
	inline constexpr double MinAnnotationRadius = 0.006;

	/**
	 * Dibujo de una anotación de wayfinding (GDD §6.2) en coordenadas de mapa:
	 * - camino de estrellas, oleaje y aves: trazos discontinuos de From a To y
	 *   punta de flecha en To;
	 * - nubes fijas y color del agua: anillo discontinuo alrededor de To con
	 *   radio DistanceMeters (mínimo MinAnnotationRadius).
	 */
	EXPLORED_API TArray<FPolyline> AnnotationStrokes(const FWayfindingAnnotation& Annotation);

	/**
	 * Boceto de receta al margen (GDD §8.5): 2–4 trazos deterministas a partir
	 * del id, en [0, 1]², para dibujarlo en un recuadro pequeño.
	 */
	EXPLORED_API TArray<FPolyline> RecipeDoodle(FName RecipeId);

	/** Hash estable de un texto (FNV-1a sobre sus caracteres), igual en el host y en Unreal para ASCII. */
	EXPLORED_API uint32 StableHash(const FString& Text);
}
