#pragma once

#include "CoreMinimal.h"

/**
 * Tipos de datos puros de la construcción (GDD §8.6, biblia §3.10). Reflejan
 * Content/Data/building_pieces.json sin UObject para que FBuildingModel se
 * pueda probar en el host (Tools/HostTests) igual que en el editor.
 */

/** Dónde encaja una pieza en la rejilla de la base. Campo "socket" del JSON. */
enum class EBuildSocket : uint8
{
	Pillar,      // "pilar": pilote o cimiento en la esquina de una celda, siempre sobre terreno
	Floor,       // "suelo": centro de celda
	Wall,        // "pared": lado de celda
	Door,        // "puerta": lado de celda (pared con hueco y hoja)
	Roof,        // "techo": centro de celda, sobre la coronación de las paredes
	Stairs,      // "escalera": arranque en una celda, ocupa dos celdas y sube un piso
	Furniture,   // "mueble": interior de celda, sobre suelo o terreno
	GroundOnly,  // "terreno": interior de celda, solo sobre terreno (fuegos, huerto, refugio)
	Count
};

/** Por qué no se puede construir (o reparar) una pieza. */
enum class EBuildFailReason : uint8
{
	None,
	UnknownPiece,
	InvalidRotation,
	UnknownBase,
	Occupied,              // el hueco está ocupado o lo bloquea otra pieza
	NoSupport,             // nada la sostiene
	Unstable,              // se sostiene, pero demasiado lejos de un apoyo en el suelo
	NeedsGround,           // pieza que solo se apoya en el terreno
	MissingRequiredPiece,  // requiresPieces: falta una pieza previa en la base
	MissingTools,
	MissingMaterials,
	NothingToRepair,
	UnknownInstance,
	Count
};

EXPLORED_API const TCHAR* LexToString(EBuildSocket Socket);
EXPLORED_API const TCHAR* LexToString(EBuildFailReason Reason);

/** Convierte el valor de "socket" del JSON; devuelve false si no es válido. */
EXPLORED_API bool ParseBuildSocket(const FString& Text, EBuildSocket& OutSocket);

/** Un ingrediente de coste: unidades de un objeto de items.json. */
struct EXPLORED_API FBuildingCost
{
	FName Item;
	int32 Count = 0;

	bool operator==(const FBuildingCost& Other) const { return Item == Other.Item && Count == Other.Count; }
};

/** Nivel de material (palma → bambú → madera → piedra). */
struct EXPLORED_API FBuildingTierDef
{
	FName Id;
	int32 Order = 0;
	FString NameEs;
	/** Herramientas que exige cualquier pieza del nivel (además de las propias). */
	TArray<FName> RequiresTools;
};

/** Definición de pieza (una entrada de "pieces"). */
struct EXPLORED_API FBuildingPieceDef
{
	FName Id;
	FString NameEs;
	FName Tier;
	FName Category;
	EBuildSocket Socket = EBuildSocket::Furniture;
	TArray<FBuildingCost> Cost;
	TArray<FName> Tools;
	TArray<FName> RequiresPieces;
	/** Minutos de juego de trabajo con eficiencia 1. */
	float BuildMinutes = 10.0f;
	/** Puntos de integridad máximos (1–100). */
	float Integrity = 50.0f;
	/** Categoría de ciclón (0–3) que aguanta sin daño si está bien apoyada. */
	int32 MaxCycloneCategory = 0;
	/** Nombre SM_ de Tools/Blender/props; NAME_None si la malla está pendiente. */
	FName Mesh;
	/** Encendida, es punto de reaparición (GDD §8.6: «las fogatas encendidas son puntos de reaparición»). */
	bool bRespawnPoint = false;
	/**
	 * Almacenamiento (biblia 03 §1.4): clase de contenedor del mundo que hace la pieza
	 * («Cesta», «Estante», «Arcon», los nombres de EWorldContainerKind); NAME_None si no guarda nada.
	 */
	FName ContainerKind;
};

