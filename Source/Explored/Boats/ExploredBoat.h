#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Boats/BoatModel.h"
#include "Interaction/ExploredInteractable.h"
#include "Ocean/OceanCurrents.h"

#include "ExploredBoat.generated.h"

class AExploredCharacter;
class AExploredOcean;
class APlayerController;
class UInputAction;
class UInputMappingContext;
class USceneComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/** Tipo de embarcación editable en el editor; refleja EBoatType (modelo puro, sin UENUM). */
UENUM(BlueprintType)
enum class EExploredBoatKind : uint8
{
	Raft UMETA(DisplayName = "Balsa"),
	Canoe UMETA(DisplayName = "Canoa"),
	Outrigger UMETA(DisplayName = "Canoa con balancín y vela"),
	Limon UMETA(DisplayName = "Barco «Limón»")
};

static_assert(static_cast<uint8>(EExploredBoatKind::Limon) + 1 == static_cast<uint8>(EBoatType::Count),
	"EExploredBoatKind debe reflejar EBoatType");

/**
 * Embarcación del jugador (GDD §8.10). Capa fina sobre FBoatModel: cada
 * fotograma reúne olas, corriente, viento y fondo, avanza el modelo y copia
 * su estado a la transformación (cinemático, sin física de Chaos).
 *
 * Se sube con el interactuable (E): el personaje queda sentado y quieto, y
 * el barco añade su propio contexto de Enhanced Input con prioridad mayor
 * que el del personaje, así que W/A/S/D y E pasan a gobernar el barco
 * mientras el ratón sigue moviendo la cámara:
 *   W paladas alternas · A/D paladas por una banda y timón · R izar/arriar
 *   Z/X cazar/largar escota · T escota automática · B achicar (mantener)
 *   F adrizar si está volcado · E bajar.
 */
UCLASS()
class EXPLORED_API AExploredBoat : public AActor, public IExploredInteractable
{
	GENERATED_BODY()

public:
	AExploredBoat();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// IExploredInteractable
	virtual void GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const override;
	virtual bool CanInteract_Implementation(AActor* InInstigator) const override;
	virtual void Interact_Implementation(AActor* InInstigator) override;

	/** Sube al personaje y le da los mandos del barco. False si está ocupado, volcado o hundido. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Barcos")
	bool Board(AExploredCharacter* Character);

	/** Baja al tripulante junto a la borda (al agua si no hay tierra) y le devuelve el control. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Barcos")
	void Leave();

	UFUNCTION(BlueprintPure, Category = "Explored|Barcos")
	bool IsOccupied() const { return Occupant.IsValid(); }

	/** Tripulante a bordo (nullptr si no hay nadie). */
	AExploredCharacter* GetOccupant() const;

	UFUNCTION(BlueprintPure, Category = "Explored|Barcos")
	bool IsSailRaised() const { return Model.GetState().bSailRaised; }

	UFUNCTION(BlueprintPure, Category = "Explored|Barcos")
	EExploredBoatKind GetBoatKind() const { return BoatKind; }

	/** Daño de otro sistema (ciclón, fauna): fracción del casco 0–1. */
	void ApplyExternalDamage(float Amount01) { Model.ApplyDamage(Amount01); }

	/**
	 * La embarcación la ha terminado el jugador en el astillero (no estaba en el mapa):
	 * al aparecer cuenta para «boats_built» y la partida la vuelve a crear al cargar.
	 * Quien la construya la crea diferida y lo marca antes de FinishSpawning.
	 */
	UPROPERTY(EditAnywhere, Category = "Explored|Barcos")
	bool bBuiltByPlayer = false;

	/** Una palada (false = babor, true = estribor). Devuelve false si aún dura la anterior. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Barcos")
	bool PaddleStroke(bool bStarboard);

	/** Timón de -1 (babor) a 1 (estribor). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Barcos")
	void SetRudder(float Rudder);

	UFUNCTION(BlueprintCallable, Category = "Explored|Barcos")
	bool SetSailRaised(bool bRaised);

	/** Cambia la escota a mano (desactiva la automática). Positivo = largar. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Barcos")
	void AdjustSailTrim(float Delta01);

	UFUNCTION(BlueprintCallable, Category = "Explored|Barcos")
	void SetAutoTrim(bool bAuto) { Controls.bAutoTrim = bAuto; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Barcos")
	void SetBailing(bool bBail) { Controls.bBailing = bBail; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Barcos")
	bool TryRight() { return Model.TryRight(); }

	UFUNCTION(BlueprintPure, Category = "Explored|Barcos")
	float GetSpeedCmS() const { return Model.SpeedCmS(); }

	/** Rumbo de brújula (0 norte, 90 este). */
	UFUNCTION(BlueprintPure, Category = "Explored|Barcos")
	float GetHeadingDeg() const { return Model.GetState().YawDeg; }

