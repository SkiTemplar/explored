#pragma once

#include "CoreMinimal.h"
#include "Save/SaveArchive.h"

/**
 * Sección «player»: transformación y valores del cuerpo como datos planos. Los
 * valores del cuerpo reflejan FSurvivalState y se enlazarán cuando exista su
 * componente; hasta entonces guardan los valores por defecto del modelo.
 */
struct EXPLORED_API FSavePlayerState
{
	FVector Location = FVector::ZeroVector;
	/** Rotación del control (cámara): Pitch, Yaw, Roll. */
	FRotator ControlRotation = FRotator::ZeroRotator;
	FVector Velocity = FVector::ZeroVector;

	float Health = 100.0f;
	float Hunger = 85.0f;
	float Thirst = 70.0f;
	float Energy = 100.0f;
	float Rest = 90.0f;
	float Morale = 60.0f;
	float BodyTemperature = 37.0f;
	float Wetness = 0.0f;

	void Save(FSaveArchive& Ar) const;
	/** Lee la sección; una rotación o velocidad no finita (partida editada a mano) queda a cero. */
	void Load(const FSaveArchive& Ar);

	/**
	 * Posición utilizable al cargar: finita y dentro de ±MaxAbsCm en cada eje (la Isla de Sal
	 * cabe en 400 × 400 m). Si no lo es, el personaje se queda donde lo puso el mapa.
	 */
	bool HasPlausibleLocation(double MaxAbsCm = 200000.0) const;

	bool operator==(const FSavePlayerState& Other) const;
};
