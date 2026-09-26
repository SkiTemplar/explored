#include "Save/SaveValue.h"

namespace SaveValueDetail
{
	/** Unidad de código sin signo (TCHAR es char en el host y UTF-16 en Unreal). */
	inline uint32 CodeOf(TCHAR C)
	{
		if constexpr (sizeof(TCHAR) == 1)
		{
			return static_cast<uint32>(static_cast<uint8>(C));
		}
		else if constexpr (sizeof(TCHAR) == 2)
		{
			return static_cast<uint32>(static_cast<uint16>(C));
		}
		else
		{
			return static_cast<uint32>(C);
		}
	}

	/** Añade un punto de código con la codificación nativa de TCHAR (UTF-8, UTF-16 o UTF-32). */
	void AppendCodePoint(FString& Out, uint32 CodePoint)
	{
		if (CodePoint > 0x10FFFF || (CodePoint >= 0xD800 && CodePoint <= 0xDFFF))
		{
			CodePoint = 0xFFFD;
		}
		if constexpr (sizeof(TCHAR) == 1)
		{
			if (CodePoint < 0x80)
			{
				Out.AppendChar(static_cast<TCHAR>(CodePoint));
			}
			else if (CodePoint < 0x800)
			{
				Out.AppendChar(static_cast<TCHAR>(0xC0 | (CodePoint >> 6)));
				Out.AppendChar(static_cast<TCHAR>(0x80 | (CodePoint & 0x3F)));
			}
			else if (CodePoint < 0x10000)
			{
				Out.AppendChar(static_cast<TCHAR>(0xE0 | (CodePoint >> 12)));
				Out.AppendChar(static_cast<TCHAR>(0x80 | ((CodePoint >> 6) & 0x3F)));
				Out.AppendChar(static_cast<TCHAR>(0x80 | (CodePoint & 0x3F)));
			}
			else
			{
				Out.AppendChar(static_cast<TCHAR>(0xF0 | (CodePoint >> 18)));
				Out.AppendChar(static_cast<TCHAR>(0x80 | ((CodePoint >> 12) & 0x3F)));
				Out.AppendChar(static_cast<TCHAR>(0x80 | ((CodePoint >> 6) & 0x3F)));
				Out.AppendChar(static_cast<TCHAR>(0x80 | (CodePoint & 0x3F)));
			}
		}
		else if constexpr (sizeof(TCHAR) == 2)
		{
			if (CodePoint < 0x10000)
			{
				Out.AppendChar(static_cast<TCHAR>(CodePoint));
			}
			else
			{
				const uint32 V = CodePoint - 0x10000;
				Out.AppendChar(static_cast<TCHAR>(0xD800 | (V >> 10)));
				Out.AppendChar(static_cast<TCHAR>(0xDC00 | (V & 0x3FF)));
			}
		}
		else
		{
			Out.AppendChar(static_cast<TCHAR>(CodePoint));
		}
	}

	void AppendAscii(FString& Out, const ANSICHAR* Text)
	{
		for (const ANSICHAR* P = Text; *P; ++P)
		{
			Out.AppendChar(static_cast<TCHAR>(*P));
		}
	}

	FString FormatInt(int64 Value)
	{
		// A mano para no depender de los especificadores de Printf con int64 en cada plataforma.
		ANSICHAR Buffer[24];
		int32 Pos = 23;
		Buffer[Pos] = 0;
		const bool bNegative = Value < 0;
		uint64 Magnitude = bNegative ? (~static_cast<uint64>(Value) + 1) : static_cast<uint64>(Value);
		do
		{
			Buffer[--Pos] = static_cast<ANSICHAR>('0' + static_cast<int32>(Magnitude % 10));
			Magnitude /= 10;
		}
		while (Magnitude > 0);
		if (bNegative)
		{
			Buffer[--Pos] = '-';
		}
		FString Out;
		AppendAscii(Out, Buffer + Pos);
		return Out;
	}

	/** Notación científica con Precision cifras significativas (1–17). */
	FString Scientific(double Value, int32 Precision)
	{
		// Printf exige un formato literal en Unreal: una rama por precisión.
		switch (Precision)
		{
		case 1: return FString::Printf(TEXT("%.0e"), Value);
		case 2: return FString::Printf(TEXT("%.1e"), Value);
		case 3: return FString::Printf(TEXT("%.2e"), Value);
		case 4: return FString::Printf(TEXT("%.3e"), Value);
		case 5: return FString::Printf(TEXT("%.4e"), Value);
		case 6: return FString::Printf(TEXT("%.5e"), Value);
		case 7: return FString::Printf(TEXT("%.6e"), Value);
		case 8: return FString::Printf(TEXT("%.7e"), Value);
		case 9: return FString::Printf(TEXT("%.8e"), Value);
		case 10: return FString::Printf(TEXT("%.9e"), Value);
		case 11: return FString::Printf(TEXT("%.10e"), Value);
		case 12: return FString::Printf(TEXT("%.11e"), Value);
		case 13: return FString::Printf(TEXT("%.12e"), Value);
		case 14: return FString::Printf(TEXT("%.13e"), Value);
		case 15: return FString::Printf(TEXT("%.14e"), Value);
		case 16: return FString::Printf(TEXT("%.15e"), Value);
		default: return FString::Printf(TEXT("%.16e"), Value);
		}
	}

