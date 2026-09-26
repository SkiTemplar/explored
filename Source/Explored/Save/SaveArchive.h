#pragma once

#include "CoreMinimal.h"
#include "Save/SaveValue.h"

/**
 * Nombres con los que se guarda un enum (enums como texto: un guardado sobrevive
 * a reordenar el enum). Cada sistema lo especializa junto a su enum:
 *
 *   template <>
 *   struct TSaveEnumNames<ESurvivalMode>
 *   {
 *       static constexpr const TCHAR* Names[] = { TEXT("Explorer"), TEXT("Survivor"), TEXT("Castaway") };
 *   };
 *
 * El índice del nombre es el valor numérico del enumerador (enums contiguos desde 0).
 */
template <typename TEnum>
struct TSaveEnumNames;

template <typename TEnum>
concept CSaveEnumWithNames = std::is_enum_v<TEnum> && requires { TSaveEnumNames<TEnum>::Names[0]; };

/**
 * Conversión de un tipo a FSaveValue y de vuelta. FromValue devuelve false si el
 * valor no encaja (tipo equivocado, fuera de rango); en ese caso la salida puede
 * quedar a medias y FSaveArchive::Read no la usa.
 */
template <typename T, typename Enable = void>
struct TSaveTraits;

class FSaveArchive;

/**
 * Objeto de guardado de una sección: pares clave → valor con lectura tolerante.
 * Read devuelve false y deja el valor de entrada intacto si la clave falta o no
 * encaja, así que el patrón para cargar es «estado por defecto + Read de cada
 * campo»: los campos nuevos toman su valor por defecto en partidas antiguas.
 */
class EXPLORED_API FSaveArchive
{
public:
	FSaveArchive() : Root(FSaveValue::MakeObject()) {}
	/** Un valor que no sea objeto se trata como archivo vacío. */
	explicit FSaveArchive(const FSaveValue& InRoot) : Root(InRoot.IsObject() ? InRoot : FSaveValue::MakeObject()) {}

	const FSaveValue& GetRoot() const { return Root; }
	bool IsEmpty() const { return Root.Num() == 0; }
	bool Has(const FString& Key) const { return Root.Contains(Key); }
	bool Remove(const FString& Key) { return Root.Remove(Key); }

	void SetValue(const FString& Key, FSaveValue Value) { Root.Set(Key, MoveTemp(Value)); }
	const FSaveValue* FindValue(const FString& Key) const { return Root.Find(Key); }

	template <typename T>
	void Write(const FString& Key, const T& Value)
	{
		Root.Set(Key, TSaveTraits<T>::ToValue(Value));
	}

	void Write(const FString& Key, const TCHAR* Value)
	{
		Root.Set(Key, FSaveValue::MakeString(FString(Value)));
	}

	template <typename T>
	bool Read(const FString& Key, T& InOutValue) const
	{
		const FSaveValue* Value = Root.Find(Key);
		if (!Value)
		{
			return false;
		}
		T Parsed = InOutValue;
		if (!TSaveTraits<T>::FromValue(*Value, Parsed))
		{
			return false;
		}
		InOutValue = MoveTemp(Parsed);
		return true;
	}

	template <typename T>
	T ReadOr(const FString& Key, const T& Default) const
	{
		T Value = Default;
		Read(Key, Value);
		return Value;
	}

	bool operator==(const FSaveArchive& Other) const { return Root == Other.Root; }
	bool operator!=(const FSaveArchive& Other) const { return !(Root == Other.Root); }

private:
	FSaveValue Root;
};

// ---------------------------------------------------------------------------
// Tipos comunes
// ---------------------------------------------------------------------------

template <>
struct TSaveTraits<FSaveValue>
{
	static FSaveValue ToValue(const FSaveValue& V) { return V; }
	static bool FromValue(const FSaveValue& V, FSaveValue& Out) { Out = V; return true; }
};

template <>
struct TSaveTraits<FSaveArchive>
{
	static FSaveValue ToValue(const FSaveArchive& V) { return V.GetRoot(); }
	static bool FromValue(const FSaveValue& V, FSaveArchive& Out)
	{
		if (!V.IsObject())
		{
			return false;
		}
		Out = FSaveArchive(V);
		return true;
	}
};

template <>
struct TSaveTraits<bool>
{
	static FSaveValue ToValue(bool V) { return FSaveValue::MakeBool(V); }
	static bool FromValue(const FSaveValue& V, bool& Out) { return V.TryGetBool(Out); }
};

