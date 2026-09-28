#pragma once

#include "CoreMinimal.h"
#include "Save/SaveValue.h"
#include "WorldGen/TerrainEditModel.h"

/**
 * Estrato que se pica (biblia 02 §2.3, `Content/Data/mining.json/strata`). Cada estrato se
 * cava como uno de los materiales de `ETerrainMaterial` y deja su objeto de `items.json`;
 * los que no tienen material propio (arcilla, azufre, vetas y cristal) usan la dureza de su
 * material pero su propio nivel mínimo de herramienta y su propio botín.
 */
enum class EMineStratum : uint8
{
	Tierra,
	Arena,
	Arcilla,
	Azufre,
	Caliza,
	VetaCobre,
	Basalto,
	HierroMeteorito,
	Obsidiana,
	Cristal,
	Count,
};

/** Herramienta con la que se golpea (biblia 02 §2.2, `mining.json/tools`). */
enum class EMineTool : uint8
{
	Mano,
	PalaTosca,
	PicoPiedra,
	PicoTallado,
	PicoObsidiana,
	PicoRescatado,
	Count,
};

struct EXPLORED_API FMineStratumInfo
{
	/** Id de `mining.json/strata`. */
	const TCHAR* Id = TEXT("");
	/** Objeto que suelta cada golpe efectivo. */
	const TCHAR* Item = TEXT("");
	/** Material con el que se cava (dureza del pincel de `FTerrainEditModel`). */
	ETerrainMaterial Material = ETerrainMaterial::Tierra;
	/** Dureza de la tabla de biblia 02 §2.3 (1–4): decide la rotura del pico de obsidiana. */
	int32 Hardness = 1;
	/** Nivel mínimo de herramienta: 0 a mano, 1 pala, 2 piedra, 3 tallado, 4 obsidiana o rescatado. */
	int32 MinToolTier = 1;
	/** Unidades de una veta finita (0 = no es veta). */
	int32 VeinUnits = 0;
	/** Días hasta que reaparece una veta agotada (0 = no reaparece). */
	int32 RespawnDays = 0;
	/** Estrato que queda cuando la veta se agota («deja el material que la rodea»). */
	EMineStratum Host = EMineStratum::Tierra;
};

struct EXPLORED_API FMineToolInfo
{
	/** Id de `mining.json/tools`. */
	const TCHAR* Id = TEXT("");
	/** `ToolTier` del C++ (el «Nivel» de la biblia más uno). */
	int32 Tier = 0;
	/** Radio nominal de la esfera que resta cada golpe (m). */
	float Radius = 0.5f;
	float SecondsPerHit = 1.0f;
	/** Durabilidad de una herramienta nueva (0 = no se gasta: las manos). */
	int32 MaxDurability = 0;
	/** Pico de obsidiana: riesgo de rotura extra contra dureza ≥ FragileMinHardness. */
	bool bFragile = false;
};

/** Lo que el jugador percibe de un golpe: sonido y partículas, nunca texto de HUD (biblia 02 §2.6). */
enum class EMineHitCue : uint8
{
	/** La herramienta no llega al estrato: rebote, chispa, sin coste de durabilidad (§2.1). */
	Rebound,
	/** Golpe al aire o a una muestra ya vacía: no pasa nada. */
	Miss,
	/** Golpe efectivo. */
	Hit,
	/** Golpe efectivo que además ha mellado el pico de obsidiana (−15). */
	Chipped,
};

/** Un golpe de minería tal como lo pide el jugador. Metros. */
struct EXPLORED_API FMineHitRequest
{
	EMineStratum Stratum = EMineStratum::Tierra;
	EMineTool Tool = EMineTool::PicoPiedra;
	FVector ImpactPoint = FVector::ZeroVector;
	FVector Direction = FVector(0.0, 0.0, -1.0);
	/**
	 * Semilla del golpe: forma del hueco y tirada de rotura. El servidor la deriva de la
	 * semilla de partida y del contador de golpes del jugador (`FMiningModel::HitSeed`).
	 */
	uint32 Seed = 0;
	/** Veta que se golpea (clave estable de la generación, p. ej. su celda). Ignorada si el estrato no es veta. */
	FIntVector Vein = FIntVector::ZeroValue;
	/** Día de juego del golpe (para el reaparecer de las vetas). */
	int32 Day = 0;
};

struct EXPLORED_API FMineHitResult
{
	EMineHitCue Cue = EMineHitCue::Miss;
	/** Estrato que se ha picado de verdad (el de alrededor si la veta estaba agotada). */
	EMineStratum Stratum = EMineStratum::Tierra;
	/** Lo que ha cambiado en el terreno (vacío si rebota). */
	FTerrainEditResult Edit;
	/** Objeto y unidades que van a la carga del jugador. */
	FString LootItem;
	int32 LootUnits = 0;
	/** Durabilidad que pierde la herramienta: 1 por golpe efectivo, +15 si se mella. */
	int32 DurabilityLoss = 0;
	/** Tiempo que ocupa el golpe (también el rebote). */
	float Seconds = 0.0f;
	/** Esta ha sido la última unidad de la veta. */
	bool bVeinExhausted = false;

	bool IsEffective() const { return Cue == EMineHitCue::Hit || Cue == EMineHitCue::Chipped; }
};

