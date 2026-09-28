#pragma once

#include "CoreMinimal.h"

/**
 * Tabla de ids de contenido para la red (biblia 08 §2.4) como modelo puro.
 *
 * En la red un objeto no viaja con su FName («rama_seca», 9 bytes o más) sino
 * con un uint16: su posición en la lista ORDENADA de ids de su fichero de
 * Content/Data (items, plantillas, piezas, plantas, barcos y logros, cada uno
 * con su propia numeración).
 *
 * Estrategia de versionado (por qué basta con ordenar):
 *   - El orden es por bytes de los ids en minúsculas ASCII, así que la tabla
 *     sale igual en cualquier máquina, lea los ficheros en el orden que los lea.
 *   - Un id nuevo SÍ puede desplazar a los que van detrás (el spec lo muestra).
 *     No importa porque el uint16 solo vive dentro de una sesión: el guardado
 *     escribe siempre el FName (SaveSystemStates), nunca este número.
 *   - Las dos puntas de una sesión tienen la misma tabla porque el saludo de
 *     conexión compara ComputeContentHash de los JSON de Content/Data; si no
 *     coinciden, el servidor rechaza al cliente con el texto de biblia 08 §6.5.
 *     Con los mismos ficheros, la misma tabla; con otros, no hay sesión.
 */

/** Ficheros de Content/Data que tienen tabla propia. Mismo orden que CONTENT_TABLES en Tools/DataCheck. */
enum class EContentKind : uint8
{
	Item,
	Template,
	BuildingPiece,
	Plant,
	Boat,
	Achievement,
	Count
};

/** Fichero de Content/Data del que salen los ids de cada tabla. */
EXPLORED_API const TCHAR* LexToString(EContentKind Kind);

/** Un fichero de Content/Data para el hash del saludo de conexión. */
struct EXPLORED_API FContentFile
{
	/** Nombre sin carpeta («items.json»). */
	FString Name;
	TArray<uint8> Bytes;
};

class EXPLORED_API FContentIdTableModel
{
public:
	/** Id de red de «ninguno» (mano vacía) o de un id que no está en la tabla. */
	static constexpr uint16 InvalidNetId = 0xFFFF;
	/** Como mucho 65 535 ids por tabla (0..65 534): InvalidNetId queda libre. */
	static constexpr int32 MaxIdsPerKind = 0xFFFF;

	static constexpr uint64 FnvOffsetBasis = 14695981039346656037ull;
	static constexpr uint64 FnvPrime = 1099511628211ull;

	/**
	 * Fija los ids de una tabla (en cualquier orden). Falla, sin tocar la tabla,
	 * con ids vacíos, repetidos (sin distinguir mayúsculas, como FName), con
	 * caracteres fuera de [a-z0-9_] o si hay más de MaxIdsPerKind.
	 */
	bool SetIds(EContentKind Kind, const TArray<FName>& Ids, FString& OutError);

	/** InvalidNetId si el id no está en la tabla. */
	uint16 ToNetId(EContentKind Kind, FName Id) const;
	/** NAME_None si el número no corresponde a ningún id (paquete corrupto o de otra versión). */
	FName FromNetId(EContentKind Kind, uint16 NetId) const;

	int32 Num(EContentKind Kind) const;
	/** Ids ordenados: el índice de cada uno es su id de red. */
	const TArray<FName>& GetIds(EContentKind Kind) const;

	/** [a-z0-9_]+ (ids de Content/Data; Tools/DataCheck exige lo mismo). */
	static bool IsValidContentId(const FString& Id);
	/** Orden por bytes (no depende de la configuración regional ni de mayúsculas de FName). */
	static bool OrdinalLess(const FString& A, const FString& B);

	/** FNV-1a de 64 bits. Hash permite encadenar varios trozos. */
	static uint64 Fnv1a64(const uint8* Data, int32 Num, uint64 Hash = FnvOffsetBasis);

	/**
	 * FExploredContentHash del saludo de conexión: FNV-1a de 64 bits de todos los
	 * ficheros ordenados por nombre. Cada fichero entra como nombre, un byte 0,
	 * su longitud en 8 bytes little-endian y su contenido, para que mover bytes
	 * de un fichero al siguiente también cambie el hash. Espejo en Tools/DataCheck.
	 */
	static uint64 ComputeContentHash(const TArray<FContentFile>& Files);

private:
	TArray<FName> Ids[static_cast<int32>(EContentKind::Count)];
	TMap<FName, uint16> Index[static_cast<int32>(EContentKind::Count)];
};
