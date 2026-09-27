#include "Misc/AutomationTest.h"

#include "Core/ExploredRandom.h"
#include "Save/SaveArchive.h"
#include "Save/SaveFormat.h"
#include "Save/SaveValue.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SaveArchiveTest
{
	enum class ETide : uint8
	{
		Low,
		High,
		Spring,
	};
}

template <>
struct TSaveEnumNames<SaveArchiveTest::ETide>
{
	static constexpr const TCHAR* Names[] = { TEXT("Low"), TEXT("High"), TEXT("Spring") };
};

namespace SaveArchiveTest
{
	FSaveValue RoundTrip(const FSaveValue& Value, ESaveTextStyle Style)
	{
		FSaveValue Out;
		FString Error;
		FSaveText::Parse(FSaveText::Write(Value, Style), Out, Error);
		return Out;
	}

	/** Documento con todos los tipos para el fuzzing y los viajes de ida y vuelta. */
	FSaveValue MakeRichValue()
	{
		FSaveValue Root = FSaveValue::MakeObject();
		Root.Set(TEXT("nulo"), FSaveValue());
		Root.Set(TEXT("sí"), FSaveValue::MakeBool(true));
		Root.Set(TEXT("no"), FSaveValue::MakeBool(false));
		Root.Set(TEXT("entero"), FSaveValue::MakeInt(-1234567890123LL));
		Root.Set(TEXT("real"), FSaveValue::MakeDouble(0.1 + 0.2));
		Root.Set(TEXT("float"), FSaveValue::MakeFloat(3.14159274f));
		Root.Set(TEXT("texto"), FSaveValue::MakeString(TEXT("Mañana: «Limón» bajo el árbol 🐢\n\t\"cita\" \\ fin")));
		FSaveValue List = FSaveValue::MakeArray();
		List.Add(FSaveValue::MakeDouble(1.5e300));
		List.Add(FSaveValue::MakeDouble(-2.5e-310));
		List.Add(FSaveValue::MakeInt(0));
		FSaveValue Nested = FSaveValue::MakeObject();
		Nested.Set(TEXT("isla"), FSaveValue::MakeString(TEXT("Esmeralda")));
		Nested.Set(TEXT("vacía"), FSaveValue::MakeArray());
		List.Add(Nested);
		Root.Set(TEXT("lista"), List);
		return Root;
	}

	/** Mutador determinista: cambia, borra, inserta, duplica o trunca caracteres. */
	FString Mutate(const FString& Source, FExploredRandom& Random)
	{
		static const TCHAR Interesting[] = {
			TEXT('{'), TEXT('}'), TEXT('['), TEXT(']'), TEXT('"'), TEXT('\\'), TEXT(','), TEXT(':'),
			TEXT('-'), TEXT('0'), TEXT('9'), TEXT('e'), TEXT('.'), TEXT('u'), TEXT('t'), TEXT('n'),
			TEXT(' '), TEXT('\n'), static_cast<TCHAR>(0), static_cast<TCHAR>(0x7F), static_cast<TCHAR>(0xC3),
		};
		constexpr int32 NumInteresting = UE_ARRAY_COUNT(Interesting);

		FString Text = Source;
		const int32 Mutations = Random.RangeInt(1, 6);
		for (int32 M = 0; M < Mutations; ++M)
		{
			const int32 Len = Text.Len();
			const int32 At = Len > 0 ? Random.RangeInt(0, Len - 1) : 0;
			const TCHAR Char = Interesting[Random.RangeInt(0, NumInteresting - 1)];
			switch (Random.RangeInt(0, 4))
			{
			case 0:
				if (Len > 0)
				{
					FString Changed = Text.Left(At);
					Changed.AppendChar(Char);
					Text = Changed + Text.Mid(At + 1);
				}
				break;
			case 1:
				if (Len > 0)
				{
					Text = Text.Left(At) + Text.Mid(At + 1);
				}
				break;
			case 2:
			{
				FString Changed = Text.Left(At);
				Changed.AppendChar(Char);
				Text = Changed + Text.Mid(At);
				break;
			}
			case 3:
			{
				const int32 Count = Random.RangeInt(1, 12);
				Text = Text.Left(At) + Text.Mid(At, Count) + Text.Mid(At);
				break;
			}
			default:
				Text = Text.Left(At);
				break;
			}
		}
		return Text;
	}
}

