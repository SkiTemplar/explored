#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"

#include "Survival/IslandBodyModel.h"

#include "IslandBodyComponent.generated.h"

class AExploredCharacter;
class AExploredShelterVolume;

/**
 * Cuerpo de Iria en la partida (GDD v3 §5.1): capa fina sobre FIslandBodyModel (desgaste,
 * avisos, velocidad, desaturación) y FBodyEventsModel (desmayo y sueño). Lee del mundo la hora,
 * el refugio (AExploredShelterVolume), las hogueras a 4 m y la lluvia; reparte el resultado a la
 * velocidad del personaje, al postproceso (UBodySignalsComponent) y a los subtítulos del HUD.
 *
 * Desmayo: una barra a 0 durante 20 s reales → fundido, +3 h, despierta en el último refugio
 * usado (por defecto, Cala Varada; si estaba en el mar, en Playa Norte) con 40/40/40/60.
 * Dormir: solo en refugio entre 20:00 y 05:00; salta a las 06:00 con deltas fijos.
 *
 * El desgaste sigue al reloj: con BalizaTimeScale x también va x veces más rápido; la cuenta del
 * desmayo es siempre en segundos reales.
 *
 * Blueprint: «Get Bar», «Try Sleep», «Can Sleep Now», «Is Fainting».
 */
UCLASS(ClassGroup = (Explored), meta = (BlueprintSpawnableComponent))
class EXPLORED_API UIslandBodyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UIslandBodyComponent();

	static UIslandBodyComponent* FindOn(const AActor* Actor);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	const FBodyBars& GetBars() const { return Bars; }
	const FIslandBodyTuning& GetTuning() const { return Tuning; }

	/** Valor 0-100 de una barra (0 sed, 1 hambre, 2 energía, 3 calor). */
	UFUNCTION(BlueprintPure, Category = "Explored|Cuerpo")
	float GetBar(int32 BarIndex) const;

	/** Comer o beber (lo llama UExploredSurvivorComponent). */
	void ApplyDelta(float Thirst, float Hunger, float Energy);

	/** Barras de una partida cargada o de depuración (se limitan a [0, 100]). */
	void RestoreBars(const FBodyBars& InBars, FName InLastShelter);
	FName GetLastShelterId() const { return LastShelterId; }

	/** «Nueva partida»: barras iniciales de survival_needs.json y refugio por defecto. */
	void ResetForNewRun();

	/** Está dentro de un ShelterVolume. */
	UFUNCTION(BlueprintPure, Category = "Explored|Cuerpo")
	bool IsSheltered() const { return bSheltered; }

	/** Se puede dormir ahora (refugio y hora). */
	UFUNCTION(BlueprintPure, Category = "Explored|Cuerpo")
	bool CanSleepNow() const;

	/** Es hora de dormir y no hay ciclón (dentro o fuera de un refugio); lo usa la pista de dormir. */
	bool IsSleepHour() const;

	/** Duerme hasta las 06:00 si se puede; si no, avisa por qué. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Cuerpo")
	bool TrySleep();

	/** Fundido en curso (desmayo, sueño o cruce forzoso): no se mueve ni interactúa. */
	UFUNCTION(BlueprintPure, Category = "Explored|Cuerpo")
	bool IsFainting() const { return bInFade; }

	/** Desmayo inmediato (depuración y pruebas de humo). */
	void ForceFaint();

	/**
	 * Fundido a negro, OnBlack a oscuras (teletransporte, reloj) y vuelta. Text se enseña como
	 * cartel al volver. Lo usan el desmayo, el sueño y el cruce forzoso de la balsa.
	 */
	void PlayBlackout(TFunction<void()> OnBlack, const FText& Text);

	/** Velocidad: 0,8 con una barra bajo 25 (GDD §5.1). */
	float GetSpeedFactor() const { return FIslandBodyModel::SpeedFactor(Bars, Tuning); }

	/** Cuántas veces se ha desmayado y dormido en esta sesión (pruebas). */
	int32 GetFaintCount() const { return FaintCount; }
	int32 GetSleepCount() const { return SleepCount; }

	/** Segundos reales de la fase a oscuras de un fundido. */
	UPROPERTY(EditAnywhere, Category = "Explored|Cuerpo")
	float FadeSeconds = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Explored|Cuerpo")
	float BlackSeconds = 1.5f;

	/** Radio del calor de una hoguera (cm): GDD §5.2, 4 m. */
	UPROPERTY(EditAnywhere, Category = "Explored|Cuerpo")
	float FireRadiusCm = 400.0f;

private:
	void Step(float RealSeconds);
	FBodyEnvironment MeasureEnvironment();
	void ShowWarning(EBodyBar Bar) const;
	void ShowLine(const FText& Text) const;
	void BeginFaint();
	void ApplyBodyOutputs();
	/** Dónde despertar tras un desmayo (cm) y con qué rumbo. */
	FVector FaintRespawnLocation(bool bAtSea) const;
	bool IsAtSea() const;
	double GetAbsHours() const;
	/** El ciclón está en curso (muros u ojo): no se duerme. */
	bool IsCycloneNow() const;
	void SetAbsHours(double AbsHours) const;
	void FinishFade();

	FIslandBodyTuning Tuning;
	FBodyBars Bars;
	FBodyWarnings Warnings;
	FFaintTracker FaintTracker;
	FName LastShelterId;
	float StepAccumulator = 0.0f;
	float LastSpeedFactor = 1.0f;
	bool bSheltered = false;
	bool bInFade = false;
	bool bRestored = false;
	int32 FaintCount = 0;
	int32 SleepCount = 0;

	FTimerHandle FadeTimer;
	FTimerHandle FadeBackTimer;
	TFunction<void()> PendingOnBlack;
	FText PendingText;
};
