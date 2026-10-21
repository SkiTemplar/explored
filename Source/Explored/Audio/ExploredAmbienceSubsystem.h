#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldGen/IslandLayout.h"

#include "ExploredAmbienceSubsystem.generated.h"

class AExploredOcean;
class UAudioComponent;
class USoundBase;
struct FIslandLayout;

/** Capas del paisaje sonoro. */
enum class EAmbienceLayer : uint8
{
	OceanCalm,
	OceanRough,
	WindLight,
	WindStrong,
	JungleDay,
	JungleNight,
	RainLight,
	RainHeavy,
	Underwater,
	RainOnRoof, // Lluvia sobre el tejado (interior del faro y refugios).
	Count
};

/** Entorno que se escucha desde un punto: factores en [0, 1]. */
struct EXPLORED_API FAmbienceEnvironment
{
	float Coast = 0.0f;
	float Altitude = 0.0f;
	float Vegetation = 0.0f;
	float Night = 0.0f;
	float Underwater = 0.0f;
	float Rain = 0.0f;
	float SeaState = 0.15f;
	/** Bajo techo (AExploredShelterVolume, p. ej. el interior del faro): el exterior suena apagado. */
	float Interior = 0.0f;
};

/** Mezcla de volúmenes de cada capa a partir del entorno (función pura). */
struct EXPLORED_API FAmbienceMixer
{
	static void Mix(const FAmbienceEnvironment& Env, float OutVolumes[static_cast<int32>(EAmbienceLayer::Count)]);
};

/**
 * Paisaje sonoro en capas que sigue al oyente: olas cerca de la costa,
 * viento con la altura, vegetación de día y de noche, lluvia, bajo el agua e
 * interior (el exterior amortiguado y la lluvia sobre el tejado).
 * Las transiciones son fundidos suaves.
 */
UCLASS()
class EXPLORED_API UExploredAmbienceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Lluvia (0–1) que fija el sistema de clima. */
	void SetRainIntensity(float Value) { Rain = FMath::Clamp(Value, 0.0f, 1.0f); }
	void SetSeaState(float Value) { SeaState = FMath::Clamp(Value, 0.0f, 1.0f); }

	/**
	 * Multiplicador de todas las capas. Lo fija UExploredGameUserSettings
	 * (ApplyToWorld y OnWorldBeginPlay): 1 si las capas pasan por la SoundClass
	 * de Ambiente, o el volumen de Ambiente si esa SoundClass aún no existe.
	 */
	void SetAmbienceVolume(float Value) { MasterVolume = FMath::Clamp(Value, 0.0f, 1.0f); }

	/** Evalúa el entorno sonoro en un punto (centímetros, espacio de mundo). */
	FAmbienceEnvironment Evaluate(const FVector& ListenerCm) const;

	/** Último entorno evaluado en el oyente (lo leen la música y otros sistemas). */
	const FAmbienceEnvironment& GetListenerEnvironment() const { return ListenerEnvironment; }

	/** Isla en cuya zona de influencia está el oyente; Count en mar abierto. */
	EIslandArchetype GetListenerIsland() const { return ListenerIsland; }

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> Layers;

	TArray<float> CurrentVolumes;
	TArray<float> TargetVolumes;
	TSharedPtr<const FIslandLayout> IslandData;
	/** Océano del nivel: la superficie real (con oleaje) decide si el oyente está sumergido. */
	TWeakObjectPtr<AExploredOcean> Ocean;
	FAmbienceEnvironment ListenerEnvironment;
	EIslandArchetype ListenerIsland = EIslandArchetype::Count;
	float SampleTimer = 0.0f;
	float Rain = 0.0f;
	float SeaState = 0.15f;
	float MasterVolume = 1.0f;
};
