#pragma once

#include "CoreMinimal.h"
#include "WorldGen/ArchipelagoLayout.h"

/** Cuánta costa es acantilado en una isla y de qué altura. Ángulos en radianes, en Q. */
struct EXPLORED_API FCliffStyle
{
	/** Fracción del perímetro buscada (por ruido del perímetro); 0 = sin acantilados. */
	float Fraction = 0.0f;
	float MinHeight = 10.0f;
	float MaxHeight = 40.0f;
	/**
	 * Tramos fijos (X = rumbo del centro, Y = semiancho) en lugar de los del ruido: en la isla
	 * de inicio el acantilado va donde no estorba a la bahía ni a la playa del spawn.
	 */
	TArray<FVector2D> Sectors;
};

/** Tramos de acantilado de una isla tabulados a lo largo del perímetro. Inmutable. */
class EXPLORED_API FCoastalCliffs
{
public:
	/** 0 = costa normal (playa o ribera), 1 = acantilado pleno. */
	float Amount(float Angle) const { return Sample(Amounts, Angle); }
	/** Altura del acantilado pleno en ese rumbo (m). */
	float Height(float Angle) const { return Sample(Heights, Angle); }
	bool IsEmpty() const { return Amounts.IsEmpty(); }

	static constexpr int32 Bins = 720;

private:
	friend class FCoastalCliffModel;
	float Sample(const TArray<float>& Table, float Angle) const;

	TArray<float> Amounts;
	TArray<float> Heights;
};

/**
 * Acantilados marinos (modelo puro, determinista por semilla): en unos tramos de la costa la
 * tierra se levanta hasta 10-60 m junto al mar y cae casi a plomo al agua, alternando con
 * calas y playas. Se aplica después de erosionar (la erosión térmica los tumbaría) como un
 * levantamiento que se apaga tierra adentro: detrás de la pared queda un rellano que baja
 * suave, y los ríos que llegaban a ese tramo acaban colgados en cascada.
 */
class EXPLORED_API FCoastalCliffModel
{
public:
	/** Estilo de cada arquetipo; false si no tiene acantilados marinos propios. */
	static bool StyleFor(EIslandArchetype Archetype, FCliffStyle& OutStyle);

	static FCoastalCliffs Build(uint32 Seed, const FCliffStyle& Style);

	/**
	 * Metros que se suman a la orilla en el rumbo Angle a la distancia U (1 - T) de la costa, en
	 * una isla de radio Radius (m): 0 en la línea de costa y en el mar, la altura del acantilado
	 * a FaceWidth metros (la pared) y se apaga tierra adentro. Continuo en U = 0: la pared no
	 * abre un escalón con el mar.
	 */
	static float Uplift(const FCoastalCliffs& Cliffs, float Angle, float U, float Radius);

	static constexpr float FaceWidth = 3.0f;
	/** El levantamiento se apaga en unas cinco veces su altura tierra adentro (~11°). */
	static constexpr float BackSlopeRun = 5.0f;
};
