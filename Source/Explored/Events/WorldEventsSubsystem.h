#pragma once

#include "CoreMinimal.h"
#include "Events/WorldEventsModel.h"
#include "Sky/MoonModel.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/UniquePtr.h"

#include "WorldEventsSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldEventStarted, const FWorldEvent& /*Event*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldEventEnded, const FWorldEvent& /*Event*/);

/**
 * Eventos del mundo vivo (GDD §9.3). Lee la hora del UTimeOfDaySubsystem,
 * consulta FWorldEventsModel y avisa del inicio y el fin de cada evento
 * (tortugas, ballenas, barco, erupción, marea viva extrema...). Toda la
 * lógica está en el modelo puro; aquí solo se sondea y se reparte.
 *
 * La Luna es la de FMoonModel, la misma que usan el cielo
 * (ExploredSky::MoonPhase) y las mareas (FOceanTide::SpringNeapFactorAt).
 */
UCLASS()
class EXPLORED_API UWorldEventsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Eventos activos en el último sondeo, ordenados por inicio. */
	const TArray<FWorldEvent>& GetActiveEvents() const { return Active; }

	/** ¿Hay un evento de ese tipo activo ahora? Si OutEvent no es nulo, lo copia. */
	bool IsEventActive(EWorldEventType Type, FWorldEvent* OutEvent = nullptr) const;

	/** Siguiente aparición de un tipo a partir de ahora (UI, diario, audio). */
	bool GetNextOccurrence(EWorldEventType Type, FWorldEvent& OutEvent) const;

	/** Eventos que se solapan con [FromDays, ToDays) en días totales de juego. */
	TArray<FWorldEvent> GetEventsInWindow(float FromDays, float ToDays) const;

	/** Luna actual (FMoonModel): fase en [0, 1), fase con nombre e iluminación. */
	float GetMoonPhase() const;
	EMoonPhase GetNamedMoonPhase() const;
	float GetMoonIllumination() const;

	/** Brillo del mar en [0, 1] por la Luna y la noche (para el material del océano). */
	float GetBioluminescence() const;

	/** Retirada extra de la marea en [0, 1] por una marea viva extrema activa (para el océano). */
	float GetExtremeTideDrawdown() const;

	/** Temblor y ceniza de la erupción activa (cero si no hay). */
	FEruptionSample GetEruption() const;

	/**
	 * La hoguera de señal llama aquí mientras está encendida: si hay un barco a
	 * la vista y aún no soltó su paquete, lo marca como soltado y devuelve true
	 * con el barco en OutShip (para crear el paquete a la deriva en su rumbo).
	 */
	bool TryDropShipPackage(bool bSignalFireLit, FWorldEvent& OutShip);

	/** Al terminar una erupción: true una sola vez para crear la obsidiana nueva. */
	bool TryDepositObsidian(const FWorldEvent& Eruption);

	/** Estado que se guarda (solo resultados de un solo uso ya consumidos). */
	const FWorldEventsState& GetSaveState() const { return State; }
	void LoadSaveState(const FWorldEventsState& InState);

	const FWorldEventsModel* GetModel() const { return Model.Get(); }

	FOnWorldEventStarted OnEventStarted;
	FOnWorldEventEnded OnEventEnded;

private:
	/** Días totales de juego actuales (0 si aún no hay reloj). */
	float GetTotalDays() const;

	/** Compara los eventos activos con el sondeo anterior y avisa de inicios y finales. */
	void Poll(float TotalDays);

	TUniquePtr<FWorldEventsModel> Model;
	FWorldEventsState State;
	TArray<FWorldEvent> Active;
	float LastPolledDays = 0.0f;
	bool bHasPolled = false;
	float PollTimer = 0.0f;
};
