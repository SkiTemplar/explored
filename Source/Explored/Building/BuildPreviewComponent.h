#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Building/BuildingTypes.h"

#include "BuildPreviewComponent.generated.h"

class UBuildingSubsystem;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
struct FInputActionValue;

/**
 * Modo construcción del jugador (GDD §8.6): fantasma de la pieza elegida encajado
 * en la rejilla, verde si se puede colocar y rojo si no, giro de 90° y confirmación.
 * Toda la validación es de FBuildingModel a través de UBuildingSubsystem.
 *
 * Entrada (contexto propio con prioridad 1, construido por código como el del
 * personaje): B entra y sale, R gira, Z/X cambian de pieza, clic izquierdo coloca.
 */
UCLASS(ClassGroup = (Explored), meta = (BlueprintSpawnableComponent))
class EXPLORED_API UBuildPreviewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildPreviewComponent();

	/** Crea las acciones, añade el contexto al jugador local y enlaza la entrada. */
	void SetupInput(UEnhancedInputComponent* Input);

	UFUNCTION(BlueprintCallable, Category = "Explored|Construcción")
	void SetBuildModeActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Explored|Construcción")
	bool IsBuildModeActive() const { return bBuildModeActive; }

	UFUNCTION(BlueprintCallable, Category = "Explored|Construcción")
	void SelectPiece(FName DefId);

	UFUNCTION(BlueprintPure, Category = "Explored|Construcción")
	FName GetSelectedPiece() const { return SelectedPiece; }

	/** Pasa a la pieza siguiente (+1) o anterior (−1) del catálogo. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Construcción")
	void CyclePiece(int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "Explored|Construcción")
	void RotatePreview();

	/** Intenta colocar la pieza donde está el fantasma. OutReason explica el rechazo. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Construcción")
	bool ConfirmPlacement(FText& OutReason);

	/** Motivo por el que el fantasma está en rojo (vacío si está en verde). */
	UFUNCTION(BlueprintPure, Category = "Explored|Construcción")
	FText GetPreviewReason() const;

	/** Alcance del modo construcción desde los ojos (cm). */
	UPROPERTY(EditAnywhere, Category = "Explored|Construcción")
	float MaxReachCm = 600.0f;

	/**
	 * Construir sin materiales ni herramientas (depuración). Sin él, se exige llevar
	 * encima lo que cuesta la pieza; aún no se descuenta (UCarryComponent no tiene API
	 * para quitar objetos: ver docs/roadmap.md, P-BUILD).
	 */
	UPROPERTY(EditAnywhere, Category = "Explored|Construcción")
	bool bFreeBuild = false;

	UPROPERTY(EditAnywhere, Category = "Explored|Construcción")
	FLinearColor ValidColor = FLinearColor(0.2f, 0.9f, 0.3f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Explored|Construcción")
	FLinearColor InvalidColor = FLinearColor(0.95f, 0.2f, 0.15f, 1.0f);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void HandleToggle(const FInputActionValue& Value);
	void HandleRotate(const FInputActionValue& Value);
	void HandleConfirm(const FInputActionValue& Value);
	void HandleNext(const FInputActionValue& Value);
	void HandlePrevious(const FInputActionValue& Value);

	void EnsureGhost();
	void RefreshGhostMesh();
	void UpdatePreview();
	bool TraceAim(FVector& OutAimPoint, bool& bOutGroundContact) const;
	void GatherCarried(TMap<FName, int32>& OutInventory, TSet<FName>& OutTools) const;
	UBuildingSubsystem* GetBuildingSubsystem() const;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Ghost;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GhostMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> BuildContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ToggleAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RotateAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ConfirmAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NextAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PreviousAction;

	/** Desplazamiento de la malla del fantasma sobre el pivote (formas básicas de reserva). */
	FVector GhostOffset = FVector::ZeroVector;
	FName SelectedPiece;
	int32 Rotation = 0;
	bool bBuildModeActive = false;
	bool bHasAim = false;
	FVector AimPoint = FVector::ZeroVector;
	bool bAimOnGround = false;
	EBuildFailReason PreviewReason = EBuildFailReason::None;
};
