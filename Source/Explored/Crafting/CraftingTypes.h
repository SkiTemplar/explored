#pragma once

#include "CoreMinimal.h"

#include "CraftingTypes.generated.h"

/** Requisito mínimo de una propiedad (biblia §2.1) para que una pieza cubra un rol de una plantilla. */
USTRUCT()
struct EXPLORED_API FCraftingRequirement
{
	GENERATED_BODY()

	UPROPERTY()
	FName Property;

	UPROPERTY()
	float MinValue = 0.0f;
};

/**
 * Un rol que debe cubrir alguna de las dos piezas combinadas (p. ej. «Mango» en
 * un hacha). No exigimos que cada rol lo cubra una pieza distinta: una pieza ya
 * compuesta (p. ej. un palo atado con una liana) puede cubrir varios roles a la
 * vez, que es justo lo que permite fabricar en dos pasos con solo dos manos
 * (ver UCraftingLibrary::TemplateMatches).
 */
USTRUCT()
struct EXPLORED_API FCraftingSlot
{
	GENERATED_BODY()

	UPROPERTY()
	FName Role;

	/** Se satisface si se cumple cualquiera de estos requisitos, salvo que bRequireAll. Vacío = cualquier pieza vale. */
	UPROPERTY()
	TArray<FCraftingRequirement> Requirements;

	UPROPERTY()
	bool bRequireAll = false;

	/** Alternativa u complemento a Requirements: la pieza debe tener alguna de estas etiquetas. */
	UPROPERTY()
	TArray<FName> RequiredTags;
};

/** Plantilla de fabricación (biblia §2.3): piezas requeridas + cómo se llama y qué hereda el resultado. */
USTRUCT()
struct EXPLORED_API FCraftingTemplateDef
{
	GENERATED_BODY()

	UPROPERTY()
	FName Id;

	UPROPERTY()
	FText NameEs;

	/** Verbos que pueden desencadenar esta plantilla (Golpear, Atar...). */
	UPROPERTY()
	TArray<FName> Verbs;

	UPROPERTY()
	TArray<FCraftingSlot> Slots;

	/** Entrada de items.json que aporta malla, tamaño y etiquetas base del resultado. */
	UPROPERTY()
	FName ResultDefinitionId;

	/** Con marcadores {0}, {1}... en el orden de Slots (biblia §2.3: «Hacha de basalto con mango de guayabo, atada con liana»). */
	UPROPERTY()
	FString NameTemplate;

	UPROPERTY()
	float BaseMaxDurability = 20.0f;

	/**
	 * Si es true, esta plantilla no fabrica un objeto nuevo: afila/repara el
	 * primer slot (la herramienta) usando el segundo (el abrasivo) y devuelve
	 * la propia herramienta con la durabilidad restaurada (biblia §2.4, verbo Afilar).
	 */
	UPROPERTY()
	bool bIsSharpen = false;
};

/** Metadatos de un verbo (verbs.json); la lógica de cada uno vive en las plantillas que lo usan. */
USTRUCT()
struct EXPLORED_API FCraftingVerbDef
{
	GENERATED_BODY()

	UPROPERTY()
	FName Id;

	UPROPERTY()
	FText NameEs;

	UPROPERTY()
	FText Description;
};
