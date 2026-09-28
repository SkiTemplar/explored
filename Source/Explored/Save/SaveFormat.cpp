#include "Save/SaveFormat.h"

namespace SaveFormatDetail
{
	const TCHAR* const KeyChecksum = TEXT("checksum");
	const TCHAR* const KeyFormat = TEXT("format");
	const TCHAR* const KeyHeader = TEXT("header");
	const TCHAR* const KeySections = TEXT("sections");
}

const TCHAR* LexToString(ESaveLoadResult Result)
{
	switch (Result)
	{
	case ESaveLoadResult::Ok: return TEXT("Ok");
	case ESaveLoadResult::NotFound: return TEXT("NotFound");
	case ESaveLoadResult::Malformed: return TEXT("Malformed");
	case ESaveLoadResult::BadChecksum: return TEXT("BadChecksum");
	case ESaveLoadResult::FutureVersion: return TEXT("FutureVersion");
	case ESaveLoadResult::MigrationFailed: return TEXT("MigrationFailed");
	}
	return TEXT("Unknown");
}

// ---------------------------------------------------------------------------
// FSaveHeader
// ---------------------------------------------------------------------------

FSaveValue FSaveHeader::ToValue() const
{
	FSaveArchive Ar;
	Ar.Write(TEXT("formatVersion"), FormatVersion);
	Ar.Write(TEXT("gameVersion"), GameVersion);
	Ar.Write(TEXT("seed"), Seed);
	Ar.Write(TEXT("playTimeSeconds"), PlayTimeSeconds);
	Ar.Write(TEXT("timestampUnix"), TimestampUnix);
	Ar.Write(TEXT("slotId"), SlotId);
	return Ar.GetRoot();
}

bool FSaveHeader::FromValue(const FSaveValue& Value)
{
	if (!Value.IsObject())
	{
		return false;
	}
	const FSaveArchive Ar(Value);
	FSaveHeader Result;
	if (!Ar.Read(TEXT("formatVersion"), Result.FormatVersion))
	{
		return false;
	}
	Ar.Read(TEXT("gameVersion"), Result.GameVersion);
	Ar.Read(TEXT("seed"), Result.Seed);
	Ar.Read(TEXT("playTimeSeconds"), Result.PlayTimeSeconds);
	// "NaN" rompería el orden estricto débil de SortForContinue (NaN != x y ni > ni <).
	if (!FMath::IsFinite(Result.PlayTimeSeconds) || Result.PlayTimeSeconds < 0.0)
	{
		Result.PlayTimeSeconds = 0.0;
	}
	Ar.Read(TEXT("timestampUnix"), Result.TimestampUnix);
	Ar.Read(TEXT("slotId"), Result.SlotId);
	*this = MoveTemp(Result);
	return true;
}

bool FSaveHeader::operator==(const FSaveHeader& Other) const
{
	return FormatVersion == Other.FormatVersion
		&& FSaveValue::CompareKeys(GameVersion, Other.GameVersion) == 0
		&& Seed == Other.Seed
		&& PlayTimeSeconds == Other.PlayTimeSeconds
		&& TimestampUnix == Other.TimestampUnix
		&& FSaveValue::CompareKeys(SlotId, Other.SlotId) == 0;
}

// ---------------------------------------------------------------------------
// FSaveMigrations
// ---------------------------------------------------------------------------

void FSaveMigrations::Register(int32 FromVersion, FSaveMigrationFunc Migration)
{
	Steps.Add(FromVersion, MoveTemp(Migration));
}

