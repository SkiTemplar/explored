#pragma once

#include "CoreMinimal.h"
#include "Save/SaveFormat.h"

/** Qué pide guardar (GDD §15: autoguardado al dormir y en hogueras). */
enum class ESaveTrigger : uint8
{
	/** «Guardar partida» desde la pausa: siempre. */
	Manual,
	/** Al dormir: siempre (autoguardado). */
	Sleep,
	/** Al encender o usar una hoguera: con un intervalo mínimo para no guardar en bucle. */
	Campfire,
	/** Al salir al menú o al escritorio: siempre (autoguardado). */
	Quit,
};

/** Estado de una ranura para el menú «Continuar» / «Cargar». */
struct EXPLORED_API FSaveSlotInfo
{
	FString SlotId;
	ESaveLoadResult Result = ESaveLoadResult::NotFound;
	/** La principal estaba dañada y se leyó la copia de seguridad. */
	bool bFromBackup = false;
	FSaveHeader Header;

	bool IsLoadable() const { return Result == ESaveLoadResult::Ok; }
};

/** Resultado de leer una ranura (principal y, si falla, copia). */
struct EXPLORED_API FSaveReadOutcome
{
	ESaveLoadResult Result = ESaveLoadResult::NotFound;
	bool bFromBackup = false;
	FSaveDocument Document;
	FString Error;
};

/** Operación de fichero del plan de escritura (la ejecuta la capa de Unreal o un sistema de ficheros de prueba). */
struct EXPLORED_API FSaveFileOp
{
	enum class EKind : uint8
	{
		/** Escribe el texto en Path. */
		Write,
		/** Mueve From a Path sustituyendo lo que hubiera. */
		MoveReplace,
	};

	EKind Kind = EKind::Write;
	/** Destino. */
	FString Path;
	/** Origen de MoveReplace. */
	FString From;
	/** Contenido de Write. */
	FString Text;
};

/**
 * Política de ranuras (pura): 3 ranuras manuales + autoguardado, cada una con
 * una copia de seguridad (la versión anterior). Nombres de fichero en
 * Saved/SaveGames/: «<ranura>.sav», «<ranura>.bak» y el temporal «<ranura>.tmp».
 */
struct EXPLORED_API FSaveSlotPolicy
{
	static constexpr int32 NumManualSlots = 3;
	/** Segundos reales mínimos entre dos autoguardados por hoguera. */
	static constexpr double MinSecondsBetweenCampfireSaves = 300.0;

	static FString AutoSlotId();
	/** «manual1»…«manual3» (Index empieza en 1). */
	static FString ManualSlotId(int32 Index);
	/** Todas las ranuras en orden fijo: auto, manual1, manual2, manual3. */
	static TArray<FString> AllSlotIds();
	static bool IsValidSlotId(const FString& SlotId);
	/**
	 * Acepta los nombres que ya usaba la UI («Auto», «Manual2», «2») y devuelve
	 * el identificador canónico, o cadena vacía si no corresponde a ninguna ranura.
	 */
	static FString NormalizeSlotId(const FString& Requested);

	static FString MainFileName(const FString& SlotId) { return SlotId + TEXT(".sav"); }
	static FString BackupFileName(const FString& SlotId) { return SlotId + TEXT(".bak"); }
	static FString TempFileName(const FString& SlotId) { return SlotId + TEXT(".tmp"); }

	/** Si un disparador debe guardar ya (SecondsSinceLastAutosave < 0 = nunca se ha guardado). */
	static bool ShouldAutosave(ESaveTrigger Trigger, double SecondsSinceLastAutosave);
	/** Ranura de un disparador: los automáticos van a «auto». */
	static FString SlotForTrigger(ESaveTrigger Trigger, const FString& ManualSlotId);

	/**
	 * Plan de escritura atómica: temporal → (principal → copia) → temporal →
	 * principal. Si el proceso muere a mitad, la principal o la copia siguen
	 * siendo una partida completa.
	 */
	static TArray<FSaveFileOp> PlanWrite(const FString& Directory, const FString& SlotId, const FString& Text, bool bMainExists);

	/**
	 * Lee una ranura a partir del texto de sus ficheros (nullptr si no existen).
	 * Si la principal falla por cualquier motivo salvo versión futura, se prueba
	 * la copia.
	 */
	static FSaveReadOutcome ReadSlot(const FString* MainText, const FString* BackupText,
		const FSaveMigrations& Migrations, int32 CurrentVersion);

	/**
	 * Orden de «Continuar»: primero las legibles, de la más reciente a la más
	 * antigua (a igualdad, más tiempo jugado y después el orden fijo de ranuras).
	 */
	static void SortForContinue(TArray<FSaveSlotInfo>& Slots);

	static FString JoinPath(const FString& Directory, const FString& FileName);
};

/** Acceso mínimo a ficheros que necesita el almacén de ranuras. */
class EXPLORED_API ISaveFileSystem
{
public:
	virtual ~ISaveFileSystem() = default;
	virtual bool FileExists(const FString& Path) const = 0;
	virtual bool ReadText(const FString& Path, FString& OutText) const = 0;
	virtual bool WriteText(const FString& Path, const FString& Text) = 0;
	/** Mueve sustituyendo el destino si existe. */
	virtual bool MoveReplace(const FString& From, const FString& To) = 0;
	virtual bool Delete(const FString& Path) = 0;
};

/**
 * Almacén de ranuras sobre un ISaveFileSystem: escribe con el plan atómico,
 * lee con caída a la copia y lista para «Continuar». Puro: los tests lo usan
 * con un sistema de ficheros en memoria y el subsistema de Unreal con el disco.
 */
class EXPLORED_API FSaveSlotStore
{
public:
	FSaveSlotStore(ISaveFileSystem& InFileSystem, const FString& InDirectory, const FSaveMigrations& InMigrations,
		int32 InCurrentVersion = ExploredSave::CurrentFormatVersion);

	/** Codifica y escribe. Rellena Header.SlotId con la ranura. */
	bool Write(const FString& SlotId, const FSaveDocument& Document, FString& OutError);
	FSaveReadOutcome Read(const FString& SlotId) const;
	/** Las ranuras con algún fichero, en orden de «Continuar». */
	TArray<FSaveSlotInfo> List() const;
	/** Ranura más reciente legible, o vacío si no hay ninguna. */
	FString FindContinueSlot() const;
	bool DeleteSlot(const FString& SlotId);

	const FString& GetDirectory() const { return Directory; }

private:
	ISaveFileSystem& FileSystem;
	FString Directory;
	FSaveMigrations Migrations;
	int32 CurrentVersion;
};
