#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Building/BuildingModel.h"
#include "Building/BuildingTypes.h"

#include "BuildingSubsystem.generated.h"

class AExploredBuildingPiece;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBuildingPiecesChanged, const FBuildingChangeResult& /*Removed*/);

/**
 * Construcción en el mundo (GDD §8.6). Carga Content/Data/building_pieces.json,
 * es dueño de FBuildingModel (toda la lógica) y mantiene un AExploredBuildingPiece
 * por pieza. Cada pocos segundos aplica al modelo el tiempo de juego transcurrido
 * con la lluvia y los temporales de UExploredWeatherSubsystem, y retira los
 * actores de lo que se rompe o se derrumba.
 *
 * Registra la sección «building» de la partida y avisa a los logros de cada
 * pieza construida. Los materiales los descuenta UBuildPreviewComponent del
 * inventario (UCarryComponent::ConsumeMaterials). Pendiente: gastar los minutos
 * de trabajo.
 */
UCLASS()
class EXPLORED_API UBuildingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Vuelve a leer building_pieces.json (conserva las piezas cuya definición siga existiendo). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Construcción")
	bool ReloadFromDisk();

	/** Parseo sin mundo, para los tests del editor que validan el JSON del proyecto. */
	static bool ParseBuildingJson(const FString& JsonText, FBuildingCatalog& OutCatalog, FString& OutError);

	/** Texto para el HUD de un motivo de rechazo. */
	static FText GetReasonText(EBuildFailReason Reason);

	FBuildingModel* GetModel() { return Model.Get(); }
	const FBuildingModel* GetModel() const { return Model.Get(); }

	/**
	 * Previsualización: encaja la pieza en la rejilla de la base cercana y dice si
	 * se podría colocar (sin mirar el inventario). Si no hay base cerca, la pieza
	 * fundaría una nueva en AimPoint. OutLocation/OutYaw son el pivote de la malla.
	 */
	EBuildFailReason PreviewPlacement(FName DefId, const FVector& AimPoint, int32 Rotation, bool bGroundContact,
		FVector& OutLocation, float& OutYawDegrees) const;

	/** Coloca la pieza (crea la base si hace falta) y su actor. INDEX_NONE si no se pudo. */
	int32 TryPlacePiece(FName DefId, const FVector& AimPoint, int32 Rotation, bool bGroundContact,
		TMap<FName, int32>& Inventory, const TSet<FName>& HeldTools, EBuildFailReason& OutReason);

	/** Desmonta una pieza y derrumba lo que dependía de ella. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Construcción")
	void RemovePiece(int32 PieceId);

	UFUNCTION(BlueprintCallable, Category = "Explored|Construcción")
	bool SetFireLit(int32 PieceId, bool bLit);

	/** Fogata encendida más cercana (GDD §8.6). False si no hay ninguna. */
	UFUNCTION(BlueprintPure, Category = "Explored|Construcción")
	bool FindRespawnPoint(const FVector& From, FVector& OutLocation) const;

	AExploredBuildingPiece* FindActor(int32 PieceId) const;

	/** Estado para P-SAVE. */
	FBuildingSaveState GetSaveState() const;
	/** Restaura un guardado y vuelve a crear todos los actores. */
	void RestoreSaveState(const FBuildingSaveState& State);

	/** Se emite cuando se quitan piezas (rotas, derrumbadas o desmontadas). */
	FOnBuildingPiecesChanged OnPiecesRemoved;

	/**
	 * Categoría de un ciclón forzado a mano (ForceState, depuración): los ciclones
	 * planificados traen la suya de FWeatherModel::CycloneCategoryAt.
	 */
	UPROPERTY(EditAnywhere, Category = "Explored|Construcción")
	float CycloneCategory = 2.0f;

	/** Cada cuánto (s reales) se aplica el desgaste y los temporales. */
	UPROPERTY(EditAnywhere, Category = "Explored|Construcción")
	float SimulationIntervalSeconds = 2.0f;

private:
	void SpawnActorFor(const FBuildingPieceState& Piece);
	void DestroyActors(const TArray<int32>& PieceIds);
	void DestroyAllActors();
	void HandleChange(const FBuildingChangeResult& Change);
	FBuildingWeather SampleWeather() const;
	float GetGameDays() const;

	TUniquePtr<FBuildingModel> Model;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<AExploredBuildingPiece>> Actors;

	float LastSimulatedDays = -1.0f;
	float SimulationTimer = 0.0f;
};
