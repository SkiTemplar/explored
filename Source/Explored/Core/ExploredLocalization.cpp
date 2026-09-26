#include "Core/ExploredLocalization.h"

namespace ExploredLocalization
{
	bool IsEnglishCulture(const FString& CultureName)
	{
		// Unreal nombra las culturas con BCP 47 ("en", "en-US"), pero se aceptan también
		// guion bajo y mayúsculas por si llegan de un .ini o de la línea de órdenes.
		const FString Lower = CultureName.ToLower();
		if (Lower.Len() < 2 || !(Lower.Left(2) == FString(TEXT("en"))))
		{
			return false;
		}
		return Lower.Len() == 2 || Lower[2] == TEXT('-') || Lower[2] == TEXT('_');
	}

	const FString& Pick(const FString& Es, const FString& En, const FString& CultureName)
	{
		return (IsEnglishCulture(CultureName) && !En.IsEmpty()) ? En : Es;
	}
}