BEGIN_DEFINE_SPEC(FSaveArchiveSpec, "Explored.Save.Archive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FSaveArchiveSpec)

void FSaveArchiveSpec::Define()
{
	using namespace SaveArchiveTest;

	Describe(TEXT("el texto"), [this]()
	{
		It("conserva cada tipo escalar en los dos estilos", [this]()
		{
			TArray<FSaveValue> Values;
			Values.Add(FSaveValue());
			Values.Add(FSaveValue::MakeBool(true));
			Values.Add(FSaveValue::MakeBool(false));
			Values.Add(FSaveValue::MakeInt(0));
			Values.Add(FSaveValue::MakeInt(TNumericLimits<int64>::Max()));
			Values.Add(FSaveValue::MakeInt(TNumericLimits<int64>::Min()));
			Values.Add(FSaveValue::MakeDouble(1.0));
			Values.Add(FSaveValue::MakeDouble(-0.0));
			Values.Add(FSaveValue::MakeDouble(0.1));
			Values.Add(FSaveValue::MakeDouble(1.0 / 3.0));
			Values.Add(FSaveValue::MakeDouble(0.1 + 0.2));
			Values.Add(FSaveValue::MakeDouble(123456789012345678.0));
			Values.Add(FSaveValue::MakeDouble(TNumericLimits<double>::Max()));
			Values.Add(FSaveValue::MakeDouble(TNumericLimits<double>::Min()));
			Values.Add(FSaveValue::MakeDouble(4.9406564584124654e-324));
			Values.Add(FSaveValue::MakeDouble(-6.02214076e23));
			Values.Add(FSaveValue::MakeString(TEXT("")));
			Values.Add(FSaveValue::MakeString(TEXT("hola")));
			for (const ESaveTextStyle Style : { ESaveTextStyle::Compact, ESaveTextStyle::Pretty })
			{
				for (const FSaveValue& Value : Values)
				{
					const FString Text = FSaveText::Write(Value, Style);
					TestTrue(FString::Printf(TEXT("Ida y vuelta de %s"), *Text), RoundTrip(Value, Style) == Value);
				}
			}
		});

		It("escribe los reales con la forma más corta que vuelve al mismo valor", [this]()
		{
			TestEqual(TEXT("0.1"), FSaveText::FormatDouble(0.1), TEXT("0.1"));
			TestEqual(TEXT("1.0 lleva punto"), FSaveText::FormatDouble(1.0), TEXT("1.0"));
			TestEqual(TEXT("0.1 + 0.2 necesita 17 cifras"), FSaveText::FormatDouble(0.1 + 0.2), TEXT("0.30000000000000004"));
			TestEqual(TEXT("Exponente grande"), FSaveText::FormatDouble(1.5e300), TEXT("1.5e300"));
			TestEqual(TEXT("Exponente pequeño"), FSaveText::FormatDouble(2.5e-7), TEXT("2.5e-7"));
			TestEqual(TEXT("Float corto"), FSaveText::FormatFloat(0.1f), TEXT("0.1"));
			TestEqual(TEXT("Float con 9 cifras"), FSaveText::FormatFloat(3.14159274f), TEXT("3.1415927"));

			const FSaveValue Integral = RoundTrip(FSaveValue::MakeDouble(42.0), ESaveTextStyle::Compact);
			TestTrue(TEXT("Un real entero sigue siendo real"), Integral.IsDouble());
		});

		It("conserva exactamente miles de reales y floats aleatorios", [this]()
		{
			FExploredRandom Random(20260926);
			int32 Failures = 0;
			for (int32 I = 0; I < 4000; ++I)
			{
				const uint64 Bits = (static_cast<uint64>(Random.NextUInt32()) << 32) | Random.NextUInt32();
				double D = 0.0;
				FMemory::Memcpy(&D, &Bits, sizeof(D));
				if (!FMath::IsFinite(D))
				{
					continue;
				}
				const float F = static_cast<float>(Random.RangeFloat(-1.0e6f, 1.0e6f)) * Random.NextFloat();

				double ReadD = 0.0;
				RoundTrip(FSaveValue::MakeDouble(D), ESaveTextStyle::Compact).TryGetDouble(ReadD);
				float ReadF = 0.0f;
				TSaveTraits<float>::FromValue(RoundTrip(FSaveValue::MakeFloat(F), ESaveTextStyle::Compact), ReadF);
				Failures += (ReadD == D ? 0 : 1) + (ReadF == F ? 0 : 1);

				// Punto fijo: reescribir lo leído da el mismo texto (la suma de control depende de ello).
				const FString FloatText = FSaveText::Write(FSaveValue::MakeFloat(F), ESaveTextStyle::Compact);
				FSaveValue Parsed;
				FString Error;
				FSaveText::Parse(FloatText, Parsed, Error);
				Failures += FSaveText::Write(Parsed, ESaveTextStyle::Compact) == FloatText ? 0 : 1;
			}
			TestEqual(TEXT("Reales que no vuelven exactos"), Failures, 0);
		});

		It("guarda NaN e infinitos como cadenas y los lee como reales", [this]()
		{
			const double Inf = TNumericLimits<double>::Max() * 2.0;
			const FString Text = FSaveText::Write(FSaveValue::MakeDouble(-Inf), ESaveTextStyle::Compact);
			TestEqual(TEXT("Menos infinito"), Text, TEXT("\"-Infinity\""));
			double Read = 0.0;
			TestTrue(TEXT("Se lee como real"), RoundTrip(FSaveValue::MakeDouble(-Inf), ESaveTextStyle::Compact).TryGetDouble(Read));
			TestTrue(TEXT("Es menos infinito"), Read < -TNumericLimits<double>::Max());
			const double NaN = Inf - Inf;
			TestTrue(TEXT("NaN"), FMath::IsNaN(RoundTrip(FSaveValue::MakeDouble(NaN), ESaveTextStyle::Compact).AsDouble()));
		});

		It("conserva cadenas con tildes, eñes, emojis, comillas y controles", [this]()
		{
			const FString Original = TEXT("Año nuevo en la isla: canción, pingüino, «Limón» ☀ 🐢 \"x\" \\ /\n\t\r\b\f");
			FString WithControl = Original;
			WithControl.AppendChar(static_cast<TCHAR>(1));
			const FSaveValue Value = FSaveValue::MakeString(WithControl);
			const FString Text = FSaveText::Write(Value, ESaveTextStyle::Compact);
			TestTrue(TEXT("Las tildes van tal cual"), Text.Contains(TEXT("canción, pingüino")));
			TestTrue(TEXT("Los controles se escapan"), Text.Contains(TEXT("\\u0001")));
			TestTrue(TEXT("Ida y vuelta"), RoundTrip(Value, ESaveTextStyle::Compact).AsString().Equals(WithControl, ESearchCase::CaseSensitive));
		});

		It("lee escapes \\u con parejas sustitutas", [this]()
		{
			FSaveValue Value;
			FString Error;
			TestTrue(TEXT("Lee"), FSaveText::Parse(TEXT("\"a\\u00f1o \\ud83d\\udc22\""), Value, Error));
			TestTrue(TEXT("ñ y tortuga"), Value.AsString().Equals(FString(TEXT("año 🐢")), ESearchCase::CaseSensitive));
			TestTrue(TEXT("Sustituta suelta"), FSaveText::Parse(TEXT("\"\\ud800x\""), Value, Error));
			TestTrue(TEXT("Se cambia por U+FFFD"), Value.AsString().Equals(FString(TEXT("\uFFFDx")), ESearchCase::CaseSensitive));
		});

		It("ordena las claves y produce el mismo texto sin importar el orden de escritura", [this]()
		{
			FSaveValue A = FSaveValue::MakeObject();
			A.Set(TEXT("zeta"), FSaveValue::MakeInt(1));
			A.Set(TEXT("alfa"), FSaveValue::MakeInt(2));
			A.Set(TEXT("Beta"), FSaveValue::MakeInt(3));
			FSaveValue B = FSaveValue::MakeObject();
			B.Set(TEXT("Beta"), FSaveValue::MakeInt(3));
			B.Set(TEXT("alfa"), FSaveValue::MakeInt(2));
			B.Set(TEXT("zeta"), FSaveValue::MakeInt(1));
			TestEqual(TEXT("Mismo texto"), FSaveText::Write(A, ESaveTextStyle::Pretty), FSaveText::Write(B, ESaveTextStyle::Pretty));
			TestEqual(TEXT("Orden ordinal"), FSaveText::Write(A, ESaveTextStyle::Compact), TEXT("{\"Beta\":3,\"alfa\":2,\"zeta\":1}"));
		});

		It("escribe un documento legible y lo vuelve a leer igual", [this]()
		{
			const FSaveValue Rich = MakeRichValue();
			const FString Pretty = FSaveText::Write(Rich, ESaveTextStyle::Pretty);
			TestTrue(TEXT("Sangrado con tabuladores"), Pretty.Contains(TEXT("\n\t\"entero\": -1234567890123")));
			TestTrue(TEXT("Listas de escalares en una línea"), FSaveText::Write(FSaveValue::MakeArray(), ESaveTextStyle::Pretty).StartsWith(TEXT("[]")));
			TestTrue(TEXT("Legible → igual"), RoundTrip(Rich, ESaveTextStyle::Pretty) == Rich);
			TestTrue(TEXT("Compacto → igual"), RoundTrip(Rich, ESaveTextStyle::Compact) == Rich);
		});

		It("rechaza entradas mal formadas con un mensaje, sin abortar", [this]()
		{
			TArray<FString> Bad = {
				TEXT(""), TEXT("   "), TEXT("{"), TEXT("}"), TEXT("[1,]"), TEXT("[1 2]"), TEXT("{\"a\":}"),
				TEXT("{\"a\" 1}"), TEXT("{a:1}"), TEXT("tru"), TEXT("nul"), TEXT("01"), TEXT("1."), TEXT("-"),
				TEXT(".5"), TEXT("1e"), TEXT("1e999"), TEXT("\"abc"), TEXT("\"\\x\""), TEXT("\"\\u12\""),
				TEXT("{\"a\":1,\"a\":2}"), TEXT("1 2"), TEXT("[\"\n\"]"), TEXT("NaN"), TEXT("'a'"),
				TEXT("123456789012345678901234567890123456789012345678901234567890123456789.5"),
			};
			FString Deep;
			for (int32 I = 0; I < 5000; ++I)
			{
				Deep.AppendChar(TEXT('['));
			}
			Bad.Add(Deep);
			// AppendChar descarta el carácter nulo en UE: se escribe directamente en el buffer.
			FString WithNull = TEXT("[1,x2]");
			WithNull[3] = static_cast<TCHAR>(0);
			Bad.Add(WithNull);

			for (const FString& Text : Bad)
			{
				FSaveValue Value = FSaveValue::MakeInt(7);
				FString Error;
				const bool bOk = FSaveText::Parse(Text, Value, Error);
				TestFalse(FString::Printf(TEXT("Rechaza «%s»"), *Text.Left(20)), bOk);
				TestFalse(TEXT("Con mensaje"), Error.IsEmpty());
				TestTrue(TEXT("No toca la salida"), Value == FSaveValue::MakeInt(7));
			}

			FSaveValue Value;
			FString Error;
			TestTrue(TEXT("Un número enorme sin decimales pasa a real"), FSaveText::Parse(TEXT("99999999999999999999"), Value, Error) && Value.IsDouble());
			TestTrue(TEXT("Admite la marca UTF-8 inicial"), FSaveText::Parse(TEXT("\uFEFF[1]"), Value, Error));
		});

		It("resiste miles de mutaciones aleatorias de documentos válidos", [this]()
		{
			FSaveDocument Document;
			Document.Header.Seed = 20260926;
			Document.Header.SlotId = TEXT("auto");
			Document.Sections.Set(TEXT("rico"), MakeRichValue());
			const FString Encoded = FSaveCodec::Encode(Document);
			const FString Compact = FSaveText::Write(MakeRichValue(), ESaveTextStyle::Compact);
			const FSaveMigrations NoMigrations;

			FExploredRandom Random(0x5A7E);
			int32 Parsed = 0;
			int32 FixedPointFailures = 0;
			int32 Accepted = 0;
			for (int32 I = 0; I < 3000; ++I)
			{
				const FString Mutated = Mutate(I % 2 == 0 ? Encoded : Compact, Random);
				FSaveValue Value;
				FString Error;
				if (FSaveText::Parse(Mutated, Value, Error))
				{
					++Parsed;
					const FString Once = FSaveText::Write(Value, ESaveTextStyle::Compact);
					FSaveValue Again;
					FString AgainError;
					FSaveText::Parse(Once, Again, AgainError);
					FixedPointFailures += FSaveText::Write(Again, ESaveTextStyle::Compact) == Once ? 0 : 1;
				}
				else
				{
					FixedPointFailures += Error.IsEmpty() ? 1 : 0;
				}
				FSaveDocument Out;
				FString DecodeError;
				Accepted += FSaveCodec::Decode(Mutated, NoMigrations, ExploredSave::CurrentFormatVersion, Out, DecodeError) == ESaveLoadResult::Ok ? 1 : 0;
			}
			TestEqual(TEXT("Lecturas válidas que no son punto fijo"), FixedPointFailures, 0);
			TestTrue(TEXT("Algunas mutaciones siguen siendo texto válido"), Parsed > 0);
			TestTrue(TEXT("La suma de control rechaza casi todas las partidas mutadas"), Accepted < 300);
		});
	});

	Describe(TEXT("el archivo de sección"), [this]()
	{
		It("guarda y lee los tipos comunes", [this]()
		{
			FSaveArchive Ar;
			Ar.Write(TEXT("pos"), FVector(1.5, -2.25, 1.0e6 + 0.125));
			Ar.Write(TEXT("uv"), FVector2D(0.1, 0.7));
			Ar.Write(TEXT("rot"), FRotator(-10.0, 270.5, 0.0));
			Ar.Write(TEXT("celda"), FIntPoint(-3, 12));
			Ar.Write(TEXT("id"), FName(TEXT("petro_07")));
			Ar.Write(TEXT("nombre"), FString(TEXT("Isla Esmeralda")));
			Ar.Write(TEXT("literal"), TEXT("ñandú"));
			Ar.Write(TEXT("semilla"), static_cast<uint64>(0xFEDCBA9876543210ULL));
			Ar.Write(TEXT("vida"), 87.25f);
			Ar.Write(TEXT("marea"), ETide::Spring);
			Ar.Write(TEXT("lista"), TArray<int32>({ 3, 1, 4, 1, 5 }));
			Ar.Write(TEXT("puntos"), TArray<FVector>({ FVector(1.0, 2.0, 3.0), FVector::ZeroVector }));
			TMap<FString, int32> Stock;
			Stock.Add(TEXT("coco"), 3);
			Stock.Add(TEXT("bambú"), 12);
			Ar.Write(TEXT("stock"), Stock);
			TMap<int32, FString> ByDay;
			ByDay.Add(12, TEXT("luna llena"));
			ByDay.Add(3, TEXT("desove"));
			Ar.Write(TEXT("porDía"), ByDay);

			FSaveValue Parsed;
			FString Error;
			TestTrue(TEXT("Texto válido"), FSaveText::Parse(FSaveText::Write(Ar.GetRoot()), Parsed, Error));
			const FSaveArchive Read(Parsed);

			TestEqual(TEXT("FVector"), Read.ReadOr(TEXT("pos"), FVector::ZeroVector), FVector(1.5, -2.25, 1.0e6 + 0.125));
			TestEqual(TEXT("FVector2D"), Read.ReadOr(TEXT("uv"), FVector2D::ZeroVector), FVector2D(0.1, 0.7));
			TestTrue(TEXT("FRotator"), Read.ReadOr(TEXT("rot"), FRotator::ZeroRotator) == FRotator(-10.0, 270.5, 0.0));
			TestTrue(TEXT("FIntPoint"), Read.ReadOr(TEXT("celda"), FIntPoint(0, 0)) == FIntPoint(-3, 12));
			TestTrue(TEXT("FName"), Read.ReadOr(TEXT("id"), FName()) == FName(TEXT("petro_07")));
			TestEqual(TEXT("FString"), Read.ReadOr(TEXT("nombre"), FString()), FString(TEXT("Isla Esmeralda")));
			TestEqual(TEXT("Literal"), Read.ReadOr(TEXT("literal"), FString()), FString(TEXT("ñandú")));
			TestTrue(TEXT("uint64"), Read.ReadOr(TEXT("semilla"), static_cast<uint64>(0)) == 0xFEDCBA9876543210ULL);
			TestTrue(TEXT("float exacto"), Read.ReadOr(TEXT("vida"), 0.0f) == 87.25f);
			TestTrue(TEXT("Enum por nombre"), Read.ReadOr(TEXT("marea"), ETide::Low) == ETide::Spring);
			TestEqual(TEXT("Enum en el texto"), Read.GetRoot().Find(TEXT("marea"))->AsString(), FString(TEXT("Spring")));
			TestTrue(TEXT("TArray<int32>"), Read.ReadOr(TEXT("lista"), TArray<int32>()) == TArray<int32>({ 3, 1, 4, 1, 5 }));
			TestEqual(TEXT("TArray<FVector>"), Read.ReadOr(TEXT("puntos"), TArray<FVector>()).Num(), 2);
			TestTrue(TEXT("TMap con clave de texto"), Read.ReadOr(TEXT("stock"), TMap<FString, int32>()).FindRef(TEXT("bambú")) == 12);
			TestTrue(TEXT("TMap con otra clave"), Read.ReadOr(TEXT("porDía"), TMap<int32, FString>()).FindRef(3) == TEXT("desove"));
		});

		It("deja el valor por defecto si la clave falta o no encaja", [this]()
		{
			FSaveArchive Ar;
			Ar.Write(TEXT("grande"), static_cast<int64>(5000000000LL));
			Ar.Write(TEXT("texto"), FString(TEXT("no soy un número")));
			Ar.Write(TEXT("marea"), FString(TEXT("Tsunami")));
			Ar.Write(TEXT("mezcla"), TArray<FString>({ TEXT("a") }));

			int32 Small = 7;
			TestFalse(TEXT("int32 fuera de rango"), Ar.Read(TEXT("grande"), Small));
			TestEqual(TEXT("Conserva el valor"), Small, 7);
			float F = 1.0f;
			TestFalse(TEXT("Tipo equivocado"), Ar.Read(TEXT("texto"), F));
			TestFalse(TEXT("Clave ausente"), Ar.Read(TEXT("nada"), F));
			TestTrue(TEXT("Conserva el float"), F == 1.0f);
			ETide Tide = ETide::High;
			TestFalse(TEXT("Nombre de enum desconocido"), Ar.Read(TEXT("marea"), Tide));
			TestTrue(TEXT("Conserva el enum"), Tide == ETide::High);
			TArray<int32> Ints = { 9 };
			TestFalse(TEXT("Lista con elementos que no encajan"), Ar.Read(TEXT("mezcla"), Ints));
			TestEqual(TEXT("Conserva la lista"), Ints.Num(), 1);
		});

		It("anida archivos como objetos", [this]()
		{
			FSaveArchive Child;
			Child.Write(TEXT("dias"), 12);
			FSaveArchive Parent;
			Parent.Write(TEXT("huerto"), Child);
			FSaveArchive Back;
			TestTrue(TEXT("Lee el hijo"), Parent.Read(TEXT("huerto"), Back));
			TestEqual(TEXT("Valor del hijo"), Back.ReadOr(TEXT("dias"), 0), 12);
			TestTrue(TEXT("Un escalar no es un archivo"), FSaveArchive(FSaveValue::MakeInt(3)).IsEmpty());
		});
	});

	Describe(TEXT("el documento"), [this]()
	{
		It("guarda cabecera y secciones y las verifica con la suma de control", [this]()
		{
			FSaveDocument Document;
			Document.Header.GameVersion = TEXT("0.1.0");
			Document.Header.Seed = 20260926;
			Document.Header.PlayTimeSeconds = 3723.5;
			Document.Header.TimestampUnix = 1790000000;
			Document.Header.SlotId = TEXT("manual2");
			FSaveArchive Player;
			Player.Write(TEXT("pos"), FVector(10.0, 20.0, 30.0));
			Document.Sections.Set(TEXT("player"), Player.GetRoot());

			const FString Text = FSaveCodec::Encode(Document);
			TestTrue(TEXT("Lleva la marca"), Text.Contains(TEXT("\"format\": \"explored-save\"")));

			FSaveDocument Read;
			FString Error;
			const FSaveMigrations Migrations;
			TestTrue(TEXT("Se lee"), FSaveCodec::Decode(Text, Migrations, ExploredSave::CurrentFormatVersion, Read, Error) == ESaveLoadResult::Ok);
			TestTrue(TEXT("Cabecera igual"), Read.Header == Document.Header);
			TestTrue(TEXT("Secciones iguales"), Read.Sections == Document.Sections);

			const FString Tampered = Text.Replace(TEXT("20260926"), TEXT("20260927"));
			TestTrue(TEXT("Un valor cambiado rompe la suma"),
				FSaveCodec::Decode(Tampered, Migrations, ExploredSave::CurrentFormatVersion, Read, Error) == ESaveLoadResult::BadChecksum);
			TestTrue(TEXT("Un fichero truncado es ilegible"),
				FSaveCodec::Decode(Text.Left(Text.Len() / 2), Migrations, ExploredSave::CurrentFormatVersion, Read, Error) == ESaveLoadResult::Malformed);
			TestTrue(TEXT("Otro JSON no es una partida"),
				FSaveCodec::Decode(TEXT("{\"a\":1}"), Migrations, ExploredSave::CurrentFormatVersion, Read, Error) == ESaveLoadResult::Malformed);
			const FString Reformatted = Text.Replace(TEXT("\n"), TEXT("\r\n")).Replace(TEXT("\t"), TEXT("    "));
			TestTrue(TEXT("El espaciado no afecta a la suma"),
				FSaveCodec::Decode(Reformatted, Migrations, ExploredSave::CurrentFormatVersion, Read, Error) == ESaveLoadResult::Ok);
		});

		It("calcula la suma sobre los bytes UTF-8 (igual en todas las plataformas)", [this]()
		{
			TestEqual(TEXT("Vacío"), FSaveChecksum::ToHex(FSaveChecksum::Compute(FString())), FString(TEXT("cbf29ce484222325")));
			TestEqual(TEXT("Con eñe y emoji"), FSaveChecksum::ToHex(FSaveChecksum::Compute(TEXT("año 🐢"))), FString(TEXT("c4a2064f0c8bc0bc")));
			uint64 Back = 0;
			TestTrue(TEXT("Hex ida y vuelta"), FSaveChecksum::FromHex(TEXT("c4a2064f0c8bc0bc"), Back) && Back == 0xC4A2064F0C8BC0BCULL);
			TestFalse(TEXT("Hex no válido"), FSaveChecksum::FromHex(TEXT("c4a2064f0c8bc0bz"), Back));
		});

		It("aplica la cadena de migraciones en orden", [this]()
		{
			FSaveDocument Old;
			Old.Header.FormatVersion = 1;
			FSaveArchive Map;
			Map.Write(TEXT("marks"), 4);
			Old.Sections.Set(TEXT("carta"), Map.GetRoot());
			const FString Text = FSaveCodec::Encode(Old);

			TArray<int32> Order;
			FSaveMigrations Migrations;
			Migrations.Register(2, [&Order](FSaveValue& Sections, FString& OutError)
			{
				Order.Add(2);
				FSaveValue* Cartography = Sections.Find(TEXT("cartography"));
				if (!Cartography)
				{
					OutError = TEXT("falta cartography");
					return false;
				}
				Cartography->Set(TEXT("inkVersion"), FSaveValue::MakeInt(2));
				return true;
			});
			Migrations.Register(1, [&Order](FSaveValue& Sections, FString& OutError)
			{
				Order.Add(1);
				const FSaveValue Moved = *Sections.Find(TEXT("carta"));
				Sections.Remove(TEXT("carta"));
				Sections.Set(TEXT("cartography"), Moved);
				return true;
			});

			FSaveDocument Read;
			FString Error;
			TestTrue(TEXT("Migra de 1 a 3"), FSaveCodec::Decode(Text, Migrations, 3, Read, Error) == ESaveLoadResult::Ok);
			TestTrue(TEXT("En orden"), Order == TArray<int32>({ 1, 2 }));
			TestEqual(TEXT("Versión actualizada"), Read.Header.FormatVersion, 3);
			const FSaveArchive Cartography(*Read.Sections.Find(TEXT("cartography")));
			TestEqual(TEXT("Dato conservado"), Cartography.ReadOr(TEXT("marks"), 0), 4);
			TestEqual(TEXT("Dato añadido"), Cartography.ReadOr(TEXT("inkVersion"), 0), 2);
			TestFalse(TEXT("Sección antigua retirada"), Read.Sections.Contains(TEXT("carta")));

			FSaveMigrations Missing;
			Missing.Register(1, [](FSaveValue&, FString&) { return true; });
			TestTrue(TEXT("Falta un paso"), FSaveCodec::Decode(Text, Missing, 3, Read, Error) == ESaveLoadResult::MigrationFailed);
			TestTrue(TEXT("El error dice qué paso"), Error.Contains(TEXT("2 a la 3")));

			FSaveMigrations Failing;
			Failing.Register(1, [](FSaveValue&, FString& OutError) { OutError = TEXT("dato imposible"); return false; });
			TestTrue(TEXT("Una migración que falla"), FSaveCodec::Decode(Text, Failing, 2, Read, Error) == ESaveLoadResult::MigrationFailed);
			TestTrue(TEXT("Con su mensaje"), Error.Contains(TEXT("dato imposible")));
		});

		It("rechaza con elegancia partidas de versiones futuras", [this]()
		{
			FSaveDocument Future;
			Future.Header.FormatVersion = ExploredSave::CurrentFormatVersion + 1;
			const FString Text = FSaveCodec::Encode(Future);
			FSaveDocument Read;
			FString Error;
			const FSaveMigrations Migrations;
			TestTrue(TEXT("Versión futura"),
				FSaveCodec::Decode(Text, Migrations, ExploredSave::CurrentFormatVersion, Read, Error) == ESaveLoadResult::FutureVersion);
			TestFalse(TEXT("Con mensaje"), Error.IsEmpty());

			FSaveDocument Zero;
			Zero.Header.FormatVersion = 0;
			TestTrue(TEXT("Versión 0 no válida"),
				FSaveCodec::Decode(FSaveCodec::Encode(Zero), Migrations, ExploredSave::CurrentFormatVersion, Read, Error) == ESaveLoadResult::Malformed);
		});
	});

	Describe(TEXT("el registro de secciones"), [this]()
	{
		It("compone las secciones y da valores por defecto a las que faltan", [this]()
		{
			struct FFarm
			{
				int32 Lemons = 0;
				FString Stage = TEXT("semilla");
			};
			FFarm Farm;
			Farm.Lemons = 5;
			Farm.Stage = TEXT("flor");
			int32 Loads = 0;

			FSaveSectionRegistry Registry;
			TestTrue(TEXT("Registra"), Registry.Register(TEXT("farm"),
				[&Farm](FSaveArchive& Ar) { Ar.Write(TEXT("lemons"), Farm.Lemons); Ar.Write(TEXT("stage"), Farm.Stage); },
				[&Farm, &Loads](const FSaveArchive& Ar)
				{
					++Loads;
					Farm = FFarm();
					Ar.Read(TEXT("lemons"), Farm.Lemons);
					Ar.Read(TEXT("stage"), Farm.Stage);
				}));
			TestFalse(TEXT("Nombre repetido"), Registry.Register(TEXT("farm"), nullptr, nullptr));
			TestFalse(TEXT("Nombre vacío"), Registry.Register(FString(), nullptr, nullptr));

			const FSaveValue Captured = Registry.Capture();
			Farm = FFarm();
			Registry.Apply(Captured);
			TestEqual(TEXT("Recupera los limones"), Farm.Lemons, 5);
			TestEqual(TEXT("Recupera la etapa"), Farm.Stage, FString(TEXT("flor")));

			Farm.Lemons = 99;
			Registry.Apply(FSaveValue::MakeObject());
			TestEqual(TEXT("Sin sección: valores por defecto"), Farm.Lemons, 0);
			TestEqual(TEXT("Sin sección: etapa por defecto"), Farm.Stage, FString(TEXT("semilla")));
			TestEqual(TEXT("Load se llamó en ambos casos"), Loads, 2);
		});

		It("conserva al volver a guardar las secciones que nadie conoce", [this]()
		{
			int32 Value = 1;
			FSaveSectionRegistry Registry;
			Registry.Register(TEXT("known"),
				[&Value](FSaveArchive& Ar) { Ar.Write(TEXT("v"), Value); },
				[&Value](const FSaveArchive& Ar) { Value = Ar.ReadOr(TEXT("v"), 0); });

			FSaveValue FromFile = FSaveValue::MakeObject();
			FSaveArchive Known;
			Known.Write(TEXT("v"), 7);
			FromFile.Set(TEXT("known"), Known.GetRoot());
			FSaveArchive Future;
			Future.Write(TEXT("boats"), 3);
			FromFile.Set(TEXT("fleet"), Future.GetRoot());

			Registry.Apply(FromFile);
			TestEqual(TEXT("Carga la conocida"), Value, 7);
			Value = 8;
			const FSaveValue Saved = Registry.Capture();
			TestTrue(TEXT("Conserva la desconocida"), Saved.Contains(TEXT("fleet")) && *Saved.Find(TEXT("fleet")) == Future.GetRoot());
			TestEqual(TEXT("Escribe el valor nuevo de la conocida"), FSaveArchive(*Saved.Find(TEXT("known"))).ReadOr(TEXT("v"), 0), 8);

			Registry.ResetToDefaults();
			TestFalse(TEXT("Partida nueva: olvida las desconocidas"), Registry.Capture().Contains(TEXT("fleet")));
			TestTrue(TEXT("Retira"), Registry.Unregister(TEXT("known")));
			TestEqual(TEXT("Sin secciones"), Registry.Num(), 0);
		});
	});
}

#endif
