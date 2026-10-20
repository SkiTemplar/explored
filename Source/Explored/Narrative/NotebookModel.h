#pragma once

#include "CoreMinimal.h"

/**
 * Cuaderno de guardia de Roig (GDD v3 §2.1): 8 páginas numeradas del 1 al 8.
 * Solo la 1 y la 8 están en el camino crítico; el epílogo completo exige las 8.
 *
 * El estado es un bitmask de 8 bits (bit 0 = página 1) y se guarda tal cual:
 * cualquier valor de 8 bits es válido. Las funciones son puras y devuelven
 * estados nuevos en lugar de modificar el original.
 */
struct EXPLORED_API FNotebookState
{
	uint8 PagesMask = 0;

	bool operator==(const FNotebookState& Other) const { return PagesMask == Other.PagesMask; }
};

/** Resultado de marcar una página: bChanged es falso si no era válida o ya estaba leída. */
struct EXPLORED_API FNotebookMarkResult
{
	bool bChanged = false;
	FNotebookState State;
};

struct EXPLORED_API FNotebookModel
{
	static constexpr int32 PageCount = 8;
	static constexpr uint8 AllPagesMask = 0xFF;

	static bool IsValidPage(int32 Page);

	/** Páginas del camino crítico: la 1 (casa del farero) y la 8 (linterna). */
	static bool IsCritical(int32 Page);

	static bool IsRead(const FNotebookState& State, int32 Page);

	/** Marca una página como leída; una página fuera de 1-8 o ya leída no cambia nada. */
	static FNotebookMarkResult MarkRead(const FNotebookState& State, int32 Page);

	static int32 ReadCount(const FNotebookState& State);

	/** Las 8 páginas leídas: epílogo completo (GDD §2, «8 de 8»). */
	static bool IsComplete(const FNotebookState& State);

	/** Las dos páginas críticas leídas. */
	static bool HasCriticalPages(const FNotebookState& State);

	/** Id de texto de la página («cuaderno_p3») y de la nota de Iria («cuaderno_p3_nota»); NAME_None si no es válida. */
	static FName PageTextId(int32 Page);
	static FName NoteTextId(int32 Page);
};
