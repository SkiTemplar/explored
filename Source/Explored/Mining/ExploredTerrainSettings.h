#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "ExploredTerrainSettings.generated.h"

class UMaterialInterface;

/**
 * Ajustes de proyecto del terreno editable (Project Settings → Game → Explored Terreno).
 * Se guardan en `Config/DefaultGame.ini`, sección `[/Script/Explored.ExploredTerrainSettings]`.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Explored Terreno"))
class EXPLORED_API UExploredTerrainSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UExploredTerrainSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/**
	 * Material de las mallas finas del terreno cavado. Vacío: el de los chunks horneados
	 * (`M_Terrain`), que se toma del primer chunk que se registra.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Remallado")
	TSoftObjectPtr<UMaterialInterface> RuntimeTerrainMaterial;
};
