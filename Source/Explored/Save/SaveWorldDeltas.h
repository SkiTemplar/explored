#pragma once

#include "CoreMinimal.h"
#include "Save/SaveArchive.h"

/**
 * Base64 estándar sin relleno para meter bytes en el texto de la partida. La
 * decodificación es estricta (codificación canónica): rechaza caracteres ajenos, una
 * longitud imposible o bits sobrantes distintos de cero, así que un texto truncado o
 * manipulado nunca decodifica a medias.
 */
struct EXPLORED_API FSaveBase64
{
	static FString Encode(const TArray<uint8>& Bytes);
	/** Devuelve false (y deja Out vacío) si el texto no es válido o pasaría de MaxBytes. */
	static bool Decode(const FString& Text, TArray<uint8>& Out, int32 MaxBytes = MAX_int32);
	/** Igual que Decode, pero sobre [Data, Data + Len). */
	static bool Decode(const TCHAR* Data, int32 Len, TArray<uint8>& Out, int32 MaxBytes = MAX_int32);
};

/**
 * Conjunto de índices no negativos (instancias de una celda) como mapa de bits.
 * Se codifica en texto con la forma más corta de dos:
 * - «r:0-4,9,12-40»: rangos ascendentes (recolección en claros contiguos);
 * - «b:<base64>»: mapa de bits en base64 sin relleno (patrones dispersos).
 */
class EXPLORED_API FSaveIndexSet
{
public:
	/** Índice máximo admitido: acota la memoria ante un fichero manipulado (128 KiB por celda). */
	static constexpr int32 MaxIndex = (1 << 20) - 1;

	/** Devuelve true si el índice es nuevo; false si ya estaba o está fuera de rango. */
	bool Add(int32 Index);
	bool Remove(int32 Index);
	bool Contains(int32 Index) const;
	int32 Num() const { return Count; }
	bool IsEmpty() const { return Count == 0; }
	/** Palabras de 32 bits reservadas (lo que ocupa en memoria: crece con el índice más alto). */
	int32 NumWords() const { return Words.Num(); }
	void Reset();

	/** Une otro conjunto. Devuelve cuántos índices eran nuevos. */
	int32 Union(const FSaveIndexSet& Other);

	/** Índices en orden ascendente. */
	TArray<int32> ToArray() const;

	FString Encode() const;
	/** Sustituye el contenido; devuelve false (y deja el conjunto vacío) si el texto no es válido. */
	bool Decode(const FString& Text);

	bool operator==(const FSaveIndexSet& Other) const;
	bool operator!=(const FSaveIndexSet& Other) const { return !(*this == Other); }

private:
	void SetRange(int32 First, int32 Last);
	void TrimTrailingZeros();
	FString EncodeRanges() const;
	FString EncodeBits() const;
	bool DecodeRanges(const FString& Text);
	bool DecodeBits(const FString& Text);

	TArray<uint32> Words;
	int32 Count = 0;
};

/** Instancias de una celda del mundo retiradas por el jugador. */
struct FSaveCellDeltas
{
	FIntPoint Cell;
	FSaveIndexSet Indices;
};

/**
 * Deltas de un tipo de instancia procedural («semilla + deltas», GDD §17.2):
 * el mundo se regenera desde la semilla y se le quitan las instancias
 * recolectadas o destruidas, identificadas por (celda, índice dentro de la
 * celda). Las celdas se guardan ordenadas (Y, X), así que el texto es
 * determinista.
 *
 * Forma en la partida: [[X, Y, "r:…"], [X, Y, "b:…"], …].
 */
class EXPLORED_API FSaveScatterDeltas
{
public:
	/**
	 * Tope de memoria al cargar, en palabras de 32 bits: 2^22 = 16 MiB por capa y
	 * también en total en FSaveWorldDeltas::Load (134 millones de índices posibles,
	 * muy por encima de lo que se recolecta en una partida). Cada celda cuesta según su
	 * índice más alto, no según cuántos lleva: "r:1048575" son 11 caracteres y 128 KiB,
	 * y 90.000 celdas así pedirían unos 11 GB. Al pasarlo se rechaza la capa.
	 */
	static constexpr int32 MaxLoadedWords = 1 << 22;