	/**
	 * Convierte la salida de %e («-d.ddde+XX») a la forma canónica del formato:
	 * notación fija si el exponente está en [-5, 16] y científica si no, sin
	 * ceros de cola y siempre con «.» o «e» para que se lea como real.
	 */
	FString Canonical(const FString& Sci)
	{
		const TCHAR* P = *Sci;
		const int32 Len = Sci.Len();
		int32 I = 0;
		bool bNegative = false;
		if (I < Len && P[I] == TEXT('-'))
		{
			bNegative = true;
			++I;
		}
		ANSICHAR Digits[32];
		int32 NumDigits = 0;
		for (; I < Len && P[I] != TEXT('e') && P[I] != TEXT('E'); ++I)
		{
			if (P[I] >= TEXT('0') && P[I] <= TEXT('9') && NumDigits < 30)
			{
				Digits[NumDigits++] = static_cast<ANSICHAR>(P[I]);
			}
		}
		int32 Exponent = 0;
		bool bNegativeExponent = false;
		if (I < Len)
		{
			++I;
			if (I < Len && (P[I] == TEXT('-') || P[I] == TEXT('+')))
			{
				bNegativeExponent = P[I] == TEXT('-');
				++I;
			}
			for (; I < Len && P[I] >= TEXT('0') && P[I] <= TEXT('9'); ++I)
			{
				Exponent = FMath::Min(Exponent * 10 + static_cast<int32>(P[I] - TEXT('0')), 100000);
			}
		}
		if (bNegativeExponent)
		{
			Exponent = -Exponent;
		}
		while (NumDigits > 1 && Digits[NumDigits - 1] == '0')
		{
			--NumDigits;
		}
		if (NumDigits == 0)
		{
			Digits[NumDigits++] = '0';
		}
		Digits[NumDigits] = 0;

		FString Out;
		if (bNegative)
		{
			Out.AppendChar(TEXT('-'));
		}
		if (NumDigits == 1 && Digits[0] == '0')
		{
			AppendAscii(Out, "0.0");
			return Out;
		}
		if (Exponent >= -5 && Exponent <= 16)
		{
			if (Exponent >= 0)
			{
				for (int32 D = 0; D <= Exponent; ++D)
				{
					Out.AppendChar(static_cast<TCHAR>(D < NumDigits ? Digits[D] : '0'));
				}
				Out.AppendChar(TEXT('.'));
				if (NumDigits > Exponent + 1)
				{
					AppendAscii(Out, Digits + Exponent + 1);
				}
				else
				{
					Out.AppendChar(TEXT('0'));
				}
			}
			else
			{
				AppendAscii(Out, "0.");
				for (int32 Z = 0; Z < -Exponent - 1; ++Z)
				{
					Out.AppendChar(TEXT('0'));
				}
				AppendAscii(Out, Digits);
			}
			return Out;
		}
		Out.AppendChar(static_cast<TCHAR>(Digits[0]));
		Out.AppendChar(TEXT('.'));
		if (NumDigits > 1)
		{
			AppendAscii(Out, Digits + 1);
		}
		else
		{
			Out.AppendChar(TEXT('0'));
		}
		Out.AppendChar(TEXT('e'));
		Out += FormatInt(Exponent);
		return Out;
	}

	void AppendIndent(FString& Out, int32 Indent)
	{
		for (int32 I = 0; I < Indent; ++I)
		{
			Out.AppendChar(TEXT('\t'));
		}
	}

	void WriteString(const FString& Value, FString& Out)
	{
		static const ANSICHAR* const Hex = "0123456789abcdef";
		Out.AppendChar(TEXT('"'));
		const TCHAR* P = *Value;
		const int32 Len = Value.Len();
		for (int32 I = 0; I < Len; ++I)
		{
			const TCHAR C = P[I];
			const uint32 Code = CodeOf(C);
			switch (Code)
			{
			case '"': AppendAscii(Out, "\\\""); break;
			case '\\': AppendAscii(Out, "\\\\"); break;
			case '\n': AppendAscii(Out, "\\n"); break;
			case '\r': AppendAscii(Out, "\\r"); break;
			case '\t': AppendAscii(Out, "\\t"); break;
			case '\b': AppendAscii(Out, "\\b"); break;
			case '\f': AppendAscii(Out, "\\f"); break;
			default:
				if (Code < 0x20)
				{
					AppendAscii(Out, "\\u00");
					Out.AppendChar(static_cast<TCHAR>(Hex[(Code >> 4) & 0xF]));
					Out.AppendChar(static_cast<TCHAR>(Hex[Code & 0xF]));
				}
				else
				{
					Out.AppendChar(C);
				}
				break;
			}
		}
		Out.AppendChar(TEXT('"'));
	}