/** Todo building_pieces.json ya parseado. */
struct EXPLORED_API FBuildingCatalog
{
	TArray<FBuildingTierDef> Tiers;
	TArray<FBuildingPieceDef> Pieces;

	const FBuildingPieceDef* FindPiece(FName Id) const;
	const FBuildingTierDef* FindTier(FName Id) const;
	/** Orden del nivel de una pieza (0 = palma); 0 si el nivel no existe. */
	int32 TierOrderOf(const FBuildingPieceDef& Piece) const;
};

/** Posición discreta de una pieza en la rejilla de una base. */
struct EXPLORED_API FBuildingPlacement
{
	int32 BaseId = 0;
	/**
	 * X, Y: celda (o esquina, para pilares; o lado, para paredes y puertas).
	 * Z: piso (0 = planta baja). Ver FBuildingModel para la convención exacta.
	 */
	FIntVector Cell = FIntVector(0, 0, 0);
	/** Giros de 90° en sentido de la guiñada de Unreal (0–3). */
	int32 Rotation = 0;

	bool operator==(const FBuildingPlacement& Other) const
	{
		return BaseId == Other.BaseId && Cell == Other.Cell && Rotation == Other.Rotation;
	}
};

/** Estado guardable de una pieza construida (P-SAVE lo persiste tal cual). */
struct EXPLORED_API FBuildingPieceState
{
	int32 Id = 0;
	FName DefId;
	FBuildingPlacement Placement;
	/** Integridad actual, en puntos (0 = rota). */
	float Integrity = 0.0f;
	/** La capa de UE comprobó que la base de la pieza toca el terreno (o el fondo marino). */
	bool bGroundContact = false;
	/** Fuego encendido (solo tiene sentido en piezas con bRespawnPoint o fuegos). */
	bool bLit = false;

	bool operator==(const FBuildingPieceState& Other) const
	{
		return Id == Other.Id && DefId == Other.DefId && Placement == Other.Placement &&
			Integrity == Other.Integrity && bGroundContact == Other.bGroundContact && bLit == Other.bLit;
	}
};

/** Una base: origen de su rejilla en el mundo (cm). Varias bases posibles (GDD §8.6). */
struct EXPLORED_API FBuildingBaseState
{
	int32 Id = 0;
	FVector Origin = FVector::ZeroVector;

	bool operator==(const FBuildingBaseState& Other) const { return Id == Other.Id && Origin == Other.Origin; }
};

/** Todo lo que hay que guardar de la construcción. La estabilidad se recalcula al cargar. */
struct EXPLORED_API FBuildingSaveState
{
	static constexpr int32 CurrentVersion = 1;

	int32 Version = CurrentVersion;
	int32 NextPieceId = 1;
	int32 NextBaseId = 1;
	TArray<FBuildingBaseState> Bases;
	TArray<FBuildingPieceState> Pieces;
};

/** Petición de colocación. */
struct EXPLORED_API FBuildingPlaceRequest
{
	FName DefId;
	FBuildingPlacement Placement;
	bool bGroundContact = false;
};

/** Tiempo que afecta a las construcciones en un intervalo. */
struct EXPLORED_API FBuildingWeather
{
	/** Lluvia 0–1 (FWeatherSample::Rain). */
	float Rain = 0.0f;
	/**
	 * Intensidad de temporal en «categorías de ciclón»: 0 = nada, 0,5 = galerna,
	 * 1–3 = ciclón de esa categoría (biblia §6.2).
	 */
	float StormCategory = 0.0f;
};

/** Qué ha pasado en un paso de simulación o al quitar piezas. */
struct EXPLORED_API FBuildingChangeResult
{
	/** Piezas rotas por daño (integridad a 0). */
	TArray<int32> Destroyed;
	/** Piezas que se han caído por quedarse sin apoyo. */
	TArray<int32> Collapsed;

	bool IsEmpty() const { return Destroyed.Num() == 0 && Collapsed.Num() == 0; }
};
