#pragma once

#include "CoreMinimal.h"

/**
 * Cómo está el nadador respecto al agua (GDD §4.1). Espejo puro de
 * EWaterState (SwimComponent.h), que es el UENUM que ve Blueprint.
 */
enum class ESwimState : uint8
{
	OnLand,
	Wading,   // Agua hasta las rodillas o la cintura; sigue de pie.
	Swimming, // A flote, en la superficie.
	Diving    // Buceando: tecla de bucear mantenida o con la cabeza bajo el agua.
};

/**
 * Ajustes del nado. USwimComponent los expone como UPROPERTY y los copia
 * aquí en cada tick; el modelo no guarda ninguno.
 */
struct EXPLORED_API FSwimTuning
{
	/** Cobertura de agua (cm sobre los pies) a partir de la cual se deja de hacer pie y se nada. */
	float SwimDepthCm = 130.0f;

	/**
	 * Histéresis de salida (H5): ya nadando, solo se vuelve a hacer pie cuando la
	 * cobertura baja de SwimDepthCm - SwimExitHysteresisCm. Sin ella, el punto de
	 * equilibrio de la flotación coincide con el umbral y cada valle de ola
	 * sacaba al nadador del agua.
	 */
	float SwimExitHysteresisCm = 50.0f;

	/** Profundidad (cm) a la que flota el centro de la cápsula bajo la superficie, con la cabeza fuera. */
	float FloatCenterDepthCm = 40.0f;

	/** Altura (cm) de la cabeza sobre el centro de la cápsula; coincide con la cámara. */
	float HeadHeightCm = 70.0f;

	/**
	 * Histéresis de la cabeza: se sumerge en cuanto el agua la cubre, pero solo se
	 * considera fuera cuando asoma esta altura (cm) sobre la superficie.
	 */
	float HeadSurfaceHysteresisCm = 10.0f;

	float SwimSpeed = 260.0f;
	float DiveSpeed = 220.0f;

	/** Velocidad (cm/s) con la que el pulmón sube al nadador sumergido al soltar la tecla de bucear. */
	float BuoyancyRiseSpeedCm = 60.0f;

	/** Frenado del movimiento en vuelo mientras se nada (cm/s²). */
	float SwimBrakingDeceleration = 900.0f;

	/** Peso (kg) a partir del cual cargar de más empieza a cansar y hundir al nadar. */
	float ComfortableWeightKg = 15.0f;

	/** Brazadas por segundo a velocidad máxima. */
	float StrokeFrequency = 0.9f;

	/** Bajo este oxígeno (0–100) en el momento de sacar la cabeza, jadea (GDD §4.1). */
	float GaspThreshold = 35.0f;

	/** Daño por segundo ahogándose sin aire (evento de daño por ahogo). */
	float DrowningDamagePerSecond = 6.0f;
};

/** Lo que el modelo necesita saber del mundo en un tick. Todo en cm y cm/s. */
struct EXPLORED_API FSwimInputs
{
	/** Falso si no hay océano o cápsula: el nadador está en tierra a todos los efectos. */
	bool bHasWater = false;

	/** Altura de la superficie del agua bajo el jugador (con oleaje y marea). */
	float WaterZ = 0.0f;

	/** Centro de la cápsula y su semialtura. */
	float CenterZ = 0.0f;
	float HalfHeight = 90.0f;

	/** Velocidad actual del movimiento; el modelo solo reescribe la componente vertical. */
	FVector Velocity = FVector::ZeroVector;

	/** Corriente marina en el punto (cm/s, FOceanCurrents::CurrentAt). */
	FVector2D CurrentCmPerSecond = FVector2D::ZeroVector;

	bool bDiveHeld = false;
	float CarriedWeightKg = 0.0f;
	float LungCapacityRatio = 1.0f;
};

/** Resultado de un tick: lo que la capa UE tiene que aplicar y los sucesos que debe emitir. */
struct EXPLORED_API FSwimStep
{
	/** Ha pasado de hacer pie a nadar en este tick: zambullida (sonido y evento, una sola vez). */
	bool bEnteredWater = false;

