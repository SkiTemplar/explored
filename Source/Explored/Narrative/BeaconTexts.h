#pragma once

#include "CoreMinimal.h"

#include "Core/ExploredLocalization.h"

/**
 * Textos de la historia de Baliza Siete leídos de Content/Data (GDD v3 §2, §2.1, §2.2 y §3):
 * notebook.json (páginas, notas de Iria, cartel de inicio y epílogo), objectives.json
 * (objetivo del HUD) y thoughts.json (pensamientos de Iria). Cada texto trae su pareja
 * español/inglés y se elige según la cultura activa (ExploredLocalization).
 *
 * Ids: cuaderno_pN y cuaderno_pN_nota (FNotebookModel), obj_* (FBeaconProgressModel),
 * pensamiento_* (FBeaconDirectorModel), epilogo_* y cartel_inicio.
 */
struct EXPLORED_API FBeaconTexts
{
	TMap<FName, FExploredLocalizedString> Texts;
	/** Segundos que dura un pensamiento en pantalla (thoughts.json). */
	float SubtitleSeconds = 4.0f;

	bool Has(FName Id) const { return Texts.Contains(Id); }
	/** Texto en la cultura activa; vacío si el id no existe. */
	FText Get(FName Id) const;
};

namespace ExploredBeaconTexts
{
	/** Textos de Content/Data (se leen una vez y se guardan). */
	EXPLORED_API const FBeaconTexts& Get();

	/** Lee los tres ficheros de Directory; los problemas van a OutErrors (un fichero que falta también). */
	EXPLORED_API FBeaconTexts LoadFromDirectory(const FString& Directory, TArray<FString>& OutErrors);
}
