#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "MuseumDisplayComponent.generated.h"

class UStaticMeshComponent;
class URuinsSubsystem;

/**
 * Mueble de exposición del museo (estantería, vitrina o panel): se registra en
 * URuinsSubsystem y encaja la malla de cada tesoro expuesto en su hueco
 * (FDisplaySlotDef::Offset, relativo a la raíz del actor). Las reglas de qué cabe
 * dónde son de FMuseumModel.
 */
UCLASS(ClassGroup = (Explored), meta = (BlueprintSpawnableComponent))
class EXPLORED_API UMuseumDisplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMuseumDisplayComponent();

	/** Tipo de mueble de artifacts.json («estanteria_museo», «vitrina_museo», «panel_museo»). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explored|Museo")
	FName DisplayId = TEXT("estanteria_museo");

	/**
	 * Clave única del mueble en la partida. La fija la construcción al colocar la pieza;
	 * si se queda en INDEX_NONE se deriva del nombre del actor (estable en actores del nivel).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explored|Museo")
	int32 DisplayKey = INDEX_NONE;

	/** Carpeta de las mallas de tesoros importadas (Tools/Blender/props/tesoros.py). */
	UPROPERTY(EditAnywhere, Category = "Explored|Museo")
	FString MeshFolder = TEXT("/Game/Generated/Meshes/Treasures");

	/** Coloca el tesoro en el primer hueco libre de este mueble donde quepa. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Museo")
	bool PlaceInFirstFreeSlot(FName ArtifactId);

	/** Retira el tesoro de un hueco; devuelve su id (o None). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Museo")
	FName RemoveFromSlot(int32 Slot);

	/** Rehace las mallas de los huecos a partir del estado del museo. */
	void RefreshSlots();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	URuinsSubsystem* GetRuinsSubsystem() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SlotMeshes;

	FDelegateHandle MuseumChangedHandle;
};
