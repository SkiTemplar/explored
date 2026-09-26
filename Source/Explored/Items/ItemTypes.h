#pragma once

#include "CoreMinimal.h"

#include "ItemTypes.generated.h"

/**
 * Tamaño de un objeto a efectos de cómo se puede transportar (GDD §4.2).
 * Los objetos DosManos exigen las dos manos libres para recogerse.
 */
UENUM(BlueprintType)
enum class EItemSize : uint8
{
	Pequeno,
	Mediano,
	Grande,
	DosManos
};

EXPLORED_API const TCHAR* LexToString(EItemSize Size);

/**
 * Definición estática de un objeto, cargada desde Content/Data/items.json por
 * UItemRegistrySubsystem. No es un UDataAsset a propósito: añadir contenido es
 * añadir una entrada de JSON, sin tocar el editor (biblia de contenido §10).
 */
USTRUCT(BlueprintType)
struct EXPLORED_API FItemDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	FText NameEs;

	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	FText NameEn;

	/** Puede apuntar a una malla básica de /Engine/BasicShapes/ como marcador. */
	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	FSoftObjectPath MeshPath;

	/** Propiedades 0-5 de la biblia de contenido §2.1 (Filo, Largo, Ata...). Claves en FName sin tildes. */
	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	TMap<FName, float> Properties;

	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	float WeightKg = 0.1f;

	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	float VolumeLiters = 0.1f;

	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	EItemSize Size = EItemSize::Pequeno;

	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	TSet<FName> Tags;

	/** Nutrición opcional; a cero en objetos que no se comen. */
	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	float NutritionEnergy = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	float NutritionProtein = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	float NutritionVitamins = 0.0f;

	/** 0 = el objeto no se desgasta (materiales en bruto, comida...). */
	UPROPERTY(BlueprintReadOnly, Category = "Explored|Objetos")
	float MaxDurability = 0.0f;

	float GetProperty(FName PropertyName) const { return Properties.FindRef(PropertyName); }
	bool HasTag(FName Tag) const { return Tags.Contains(Tag); }
};

/**
 * Instancia concreta de un objeto en el mundo o en las manos del jugador.
 *
 * Las propiedades efectivas NO se guardan aquí: se calculan (ver ItemEffective,
 * más abajo) a partir de la definición base y, si el objeto es compuesto, de sus
 * Components. Así un hacha hereda el filo de la lasca con la que se fabricó sin
 * copiar números al crearla, y una reparación futura que sustituya una pieza
 * rota actualiza las estadísticas automáticamente.
 */
USTRUCT(BlueprintType)
struct EXPLORED_API FItemInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Explored|Objetos")
	FName DefinitionId;

	/** 1 a 5 estrellas (biblia §2.4). */
	UPROPERTY(BlueprintReadWrite, Category = "Explored|Objetos")
	int32 Quality = 3;

	/** Fracción 0..1 sobre la durabilidad máxima de la definición o de la plantilla que lo creó. */
	UPROPERTY(BlueprintReadWrite, Category = "Explored|Objetos")
	float Durability = 1.0f;

	/** Solo tiene sentido para objetos Pequeño apilables en los bolsillos. */
	UPROPERTY(BlueprintReadWrite, Category = "Explored|Objetos")
	int32 Count = 1;

	/**
	 * Piezas combinadas para fabricar este objeto; vacío en materiales en bruto.
	 * Sin UPROPERTY a propósito: UHT no admite la recursión de un USTRUCT vía
	 * TArray de sí mismo («'Struct' recursion via arrays is unsupported for
	 * properties»). No hay ningún acceso a este campo fuera de C++ (Items,
	 * Carry, Crafting); si algún día hace falta desde Blueprint, envolver el
	 * array en un tipo indirecto (p. ej. TArray<TInstancedStruct<FItemInstance>>
	 * o un USTRUCT contenedor con un puntero) en vez de quitar este comentario.
	 */
	TArray<FItemInstance> Components;

	/** Nombre generado por la fabricación; vacío = usar el nombre de la definición. */
	UPROPERTY(BlueprintReadWrite, Category = "Explored|Objetos")
	FText GeneratedName;

	bool IsValid() const { return !DefinitionId.IsNone(); }
};

/**
 * Cálculo de propiedades efectivas a partir de una definición y, recursivamente,
 * de las piezas de un objeto compuesto. Funciones libres y puras (sin UObject)
 * para que UItemRegistrySubsystem, UCraftingLibrary y los tests de Automation
 * comparen exactamente la misma lógica sin depender de un mundo.
 */
namespace ItemEffective
{
	/** Máximo entre el valor propio de la definición y el de cualquier pieza (recursivo). */
	EXPLORED_API float GetProperty(const FItemInstance& Instance, FName PropertyName, const TMap<FName, FItemDefinition>& Items);

	/** Suma de pesos de las piezas si es compuesto; si no, peso de la definición × Count. */
	EXPLORED_API float GetWeightKg(const FItemInstance& Instance, const TMap<FName, FItemDefinition>& Items);

	EXPLORED_API float GetVolumeLiters(const FItemInstance& Instance, const TMap<FName, FItemDefinition>& Items);

	EXPLORED_API EItemSize GetSize(const FItemInstance& Instance, const TMap<FName, FItemDefinition>& Items);

	EXPLORED_API bool HasTag(const FItemInstance& Instance, FName Tag, const TMap<FName, FItemDefinition>& Items);

	/** Nombre generado si lo hay; si no, el de la definición base. */
	EXPLORED_API FText GetDisplayName(const FItemInstance& Instance, const TMap<FName, FItemDefinition>& Items);

	/**
	 * Recorre las piezas y devuelve el nombre de la hoja (pieza sin componentes)
	 * con mayor valor de PropertyName; se usa para nombrar los objetos fabricados
	 * («hacha DE BASALTO con mango...»).
	 */
	EXPLORED_API FText FindDominantLeafName(const FItemInstance& Instance, FName PropertyName, const TMap<FName, FItemDefinition>& Items);
}
