#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PauseMenuSettingsSaveGame.generated.h"

/** Settings that are not covered by Unreal's built-in UGameUserSettings. */
UCLASS()
class TFG_API UPauseMenuSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float MasterVolume = 1.0f;

	UPROPERTY()
	float MouseSensitivity = 1.0f;

	UPROPERTY()
	bool bInvertVerticalLook = false;
};
