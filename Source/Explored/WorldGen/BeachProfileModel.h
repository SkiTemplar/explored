#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

/**
 * Perfil de las playas del terreno: cara de playa casi recta de la berma al agua, que sigue
 * bajo el agua con la misma pendiente hasta el fondo cercano a la orilla.
 *
 * El relieve de cada isla (FTerrainDensity) se diseñó en una métrica radial normalizada, T,
 * que no es una distancia: a pocos metros de la costa su gradiente cambia hasta un 50 % por
 * el ruido de la silueta. Por eso la playa no se construye con T, sino con la distancia
 * firmada, en metros, a la línea de costa real del relieve base (FBeachShoreField), y
 * FBeachProfileModel::ComposeHeight mezcla la rampa con ese relieve.
 *
 * Replicación (biblia 08): nada. El terreno base es una función pura de la semilla del
 * mundo; cada máquina lo genera igual y solo se replican los deltas de edición (§2.2).
 */

/** Parámetros del perfil de un tramo de costa. Metros y tangentes. */
struct EXPLORED_API FBeachProfileParams
{
	/** Tangente de la cara de playa (2°..6° -> 0,035..0,105) entre la berma y el agua. */
	float Slope = 0.07f;
	/**
	 * Bajo el agua la pendiente crece hasta Slope * UnderwaterSlopeScale. El cambio empieza a
	 * UnderwaterSteepenDepth m de fondo y dura UnderwaterSteepenLength m, así que la orilla
	 * sigue recta (sin curva hacia abajo) y el aumento no se nota como arista.
	 */
	float UnderwaterSlopeScale = 1.15f;
	float UnderwaterSteepenDepth = 0.6f;
	float UnderwaterSteepenLength = 20.0f;
	/** Altura (m) de la berma: la cresta de arena donde acaba la cara, sobre la pleamar viva. */
	float BermHeight = 1.8f;
	/** Suavidad (m) de las uniones con la berma y con el fondo; 0 deja aristas. */
	float JoinSoftness = 0.5f;
	/** Junto a la orilla el fondo no queda más somero que esto: la cara sigue bajando hasta aquí. */
	float NearshoreDepth = 5.6f;
	/** Tierra adentro, la playa cede el sitio al relieve de la isla entre estas dos distancias. */
	float BlendStart = 35.0f;
	float BlendEnd = 70.0f;
	/** Mar adentro, entre estas dos distancias el fondo vuelve a ser el del relieve base. */
	float OffshoreFadeStart = 120.0f;
	float OffshoreFadeEnd = 160.0f;
};

/** Resultado del análisis de un transecto perpendicular a la costa. */
struct EXPLORED_API FBeachTransectReport
{
	/** Hay línea de agua (paso de > 0 a <= 0) en el transecto. */
	bool bHasWaterline = false;
	/** Índice de la primera muestra en el agua. */
	int32 WaterlineIndex = INDEX_NONE;
	/** Pendiente máxima (tangente) entre muestras con altura en [FaceBottom, FaceTop]. */
	float MaxFaceSlope = 0.0f;
	/** Pendiente mínima (tangente) en la franja de la orilla [ShoreBottom, ShoreTop]. */
	float MinShoreSlope = 0.0f;
	/** Mayor subida (m) entre dos muestras seguidas yendo mar adentro dentro de la franja. */
	float MaxSeawardRise = 0.0f;
	/** Segunda derivada mínima (1/m) en la franja de la orilla; < 0 = curva hacia abajo. */
	float MinShoreCurvature = 0.0f;
	/** Muestras que caen en la franja [FaceBottom, FaceTop] alrededor de la línea de agua. */
	int32 FaceSamples = 0;
};

/** Franjas de altura del análisis de un transecto. */
struct EXPLORED_API FBeachTransectBands
{
	/** Franja de la cara: pendiente máxima y monotonía (por defecto ±5 m del nivel del mar). */
	float FaceBottom = -5.0f;
	float FaceTop = 5.0f;
	/** Franja de la orilla: pendiente mínima y curvatura. */
	float ShoreBottom = -0.5f;
	float ShoreTop = 0.5f;
	/** Base (m) de la segunda derivada: filtra el escalonado de la rejilla sin ocultar un hombro. */
	float CurvatureBaseline = 2.0f;
};

/**
 * Distancia firmada (m) a la línea de nivel 0 de un campo de alturas: > 0 en tierra, < 0 en
 * el agua. Se muestrea una rejilla regular (primero gruesa y luego fina solo cerca de la
 * costa), se traza la costa en segmentos (marching squares, cruces interpolados) y se
 * propaga el segmento más cercano con barridos de ida y vuelta (8SSEDT). Determinista: el
 * orden de evaluación de las alturas no cambia el resultado.
 */
class EXPLORED_API FBeachShoreField
{
public:
	/**
	 * Construye el campo sobre [Min, Max] con celdas de CellSize metros. CoarseFactor celdas
	 * finas forman una celda gruesa; solo se evalúan con detalle las celdas gruesas que
	 * cruzan la costa o que tienen alguna esquina a menos de ActiveBand metros del nivel 0.
	 */
	void Build(const FVector2D& Min, const FVector2D& Max, float CellSize, int32 CoarseFactor, float ActiveBand,
		TFunctionRef<float(double X, double Y)> Height);

	bool IsValid() const { return Width > 1 && Height > 1; }
	bool Contains(double X, double Y) const;
	/** Distancia firmada interpolada; fuera de la rejilla devuelve false y no toca OutDistance. */
	bool SignedDistance(double X, double Y, float& OutDistance) const;
	/** Segmentos de costa encontrados en la rejilla fina. */
	int32 NumShoreSegments() const { return ShoreSegments; }

private:
	FVector2D Origin = FVector2D::ZeroVector;
	float Cell = 1.0f;
	int32 Width = 0;
	int32 Height = 0;
	int32 ShoreSegments = 0;
	TArray<float> Distances;
};

class EXPLORED_API FBeachProfileModel
{
public:
	/** Tangente de una pendiente en grados, recortada a [0°, 60°]. */
	static float SlopeFromDegrees(float Degrees);
	static float DegreesFromSlope(float Slope);

	/**
	 * Altura final de una columna de costa. ShoreDistance es la distancia firmada a la orilla
	 * (> 0 tierra adentro) y BaseHeight la altura del relieve base en ese punto.
	 * - En tierra: rampa Slope * d que se aplana en la berma y, entre BlendStart y BlendEnd,
	 *   deja paso al relieve base (duna, palmeral, ladera o farallón).
	 * - En el agua: la misma rampa, algo más empinada a partir de UnderwaterSteepenDepth,
	 *   hasta NearshoreDepth, donde se funde con el fondo.
	 * El relieve base no interviene en la cara de la playa: su hombro antiguo junto al agua
	 * (la caída convexa) desaparece. Vale exactamente 0 en d = 0.
	 */
	static float ComposeHeight(const FBeachProfileParams& Params, float ShoreDistance, float BaseHeight);

	/** 1 donde manda la playa, 0 donde manda el relieve base (tierra o mar adentro). */
	static float BeachAmount(const FBeachProfileParams& Params, float ShoreDistance);

	/**
	 * Analiza alturas equiespaciadas (Step m) ordenadas de tierra a mar. Mide solo el tramo
	 * contiguo alrededor de la primera línea de agua cuya altura cae en la franja de la cara.
	 */
	static FBeachTransectReport AnalyzeTransect(const TArray<float>& Heights, float Step, const FBeachTransectBands& Bands = FBeachTransectBands());
};