ESaveLoadResult FSaveMigrations::Apply(int32 FromVersion, int32 TargetVersion, FSaveValue& Sections, FString& OutError) const
{
	if (FromVersion > TargetVersion)
	{
		OutError = FString::Printf(TEXT("La partida es de la versión %d y este juego solo lee hasta la %d"), FromVersion, TargetVersion);
		return ESaveLoadResult::FutureVersion;
	}
	// Se trabaja sobre una copia: una migración a medias no deja la partida corrupta.
	FSaveValue Working = Sections;
	for (int32 Version = FromVersion; Version < TargetVersion; ++Version)
	{
		const FSaveMigrationFunc* Step = Steps.Find(Version);
		if (!Step || !*Step)
		{
			OutError = FString::Printf(TEXT("Falta la migración de la versión %d a la %d"), Version, Version + 1);
			return ESaveLoadResult::MigrationFailed;
		}
		FString StepError;
		if (!(*Step)(Working, StepError))
		{
			OutError = FString::Printf(TEXT("Falló la migración de la versión %d a la %d: %s"), Version, Version + 1, *StepError);
			return ESaveLoadResult::MigrationFailed;
		}
		if (!Working.IsObject())
		{
			OutError = FString::Printf(TEXT("La migración de la versión %d dejó las secciones sin forma de objeto"), Version);
			return ESaveLoadResult::MigrationFailed;
		}
	}
	Sections = MoveTemp(Working);
	return ESaveLoadResult::Ok;
}

// ---------------------------------------------------------------------------
// FSaveCodec
// ---------------------------------------------------------------------------

FString FSaveCodec::PayloadText(const FSaveValue& Header, const FSaveValue& Sections)
{
	FSaveValue Payload = FSaveValue::MakeObject();
	Payload.Set(SaveFormatDetail::KeyHeader, Header);
	Payload.Set(SaveFormatDetail::KeySections, Sections);
	return FSaveText::Write(Payload, ESaveTextStyle::Compact);
}

FString FSaveCodec::Encode(const FSaveDocument& Document)
{
	const FSaveValue Header = Document.Header.ToValue();
	const FSaveValue Sections = Document.Sections.IsObject() ? Document.Sections : FSaveValue::MakeObject();

	FSaveValue Root = FSaveValue::MakeObject();
	Root.Set(SaveFormatDetail::KeyFormat, FSaveValue::MakeString(ExploredSave::FormatTag()));
	Root.Set(SaveFormatDetail::KeyHeader, Header);
	Root.Set(SaveFormatDetail::KeySections, Sections);
	Root.Set(SaveFormatDetail::KeyChecksum, FSaveValue::MakeString(FSaveChecksum::ToHex(FSaveChecksum::Compute(PayloadText(Header, Sections)))));
	return FSaveText::Write(Root, ESaveTextStyle::Pretty);
}

ESaveLoadResult FSaveCodec::Decode(const FString& Text, const FSaveMigrations& Migrations, int32 CurrentVersion,
	FSaveDocument& OutDocument, FString& OutError)
{
	FSaveValue Root;
	FString ParseError;
	if (!FSaveText::Parse(Text, Root, ParseError))
	{
		OutError = FString::Printf(TEXT("Texto ilegible: %s"), *ParseError);
		return ESaveLoadResult::Malformed;
	}
	if (!Root.IsObject())
	{
		OutError = TEXT("La raíz no es un objeto");
		return ESaveLoadResult::Malformed;
	}
	const FSaveValue* Format = Root.Find(SaveFormatDetail::KeyFormat);
	if (!Format || FSaveValue::CompareKeys(Format->AsString(), ExploredSave::FormatTag()) != 0)
	{
		OutError = TEXT("No es una partida de Explored");
		return ESaveLoadResult::Malformed;
	}
	const FSaveValue* HeaderValue = Root.Find(SaveFormatDetail::KeyHeader);
	const FSaveValue* SectionsValue = Root.Find(SaveFormatDetail::KeySections);
	const FSaveValue* ChecksumValue = Root.Find(SaveFormatDetail::KeyChecksum);
	if (!HeaderValue || !HeaderValue->IsObject() || !SectionsValue || !SectionsValue->IsObject() || !ChecksumValue)
	{
		OutError = TEXT("Faltan la cabecera, las secciones o la suma de control");
		return ESaveLoadResult::Malformed;
	}
	uint64 Expected = 0;
	if (!FSaveChecksum::FromHex(ChecksumValue->AsString(), Expected))
	{
		OutError = TEXT("Suma de control con formato no válido");
		return ESaveLoadResult::Malformed;
	}
	if (FSaveChecksum::Compute(PayloadText(*HeaderValue, *SectionsValue)) != Expected)
	{
		OutError = TEXT("La suma de control no coincide: la partida está dañada");
		return ESaveLoadResult::BadChecksum;
	}

	FSaveDocument Result;
	if (!Result.Header.FromValue(*HeaderValue))
	{
		OutError = TEXT("Cabecera sin versión de formato");
		return ESaveLoadResult::Malformed;
	}
	if (Result.Header.FormatVersion > CurrentVersion)
	{
		OutError = FString::Printf(TEXT("La partida es de la versión %d y este juego solo lee hasta la %d"),
			Result.Header.FormatVersion, CurrentVersion);
		return ESaveLoadResult::FutureVersion;
	}
	if (Result.Header.FormatVersion < 1)
	{
		OutError = FString::Printf(TEXT("Versión de formato no válida: %d"), Result.Header.FormatVersion);
		return ESaveLoadResult::Malformed;
	}
	Result.Sections = *SectionsValue;
	const ESaveLoadResult Migrated = Migrations.Apply(Result.Header.FormatVersion, CurrentVersion, Result.Sections, OutError);
	if (Migrated != ESaveLoadResult::Ok)
	{
		return Migrated;
	}
	Result.Header.FormatVersion = CurrentVersion;
	OutDocument = MoveTemp(Result);
	OutError.Reset();
	return ESaveLoadResult::Ok;
}

