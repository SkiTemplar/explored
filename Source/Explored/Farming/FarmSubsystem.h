#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/UniquePtr.h"

#include "Farming/FarmModel.h"

#include "FarmSubsystem.generated.h"

class UTimeOfDaySubsystem;

/** Una parcela cambió (acción del jugador o cierre de día). INDEX_NONE = todas. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFarmPlotChanged, int32 /*PlotId*/);

/**
 * Huerto del mundo: carga Content/Data/plants.json, guarda el FFarmModel y lo
 * hace avanzar un día cada vez que UTimeOfDaySubsystem cambia de día, con la
 * lluvia del día que termina (planificador del clima y lluvia real medida en
 * juego, la mayor de las dos). AExploredPlantActor registra aquí su parcela y
 * lee la etapa que debe mostrar.
 */
UCLASS()
class EXPLORED_API UFarmSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Vuelve a leer plants.json y conserva el estado del huerto. */
	bool ReloadFromDisk();

	/** Parsea plants.json. Estático para que los tests validen el fichero del proyecto. */
	static bool ParsePlantsJson(const FString& JsonText, TArray<FPlantDef>& OutPlants, FString& OutError);

	// --- Parcelas y acciones (el día lo pone el reloj del mundo) ---------------------

	int32 RegisterPlot(const FVector& Location, const TArray<FName>& Pieces);
	EFarmResult UnregisterPlot(int32 PlotId);
	void AddScarecrow(const FVector& Location);
	void RemoveScarecrow(const FVector& Location);

	/** Cultivo que plantaría ese objeto en la parcela hoy (NAME_None si ninguno). */
	FName PlantForItem(int32 PlotId, FName Item) const;
	EFarmResult Plant(int32 PlotId, FName Item);
	EFarmResult Water(int32 PlotId, float Waterings = 1.0f);
	EFarmResult ApplyCompost(int32 PlotId);
	EFarmResult Harvest(int32 PlotId, FFarmHarvest& Out);
	EFarmResult ClearPlot(int32 PlotId);

	FFarmStageView GetStageView(int32 PlotId) const;
	const FPlantDef* FindPlant(FName PlantId) const;
	int32 GetCurrentDay() const;

	/** Estado plano para P-SAVE. */
	FFarmState GetSaveState() const;
	void LoadSaveState(const FFarmState& InState);

	FOnFarmPlotChanged OnPlotChanged;

private:
	void HandleNewDay(int32 NewDay);
	/** Cierra en el modelo todos los días anteriores a CurrentDay que falten. */
	void CatchUpTo(int32 CurrentDay);

	TUniquePtr<FFarmModel> Model;

	/** Lluvia real medida en el día en curso (horas de lluvia plena) y a qué día pertenece. */
	float LiveRainHours = 0.0f;
	int32 LiveRainDay = INDEX_NONE;
	float LastTotalDays = -1.0f;

	FDelegateHandle NewDayHandle;
	TWeakObjectPtr<UTimeOfDaySubsystem> TimeOfDay;
};
