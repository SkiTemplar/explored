#include "ExploredGameMode.h"

#include "Player/ExploredCharacter.h"
#include "UI/ExploredHUD.h"
#include "UI/ExploredPlayerController.h"

AExploredGameMode::AExploredGameMode()
{
	DefaultPawnClass = AExploredCharacter::StaticClass();
	HUDClass = AExploredHUD::StaticClass();
	PlayerControllerClass = AExploredPlayerController::StaticClass();
}
