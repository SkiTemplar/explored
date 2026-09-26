#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Fishing/FishingModel.h"

#include "ExploredFishingSubsystem.generated.h"

class AExploredOcean;

/**
 * Estado de la pesca del mundo: trampas colocadas, legendarias capturadas,
 * pozas vaciadas y presión de pesca por zona (FFishingSaveState, que P-SAVE
 * guardará tal cual). También traduce el mundo a FFishingConditions (hora,
 * marea, luna, clima, profundidad) para UFishingComponent y AExploredTrap.
 */
UCLASS()
class EXPLORED_API UExploredFishingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	FFishingSaveState& GetState() { return State; }
	const FFishingSaveState& GetState() const { return State; }
	uint32 GetSeed() const { return Seed; }

	/** Días totales de juego según UTimeOfDaySubsystem (0 si no hay reloj). */
	float GetNowDays() const;

	/** Altura del agua (cm) en una posición; false si no hay océano en el nivel. */
	bool GetWaterHeightAt(const FVector& Location, float& OutWaterZ) const;

	/** Profundidad del fondo (m) bajo un punto de la superficie del agua. */
	float MeasureDepthM(const FVector& SurfacePoint, const AActor* IgnoredActor) const;

	/** Condiciones de pesca en un punto de agua: hora, marea, luna, clima, hábitat y sobrepesca. */
	FFishingConditions MakeConditions(const FVector& SurfacePoint, float DepthM) const;

private:
	AExploredOcean* FindOcean() const;

	FFishingSaveState State;
	uint32 Seed = 0;

	/** Caché débil del océano (no retiene el actor; no necesita UPROPERTY). */
	mutable TWeakObjectPtr<AExploredOcean> CachedOcean;
};
