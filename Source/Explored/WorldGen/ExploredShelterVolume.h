#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ExploredShelterVolume.generated.h"

class UBoxComponent;

/**
 * Refugio (GDD §6): caja de solapamiento bajo techo. Los dos refugios de la Isla de Sal (Cala
 * Varada y base del faro) son además el punto de reaparición del desmayo. El cuerpo y el clima
 * consultan IsActorSheltered; el desmayo, GetRespawnLocation del refugio más cercano.
 */
UCLASS()
class EXPLORED_API AExploredShelterVolume : public AActor
{
	GENERATED_BODY()

public:
	AExploredShelterVolume();

	/** Semiextensión de la caja en metros (island_props.json/extent); la base queda en el pivote del actor. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Refugio")
	void SetExtentMeters(const FVector& HalfExtentMeters);

	/** true si el actor (su pivote) está dentro de la caja. */
	UFUNCTION(BlueprintPure, Category = "Explored|Refugio")
	bool IsActorSheltered(const AActor* Actor) const;

	/** true si el punto está dentro de la caja (un fuego bajo el techo no lo apaga el ciclón). */
	UFUNCTION(BlueprintPure, Category = "Explored|Refugio")
	bool IsLocationSheltered(const FVector& Location) const;

	/** Punto del suelo del refugio donde reaparecer tras un desmayo (cm). */
	UFUNCTION(BlueprintPure, Category = "Explored|Refugio")
	FVector GetRespawnLocation() const;

	UFUNCTION(BlueprintPure, Category = "Explored|Refugio")
	bool IsRespawnPoint() const { return bRespawnPoint; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explored|Refugio")
	FName ShelterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explored|Refugio")
	bool bRespawnPoint = true;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Explored|Refugio")
	TObjectPtr<UBoxComponent> Box;
};
