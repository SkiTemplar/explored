#pragma once

#include "CoreMinimal.h"

/** Tipo de un nodo del árbol de guardado (subconjunto de JSON). */
enum class ESaveValueType : uint8
{
	Null,
	Bool,
	Int,
	Double,
	String,
	Array,
	Object,
};

/**
 * Nodo del árbol de guardado: nulo, booleano, entero de 64 bits, real doble,
 * cadena, lista u objeto. Es un dato plano y copiable, sin UObject, para que
 * el formato se pueda probar en el host y cada sistema lo rellene sin conocer
 * el disco.
 *
 * Los objetos guardan sus claves ordenadas (orden ordinal por unidades de
 * código), así que la codificación es determinista con independencia del
 * orden en que se escriban los campos.
 */
class EXPLORED_API FSaveValue
{
public:
	FSaveValue() = default;

	static FSaveValue MakeBool(bool bValue);
	static FSaveValue MakeInt(int64 Value);
	static FSaveValue MakeDouble(double Value);
	/** Real que se escribe con la precisión justa de un float (hasta 9 cifras). */
	static FSaveValue MakeFloat(float Value);
	static FSaveValue MakeString(const FString& Value);
	static FSaveValue MakeArray();
	static FSaveValue MakeObject();

	ESaveValueType GetType() const { return Type; }
	bool IsNull() const { return Type == ESaveValueType::Null; }
	bool IsBool() const { return Type == ESaveValueType::Bool; }
	bool IsInt() const { return Type == ESaveValueType::Int; }
	bool IsDouble() const { return Type == ESaveValueType::Double; }
	/** Entero o real (o las cadenas «NaN», «Infinity» y «-Infinity»; ver TryGetDouble). */
	bool IsNumber() const { return Type == ESaveValueType::Int || Type == ESaveValueType::Double; }
	bool IsString() const { return Type == ESaveValueType::String; }
	bool IsArray() const { return Type == ESaveValueType::Array; }
	bool IsObject() const { return Type == ESaveValueType::Object; }
	bool IsFloatHint() const { return bFloatHint; }

	// --- Lectura tipada: devuelven false (sin tocar la salida) si el tipo no encaja. ---

	bool TryGetBool(bool& Out) const;
	/** Acepta enteros y reales exactamente enteros dentro del rango de int64. */
	bool TryGetInt(int64& Out) const;
	/** Acepta enteros, reales y las cadenas «NaN», «Infinity» y «-Infinity». */
	bool TryGetDouble(double& Out) const;
	bool TryGetString(FString& Out) const;

	bool AsBool(bool Default = false) const;
	int64 AsInt(int64 Default = 0) const;
	double AsDouble(double Default = 0.0) const;
	/** Cadena vacía si no es una cadena. */
	const FString& AsString() const;

	// --- Listas ---

	/** Elementos de una lista o campos de un objeto; 0 para escalares. */
	int32 Num() const;
	/** Elemento de una lista; un nulo compartido si el índice no es válido. */
	const FSaveValue& At(int32 Index) const;
	FSaveValue* AtMutable(int32 Index);
	/** Añade a una lista (convierte en lista un nulo). Devuelve el elemento añadido. */
	FSaveValue& Add(FSaveValue Value);

	// --- Objetos ---

	const FSaveValue* Find(const FString& Key) const;
	FSaveValue* Find(const FString& Key);
	bool Contains(const FString& Key) const { return Find(Key) != nullptr; }
	/** Asigna un campo (convierte en objeto un nulo). Devuelve el valor guardado. */
	FSaveValue& Set(const FString& Key, FSaveValue Value);
	bool Remove(const FString& Key);
	/** Claves de un objeto, en orden ordinal. */
	const TArray<FString>& GetKeys() const { return Keys; }
	/** Valor del campo I (en el orden de GetKeys) o elemento I de una lista. */
	const FSaveValue& GetValueAt(int32 Index) const { return At(Index); }

	/**
	 * Igualdad profunda por valor: NaN es igual a NaN y, si uno de los dos reales
	 * es float, se comparan con precisión de float (lo que conserva el texto).
	 */
	bool operator==(const FSaveValue& Other) const;
	bool operator!=(const FSaveValue& Other) const { return !(*this == Other); }

	/** Nulo compartido que devuelven los accesos fallidos. */
	static const FSaveValue& NullValue();

	/** Compara dos claves por unidades de código (orden estable en todas las plataformas). */
	static int32 CompareKeys(const FString& A, const FString& B);

private:
	int32 FindKeyIndex(const FString& Key, bool& bOutFound) const;

	ESaveValueType Type = ESaveValueType::Null;
	bool bBool = false;
	bool bFloatHint = false;
	int64 IntValue = 0;
	double DoubleValue = 0.0;
	FString StringValue;
	/** Claves de un objeto (ordenadas); vacío en listas. */
	TArray<FString> Keys;
	/** Elementos de una lista o valores de un objeto (paralelos a Keys). */
	TArray<FSaveValue> Values;
};

/** Opciones del escritor de texto. */
enum class ESaveTextStyle : uint8
{
	/** Una línea sin espacios: la forma canónica sobre la que se calcula la suma de control. */
	Compact,
	/** Sangrado con tabuladores; las listas de escalares van en una línea. */
	Pretty,
};

/**
 * Codificación de texto del árbol (subconjunto de JSON escrito a mano, sin el
 * módulo Json de Unreal para que compile en el host):
 * - enteros sin punto decimal; reales siempre con «.» o exponente, con la
 *   representación más corta que vuelve exactamente al mismo valor (15–17
 *   cifras para doble, 6–9 para float); NaN e infinitos como cadenas;
 * - caracteres no ASCII tal cual (el fichero se guarda en UTF-8);
 * - el lector nunca se cuelga ni aborta: ante cualquier entrada mal formada
 *   devuelve false y un mensaje con la posición.
 */
struct EXPLORED_API FSaveText
{
	/** Profundidad máxima de anidamiento que acepta el lector. */
	static constexpr int32 MaxDepth = 64;

	static FString Write(const FSaveValue& Value, ESaveTextStyle Style = ESaveTextStyle::Pretty);
	static bool Parse(const FString& Text, FSaveValue& OutValue, FString& OutError);

	/** Texto de un real con el formato del escritor (útil en tests y en la documentación). */
	static FString FormatDouble(double Value);
	static FString FormatFloat(float Value);

	/** Conversión a float sin comportamiento indefinido: fuera de rango da ±infinito. */
	static float DoubleToFloat(double Value);
};

/** Suma de control del contenido: FNV-1a de 64 bits sobre los bytes UTF-8 del texto. */
struct EXPLORED_API FSaveChecksum
{
	static uint64 Compute(const FString& Text);
	/** 16 cifras hexadecimales en minúscula. */
	static FString ToHex(uint64 Value);
	static bool FromHex(const FString& Text, uint64& OutValue);
};
