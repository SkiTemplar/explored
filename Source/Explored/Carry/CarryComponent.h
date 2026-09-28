#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Building/BuildingTypes.h"
#include "Carry/CarryTypes.h"
#include "Carry/InventoryModel.h"
#include "Items/ItemTypes.h"

#include "CarryComponent.generated.h"

class AExploredContainer;
class AExploredItemActor;
class AExploredSledge;
class UItemRegistrySubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCarryChanged);
/** Se dispara al recoger algo del mundo con éxito, para el aviso flotante del HUD. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemPickedUp, FText, DisplayName);

/**
 * Manos, bolsillos, cinturón, bolsa estanca, mochila y angarillas del jugador
 * (GDD §8.2, biblia §3.8). Las reglas de capacidad y peso están en el modelo
 * puro FInventoryModel; este componente solo traduce FItemInstance a los
 * registros del modelo (FInventoryItem), guarda aparte la instancia completa
 * (calidad, piezas, nombre) por id de instancia y conecta con el mundo
 * (actores que se sueltan, contenedores, angarillas).
 *
 * No sabe nada de fabricación: UCraftingLibrary y UInteractionComponent son
 * quienes lo usan.
 */
UCLASS(ClassGroup = (Explored), meta = (BlueprintSpawnableComponent))
class EXPLORED_API UCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCarryComponent();

	/** Va a la primera mano libre; si el objeto es DosManos, exige las dos vacías. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool TryPickUp(AExploredItemActor* ItemActor, FText& OutFailReason);

	/** Genera el actor en el mundo, delante del jugador. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool Drop(EHand Hand, FText& OutFailReason);

	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool StoreFromHand(EHand Hand, ECarrySlot Slot, FText& OutFailReason);

	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool TakeToHand(ECarrySlot Slot, int32 Index, EHand Hand, FText& OutFailReason);

	/** Guarda lo de una mano donde propone FInventoryModel::SuggestStowSlot. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool AutoStowFromHand(EHand Hand, ECarrySlot& OutSlot, FText& OutFailReason);

	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool SwapHands();

	/**
	 * Pasa Count unidades de la pila de una mano a la otra, que tiene que estar
	 * libre (biblia 03 §1.3). La parte nueva conserva la instancia (calidad...).
	 */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool SplitFromHand(EHand Hand, int32 Count, FText& OutFailReason);

	/** Junta la pila de la mano derecha con la de la izquierda (hasta 10); lo que sobra se queda en la derecha. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool MergeHands(FText& OutFailReason);

	/**
	 * Gasta una unidad del objeto de una mano (plantar una semilla, comer): si
	 * era la última, la mano queda vacía. Un objeto DosManos vacía las dos.
	 */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool ConsumeOneFromHand(EHand Hand);

	/**
	 * Saca una unidad de lo que hay en una mano sin soltarla al mundo (echar
	 * leña al fuego, gastar una cerilla, meter algo en la olla). Si el objeto
	 * es apilable y hay varias, la mano se queda con el resto. No vale para
	 * objetos DosManos.
	 */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool TakeOneFromHand(EHand Hand, FItemInstance& OutTaken);

	/**
	 * Vacía las dos manos (las piezas ya se consumieron al fabricar) y coloca
	 * el resultado en la mano libre, o en las dos si es DosManos. Lo usa
	 * AExploredCharacter::HandleCombine tras UCraftingLibrary::Apply.
	 */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool ReplaceHandsWithCraftResult(const FItemInstance& Result, FText& OutFailReason);

	// ----------------------------------------------------------------- equipo

	/** Se pone lo que hay en esa mano: mochila, cinturón de cuero o angarillas. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool EquipFromHand(EHand Hand, FText& OutFailReason);

	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool UnequipBackpack(EHand Hand, FText& OutFailReason);

	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool UnequipBelt(EHand Hand, FText& OutFailReason);

	/** Engancha unas angarillas que estaban en el suelo, con su carga. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool AttachSledge(AExploredSledge* Sledge, FText& OutFailReason);

	/** Suelta las angarillas detrás del jugador con toda su carga. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool DetachSledge();

	/** Lo llama el personaje al empezar a nadar: las angarillas se quedan en la orilla. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	void HandleEnterWater();

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool HasSledge() const { return Model.HasSledge(); }

	// ------------------------------------------------- contenedores del mundo

	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool StoreInContainer(EHand Hand, AExploredContainer* Container, FText& OutFailReason);

	/** SlotIndex es el hueco visible del contenedor (FInventoryEntry::SlotIndex). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	bool TakeFromContainer(AExploredContainer* Container, int32 SlotIndex, EHand Hand, FText& OutFailReason);

	// ------------------------------------------------------------ consultas

	/** Peso de lo que se lleva encima (sin las angarillas). */
	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	float GetTotalWeight() const;

	/** Peso / capacidad cómoda; alimenta FSurvivalInputs::CarriedWeightRatio. */
	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	float GetCarriedWeightRatio() const { return Model.GetCarriedWeightRatio(); }

	/** Solo lo que va encima: lo que cansa al nadar. */
	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	float GetSwimLoadRatio() const { return Model.GetSwimLoadRatio(); }

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	float GetNoiseLevel() const { return Model.GetNoiseLevel(); }

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	float GetMoveSpeedMultiplier() const { return Model.GetMoveSpeedMultiplier(); }

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool CanSwim() const { return Model.CanSwim(); }

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool CanClimb() const { return Model.CanClimb(); }

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool HasItemWithTag(FName Tag) const { return Model.HasItemWithTag(Tag); }

	/** Brújula en la mano, el bolsillo o el cinturón (trazo de costa corregido, GDD §5.5). */
	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool HasCompassAtHand() const { return Model.HasCompassAtHand(); }

	/** Todo lo que lleva esa etiqueta (p. ej. «mapa») va en la bolsa estanca. */
	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool AreTaggedItemsDry(FName Tag) const { return Model.AreTaggedItemsDry(Tag); }

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	float GetCarriedWaterLiters() const { return Model.GetCarriedWaterLiters(); }

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool GetHandItem(EHand Hand, FItemInstance& OutItem) const;

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool IsHandEmpty(EHand Hand) const;

	/** Mochila sin objeto con capacidad fija (tests y ajustes de diseño). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Carga")
	void SetBackpack(bool bInHasBackpack, float CapacityVolumeLiters, float CapacityWeightKg);

	UFUNCTION(BlueprintPure, Category = "Explored|Carga")
	bool HasBackpack() const { return Model.HasBackpack(); }

	/** Directo, sin comprobar registro: lo usan las manos-en-pantalla y el HUD. */
	const FItemInstance* GetHandItemPtr(EHand Hand) const;

	/** Las dos manos sostienen el mismo objeto DosManos (no son dos piezas distintas). */
	bool IsHoldingTwoHandedItem() const { return Model.IsHoldingTwoHanded(); }

	const TArray<FItemInstance>& GetPocketItems() const { return Pockets; }
	const TArray<FItemInstance>& GetBeltItems() const { return Belt; }
	const TArray<FItemInstance>& GetBackpackItems() const { return BackpackItems; }
	const TArray<FItemInstance>& GetPouchItems() const { return PouchItems; }
	const TArray<FItemInstance>& GetSledgeItems() const { return SledgeItems; }

	/** Reglas y estado del inventario, para la interfaz y el guardado. */
	const FInventoryModel& GetInventoryModel() const { return Model; }

	/** Instancia completa de un objeto que lleva el jugador, por su id del modelo. */
	const FItemInstance* FindInstance(int64 InstanceId) const { return Payloads.Find(InstanceId); }

	// ------------------------------------------------ materiales (construcción)

	/**
	 * Unidades de cada definición que se llevan encima (manos, bolsillos, cinturón,
	 * bolsa, mochila y angarillas) y las definiciones presentes como herramientas.
	 */
	void CountMaterials(TMap<FName, int32>& OutCounts, TSet<FName>& OutTools) const;

	/**
	 * Gasta materiales (P-WIRE): primero de las angarillas, luego de la mochila y
	 * los contenedores del cuerpo y al final de las manos (ExploredLinks::PlanMaterialTakes).
	 * Todo o nada: false si no alcanza, sin tocar nada.
	 */
	bool ConsumeMaterials(const TArray<FBuildingCost>& Costs);

	/** Guardado (P-SAVE): estado plano del modelo y las instancias completas por id. */
	void ExportState(FInventoryState& OutState, TMap<int64, FItemInstance>& OutInstances) const;
	/**
	 * Carga una partida. Antes quita lo que ya no existe en items.json y suelta a
	 * los pies lo que se queda sin sitio (FInventoryModel::SanitizeUnknownDefinitions),
	 * y pasa a unidades las pilas guardadas antes de que existieran (biblia 03 §1.3).
	 */
	bool ImportState(const FInventoryState& InState, const TMap<int64, FItemInstance>& InInstances);

	/** Registro plano del modelo para una instancia (peso, volumen, tamaño y etiquetas del registro). */
	static FInventoryItem MakeRecord(const FItemInstance& Instance, int64 InstanceId, const UItemRegistrySubsystem* Registry);

	/** Texto para el jugador de un motivo de fallo del modelo. */
	static FText FailToText(EInventoryFail Fail, EInventorySlot Target);

	UPROPERTY(BlueprintAssignable, Category = "Explored|Carga")
	FOnCarryChanged OnCarryChanged;

	/** Recogida del mundo con éxito (no se dispara al mover cosas entre manos y bolsillos). */
	UPROPERTY(BlueprintAssignable, Category = "Explored|Carga")
	FOnItemPickedUp OnItemPickedUp;

	static constexpr int32 MaxPocketSlots = FInventoryModel::PocketSlots;
	static constexpr int32 MaxBeltSlots = FInventoryModel::BaseBeltHooks;