	bool Add(const FIntPoint& Cell, int32 Index);
	bool Remove(const FIntPoint& Cell, int32 Index);
	bool Contains(const FIntPoint& Cell, int32 Index) const;
	/** Índices retirados de una celda, o nullptr si no hay ninguno. */
	const FSaveIndexSet* FindCell(const FIntPoint& Cell) const;

	int32 Num() const;
	int32 NumCells() const { return Cells.Num(); }
	/** Memoria de todas las celdas en palabras de 32 bits (ver MaxLoadedWords). */
	int64 NumWords() const;
	bool IsEmpty() const { return Cells.Num() == 0; }
	void Reset() { Cells.Reset(); }

	/** Une los deltas de otra partida o de otro hilo de juego. */
	void Merge(const FSaveScatterDeltas& Other);

	/** Recorre (celda, índice) en orden determinista. */
	void ForEach(TFunctionRef<void(const FIntPoint&, int32)> Visit) const;

	FSaveValue ToValue() const;
	/** Sustituye el contenido; devuelve false (y queda vacío) si el valor no es válido. */
	bool FromValue(const FSaveValue& Value);

	bool operator==(const FSaveScatterDeltas& Other) const;

private:
	int32 FindCellIndex(const FIntPoint& Cell, bool& bOutFound) const;

	TArray<FSaveCellDeltas> Cells;
};

template <>
struct TSaveTraits<FSaveScatterDeltas>
{
	static FSaveValue ToValue(const FSaveScatterDeltas& V) { return V.ToValue(); }
	static bool FromValue(const FSaveValue& V, FSaveScatterDeltas& Out) { return Out.FromValue(V); }
};

/**
 * Sección «world»: semilla + deltas por capa («harvested», «destroyed»…) + ediciones
 * del terreno. Cada sistema que genera instancias procedurales elige su capa por nombre.
 */
struct EXPLORED_API FSaveWorldDeltas
{
	int64 Seed = 0;
	TMap<FName, FSaveScatterDeltas> Layers;
	/**
	 * Capa «terrain»: ediciones del terreno volumétrico (pico, pala, escaleras) tal como
	 * las serializa FTerrainEditModel::ToValue. Opaca para el guardado; nulo si no hay.
	 */
	FSaveValue Terrain;

	FSaveScatterDeltas& Layer(FName Name) { return Layers.FindOrAdd(Name); }
	const FSaveScatterDeltas* FindLayer(FName Name) const { return Layers.Find(Name); }

	void Merge(const FSaveWorldDeltas& Other);

	void Save(FSaveArchive& Ar) const;
	/** Estado por defecto + lo que haya en el archivo; una capa ilegible se descarta. */
	void Load(const FSaveArchive& Ar);

	bool operator==(const FSaveWorldDeltas& Other) const;
};

/**
 * Sección «player»: transformación y valores del cuerpo como datos planos. Los
 * valores del cuerpo reflejan FSurvivalState y se enlazarán cuando exista su
 * componente; hasta entonces guardan los valores por defecto del modelo.
 */
struct EXPLORED_API FSavePlayerState
{
	FVector Location = FVector::ZeroVector;
	/** Rotación del control (cámara): Pitch, Yaw, Roll. */
	FRotator ControlRotation = FRotator::ZeroRotator;
	FVector Velocity = FVector::ZeroVector;

	float Health = 100.0f;
	float Hunger = 85.0f;
	float Thirst = 70.0f;
	float Energy = 100.0f;
	float Rest = 90.0f;
	float Morale = 60.0f;
	float BodyTemperature = 37.0f;
	float Wetness = 0.0f;

	void Save(FSaveArchive& Ar) const;
	void Load(const FSaveArchive& Ar);

	bool operator==(const FSavePlayerState& Other) const;
};
