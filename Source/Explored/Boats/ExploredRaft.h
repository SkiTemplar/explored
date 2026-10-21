#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputActionValue.h"

#include "Boats/RaftModel.h"
#include "Interaction/ExploredInteractable.h"
#include "WorldGen/ExploredPlaceholder.h"

#include "ExploredRaft.generated.h"

class AExploredCharacter;
class AExploredRaft;
class APlayerController;
class UInputAction;
class UInputMappingContext;
class UStaticMeshComponent;

/**
 * Sitio de la balsa en Playa Norte (GDD v3 §5.6 receta 8 y §5.7): Iria deja aquí los 4 troncos de
 * uno en uno («E · Dejar tronco») y la ata con 2 cuerdas («E · Atar balsa», 20 s); entonces
 * aparece la AExploredRaft en la orilla. También vigila el reloj: a las 18:00 del día 2 avisa
 * «El mar se cierra» y a las 18:45, si Iria sigue en el islote, la devuelve con un fundido a
 * Playa Norte (sed y hambre -20) y deja la balsa varada.
 *
 * El estado (troncos, atada, posición de la balsa) se guarda en la sección «island».
 * Blueprint: «Get Logs Placed», «Is Raft Built».
 */
UCLASS()
class EXPLORED_API AExploredRaftSite : public AExploredPlaceholder, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredRaftSite();

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

	UFUNCTION(BlueprintPure, Category = "Explored|Balsa")
	int32 GetLogsPlaced() const { return Site.LogsPlaced; }

	UFUNCTION(BlueprintPure, Category = "Explored|Balsa")
	bool IsRaftBuilt() const { return Site.bBuilt; }

	const FRaftSiteState& GetSiteState() const { return Site; }
	AExploredRaft* GetRaft() const { return Raft.Get(); }

	/** Punto de la arena junto al sitio donde se aparece tras un fundido (cm). */
	FVector GetShoreLocation() const;

	/** Rumbo hacia el islote del carguero (grados): la corriente empuja a su izquierda. */
	float GetRouteYawDeg() const { return RouteYawDeg; }

	/** Ajusta el sitio y la balsa al estado guardado (o al de depuración). */
	void RefreshFromIslandState();

	/** Depuración y pruebas de humo: deja la balsa hecha sin gastar nada. */
	void DebugBuild();

	/** Radio del islote del carguero (m) a partir del que Iria «sigue en el islote». */
	UPROPERTY(EditAnywhere, Category = "Explored|Balsa")
	float IsletRadiusMeters = 50.0f;

protected:
	virtual void BeginPlay() override;
	virtual void OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type) override;

private:
	void PlaceLog(AActor* InInstigator);
	void Tie(AActor* InInstigator);
	void FinishTie(AActor* InInstigator);
	void UpdateLogVisuals();
	void EnsureRaft();
	/** Primer punto de agua desde el sitio hacia el islote (cm). */
	FVector FindLaunchLocation() const;
	void WatchClock();
	void ForceReturn();
	/** Si Iria se desmaya en la balsa, la baja y la deja varada en el sitio de Playa Norte. */
	void ReturnRaftIfOccupantFainted();
	void SaveToIslandState() const;
	bool IsPlayerOnIslet(const APawn& Pawn) const;

	FRaftSiteState Site;
	FRaftTuning Tuning;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LogMeshes;

	UPROPERTY(Transient)
	TWeakObjectPtr<AExploredRaft> Raft;

	FVector IsletCenter = FVector::ZeroVector;
	bool bHasIslet = false;
	float RouteYawDeg = 37.7f;
	double LastSeenHours = -1.0;
	bool bReturning = false;
};

/**
 * Balsa de cubos (GDD v3 §5.7): un asiento; W avanza a remo a 2 m/s, A/D giran, E baja. Sin vela,
 * sin timón y sin vuelco: el movimiento es cinemático (FRaftModel) sobre la altura del océano,
 * con un cabeceo visual amortiguado. Corriente lateral de 0,3 m/s hacia la izquierda de la ruta
 * Playa Norte → islote. Encalla al tocar fondo; al bajar en la orilla Iria queda en tierra.
 *
 * Blueprint: «Board», «Leave», «Is Occupied».
 */
UCLASS()
class EXPLORED_API AExploredRaft : public AActor, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredRaft();

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

	UFUNCTION(BlueprintCallable, Category = "Explored|Balsa")
	bool Board(AExploredCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Explored|Balsa")
	void Leave();

	UFUNCTION(BlueprintPure, Category = "Explored|Balsa")
	bool IsOccupied() const { return Occupant.IsValid(); }

	AExploredCharacter* GetOccupant() const { return Occupant.Get(); }
	bool IsGrounded() const { return bGrounded; }
	const FRaftMotion& GetMotion() const { return Motion; }

	/** Coloca la balsa (cm, grados) y para su movimiento. */
	void PlaceAt(const FVector& Location, float YawDeg);

	/** Corriente (m/s, ya orientada) y si se puede remar (no a partir de las 18:00 del día 2). */
	void SetCurrent(const FVector2D& InCurrentMps) { CurrentMps = InCurrentMps; }

	/** Mando directo (pruebas de humo): W 0-1, giro -1..1. */
	void SetControls(float Forward, float Turn) { Controls.Forward = Forward; Controls.Turn = Turn; }

	/** Altura del agua más baja que el suelo menos esto (cm) = encallada. */
	UPROPERTY(EditAnywhere, Category = "Explored|Balsa")
	float DraftCm = 30.0f;

private:
	bool ProbeGround(const FVector2D& PositionM, float& OutGroundZ) const;
	float WaterHeightAt(const FVector& Location) const;
	void ApplyMotion(float DeltaSeconds);
	void BuildInputAssets();
	void BindInput();
	void HandleRow(const FInputActionValue& Value);
	void HandleRowReleased(const FInputActionValue& Value);
	void HandleTurn(const FInputActionValue& Value);
	void HandleTurnReleased(const FInputActionValue& Value);
	void HandleLeave(const FInputActionValue& Value);
	bool OccupantHasPaddle() const;
	APlayerController* GetOccupantController() const;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Balsa")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Balsa")
	TObjectPtr<UStaticMeshComponent> Hull;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Balsa")
	TObjectPtr<USceneComponent> Seat;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RowAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TurnAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LeaveAction;

	TWeakObjectPtr<AExploredCharacter> Occupant;
	FRaftMotion Motion;
	FRaftControls Controls;
	FRaftTuning Tuning;
	FVector2D CurrentMps = FVector2D::ZeroVector;
	float BaseZ = 0.0f;
	bool bGrounded = false;
	bool bInputBound = false;
};
