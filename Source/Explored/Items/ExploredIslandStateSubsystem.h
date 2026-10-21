#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Items/BackpackModel.h"
#include "Narrative/SupplyCrateModel.h"
#include "Weather/RainBarrelModel.h"

#include "ExploredIslandStateSubsystem.generated.h"

class AExploredPickup;
class FSaveArchive;
class UExploredSaveSubsystem;

/** Objeto soltado en el suelo durante la partida (un tronco que se deja en el sitio de la balsa...). */
struct EXPLORED_API FIslandDroppedItem
{
	FName ItemId;
	int32 Count = 1;
	FVector Location = FVector::ZeroVector;
};

/**
 * Estado de la Isla de Sal que se guarda (GDD v3 §5.3 y §5.10): ids de island_props.json ya
 * recogidos (no reaparecen), cocos que quedan en cada palma, cargas de cada barril, la caja de
 * suministros, objetos soltados y la mochila de Iria. Sección de guardado «island».
 *
 * Los actores (AExploredPickup, AExploredCoconutPalm, AExploredRainBarrel, AExploredSupplyCrate)
 * leen de aquí al empezar y escriben al cambiar; al cargar una partida, ApplyToWorld los ajusta.
 */
UCLASS()
class EXPLORED_API UExploredIslandStateSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	static UExploredIslandStateSubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Explored|Isla")
	bool IsCollected(FName PropId) const { return Collected.Contains(PropId); }

	UFUNCTION(BlueprintCallable, Category = "Explored|Isla")
	void MarkCollected(FName PropId);

	UFUNCTION(BlueprintPure, Category = "Explored|Isla")
	int32 GetCollectedCount() const { return Collected.Num(); }

	/** Cocos que quedan en la palma (Default si aún no se ha tocado). */
	int32 GetPalmCoconuts(FName PalmId, int32 Default) const;
	void SetPalmCoconuts(FName PalmId, int32 Coconuts);

	FRainBarrelState GetBarrel(FName BarrelId) const { return Barrels.FindRef(BarrelId); }
	void SetBarrel(FName BarrelId, const FRainBarrelState& State) { Barrels.Add(BarrelId, State); }

	FSupplyCrateState GetCrate(FName CrateId) const { return Crates.FindRef(CrateId); }
	void SetCrate(FName CrateId, const FSupplyCrateState& State) { Crates.Add(CrateId, State); }

	/** Crea un AExploredPickup suelto y lo apunta para el guardado. */
	AExploredPickup* SpawnDropped(FName ItemId, int32 Count, const FVector& Location);
	/** Quita de la lista un objeto soltado que se ha vuelto a coger. */
	void ForgetDropped(const AExploredPickup* Pickup);

	/** Ajusta los actores del mundo al estado (tras cargar o al poblar la isla). */
	void ApplyToWorld();

	void Save(FSaveArchive& Ar) const;
	void Load(const FSaveArchive& Ar);

private:
	void RegisterSection();
	UExploredSaveSubsystem* FindSave() const;

	TSet<FName> Collected;
	TMap<FName, int32> PalmCoconuts;
	TMap<FName, FRainBarrelState> Barrels;
	TMap<FName, FSupplyCrateState> Crates;
	TArray<FIslandDroppedItem> Dropped;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AExploredPickup>> DroppedActors;

	/** Mochila leída de la partida, a la espera de que exista el personaje. */
	TOptional<FBackpackState> PendingBackpack;
	bool bSectionRegistered = false;

public:
	/** La recoge UExploredSurvivorComponent al empezar (si no hay, usa el kit inicial). */
	TOptional<FBackpackState> TakePendingBackpack();
};
