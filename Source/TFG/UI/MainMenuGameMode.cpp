#include "UI/MainMenuGameMode.h"

#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	PlayerControllerClass = APlayerController::StaticClass();
	bStartPlayersAsSpectators = true;
}
