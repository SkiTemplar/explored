#pragma once

#include "CoreMinimal.h"

/**
 * Elección de idioma para los textos de datos (GDD §15: Ajustes → Juego → idioma ES/EN).
 *
 * Los textos del código van con NSLOCTEXT/LOCTEXT y los traduce Unreal con la cultura
 * activa. Los de Content/Data/*.json no pasan por ese sistema: cada uno trae su pareja
 * (nameEs/nameEn, label/labelEn...) y el juego elige aquí según la cultura activa
 * (FInternationalization::Get().GetCurrentCulture()->GetName()). Modelo puro: sin UObject.
 * Ver docs/tecnico/localizacion.md.
 */
namespace ExploredLocalization
{
	/** Cultura nativa del juego: el español es el texto fuente. */
	inline constexpr const TCHAR* NativeCulture = TEXT("es");

	/** "en", "en-US", "EN_gb"... → true. Cualquier otra cultura (o vacía) usa el español. */
	EXPLORED_API bool IsEnglishCulture(const FString& CultureName);

	/** El texto del idioma de la cultura; si falta el inglés, cae al español (nunca vacío si hay español). */
	EXPLORED_API const FString& Pick(const FString& Es, const FString& En, const FString& CultureName);
}

/** Texto bilingüe de los datos (p. ej. nameEs/nameEn de items.json). */
struct EXPLORED_API FExploredLocalizedString
{
	FString Es;
	FString En;

	FExploredLocalizedString() = default;
	FExploredLocalizedString(const FString& InEs, const FString& InEn) : Es(InEs), En(InEn) {}

	const FString& Get(const FString& CultureName) const { return ExploredLocalization::Pick(Es, En, CultureName); }

	/** Tiene los dos idiomas (lo exige Tools/Localization para todo texto que ve el jugador). */
	bool IsComplete() const { return !Es.IsEmpty() && !En.IsEmpty(); }
};
