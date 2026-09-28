#pragma once

#include "CoreMinimal.h"
#include "Core/ExploredNoise.h"

/**
 * Torre caliza (mogote) de un macizo kárstico tipo El Nido / bahía de Ha Long. Coordenadas
 * Q locales de la isla (normalizadas por su radio); alturas como fracción de la altura
 * máxima de la isla.
 */
struct EXPLORED_API FKarstTower
{
	FVector2D Center = FVector2D::ZeroVector;
	float Radius = 0.1f;
	float Height = 0.8f;
	/** Alargamiento de la planta (1 = redonda) y orientación de su eje largo. */
	float Aspect = 1.0f;
	float Angle = 0.0f;
	/** Exponente de la superelipse de la planta: 2 = elipse, 4 = casi un bloque. */
	float Squareness = 3.0f;
	uint32 Seed = 0;
};

/** Laguna interior rodeada de torres, con fondo de arena por debajo del mar. */
struct EXPLORED_API FKarstLagoon
{
	FVector2D Center = FVector2D::ZeroVector;
	float Radius = 0.1f;
	/** Profundidad del fondo, m (negativa). */
	float Floor = -3.0f;
};

struct EXPLORED_API FKarstLayout
{
	TArray<FKarstTower> Towers;
	TArray<FKarstLagoon> Lagoons;
};

/**
 * Macizo kárstico: torres de caliza de paredes casi a plomo con cimas redondeadas y
 * quebradas, lagunas interiores y llanos bajos entre ellas donde la erosión abre canales y
 * deja derrubios al pie de las paredes. Modelo puro: FTerrainDensity lo combina con la
 * rejilla erosionada de FIslandReliefModel (las torres se aplican después de erosionar, así
 * la erosión térmica nunca las convierte en conos).
 */
class EXPLORED_API FKarstTowerModel
{
public:
	/** Torres y lagunas deterministas por semilla de isla. */
	static FKarstLayout Generate(uint32 IslandSeed);

	/**
	 * Altura de las torres en Q (fracción de la altura máxima; 0 fuera de ellas). OutCore, si
	 * se pide, vale 1 dentro de la planta de alguna torre y baja a 0 en el pie de la pared.
	 */
	static float TowerHeight(const FKarstLayout& Layout, float Qx, float Qy, float* OutCore = nullptr);

	/**
	 * Llano entre torres (m): relieve bajo y ondulado y el arranque de las torres como
	 * montículos de derrubios (TalusFraction de su altura) para que la erosión tenga material
	 * que repartir al pie de las paredes. U = 1 - T (costa).
	 */
	static float LowlandHeight(const FKarstLayout& Layout, const FExploredNoise& Noise, float Qx, float Qy, float U, float MaxHeight);

	/**
	 * Levanta las torres sobre Land (m): dentro de cada planta la altura sube a la cima de la
	 * torre y en el pie de la pared se funde con Land. Fuera de las torres devuelve Land sin
	 * tocar (un máximo con 0 aplanaba a cota 0 todo lo que quedaba bajo el mar, lagunas incluidas).
	 */
	static float ApplyTowers(const FKarstLayout& Layout, float Qx, float Qy, float Land, float MaxHeight);

	/**
	 * Hunde las lagunas bajo el mar. Se aplica después de erosionar, como las torres: si no,
	 * los derrubios que la erosión térmica baja de las paredes las ciegan.
	 */
	static float ApplyLagoons(const FKarstLayout& Layout, float Qx, float Qy, float Land);

	/** Anchura de la pared, en fracción del radio de la torre: 0,07 da unos 85-88°. */
	static constexpr float WallSoftness = 0.07f;
	/** Parte de la altura de cada torre que entra en el llano como montículo de derrubios. */
	static constexpr float TalusFraction = 0.16f;
	static constexpr int32 MinTowers = 9;
	static constexpr int32 MaxTowers = 18;
};