	bool IsScalar(const FSaveValue& Value)
	{
		return !Value.IsArray() && !Value.IsObject();
	}

	void WriteValue(const FSaveValue& Value, FString& Out, ESaveTextStyle Style, int32 Indent)
	{
		const bool bPretty = Style == ESaveTextStyle::Pretty;
		switch (Value.GetType())
		{
		case ESaveValueType::Null:
			AppendAscii(Out, "null");
			return;
		case ESaveValueType::Bool:
			AppendAscii(Out, Value.AsBool() ? "true" : "false");
			return;
		case ESaveValueType::Int:
			Out += FormatInt(Value.AsInt());
			return;
		case ESaveValueType::Double:
		{
			const double D = Value.AsDouble();
			if (FMath::IsNaN(D))
			{
				AppendAscii(Out, "\"NaN\"");
			}
			else if (!FMath::IsFinite(D))
			{
				AppendAscii(Out, D > 0.0 ? "\"Infinity\"" : "\"-Infinity\"");
			}
			else
			{
				Out += Value.IsFloatHint() ? FSaveText::FormatFloat(static_cast<float>(D)) : FSaveText::FormatDouble(D);
			}
			return;
		}
		case ESaveValueType::String:
			WriteString(Value.AsString(), Out);
			return;
		case ESaveValueType::Array:
		{
			const int32 Count = Value.Num();
			if (Count == 0)
			{
				AppendAscii(Out, "[]");
				return;
			}
			bool bInline = !bPretty;
			if (bPretty)
			{
				bInline = true;
				for (int32 I = 0; I < Count && bInline; ++I)
				{
					bInline = IsScalar(Value.At(I));
				}
			}
			Out.AppendChar(TEXT('['));
			for (int32 I = 0; I < Count; ++I)
			{
				if (I > 0)
				{
					Out.AppendChar(TEXT(','));
					if (bPretty && bInline)
					{
						Out.AppendChar(TEXT(' '));
					}
				}
				if (!bInline)
				{
					Out.AppendChar(TEXT('\n'));
					AppendIndent(Out, Indent + 1);
				}
				WriteValue(Value.At(I), Out, Style, Indent + 1);
			}
			if (!bInline)
			{
				Out.AppendChar(TEXT('\n'));
				AppendIndent(Out, Indent);
			}
			Out.AppendChar(TEXT(']'));
			return;
		}
		case ESaveValueType::Object:
		{
			const TArray<FString>& Keys = Value.GetKeys();
			if (Keys.Num() == 0)
			{
				AppendAscii(Out, "{}");
				return;
			}
			Out.AppendChar(TEXT('{'));
			for (int32 I = 0; I < Keys.Num(); ++I)
			{
				if (I > 0)
				{
					Out.AppendChar(TEXT(','));
				}
				if (bPretty)
				{
					Out.AppendChar(TEXT('\n'));
					AppendIndent(Out, Indent + 1);
				}
				WriteString(Keys[I], Out);
				Out.AppendChar(TEXT(':'));
				if (bPretty)
				{
					Out.AppendChar(TEXT(' '));
				}
				WriteValue(Value.GetValueAt(I), Out, Style, Indent + 1);
			}
			if (bPretty)
			{
				Out.AppendChar(TEXT('\n'));
				AppendIndent(Out, Indent);
			}
			Out.AppendChar(TEXT('}'));
			return;
		}
		}
	}

	/** Lector recursivo con límite de profundidad; nunca lee fuera del texto. */
	struct FParser
	{
		const TCHAR* Data = nullptr;
		int32 Len = 0;
		int32 Pos = 0;
		FString Error;

		bool Fail(const TCHAR* Message)
		{
			if (Error.IsEmpty())
			{
				Error = FString::Printf(TEXT("%s (posición %d)"), Message, Pos);
			}
			return false;
		}

		uint32 Peek() const { return Pos < Len ? CodeOf(Data[Pos]) : 0; }

		void SkipWhitespace()
		{
			while (Pos < Len)
			{
				const uint32 C = CodeOf(Data[Pos]);
				if (C != ' ' && C != '\t' && C != '\n' && C != '\r')
				{
					break;
				}
				++Pos;
			}
		}

		bool Expect(const ANSICHAR* Literal)
		{
			for (const ANSICHAR* P = Literal; *P; ++P)
			{
				if (Pos >= Len || CodeOf(Data[Pos]) != static_cast<uint32>(static_cast<uint8>(*P)))
				{
					return Fail(TEXT("literal no válido"));
				}
				++Pos;
			}
			return true;
		}

