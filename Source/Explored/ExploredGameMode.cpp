#include "ExploredGameMode.h"

#include "Player/ExploredCharacter.h"

AExploredGameMode::AExploredGameMode()
{
	DefaultPawnClass = AExploredCharacter::StaticClass();
}
