#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Audio/SfxRules.h"

#include "ExploredSfxSubsystem.generated.h"

class AExploredFire;
class AExploredOcean;
class AExploredRaft;
class UAudioComponent;
class USoundBase;
class USoundClass;

/**
 * Efectos de sonido del mundo (GDD v3 §10) que no pertenecen a un solo actor: pasos por superficie,
 * recoger, remadas de la balsa, el fuego y la parrilla de cada hoguera, y gaviotas en la costa.
 * Observa el estado del juego (no hace falta cablear cada acción) y aplica las reglas de
 * ExploredSfxRules. Todo pasa por la SoundClass de Efectos (los gritos de gaviota, por la de
 * Ambiente). Los sonidos son de Tools/Audio (/Game/Generated/Audio/Efectos).
 *
 * Blueprint: «Play Effect» (un efecto por nombre, 2D o en un punto).
 */
UCLASS()
class EXPLORED_API UExploredSfxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Reproduce sfx_<Name> (sin prefijo de ruta). Con bAtLocation, en Location (3D). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Sonido")
	void PlayEffect(const FString& Name, FVector Location, bool bAtLocation = false, float Volume = 1.0f, float Pitch = 1.0f);

	/** Pasos dados desde el inicio (lo leen el bot y los tests de humo). */
	int32 GetFootstepCount() const { return Footsteps; }

private:
	USoundBase* GetSound(const FString& Name);
	void UpdateFootsteps(float DeltaTime);
	void UpdateRaft(float DeltaTime);
	void UpdateFires();
	void UpdateGulls(float DeltaTime);
	void BindPickups();

	UFUNCTION()
	void HandlePickedUp(FText DisplayName);

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<USoundBase>> Sounds;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> EffectsClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> AmbientClass;

	/** Bucle de fuego y de parrilla de cada hoguera encendida. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<AExploredFire>, TObjectPtr<UAudioComponent>> FireLoops;

	UPROPERTY(Transient)
	TMap<TObjectPtr<AExploredFire>, TObjectPtr<UAudioComponent>> SizzleLoops;

	TWeakObjectPtr<AExploredOcean> Ocean;
	TWeakObjectPtr<AExploredRaft> Raft;
	TWeakObjectPtr<UObject> BoundCarry;
	FSfxCadence StepCadence;
	FSfxCadence PaddleCadence;
	float SlowTimer = 0.0f;
	float GullTimer = 6.0f;
	int32 Footsteps = 0;
	int32 Variant = 0;
	FRandomStream Random{0x5F00D};
};