		bool ReadHex4(uint32& Out)
		{
			Out = 0;
			for (int32 I = 0; I < 4; ++I)
			{
				if (Pos >= Len)
				{
					return Fail(TEXT("escape \\u incompleto"));
				}
				const uint32 C = CodeOf(Data[Pos]);
				uint32 Digit = 0;
				if (C >= '0' && C <= '9') { Digit = C - '0'; }
				else if (C >= 'a' && C <= 'f') { Digit = C - 'a' + 10; }
				else if (C >= 'A' && C <= 'F') { Digit = C - 'A' + 10; }
				else { return Fail(TEXT("cifra hexadecimal no válida")); }
				Out = (Out << 4) | Digit;
				++Pos;
			}
			return true;
		}

		bool ParseString(FString& Out)
		{
			if (Peek() != '"')
			{
				return Fail(TEXT("se esperaba una cadena"));
			}
			++Pos;
			while (true)
			{
				if (Pos >= Len)
				{
					return Fail(TEXT("cadena sin cerrar"));
				}
				const TCHAR C = Data[Pos];
				const uint32 Code = CodeOf(C);
				if (Code == '"')
				{
					++Pos;
					return true;
				}
				if (Code < 0x20)
				{
					return Fail(TEXT("carácter de control dentro de una cadena"));
				}
				if (Code != '\\')
				{
					Out.AppendChar(C);
					++Pos;
					continue;
				}
				++Pos;
				if (Pos >= Len)
				{
					return Fail(TEXT("escape incompleto"));
				}
				const uint32 Escape = CodeOf(Data[Pos]);
				++Pos;
				switch (Escape)
				{
				case '"': Out.AppendChar(TEXT('"')); break;
				case '\\': Out.AppendChar(TEXT('\\')); break;
				case '/': Out.AppendChar(TEXT('/')); break;
				case 'b': Out.AppendChar(TEXT('\b')); break;
				case 'f': Out.AppendChar(TEXT('\f')); break;
				case 'n': Out.AppendChar(TEXT('\n')); break;
				case 'r': Out.AppendChar(TEXT('\r')); break;
				case 't': Out.AppendChar(TEXT('\t')); break;
				case 'u':
				{
					uint32 CodePoint = 0;
					if (!ReadHex4(CodePoint))
					{
						return false;
					}
					if (CodePoint >= 0xD800 && CodePoint <= 0xDBFF)
					{
						// Pareja sustituta: solo se une si le sigue un \u bajo; si no, U+FFFD.
						if (Pos + 1 < Len && CodeOf(Data[Pos]) == '\\' && CodeOf(Data[Pos + 1]) == 'u')
						{
							const int32 Saved = Pos;
							Pos += 2;
							uint32 Low = 0;
							if (!ReadHex4(Low))
							{
								return false;
							}
							if (Low >= 0xDC00 && Low <= 0xDFFF)
							{
								CodePoint = 0x10000 + ((CodePoint - 0xD800) << 10) + (Low - 0xDC00);
							}
							else
							{
								Pos = Saved;
								CodePoint = 0xFFFD;
							}
						}
						else
						{
							CodePoint = 0xFFFD;
						}
					}
					AppendCodePoint(Out, CodePoint);
					break;
				}
				default:
					return Fail(TEXT("escape no válido"));
				}
			}
		}

		bool IsDigitAt(int32 At) const
		{
			if (At >= Len)
			{
				return false;
			}
			const uint32 C = CodeOf(Data[At]);
			return C >= '0' && C <= '9';
		}

		bool ParseNumber(FSaveValue& Out)
		{
			const int32 Start = Pos;
			bool bNegative = false;
			if (Peek() == '-')
			{
				bNegative = true;
				++Pos;
			}
			if (Peek() == '0')
			{
				++Pos;
			}
			else if (IsDigitAt(Pos))
			{
				while (IsDigitAt(Pos)) { ++Pos; }
			}
			else
			{
				return Fail(TEXT("número no válido"));
			}
			bool bInteger = true;
			if (Peek() == '.')
			{
				bInteger = false;
				++Pos;
				if (!IsDigitAt(Pos))
				{
					return Fail(TEXT("faltan decimales"));
				}
				while (IsDigitAt(Pos)) { ++Pos; }
			}
			if (Peek() == 'e' || Peek() == 'E')
			{
				bInteger = false;
				++Pos;
				if (Peek() == '+' || Peek() == '-')
				{
					++Pos;
				}
				if (!IsDigitAt(Pos))
				{
					return Fail(TEXT("falta el exponente"));
				}
				while (IsDigitAt(Pos)) { ++Pos; }
			}
			const int32 Length = Pos - Start;
			if (Length >= 64)
			{
				return Fail(TEXT("número demasiado largo"));
			}

			if (bInteger)
			{
				// Magnitud con detección de desbordamiento; si no cabe en int64 pasa a real.
				const uint64 Limit = bNegative ? (static_cast<uint64>(1) << 63) : (static_cast<uint64>(1) << 63) - 1;
				uint64 Magnitude = 0;
				bool bOverflow = false;
				for (int32 I = Start + (bNegative ? 1 : 0); I < Pos; ++I)
				{
					const uint64 Digit = CodeOf(Data[I]) - '0';
					if (Magnitude > (Limit - Digit) / 10)
					{
						bOverflow = true;
						break;
					}
					Magnitude = Magnitude * 10 + Digit;
				}
				if (!bOverflow)
				{
					const int64 Value = bNegative ? static_cast<int64>(~Magnitude + 1) : static_cast<int64>(Magnitude);
					Out = FSaveValue::MakeInt(Value);
					return true;
				}
			}

			FString Number;
			for (int32 I = Start; I < Pos; ++I)
			{
				Number.AppendChar(Data[I]);
			}
			const double Value = FCString::Atod(*Number);
			if (!FMath::IsFinite(Value))
			{
				return Fail(TEXT("número fuera de rango"));
			}
			Out = FSaveValue::MakeDouble(Value);
			return true;
		}

