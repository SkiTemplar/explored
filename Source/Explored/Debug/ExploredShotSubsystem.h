#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "ExploredShotSubsystem.generated.h"

class ACameraActor;

/** Un punto de vista para la verificación visual. */
struct FExploredShot
{
	FString Name;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float Hours = 12.0f;
};

/**
 * Herramienta de verificación visual (no se incluye en Shipping). Con
 * «-ExploredShots=<conjunto>» en la línea de comandos del juego espera a que
 * terminen los shaders, recorre una lista de cámaras y horas, guarda una
 * captura de cada una en Saved/Shots/ y cierra el juego.
 *
 * Conjuntos: «islands» (una vista por isla a media tarde), «day» (ciclo del
 * día en la isla de inicio), «spawn» (vista del jugador al aparecer) y «all».
 */
UCLASS()
class EXPLORED_API UExploredShotSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bActive; }

private:
	void BuildShotList(const FString& Set);
	void BeginShot(int32 Index);

	TArray<FExploredShot> Shots;
	int32 Current = INDEX_NONE;
	float Timer = 0.0f;
	bool bActive = false;
	bool bRequested = false;
	FString OutputDir;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;
};