/** Enteros con signo o sin signo de hasta 32 bits y int64: se comprueba el rango al leer. */
template <typename T>
struct TSaveTraits<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool> && (sizeof(T) < 8 || std::is_signed_v<T>)>>
{
	static FSaveValue ToValue(T V) { return FSaveValue::MakeInt(static_cast<int64>(V)); }
	static bool FromValue(const FSaveValue& V, T& Out)
	{
		int64 Value = 0;
		if (!V.TryGetInt(Value))
		{
			return false;
		}
		if constexpr (sizeof(T) < 8)
		{
			if (Value < static_cast<int64>(TNumericLimits<T>::Min()) || Value > static_cast<int64>(TNumericLimits<T>::Max()))
			{
				return false;
			}
		}
		Out = static_cast<T>(Value);
		return true;
	}
};

/** uint64 (semillas, máscaras): se guarda con el mismo patrón de bits que un int64. */
template <>
struct TSaveTraits<uint64>
{
	static FSaveValue ToValue(uint64 V) { return FSaveValue::MakeInt(static_cast<int64>(V)); }
	static bool FromValue(const FSaveValue& V, uint64& Out)
	{
		int64 Value = 0;
		if (!V.TryGetInt(Value))
		{
			return false;
		}
		Out = static_cast<uint64>(Value);
		return true;
	}
};

template <>
struct TSaveTraits<float>
{
	static FSaveValue ToValue(float V) { return FSaveValue::MakeFloat(V); }
	static bool FromValue(const FSaveValue& V, float& Out)
	{
		double Value = 0.0;
		if (!V.TryGetDouble(Value))
		{
			return false;
		}
		Out = FSaveText::DoubleToFloat(Value);
		return true;
	}
};

template <>
struct TSaveTraits<double>
{
	static FSaveValue ToValue(double V) { return FSaveValue::MakeDouble(V); }
	static bool FromValue(const FSaveValue& V, double& Out) { return V.TryGetDouble(Out); }
};

template <>
struct TSaveTraits<FString>
{
	static FSaveValue ToValue(const FString& V) { return FSaveValue::MakeString(V); }
	static bool FromValue(const FSaveValue& V, FString& Out) { return V.TryGetString(Out); }
};

template <>
struct TSaveTraits<FName>
{
	static FSaveValue ToValue(const FName& V) { return FSaveValue::MakeString(V.ToString()); }
	static bool FromValue(const FSaveValue& V, FName& Out)
	{
		if (!V.IsString())
		{
			return false;
		}
		Out = FName(*V.AsString());
		return true;
	}
};

/** Vectores como listas de reales: [X, Y, Z]. */
template <>
struct TSaveTraits<FVector>
{
	static FSaveValue ToValue(const FVector& V)
	{
		FSaveValue Out = FSaveValue::MakeArray();
		Out.Add(FSaveValue::MakeDouble(V.X));
		Out.Add(FSaveValue::MakeDouble(V.Y));
		Out.Add(FSaveValue::MakeDouble(V.Z));
		return Out;
	}
	static bool FromValue(const FSaveValue& V, FVector& Out)
	{
		return V.IsArray() && V.Num() == 3
			&& V.At(0).TryGetDouble(Out.X) && V.At(1).TryGetDouble(Out.Y) && V.At(2).TryGetDouble(Out.Z);
	}
};

template <>
struct TSaveTraits<FVector2D>
{
	static FSaveValue ToValue(const FVector2D& V)
	{
		FSaveValue Out = FSaveValue::MakeArray();
		Out.Add(FSaveValue::MakeDouble(V.X));
		Out.Add(FSaveValue::MakeDouble(V.Y));
		return Out;
	}
	static bool FromValue(const FSaveValue& V, FVector2D& Out)
	{
		return V.IsArray() && V.Num() == 2 && V.At(0).TryGetDouble(Out.X) && V.At(1).TryGetDouble(Out.Y);
	}
};

/** Rotaciones como [Pitch, Yaw, Roll] en grados. */
template <>
struct TSaveTraits<FRotator>
{
	static FSaveValue ToValue(const FRotator& V)
	{
		FSaveValue Out = FSaveValue::MakeArray();
		Out.Add(FSaveValue::MakeDouble(V.Pitch));
		Out.Add(FSaveValue::MakeDouble(V.Yaw));
		Out.Add(FSaveValue::MakeDouble(V.Roll));
		return Out;
	}
	static bool FromValue(const FSaveValue& V, FRotator& Out)
	{
		return V.IsArray() && V.Num() == 3
			&& V.At(0).TryGetDouble(Out.Pitch) && V.At(1).TryGetDouble(Out.Yaw) && V.At(2).TryGetDouble(Out.Roll);
	}
};

template <>
struct TSaveTraits<FIntPoint>
{
	static FSaveValue ToValue(const FIntPoint& V)
	{
		FSaveValue Out = FSaveValue::MakeArray();
		Out.Add(FSaveValue::MakeInt(V.X));
		Out.Add(FSaveValue::MakeInt(V.Y));
		return Out;
	}
	static bool FromValue(const FSaveValue& V, FIntPoint& Out)
	{
		return V.IsArray() && V.Num() == 2
			&& TSaveTraits<int32>::FromValue(V.At(0), Out.X) && TSaveTraits<int32>::FromValue(V.At(1), Out.Y);
	}
};

