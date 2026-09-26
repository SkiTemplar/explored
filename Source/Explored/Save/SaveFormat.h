#pragma once

#include "CoreMinimal.h"
#include "Save/SaveArchive.h"
#include "Save/SaveValue.h"

namespace ExploredSave
{
	/** Versión actual del formato. Al cambiarla, registra la migración desde la anterior. */
	constexpr int32 CurrentFormatVersion = 1;

	/** Marca del documento: descarta ficheros que no son partidas de Explored. */
	inline const TCHAR* FormatTag() { return TEXT("explored-save"); }
}

/** Resultado de leer una partida. */
enum class ESaveLoadResult : uint8
{
	Ok,
	/** No hay fichero. */
	NotFound,
	/** Texto ilegible, sin marca o con estructura inesperada. */
	Malformed,
	/** El contenido no coincide con su suma de control (fichero dañado o editado). */
	BadChecksum,
	/** Partida de una versión del juego más nueva que esta. */
	FutureVersion,
	/** Falta un paso de migración o una migración falló. */
	MigrationFailed,
};

EXPLORED_API const TCHAR* LexToString(ESaveLoadResult Result);

/** Cabecera de la partida: lo que se muestra en «Continuar» sin cargar el mundo. */
struct EXPLORED_API FSaveHeader
{
	int32 FormatVersion = ExploredSave::CurrentFormatVersion;
	/** Versión del juego que escribió la partida (informativa). */
	FString GameVersion;
	/** Semilla del archipiélago: el mundo se regenera desde ella y se le aplican los deltas. */
	int64 Seed = 0;
	/** Tiempo jugado en segundos reales. */
	double PlayTimeSeconds = 0.0;
	/** Momento del guardado (segundos Unix, UTC). Ordena «Continuar». */
	int64 TimestampUnix = 0;
	/** Ranura en la que se escribió («manual1»…«manual3», «auto»). */
	FString SlotId;

	FSaveValue ToValue() const;
	/** Lee la cabecera; falla si falta la versión. El resto de campos son opcionales. */
	bool FromValue(const FSaveValue& Value);

	bool operator==(const FSaveHeader& Other) const;
};

/** Partida en memoria: cabecera + secciones (objeto «nombre de sección → datos»). */
struct EXPLORED_API FSaveDocument
{
	FSaveHeader Header;
	FSaveValue Sections = FSaveValue::MakeObject();
};

/** Paso de migración de la versión N a la N+1 sobre el objeto de secciones. */
using FSaveMigrationFunc = TFunction<bool(FSaveValue& /*Sections*/, FString& /*OutError*/)>;

/**
 * Registro de migraciones (versión origen → función). Se aplican en orden,
 * una por versión, hasta llegar a la actual; si falta un paso, la carga falla
 * limpiamente en vez de interpretar datos antiguos como nuevos.
 */
class EXPLORED_API FSaveMigrations
{
public:
	/** Registra el paso FromVersion → FromVersion + 1 (sustituye uno anterior). */
	void Register(int32 FromVersion, FSaveMigrationFunc Migration);
	bool HasStep(int32 FromVersion) const { return Steps.Contains(FromVersion); }

	/** Lleva Sections de FromVersion a TargetVersion. */
	ESaveLoadResult Apply(int32 FromVersion, int32 TargetVersion, FSaveValue& Sections, FString& OutError) const;

private:
	TMap<int32, FSaveMigrationFunc> Steps;
};

/**
 * Documento de texto de una partida:
 *
 *   {
 *       "checksum": "<FNV-1a 64 del contenido>",
 *       "format": "explored-save",
 *       "header": { "formatVersion": 1, "seed": …, … },
 *       "sections": { "player": { … }, "world": { … }, … }
 *   }
 *
 * La suma de control cubre la forma compacta canónica de {header, sections};
 * como el escritor es determinista y la lectura reescribe exactamente el mismo
 * texto, cualquier cambio de un valor (o un fichero truncado) la rompe.
 */
struct EXPLORED_API FSaveCodec
{
	static FString Encode(const FSaveDocument& Document);

	/**
	 * Lee, verifica la suma, rechaza versiones futuras y migra las antiguas a
	 * CurrentVersion. Nunca aborta con entradas mal formadas.
	 */
	static ESaveLoadResult Decode(const FString& Text, const FSaveMigrations& Migrations, int32 CurrentVersion,
		FSaveDocument& OutDocument, FString& OutError);

	/** Texto canónico (compacto) sobre el que se calcula la suma. */
	static FString PayloadText(const FSaveValue& Header, const FSaveValue& Sections);
};

/**
 * Registro de secciones: cada sistema aporta un nombre y dos callbacks. El
 * subsistema de guardado compone la partida sin conocer a los sistemas.
 *
 * - Al guardar se llama a Save de cada sección con un archivo vacío.
 * - Al cargar se llama a Load de cada sección registrada, en orden de registro;
 *   si la partida no trae la sección, Load recibe un archivo vacío y el sistema
 *   debe quedar en su estado por defecto (compatibilidad hacia delante).
 * - Las secciones de la partida que nadie ha registrado se conservan y se
 *   vuelven a escribir al guardar (no se pierden datos de sistemas ausentes).
 */
class EXPLORED_API FSaveSectionRegistry
{
public:
	using FSaveFunc = TFunction<void(FSaveArchive&)>;
	using FLoadFunc = TFunction<void(const FSaveArchive&)>;

	/** Devuelve false si el nombre está vacío o ya registrado. */
	bool Register(const FString& Name, FSaveFunc Save, FLoadFunc Load);
	bool Unregister(const FString& Name);
	bool IsRegistered(const FString& Name) const;
	int32 Num() const { return Sections.Num(); }

	/** Objeto de secciones listo para FSaveDocument::Sections. */
	FSaveValue Capture() const;

	/** Reparte las secciones de una partida entre los sistemas registrados. */
	void Apply(const FSaveValue& InSections);

	/** Estado de partida nueva: Load con archivo vacío en todas y olvida las desconocidas. */
	void ResetToDefaults();

	/** Secciones desconocidas conservadas de la última carga. */
	const FSaveValue& GetPreservedSections() const { return Preserved; }

private:
	struct FSection
	{
		FString Name;
		FSaveFunc Save;
		FLoadFunc Load;
	};

	int32 IndexOf(const FString& Name) const;

	TArray<FSection> Sections;
	FSaveValue Preserved = FSaveValue::MakeObject();
};
