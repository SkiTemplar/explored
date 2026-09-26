#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Ocean/OceanCurrents.h"

#include "SwimComponent.generated.h"

class ACharacter;
class AExploredOcean;
class UCarryComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOxygenChanged, float, Oxygen01);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGaspForAir);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDrowningDamage, float, DamagePerSecond);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDiveSplash);

/** Cómo está el jugador respecto al agua en este instante (GDD §4.1). */
UENUM(BlueprintType)
enum class EWaterState : uint8
{
	OnLand,
	Wading,   // Agua hasta las rodillas o la cintura; sigue de pie.
	Swimming, // A flote, en la superficie.
	Diving    // Sumergido, en apnea.
};

/**
 * Nado y buceo del jugador. Sigue la altura y la pendiente reales del
 * oleaje de Gerstner (Ocean/OceanWaves.h, la misma función que evalúa el
 * material del océano), aplica las corrientes de los estrechos entre islas
 * (Ocean/OceanCurrents.h) y gestiona la apnea con las tasas puras de
 * FSurvivalModel::OxygenDrainPerSecond / OxygenRecoveryPerSecond.
 *
 * No sabe nada de fabricación, del HUD ni de la salud del jugador: expone
 * delegados (oxígeno, jadeo, daño por ahogo, zambullida) para que los
 * sistemas correspondientes reaccionen cuando existan.
 */
UCLASS(ClassGroup = (Explored), meta = (BlueprintSpawnableComponent))
class EXPLORED_API USwimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USwimComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Mantener para bucear hacia abajo; soltar para dejarse subir a flote. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Nado")
	void SetDiveHeld(bool bHeld) { bDiveHeld = bHeld; }

	UFUNCTION(BlueprintPure, Category = "Explored|Nado")
	EWaterState GetWaterState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Explored|Nado")
	bool IsInWater() const { return State != EWaterState::OnLand; }

	/** Oxígeno restante en apnea, 0 (ahogándose) a 1 (pulmones llenos). */
	UFUNCTION(BlueprintPure, Category = "Explored|Nado")
	float GetOxygen01() const { return Oxygen / 100.0f; }

	UFUNCTION(BlueprintPure, Category = "Explored|Nado")
	float GetLungCapacityRatio() const { return LungCapacityRatio; }

	/** Mejora de pulmones (objeto fabricado o hallazgo, GDD §4.1): multiplicador sobre la apnea base. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Nado")
	void SetLungCapacityRatio(float NewRatio) { LungCapacityRatio = FMath::Max(0.1f, NewRatio); }

	/** Fase de brazada en [0, 1): la usa el personaje para animar las manos en el nado. */
	UFUNCTION(BlueprintPure, Category = "Explored|Nado")
	float GetStrokePhase() const { return StrokePhase; }

	/** Balanceo de la ola bajo el jugador (roll/pitch en grados) para inclinar la cámara al nadar en superficie. */
	UFUNCTION(BlueprintPure, Category = "Explored|Nado")
	FRotator GetWaveTilt() const { return WaveTilt; }

	UPROPERTY(BlueprintAssignable, Category = "Explored|Nado")
	FOnOxygenChanged OnOxygenChanged;

	/** Jadeo al emerger tras estar apurado de aire. TODO(audio): confirmar sfx_breath_tired como jadeo definitivo cuando exista voz interna. */
	UPROPERTY(BlueprintAssignable, Category = "Explored|Nado")
	FOnGaspForAir OnGaspForAir;

	/** Daño por segundo mientras los pulmones están vacíos y sigue sumergido. Nadie aplica salud todavía: engancha aquí cuando exista. */
	UPROPERTY(BlueprintAssignable, Category = "Explored|Nado")
	FOnDrowningDamage OnDrowningDamage;

	UPROPERTY(BlueprintAssignable, Category = "Explored|Nado")
	FOnDiveSplash OnDiveSplash;

protected:
	virtual void BeginPlay() override;

private:
	void UpdateWaterState(float DeltaTime);
	void ApplySwimMovement(float DeltaTime);
	void TickBreath(float DeltaTime);
	float GetCarriedWeightRatio() const;
	AExploredOcean* FindOcean() const;
	void PlayOneShot(const TSoftObjectPtr<USoundBase>& SoundRef) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AExploredOcean> Ocean;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCarryComponent> Carry;

	TArray<FOceanStrait> Straits;

	EWaterState State = EWaterState::OnLand;
	bool bDiveHeld = false;
	bool bWasSubmergedLastTick = false;
	bool bIsExertingUnderwater = false;
	float Oxygen = 100.0f;
	float LungCapacityRatio = 1.0f;
	float StrokePhase = 0.0f;
	float BubbleTimer = 2.0f;
	float TimeSinceOceanSearch = 0.0f;
	FRotator WaveTilt = FRotator::ZeroRotator;

	/** Bajo el umbral de oxígeno en el momento de emerger, jadea (GDD §4.1). */
	static constexpr float GaspThreshold = 35.0f;
	/** Daño por segundo ahogándose sin aire (evento OnDrowningDamage). */
	static constexpr float DrowningDamagePerSecond = 6.0f;

	/** Profundidad de agua (cm) sobre los pies a partir de la cual hay que nadar de verdad. */
	UPROPERTY(EditAnywhere, Category = "Explored|Nado")
	float SwimDepthCm = 130.0f;

	UPROPERTY(EditAnywhere, Category = "Explored|Nado")
	float SwimSpeed = 260.0f;

	UPROPERTY(EditAnywhere, Category = "Explored|Nado")
	float DiveSpeed = 220.0f;

	/** Peso (kg) a partir del cual cargar de más empieza a cansar y hundir al nadar. */
	UPROPERTY(EditAnywhere, Category = "Explored|Nado")
	float ComfortableWeightKg = 15.0f;

	/** Brazadas por segundo a velocidad máxima. */
	UPROPERTY(EditAnywhere, Category = "Explored|Nado")
	float StrokeFrequency = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Explored|Nado|Sonido")
	TArray<TSoftObjectPtr<USoundBase>> SwimStrokeSounds;

	UPROPERTY(EditAnywhere, Category = "Explored|Nado|Sonido")
	TArray<TSoftObjectPtr<USoundBase>> BubbleSounds;

	UPROPERTY(EditAnywhere, Category = "Explored|Nado|Sonido")
	TSoftObjectPtr<USoundBase> DiveSplashSound;

	UPROPERTY(EditAnywhere, Category = "Explored|Nado|Sonido")
	TSoftObjectPtr<USoundBase> GaspSound;
};