protected:
	virtual void BeginPlay() override;

private:
	const UItemRegistrySubsystem* GetRegistry() const;
	AExploredItemActor* SpawnDropped(const FItemInstance& Instance) const;
	/** Instancia completa con el líquido y la cuenta actuales del modelo (para soltarla o guardarla fuera). */
	FItemInstance TakePayload(const FInventoryItem& Record);
	/** Ajusta Payloads tras FInventoryModel::StowMerging (pila nueva o registro fundido del todo). */
	void ApplyStowResult(int64 SourceId, const FInventoryStowResult& Result);
	/** Rehace las copias en FItemInstance que usan el HUD y el personaje, y avisa del cambio. */
	void SyncFromModel();
	void PlaceSledgeActorBehindOwner(AExploredSledge* Sledge) const;

	/** Fuente de verdad de lo que se lleva y dónde (datos planos, sin UObject). */
	FInventoryModel Model;

	/** Instancias completas por id de instancia del modelo; su Count se copia del modelo en SyncFromModel. */
	UPROPERTY()
	TMap<int64, FItemInstance> Payloads;

	/** Angarillas enganchadas: el actor se queda detrás del jugador como imagen de la carga. */
	UPROPERTY()
	TWeakObjectPtr<AExploredSledge> AttachedSledge;

	// Copias para la API existente (HUD, personaje, tests); se rehacen en SyncFromModel.
	UPROPERTY(Transient)
	FItemInstance HandLeft;
	UPROPERTY(Transient)
	FItemInstance HandRight;
	UPROPERTY(Transient)
	TArray<FItemInstance> Pockets;
	UPROPERTY(Transient)
	TArray<FItemInstance> Belt;
	UPROPERTY(Transient)
	TArray<FItemInstance> BackpackItems;
	UPROPERTY(Transient)
	TArray<FItemInstance> PouchItems;
	UPROPERTY(Transient)
	TArray<FItemInstance> SledgeItems;

	/** Mochila de partida sin objeto (si se marca en el editor); ver SetBackpack. */
	UPROPERTY(EditAnywhere, Category = "Explored|Carga")
	bool bHasBackpack = false;

	UPROPERTY(EditAnywhere, Category = "Explored|Carga")
	float BackpackCapacityVolumeLiters = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Explored|Carga")
	float BackpackCapacityWeightKg = 0.0f;
};