// ---------------------------------------------------------------------------
// FSaveSectionRegistry
// ---------------------------------------------------------------------------

int32 FSaveSectionRegistry::IndexOf(const FString& Name) const
{
	for (int32 I = 0; I < Sections.Num(); ++I)
	{
		if (FSaveValue::CompareKeys(Sections[I].Name, Name) == 0)
		{
			return I;
		}
	}
	return INDEX_NONE;
}

bool FSaveSectionRegistry::Register(const FString& Name, FSaveFunc Save, FLoadFunc Load)
{
	if (Name.IsEmpty() || IndexOf(Name) != INDEX_NONE)
	{
		return false;
	}
	FSection& Section = Sections.AddDefaulted_GetRef();
	Section.Name = Name;
	Section.Save = MoveTemp(Save);
	Section.Load = MoveTemp(Load);
	return true;
}

bool FSaveSectionRegistry::Unregister(const FString& Name)
{
	const int32 Index = IndexOf(Name);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Sections.RemoveAt(Index);
	return true;
}

bool FSaveSectionRegistry::IsRegistered(const FString& Name) const
{
	return IndexOf(Name) != INDEX_NONE;
}

FSaveValue FSaveSectionRegistry::Capture() const
{
	// Primero las desconocidas conservadas; las registradas las sustituyen si coinciden.
	// Copia: un callback puede registrar o retirar secciones mientras se recorre.
	const TArray<FSection> Snapshot = Sections;
	FSaveValue Out = Preserved.IsObject() ? Preserved : FSaveValue::MakeObject();
	for (const FSection& Section : Snapshot)
	{
		FSaveArchive Ar;
		if (Section.Save)
		{
			Section.Save(Ar);
		}
		Out.Set(Section.Name, Ar.GetRoot());
	}
	return Out;
}

void FSaveSectionRegistry::Apply(const FSaveValue& InSections)
{
	FSaveValue Unknown = FSaveValue::MakeObject();
	if (InSections.IsObject())
	{
		const TArray<FString>& Keys = InSections.GetKeys();
		for (int32 I = 0; I < Keys.Num(); ++I)
		{
			if (IndexOf(Keys[I]) == INDEX_NONE)
			{
				Unknown.Set(Keys[I], InSections.GetValueAt(I));
			}
		}
	}
	Preserved = MoveTemp(Unknown);

	const TArray<FSection> Snapshot = Sections;
	for (const FSection& Section : Snapshot)
	{
		const FSaveValue* Data = InSections.IsObject() ? InSections.Find(Section.Name) : nullptr;
		const FSaveArchive Ar = Data ? FSaveArchive(*Data) : FSaveArchive();
		if (Section.Load)
		{
			Section.Load(Ar);
		}
	}
}

void FSaveSectionRegistry::ResetToDefaults()
{
	Apply(FSaveValue::MakeObject());
}