	UFUNCTION(BlueprintPure, Category = "Explored|Barcos")
	bool IsGrounded() const { return Model.GetState().bGrounded; }

	/** La balsa ha llegado al borde del mar abierto (para el aviso del HUD). */
	UFUNCTION(BlueprintPure, Category = "Explored|Barcos")
	bool IsAtOpenOceanLimit() const { return Model.GetState().bAtOpenOceanLimit; }

	UFUNCTION(BlueprintPure, Category = "Explored|Barcos")
	float GetHullDamage01() const { return Model.GetState().HullDamage01; }

	/** Error de rumbo frente al punto del horizonte de una estrella (navegación nocturna, GDD §6.3). */
	UFUNCTION(BlueprintPure, Category = "Explored|Barcos")
	float GetHeadingErrorToBearing(float StarBearingDeg) const
	{
		return FBoatNavigation::HeadingErrorDeg(Model.GetState().YawDeg, StarBearingDeg);
	}

	const FBoatModel& GetModel() const { return Model; }

	/** Datos planos para la sección «boats» (UExploredWiringSubsystem). */
	FBoatSaveData GetSaveData() const { return Model.ToSaveData(); }
	void RestoreFromSaveData(const FBoatSaveData& Data);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyMeshes();
	void ApplyModelTransform();
	void UpdateSailVisual();
	void DrivePaddling();
	FBoatEnvironment GatherEnvironment();
	AExploredOcean* ResolveOcean(float DeltaSeconds);
	float TraceDepthCm(const FVector2D& Position) const;
	APlayerController* GetOccupantController() const;

	void BuildInputAssets();
	void BindInput(APlayerController* PC);
	void HandlePaddle(const FInputActionValue& Value);
	void HandlePaddleReleased(const FInputActionValue& Value);
	void HandleToggleSail(const FInputActionValue& Value);
	void HandleTrim(const FInputActionValue& Value);
	void HandleAutoTrim(const FInputActionValue& Value);
	void HandleBailStarted(const FInputActionValue& Value);
	void HandleBailCompleted(const FInputActionValue& Value);
	void HandleRight(const FInputActionValue& Value);
	void HandleLeave(const FInputActionValue& Value);

	UPROPERTY(VisibleAnywhere, Category = "Explored|Barcos")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Barcos")
	TObjectPtr<UStaticMeshComponent> Hull;

	/** Vela suelta de boats.py (SM_Sail) sobre el mástil; solo en el balancín y el «Limón». */
	UPROPERTY(VisibleAnywhere, Category = "Explored|Barcos")
	TObjectPtr<UStaticMeshComponent> Sail;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Barcos")
	TObjectPtr<USceneComponent> Seat;

	UPROPERTY(EditAnywhere, Category = "Explored|Barcos")
	EExploredBoatKind BoatKind = EExploredBoatKind::Canoe;

	/**
	 * boats.py modela la eslora a lo largo del eje Y de Blender; el barco
	 * avanza por +X. Giro de la malla importada para alinearla (verificar en
	 * local con la importación real).
	 */
	UPROPERTY(EditAnywhere, Category = "Explored|Barcos")
	float MeshYawOffsetDeg = 90.0f;

	/** Profundidad que se da por «mar abierto» cuando el sondeo no encuentra fondo (cm). */
	UPROPERTY(EditAnywhere, Category = "Explored|Barcos")
	float MaxProbeDepthCm = 20000.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<AExploredOcean> Ocean;

	UPROPERTY(Transient)
	TWeakObjectPtr<AExploredCharacter> Occupant;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PaddleAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SailAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TrimAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AutoTrimAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BailAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RightAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LeaveAction;

	FBoatModel Model;
	FBoatControls Controls;
	TArray<FOceanStrait> Straits;
	FVector2D PaddleAxis = FVector2D::ZeroVector;
	EBoatSide NextStrokeSide = EBoatSide::Port;
	float TimeSinceOceanSearch = 0.0f;
	bool bInputBound = false;
};