		bool ParseValue(FSaveValue& Out, int32 Depth)
		{
			SkipWhitespace();
			if (Pos >= Len)
			{
				return Fail(TEXT("fin inesperado del texto"));
			}
			const uint32 C = Peek();
			switch (C)
			{
			case '{':
			{
				if (Depth >= FSaveText::MaxDepth)
				{
					return Fail(TEXT("anidamiento excesivo"));
				}
				++Pos;
				Out = FSaveValue::MakeObject();
				SkipWhitespace();
				if (Peek() == '}')
				{
					++Pos;
					return true;
				}
				while (true)
				{
					SkipWhitespace();
					FString Key;
					if (!ParseString(Key))
					{
						return false;
					}
					SkipWhitespace();
					if (Peek() != ':')
					{
						return Fail(TEXT("se esperaba «:»"));
					}
					++Pos;
					FSaveValue Child;
					if (!ParseValue(Child, Depth + 1))
					{
						return false;
					}
					if (Out.Contains(Key))
					{
						return Fail(TEXT("clave duplicada"));
					}
					Out.Set(Key, MoveTemp(Child));
					SkipWhitespace();
					if (Peek() == ',')
					{
						++Pos;
						continue;
					}
					if (Peek() == '}')
					{
						++Pos;
						return true;
					}
					return Fail(TEXT("se esperaba «,» o «}»"));
				}
			}
			case '[':
			{
				if (Depth >= FSaveText::MaxDepth)
				{
					return Fail(TEXT("anidamiento excesivo"));
				}
				++Pos;
				Out = FSaveValue::MakeArray();
				SkipWhitespace();
				if (Peek() == ']')
				{
					++Pos;
					return true;
				}
				while (true)
				{
					FSaveValue Child;
					if (!ParseValue(Child, Depth + 1))
					{
						return false;
					}
					Out.Add(MoveTemp(Child));
					SkipWhitespace();
					if (Peek() == ',')
					{
						++Pos;
						continue;
					}
					if (Peek() == ']')
					{
						++Pos;
						return true;
					}
					return Fail(TEXT("se esperaba «,» o «]»"));
				}
			}
			case '"':
			{
				FString Text;
				if (!ParseString(Text))
				{
					return false;
				}
				Out = FSaveValue::MakeString(Text);
				return true;
			}
			case 't':
				Out = FSaveValue::MakeBool(true);
				return Expect("true");
			case 'f':
				Out = FSaveValue::MakeBool(false);
				return Expect("false");
			case 'n':
				Out = FSaveValue();
				return Expect("null");
			default:
				if (C == '-' || (C >= '0' && C <= '9'))
				{
					return ParseNumber(Out);
				}
				return Fail(TEXT("carácter inesperado"));
			}
		}
	};