/** Enums por nombre (ver TSaveEnumNames). Un nombre desconocido no se carga. */
template <typename TEnum>
struct TSaveTraits<TEnum, std::enable_if_t<std::is_enum_v<TEnum>>>
{
	static_assert(CSaveEnumWithNames<TEnum>, "Especializa TSaveEnumNames<TEnum> para guardar este enum por nombre.");

	static constexpr int32 NumNames()
	{
		return static_cast<int32>(sizeof(TSaveEnumNames<TEnum>::Names) / sizeof(TSaveEnumNames<TEnum>::Names[0]));
	}

	static FSaveValue ToValue(TEnum V)
	{
		const int64 Index = static_cast<int64>(V);
		if (Index >= 0 && Index < NumNames())
		{
			return FSaveValue::MakeString(FString(TSaveEnumNames<TEnum>::Names[Index]));
		}
		return FSaveValue::MakeInt(Index);
	}

	static bool FromValue(const FSaveValue& V, TEnum& Out)
	{
		if (!V.IsString())
		{
			return false;
		}
		for (int32 I = 0; I < NumNames(); ++I)
		{
			if (FSaveValue::CompareKeys(V.AsString(), FString(TSaveEnumNames<TEnum>::Names[I])) == 0)
			{
				Out = static_cast<TEnum>(I);
				return true;
			}
		}
		return false;
	}
};

/** Listas: todo o nada (un elemento que no encaja invalida la lista). */
template <typename T>
struct TSaveTraits<TArray<T>>
{
	static FSaveValue ToValue(const TArray<T>& V)
	{
		FSaveValue Out = FSaveValue::MakeArray();
		for (const T& Item : V)
		{
			Out.Add(TSaveTraits<T>::ToValue(Item));
		}
		return Out;
	}
	static bool FromValue(const FSaveValue& V, TArray<T>& Out)
	{
		if (!V.IsArray())
		{
			return false;
		}
		TArray<T> Result;
		Result.Reserve(V.Num());
		for (int32 I = 0; I < V.Num(); ++I)
		{
			T Item{};
			if (!TSaveTraits<T>::FromValue(V.At(I), Item))
			{
				return false;
			}
			Result.Add(MoveTemp(Item));
		}
		Out = MoveTemp(Result);
		return true;
	}
};

/**
 * Mapas: con clave FString o FName se guardan como objeto (claves ordenadas,
 * deterministas); con otra clave, como lista de pares [clave, valor] en el
 * orden de iteración del mapa.
 */
template <typename K, typename V>
struct TSaveTraits<TMap<K, V>>
{
	static constexpr bool bStringKey = std::is_same_v<K, FString> || std::is_same_v<K, FName>;

	static FSaveValue ToValue(const TMap<K, V>& Map)
	{
		if constexpr (bStringKey)
		{
			FSaveValue Out = FSaveValue::MakeObject();
			for (const auto& Pair : Map)
			{
				Out.Set(TSaveTraits<K>::ToValue(Pair.Key).AsString(), TSaveTraits<V>::ToValue(Pair.Value));
			}
			return Out;
		}
		else
		{
			FSaveValue Out = FSaveValue::MakeArray();
			for (const auto& Pair : Map)
			{
				FSaveValue Entry = FSaveValue::MakeArray();
				Entry.Add(TSaveTraits<K>::ToValue(Pair.Key));
				Entry.Add(TSaveTraits<V>::ToValue(Pair.Value));
				Out.Add(MoveTemp(Entry));
			}
			return Out;
		}
	}

	static bool FromValue(const FSaveValue& Value, TMap<K, V>& Out)
	{
		TMap<K, V> Result;
		if constexpr (bStringKey)
		{
			if (!Value.IsObject())
			{
				return false;
			}
			for (int32 I = 0; I < Value.Num(); ++I)
			{
				K Key{};
				V Item{};
				if (!TSaveTraits<K>::FromValue(FSaveValue::MakeString(Value.GetKeys()[I]), Key)
					|| !TSaveTraits<V>::FromValue(Value.GetValueAt(I), Item))
				{
					return false;
				}
				Result.Add(Key, Item);
			}
		}
		else
		{
			if (!Value.IsArray())
			{
				return false;
			}
			for (int32 I = 0; I < Value.Num(); ++I)
			{
				const FSaveValue& Entry = Value.At(I);
				K Key{};
				V Item{};
				if (!Entry.IsArray() || Entry.Num() != 2
					|| !TSaveTraits<K>::FromValue(Entry.At(0), Key) || !TSaveTraits<V>::FromValue(Entry.At(1), Item))
				{
					return false;
				}
				Result.Add(Key, Item);
			}
		}
		Out = MoveTemp(Result);
		return true;
	}
};
