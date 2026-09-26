#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "ExploredHUD.generated.h"

class AExploredCharacter;

/**
 * HUD mínimo por Canvas (GDD §7): punto de mira, texto contextual del foco y
 * el nombre de lo que hay en cada mano. Nada de widgets UMG: es deliberadamente
 * ligero para no competir visualmente con el mundo (biblia §5.4, «HUD mínimo»).
 */
UCLASS()
class EXPLORED_API AExploredHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	AExploredCharacter* GetExploredCharacter() const;
	void DrawReticle();
	void DrawContextPrompt(const AExploredCharacter& Character);
	void DrawHandLabels(const AExploredCharacter& Character);
};