	/** Real a partir de su patrón de bits (NaN e infinitos sin depender de <limits>). */
	double BitsToDouble(uint64 Bits)
	{
		double Value = 0.0;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	void HashByte(uint64& Hash, uint32 Byte)
	{
		Hash ^= static_cast<uint64>(Byte & 0xFF);
		Hash *= 0x100000001B3ULL;
	}
}

// ---------------------------------------------------------------------------
// FSaveValue
// ---------------------------------------------------------------------------

FSaveValue FSaveValue::MakeBool(bool bValue)
{
	FSaveValue V;
	V.Type = ESaveValueType::Bool;
	V.bBool = bValue;
	return V;
}

FSaveValue FSaveValue::MakeInt(int64 Value)
{
	FSaveValue V;
	V.Type = ESaveValueType::Int;
	V.IntValue = Value;
	return V;
}

FSaveValue FSaveValue::MakeDouble(double Value)
{
	FSaveValue V;
	V.Type = ESaveValueType::Double;
	V.DoubleValue = Value;
	return V;
}

FSaveValue FSaveValue::MakeFloat(float Value)
{
	FSaveValue V = MakeDouble(static_cast<double>(Value));
	V.bFloatHint = true;
	return V;
}

FSaveValue FSaveValue::MakeString(const FString& Value)
{
	FSaveValue V;
	V.Type = ESaveValueType::String;
	V.StringValue = Value;
	return V;
}

FSaveValue FSaveValue::MakeArray()
{
	FSaveValue V;
	V.Type = ESaveValueType::Array;
	return V;
}

FSaveValue FSaveValue::MakeObject()
{
	FSaveValue V;
	V.Type = ESaveValueType::Object;
	return V;
}

const FSaveValue& FSaveValue::NullValue()
{
	static const FSaveValue Null;
	return Null;
}

bool FSaveValue::TryGetBool(bool& Out) const
{
	if (Type != ESaveValueType::Bool)
	{
		return false;
	}
	Out = bBool;
	return true;
}

bool FSaveValue::TryGetInt(int64& Out) const
{
	if (Type == ESaveValueType::Int)
	{
		Out = IntValue;
		return true;
	}
	if (Type == ESaveValueType::Double && FMath::IsFinite(DoubleValue)
		&& FMath::FloorToDouble(DoubleValue) == DoubleValue
		&& DoubleValue >= -9223372036854775808.0 && DoubleValue < 9223372036854775808.0)
	{
		Out = static_cast<int64>(DoubleValue);
		return true;
	}
	return false;
}

bool FSaveValue::TryGetDouble(double& Out) const
{
	switch (Type)
	{
	case ESaveValueType::Int:
		Out = static_cast<double>(IntValue);
		return true;
	case ESaveValueType::Double:
		Out = DoubleValue;
		return true;
	case ESaveValueType::String:
		if (CompareKeys(StringValue, TEXT("NaN")) == 0)
		{
			Out = SaveValueDetail::BitsToDouble(0x7FF8000000000000ULL);
			return true;
		}
		if (CompareKeys(StringValue, TEXT("Infinity")) == 0)
		{
			Out = SaveValueDetail::BitsToDouble(0x7FF0000000000000ULL);
			return true;
		}
		if (CompareKeys(StringValue, TEXT("-Infinity")) == 0)
		{
			Out = SaveValueDetail::BitsToDouble(0xFFF0000000000000ULL);
			return true;
		}
		return false;
	default:
		return false;
	}
}

bool FSaveValue::TryGetString(FString& Out) const
{
	if (Type != ESaveValueType::String)
	{
		return false;
	}
	Out = StringValue;
	return true;
}

bool FSaveValue::AsBool(bool Default) const
{
	bool V = Default;
	TryGetBool(V);
	return V;
}

int64 FSaveValue::AsInt(int64 Default) const
{
	int64 V = Default;
	TryGetInt(V);
	return V;
}

double FSaveValue::AsDouble(double Default) const
{
	double V = Default;
	TryGetDouble(V);
	return V;
}

const FString& FSaveValue::AsString() const
{
	static const FString Empty;
	return Type == ESaveValueType::String ? StringValue : Empty;
}

int32 FSaveValue::Num() const
{
	return (Type == ESaveValueType::Array || Type == ESaveValueType::Object) ? Values.Num() : 0;
}

const FSaveValue& FSaveValue::At(int32 Index) const
{
	if ((Type != ESaveValueType::Array && Type != ESaveValueType::Object) || !Values.IsValidIndex(Index))
	{
		return NullValue();
	}
	return Values[Index];
}

FSaveValue* FSaveValue::AtMutable(int32 Index)
{
	if ((Type != ESaveValueType::Array && Type != ESaveValueType::Object) || !Values.IsValidIndex(Index))
	{
		return nullptr;
	}
	return &Values[Index];
}

FSaveValue& FSaveValue::Add(FSaveValue Value)
{
	if (Type != ESaveValueType::Array)
	{
		*this = MakeArray();
	}
	const int32 Index = Values.Add(MoveTemp(Value));
	return Values[Index];
}

int32 FSaveValue::CompareKeys(const FString& A, const FString& B)
{
	const TCHAR* PA = *A;
	const TCHAR* PB = *B;
	const int32 LenA = A.Len();
	const int32 LenB = B.Len();
	const int32 Common = FMath::Min(LenA, LenB);
	for (int32 I = 0; I < Common; ++I)
	{
		const uint32 CA = SaveValueDetail::CodeOf(PA[I]);
		const uint32 CB = SaveValueDetail::CodeOf(PB[I]);
		if (CA != CB)
		{
			return CA < CB ? -1 : 1;
		}
	}
	return LenA == LenB ? 0 : (LenA < LenB ? -1 : 1);
}

int32 FSaveValue::FindKeyIndex(const FString& Key, bool& bOutFound) const
{
	int32 Lo = 0;
	int32 Hi = Keys.Num();
	while (Lo < Hi)
	{
		const int32 Mid = Lo + (Hi - Lo) / 2;
		const int32 Cmp = CompareKeys(Keys[Mid], Key);
		if (Cmp == 0)
		{
			bOutFound = true;
			return Mid;
		}
		if (Cmp < 0)
		{
			Lo = Mid + 1;
		}
		else
		{
			Hi = Mid;
		}
	}
	bOutFound = false;
	return Lo;
}

const FSaveValue* FSaveValue::Find(const FString& Key) const
{
	if (Type != ESaveValueType::Object)
	{
		return nullptr;
	}
	bool bFound = false;
	const int32 Index = FindKeyIndex(Key, bFound);
	return bFound ? &Values[Index] : nullptr;
}

FSaveValue* FSaveValue::Find(const FString& Key)
{
	return const_cast<FSaveValue*>(static_cast<const FSaveValue*>(this)->Find(Key));
}

FSaveValue& FSaveValue::Set(const FString& Key, FSaveValue Value)
{
	if (Type != ESaveValueType::Object)
	{
		*this = MakeObject();
	}
	bool bFound = false;
	const int32 Index = FindKeyIndex(Key, bFound);
	if (bFound)
	{
		Values[Index] = MoveTemp(Value);
	}
	else
	{
		Keys.Insert(Key, Index);
		Values.Insert(MoveTemp(Value), Index);
	}
	return Values[Index];
}

bool FSaveValue::Remove(const FString& Key)
{
	if (Type != ESaveValueType::Object)
	{
		return false;
	}
	bool bFound = false;
	const int32 Index = FindKeyIndex(Key, bFound);
	if (!bFound)
	{
		return false;
	}
	Keys.RemoveAt(Index);
	Values.RemoveAt(Index);
	return true;
}

bool FSaveValue::operator==(const FSaveValue& Other) const
{
	if (Type != Other.Type)
	{
		return false;
	}
	switch (Type)
	{
	case ESaveValueType::Null: return true;
	case ESaveValueType::Bool: return bBool == Other.bBool;
	case ESaveValueType::Int: return IntValue == Other.IntValue;
	case ESaveValueType::Double:
		if (FMath::IsNaN(DoubleValue) || FMath::IsNaN(Other.DoubleValue))
		{
			return FMath::IsNaN(DoubleValue) && FMath::IsNaN(Other.DoubleValue);
		}
		// Un real marcado como float solo guarda precisión de float: se compara a esa precisión.
		if (bFloatHint || Other.bFloatHint)
		{
			return FSaveText::DoubleToFloat(DoubleValue) == FSaveText::DoubleToFloat(Other.DoubleValue);
		}
		return DoubleValue == Other.DoubleValue;
	// Ordinal: el operador == de FString en Unreal no distingue mayúsculas.
	case ESaveValueType::String: return CompareKeys(StringValue, Other.StringValue) == 0;
	case ESaveValueType::Array:
	case ESaveValueType::Object:
		if (Keys.Num() != Other.Keys.Num() || Values.Num() != Other.Values.Num())
		{
			return false;
		}
		for (int32 I = 0; I < Keys.Num(); ++I)
		{
			if (CompareKeys(Keys[I], Other.Keys[I]) != 0)
			{
				return false;
			}
		}
		for (int32 I = 0; I < Values.Num(); ++I)
		{
			if (Values[I] != Other.Values[I])
			{
				return false;
			}
		}
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// FSaveText
// ---------------------------------------------------------------------------

FString FSaveText::FormatDouble(double Value)
{
	// Desde 15 cifras: así el texto de un float leído como doble se reescribe
	// idéntico (la suma de control se calcula sobre esa reescritura).
	FString Best;
	for (int32 Precision = 15; Precision <= 17; ++Precision)
	{
		Best = SaveValueDetail::Canonical(SaveValueDetail::Scientific(Value, Precision));
		if (FCString::Atod(*Best) == Value)
		{
			break;
		}
	}
	return Best;
}

float FSaveText::DoubleToFloat(double Value)
{
	// Mayor float + medio ulp: por debajo, la conversión redondea a un float finito.
	constexpr double FloatOverflow = 3.4028235677973366e38;
	if (Value >= FloatOverflow)
	{
		return static_cast<float>(SaveValueDetail::BitsToDouble(0x7FF0000000000000ULL));
	}
	if (Value <= -FloatOverflow)
	{
		return static_cast<float>(SaveValueDetail::BitsToDouble(0xFFF0000000000000ULL));
	}
	return static_cast<float>(Value);
}

FString FSaveText::FormatFloat(float Value)
{
	FString Best;
	const double AsDouble = static_cast<double>(Value);
	for (int32 Precision = 1; Precision <= 9; ++Precision)
	{
		Best = SaveValueDetail::Canonical(SaveValueDetail::Scientific(AsDouble, Precision));
		if (DoubleToFloat(FCString::Atod(*Best)) == Value)
		{
			break;
		}
	}
	return Best;
}

FString FSaveText::Write(const FSaveValue& Value, ESaveTextStyle Style)
{
	FString Out;
	SaveValueDetail::WriteValue(Value, Out, Style, 0);
	if (Style == ESaveTextStyle::Pretty)
	{
		Out.AppendChar(TEXT('\n'));
	}
	return Out;
}

bool FSaveText::Parse(const FString& Text, FSaveValue& OutValue, FString& OutError)
{
	SaveValueDetail::FParser Parser;
	Parser.Data = *Text;
	Parser.Len = Text.Len();

	// Se tolera una marca de orden de bytes al principio (U+FEFF).
	if (Parser.Len > 0 && SaveValueDetail::CodeOf(Parser.Data[0]) == 0xFEFF)
	{
		Parser.Pos = 1;
	}
	else if (sizeof(TCHAR) == 1 && Parser.Len >= 3 && SaveValueDetail::CodeOf(Parser.Data[0]) == 0xEF
		&& SaveValueDetail::CodeOf(Parser.Data[1]) == 0xBB && SaveValueDetail::CodeOf(Parser.Data[2]) == 0xBF)
	{
		Parser.Pos = 3;
	}

	FSaveValue Result;
	if (!Parser.ParseValue(Result, 0))
	{
		OutError = Parser.Error;
		return false;
	}
	Parser.SkipWhitespace();
	if (Parser.Pos != Parser.Len)
	{
		Parser.Fail(TEXT("texto sobrante tras el valor"));
		OutError = Parser.Error;
		return false;
	}
	OutValue = MoveTemp(Result);
	OutError.Reset();
	return true;
}

// ---------------------------------------------------------------------------
// FSaveChecksum
// ---------------------------------------------------------------------------

uint64 FSaveChecksum::Compute(const FString& Text)
{
	uint64 Hash = 0xCBF29CE484222325ULL;
	const TCHAR* P = *Text;
	const int32 Len = Text.Len();
	for (int32 I = 0; I < Len; ++I)
	{
		uint32 Code = SaveValueDetail::CodeOf(P[I]);
		if constexpr (sizeof(TCHAR) == 1)
		{
			// El texto ya es UTF-8: se suman los bytes tal cual.
			SaveValueDetail::HashByte(Hash, Code);
			continue;
		}
		if (sizeof(TCHAR) == 2 && Code >= 0xD800 && Code <= 0xDBFF && I + 1 < Len)
		{
			const uint32 Low = SaveValueDetail::CodeOf(P[I + 1]);
			if (Low >= 0xDC00 && Low <= 0xDFFF)
			{
				Code = 0x10000 + ((Code - 0xD800) << 10) + (Low - 0xDC00);
				++I;
			}
		}
		if (Code < 0x80)
		{
			SaveValueDetail::HashByte(Hash, Code);
		}
		else if (Code < 0x800)
		{
			SaveValueDetail::HashByte(Hash, 0xC0 | (Code >> 6));
			SaveValueDetail::HashByte(Hash, 0x80 | (Code & 0x3F));
		}
		else if (Code < 0x10000)
		{
			SaveValueDetail::HashByte(Hash, 0xE0 | (Code >> 12));
			SaveValueDetail::HashByte(Hash, 0x80 | ((Code >> 6) & 0x3F));
			SaveValueDetail::HashByte(Hash, 0x80 | (Code & 0x3F));
		}
		else
		{
			SaveValueDetail::HashByte(Hash, 0xF0 | (Code >> 18));
			SaveValueDetail::HashByte(Hash, 0x80 | ((Code >> 12) & 0x3F));
			SaveValueDetail::HashByte(Hash, 0x80 | ((Code >> 6) & 0x3F));
			SaveValueDetail::HashByte(Hash, 0x80 | (Code & 0x3F));
		}
	}
	return Hash;
}

FString FSaveChecksum::ToHex(uint64 Value)
{
	static const ANSICHAR* const Hex = "0123456789abcdef";
	FString Out;
	for (int32 Shift = 60; Shift >= 0; Shift -= 4)
	{
		Out.AppendChar(static_cast<TCHAR>(Hex[(Value >> Shift) & 0xF]));
	}
	return Out;
}

bool FSaveChecksum::FromHex(const FString& Text, uint64& OutValue)
{
	if (Text.Len() != 16)
	{
		return false;
	}
	uint64 Value = 0;
	const TCHAR* P = *Text;
	for (int32 I = 0; I < 16; ++I)
	{
		const uint32 C = SaveValueDetail::CodeOf(P[I]);
		uint64 Digit = 0;
		if (C >= '0' && C <= '9') { Digit = C - '0'; }
		else if (C >= 'a' && C <= 'f') { Digit = C - 'a' + 10; }
		else if (C >= 'A' && C <= 'F') { Digit = C - 'A' + 10; }
		else { return false; }
		Value = (Value << 4) | Digit;
	}
	OutValue = Value;
	return true;
}
