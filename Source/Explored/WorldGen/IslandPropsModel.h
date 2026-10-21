#pragma once

#include "CoreMinimal.h"

/** Primitiva de reserva mientras no exista la malla definitiva (formas de /Engine/BasicShapes). */
enum class EIslandPropShape : uint8
{
	Cube,
	Cylinder,
	Sphere,
	Cone,
};

/** Colisión del actor colocado: bloquea todo, solo responde a trazas (interacción) o ninguna. */
enum class EIslandPropCollision : uint8
{
	Solid,
	Query,
	None,
};

/** Tipo de colocación de island_props.json («types»). Tamaños en metros. */
struct EXPLORED_API FIslandPropType
{
	FName Name;
	/** Ruta definitiva del GDD §11.1 (/Game/Art/<Kit>/SM_<Nombre>_01); vacía = solo blockout. */
	FString MeshPath;
	EIslandPropShape Shape = EIslandPropShape::Cube;
	/** Caja que ocupa la primitiva de reserva (m). */
	FVector Size = FVector(1.0);
	/** Celda de la hoja de paleta (Tools/Style/paleta.json, 0-31). */
	int32 ColorIndex = 0;
	/** Muestra la etiqueta de depuración sobre la primitiva. */
	bool bLabel = false;
	EIslandPropCollision Collision = EIslandPropCollision::Solid;
	/** Objeto de juego (no decorado): el validador exige que quede en tierra firme. */
	bool bGameplay = false;
};

/** Una colocación de island_props.json («props»). Metros y grados; +X norte, +Y este. */
struct EXPLORED_API FIslandProp
{
	FName Id;
	FName Type;
	FVector2D Location = FVector2D::ZeroVector;
	float Yaw = 0.0f;
	float Pitch = 0.0f;
	/** Alabeo (grados; positivo baja el costado +Y): el puente sigue la escora del casco varado. */
	float Roll = 0.0f;
	/** Metros sobre el suelo trazado en Location, o sobre el suelo del ancla si hay Anchor. */
	float Elevation = 0.0f;
	/** Id de otra colocación (anterior) cuyo suelo sirve de referencia: piezas sobre el casco o la torre. */
	FName Anchor;
	/** Malla definitiva distinta de la del tipo (variantes: SM_PalmB). */
	FString MeshOverride;
	/** Unidades del recurso (leña por montón, fibras...). */
	int32 Amount = 0;
	/** Cocos de una palma. */
	int32 Cocos = 0;
	/** Número de página del cuaderno (1-8); 0 si no es una página. */
	int32 Page = 0;
	/** Semiextensión (m) de un volumen (refugios). */
	FVector Extent = FVector::ZeroVector;
	/** El volumen es punto de reaparición del desmayo. */
	bool bRespawn = false;
};

/**
 * Colocaciones de la Isla de Sal (Content/Data/island_props.json; GDD v3 §5.3 y §6). Modelo puro:
 * lo lee ExploredIslandProps::Get y lo usa AExploredIslandPopulator, que es una capa fina encima.
 */
struct EXPLORED_API FIslandProps
{
	static constexpr int32 PaletteSize = 32;

	TMap<FName, FIslandPropType> Types;
	TArray<FIslandProp> Props;

	const FIslandPropType* FindType(FName Name) const { return Types.Find(Name); }
	const FIslandProp* FindProp(FName Id) const;

	int32 CountOf(FName Type) const;
	/** Suma de Amount (o de Cocos si bCocos) de las colocaciones del tipo. */
	int32 TotalOf(FName Type, bool bCocos = false) const;

	/** Malla definitiva de la colocación: la propia si la trae, si no la del tipo. */
	FString MeshPathFor(const FIslandProp& Prop) const;

	/** Coherencia: ids únicos, tipos definidos, anclas anteriores, tamaños y colores válidos. */
	bool Validate(FString& OutError) const;

	/** Cantidades cerradas del GDD (8 palmas con 16 cocos, 10 montones de leña con 20...). */
	bool MatchesGdd(FString& OutError) const;
};

/** Transformaciones de la primitiva de reserva (las de /Engine/BasicShapes miden 100 cm con el pivote en el centro). */
namespace IslandPropsPlacement
{
	/** Escala del componente para que la primitiva ocupe Type.Size metros. */
	EXPLORED_API FVector FallbackScale(const FIslandPropType& Type);
	/** Desplazamiento relativo (cm) que apoya la base de la primitiva en el pivote del actor. */
	EXPLORED_API FVector FallbackOffset(const FIslandPropType& Type);
	/** Posición del actor (cm) con el suelo de referencia en metros. */
	EXPLORED_API FVector WorldLocation(const FIslandProp& Prop, float GroundMeters);
	/** Ruta de la forma básica del motor para la primitiva. */
	EXPLORED_API const TCHAR* BasicShapePath(EIslandPropShape Shape);
	/** U del centro de la celda de paleta (como Tools/Style: (índice + 0,5) / 32). */
	EXPLORED_API float PaletteU(int32 ColorIndex);

	EXPLORED_API bool ShapeFromName(const FString& Name, EIslandPropShape& Out);
	EXPLORED_API bool CollisionFromName(const FString& Name, EIslandPropCollision& Out);
}