/**
 * Minería por estrato sobre `FTerrainEditModel` (biblia 02 §2.1–2.3; GDD v2 §3.4).
 *
 * - **Picado por esfera:** cada golpe efectivo resta una esfera en el terreno. El radio
 *   depende de la herramienta, no del material; la dureza del material del estrato decide
 *   cuánta densidad arranca (la cuenta de `FTerrainEditModel::Pickaxe`).
 * - **Rebote:** con herramienta por debajo del mínimo del estrato no se toca el terreno ni
 *   la durabilidad; solo cuesta el tiempo del golpe.
 * - **Botín determinista:** un golpe que quita sólido = una unidad del objeto del estrato.
 *   Un golpe que no quita nada (aire) no da nada.
 * - **Vetas finitas:** cobre 10 golpes y reaparece a los 20 días; hierro de meteorito 4 y
 *   cristal 12, sin reaparecer. Agotada, la veta se pica como la roca que la rodea.
 * - **Pico de obsidiana frágil:** contra dureza ≥ 3, cada golpe efectivo tiene un 8 % de
 *   perder 15 de durabilidad además del 1 normal. La tirada sale de la semilla del golpe:
 *   mismo golpe, misma suerte, en el servidor y en una repetición.
 *
 * Red (biblia 08 §1.2, §2.2 y §2.12): solo el servidor llama a `Hit`. Valida la cadencia
 * con `ToolInfo(Tool).SecondsPerHit` (−15 % de tolerancia) y la herramienta con **su**
 * copia del inventario; el terreno sale por la cola de deltas y el botín por el
 * inventario replicado. El estado de las vetas no se replica: solo lo lee el servidor.
 * El cliente reproduce el sonido de `Cue` sin esperar, porque no cambia nada del mundo.
 */
class EXPLORED_API FMiningModel
{
public:
	/** Riesgo extra del pico de obsidiana (biblia 02 §2.2). */
	static constexpr float FragileChance = 0.08f;
	static constexpr int32 FragileDurabilityLoss = 15;
	static constexpr int32 FragileMinHardness = 3;
	/** Desgaste de un golpe efectivo con cualquier herramienta. */
	static constexpr int32 DurabilityPerHit = 1;
	/** Sólido mínimo (m³) para que un golpe cuente como efectivo: por debajo es un roce. */
	static constexpr double MinEffectiveVolume = 0.001;

	static const FMineStratumInfo& StratumInfo(EMineStratum Stratum);
	static const FMineToolInfo& ToolInfo(EMineTool Tool);
	/** Estrato por id de `mining.json`; false si no existe. */
	static bool StratumFromId(const FString& Id, EMineStratum& Out);
	static bool ToolFromId(const FString& Id, EMineTool& Out);
	/** La herramienta puede con el estrato (si no, rebota). */
	static bool CanMine(EMineStratum Stratum, EMineTool Tool);
	/** Semilla de un golpe a partir de la semilla de partida, el jugador y su contador de golpes. */
	static uint32 HitSeed(uint32 WorldSeed, uint32 PlayerId, uint32 HitCounter);
	/** Tirada de rotura del pico de obsidiana para una semilla (independiente de la forma del hueco). */
	static bool RollChip(uint32 Seed);

	/** Resuelve el golpe y lo aplica al terreno y a las vetas. */
	FMineHitResult Hit(const FMineHitRequest& Request, FTerrainEditModel& Terrain, FTerrainEditModel::FBaseDensity Base);

	/** Unidades que le quedan a la veta el día dado (las de una veta nueva si no se ha tocado). */
	int32 VeinRemaining(EMineStratum Stratum, const FIntVector& Vein, int32 Day) const;
	int32 NumTrackedVeins() const { return Veins.Num(); }
	bool IsEmpty() const { return Veins.Num() == 0; }
	void Reset() { Veins.Reset(); }

	// --- Guardado ---

	/** {"v":1,"veins":[[Estrato,X,Y,Z,Restantes,DíaAgotada],…]} en orden (estrato, Z, Y, X). */
	FSaveValue ToValue() const;
	/** Sustituye el contenido; false (y queda vacío) si no es válido. */
	bool FromValue(const FSaveValue& Value);

	bool operator==(const FMiningModel& Other) const;
	bool operator!=(const FMiningModel& Other) const { return !(*this == Other); }

private:
	struct FVeinKey
	{
		EMineStratum Stratum = EMineStratum::Tierra;
		FIntVector Cell = FIntVector::ZeroValue;

		bool operator==(const FVeinKey& Other) const { return Stratum == Other.Stratum && Cell == Other.Cell; }
		friend uint32 GetTypeHash(const FVeinKey& Key)
		{
			return HashCombine(GetTypeHash(Key.Cell), static_cast<uint32>(Key.Stratum));
		}
	};

	struct FVeinState
	{
		int32 Remaining = 0;
		/** Día en que se agotó (solo con Remaining == 0). */
		int32 ExhaustedDay = 0;
	};

	/** Aplica el reaparecer: una veta agotada hace RespawnDays vuelve a estar llena. */
	static int32 RemainingOn(const FMineStratumInfo& Info, const FVeinState* State, int32 Day);
	TArray<FVeinKey> SortedVeinKeys() const;

	/** Solo las vetas tocadas. Una veta llena que reaparece se borra del mapa. */
	TMap<FVeinKey, FVeinState> Veins;
};