	/** Ha dejado de nadar en este tick: volver a caminar/caer con gravedad. */
	bool bLeftWater = false;

	/** Nadando o buceando: la capa UE debe aplicar Velocity, MaxSpeed y CurrentOffset. */
	bool bSwimming = false;

	/** Velocidad deseada para el movimiento (solo cambia Z respecto a la de entrada). */
	FVector Velocity = FVector::ZeroVector;

	/** Velocidad máxima de vuelo con la fatiga de la carga aplicada. */
	float MaxSpeed = 0.0f;

	/** Frenado a aplicar al movimiento en vuelo. */
	float BrakingDeceleration = 0.0f;

	/**
	 * Desplazamiento (cm) que la corriente arrastra en este tick (M4). Es la corriente
	 * en cm/s por DeltaTime: se aplica como desplazamiento del actor y no como
	 * aceleración, para que el frenado del movimiento no la anule.
	 */
	FVector2D CurrentOffset = FVector2D::ZeroVector;

	/** Se ha completado una brazada (la fase ha dado la vuelta). */
	bool bStrokeCompleted = false;

	/** El oxígeno ha cambiado en este tick. */
	bool bOxygenChanged = false;

	/** Ha sacado la cabeza apurado de aire. */
	bool bGasp = false;

	/** Daño por segundo por ahogo en este tick (0 si respira o le queda aire). */
	float DrowningDamagePerSecond = 0.0f;
};

/**
 * Máquina de estados del nado y apnea, sin UObject. USwimComponent le pasa
 * la altura del agua, la cápsula y la velocidad, y aplica lo que devuelve.
 *
 * - Entrar/salir del agua lleva histéresis (H5).
 * - La apnea depende de si la cabeza está bajo el agua, no de la tecla (H4).
 * - El oxígeno se acumula en doble precisión: a FPS muy altos el paso por
 *   fotograma es diminuto y no se pierde (M1).
 * - La corriente se devuelve como desplazamiento (M4).
 */
class EXPLORED_API FSwimModel
{
public:
	/** Avanza un fotograma. DeltaTime en segundos reales. */
	FSwimStep Tick(const FSwimTuning& Tuning, const FSwimInputs& In, float DeltaTime);

	ESwimState GetState() const { return State; }
	bool IsSwimming() const { return State == ESwimState::Swimming || State == ESwimState::Diving; }
	bool IsHeadUnderwater() const { return bHeadUnderwater; }
	bool IsExertingUnderwater() const { return bExerting; }

	/** Oxígeno restante (0–100). */
	float GetOxygen() const { return static_cast<float>(Oxygen); }
	float GetOxygen01() const { return static_cast<float>(Oxygen / 100.0); }
	void SetOxygen(float NewOxygen) { Oxygen = FMath::Clamp(static_cast<double>(NewOxygen), 0.0, 100.0); }

	/** Fase de brazada en [0, 1). */
	float GetStrokePhase() const { return StrokePhase; }

	/** Cobertura de agua (cm sobre los pies). */
	static float CoverageCm(const FSwimInputs& In);

	/** Profundidad (cm) de la cabeza bajo la superficie; negativa si asoma. */
	static float HeadDepthCm(const FSwimTuning& Tuning, const FSwimInputs& In);

	/**
	 * Estado siguiente respecto al agua, con histéresis de entrada/salida.
	 * bHeadUnderwater es el estado de la cabeza ya actualizado.
	 */
	static ESwimState NextState(const FSwimTuning& Tuning, ESwimState Previous, float Coverage, bool bDiveHeld,
		bool bHeadUnderwater);

private:
	void TickMovement(const FSwimTuning& Tuning, const FSwimInputs& In, float DeltaTime, FSwimStep& Out);
	void TickBreath(const FSwimTuning& Tuning, const FSwimInputs& In, float DeltaTime, bool bWasHeadUnderwater,
		FSwimStep& Out);

	ESwimState State = ESwimState::OnLand;
	bool bHeadUnderwater = false;
	bool bExerting = false;
	double Oxygen = 100.0;
	float StrokePhase = 0.0f;
};
