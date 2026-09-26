#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Cartography/CartographyModel.h"

#include "CartographyComponent.generated.h"

class USwimComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIslandCharted, int32, IslandIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFirstCoastDrawn);

/**
 * Mapa dibujado a mano del jugador (GDD §5). Capa fina sobre FCartographyModel:
 * cada ~0,33 s muestrea la posición, cómo se mueve (andar, correr, nadar), la
 * distancia a la costa de referencia (trazada una vez del terreno puro al empezar)
 * y el agua que recibe el mapa (lluvia del clima, mar al nadar).
 *
 * No sabe de inventario: quien gestione la brújula o la funda seca llama a
 * SetHasCompass / SetMapStoredDry. La presentación (el mapa en las manos) es de la
 * UI; SExploredMapSheet dibuja la hoja a partir de GetModel().
 */
UCLASS(ClassGroup = (Explored), meta = (BlueprintSpawnableComponent))
class EXPLORED_API UCartographyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCartographyComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	const FCartographyModel& GetModel() const { return Model; }

	/** Restaura el mapa de una partida guardada (P-SAVE). */
	void LoadState(const FCartographyState& State);

	UFUNCTION(BlueprintCallable, Category = "Explored|Mapa")
	void SetHasCompass(bool bInHasCompass) { bHasCompass = bInHasCompass; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Mapa")
	void SetMapStoredDry(bool bInStoredDry) { bMapStoredDry = bInStoredDry; }

	/** Marca a mano donde está el jugador (id de `map_marks` o None para una nota). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Mapa")
	bool AddMarkHere(FName StampId, const FString& Text);

	/** Marca con el catalejo un punto visto a distancia (centímetros, espacio de mundo). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Mapa")
	bool AddSpyglassMark(FName StampId, FVector TargetLocation, const FString& Text);

	/** Marca con el sextante la posición exacta del jugador, también en mar abierto. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Mapa")
	bool AddSextantMarkHere(FName StampId, const FString& Text);

	/** Desde un mirador: bocetos de las islas cuya costa queda a menos de RangeMeters. Devuelve cuántas. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Mapa")
	int32 SketchFromViewpoint(float RangeMeters = 1500.0f);

	UFUNCTION(BlueprintCallable, Category = "Explored|Mapa")
	bool NoteRecipe(FName RecipeId, bool bDoodle = true) { return Model.NoteRecipe(RecipeId, bDoodle); }

	/** Mesa de cartografía de la base: copia en limpio lo que aún se lee. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Mapa")
	int32 CopyToCleanSheet() { return Model.CopyToCleanSheet(); }

	UFUNCTION(BlueprintPure, Category = "Explored|Mapa")
	float GetIslandCoverage(int32 IslandIndex) const { return Model.GetIslandCoverage(IslandIndex); }

	UFUNCTION(BlueprintPure, Category = "Explored|Mapa")
	bool HasDrawnAnyCoast() const { return Model.HasDrawnAnyCoast(); }

	UFUNCTION(BlueprintPure, Category = "Explored|Mapa")
	float GetWetness() const { return Model.GetWetness(); }

	/** Una isla llega a la cobertura completa (logro «Cartógrafo»). */
	UPROPERTY(BlueprintAssignable, Category = "Explored|Mapa")
	FOnIslandCharted OnIslandCharted;

	/** Primera costa dibujada (invalida el logro «Sin mapa»). */
	UPROPERTY(BlueprintAssignable, Category = "Explored|Mapa")
	FOnFirstCoastDrawn OnFirstCoastDrawn;

protected:
	virtual void BeginPlay() override;

private:
	void BuildReferenceCoasts();
	FVector2D GetOwnerPositionMeters() const;
	ECartographyLocomotion ReadLocomotion() const;
	FCartographyExposure ReadExposure() const;
	void BroadcastProgress(bool bHadDrawnCoast);

	FCartographyModel Model;

	UPROPERTY(Transient)
	TWeakObjectPtr<USwimComponent> Swim;

	/** Islas ya anunciadas como cartografiadas (para disparar el evento una sola vez). */
	TArray<uint8> ChartedIslands;
	double LastSampleTime = -1.0;

	/** Segundos entre muestras (GDD: 0,25–0,5 s). */
	UPROPERTY(EditAnywhere, Category = "Explored|Mapa")
	float SampleInterval = 0.33f;

	/** Velocidad horizontal (cm/s) a partir de la cual se considera que corre. */
	UPROPERTY(EditAnywhere, Category = "Explored|Mapa")
	float RunSpeedThreshold = 600.0f;

	/** Rayos por isla para trazar la costa de referencia al empezar. */
	UPROPERTY(EditAnywhere, Category = "Explored|Mapa")
	int32 CoastRays = 180;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explored|Mapa", meta = (AllowPrivateAccess = "true"))
	bool bHasCompass = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explored|Mapa", meta = (AllowPrivateAccess = "true"))
	bool bMapStoredDry = false;
};
