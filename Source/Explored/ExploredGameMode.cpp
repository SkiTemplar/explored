#include "ExploredGameMode.h"

#include "Player/ExploredCharacter.h"
#include "UI/ExploredHUD.h"

AExploredGameMode::AExploredGameMode()
{
	DefaultPawnClass = AExploredCharacter::StaticClass();
	HUDClass = AExploredHUD::StaticClass();
}
