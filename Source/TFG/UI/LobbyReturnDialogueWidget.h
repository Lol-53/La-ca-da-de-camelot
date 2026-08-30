#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LobbyReturnDialogueWidget.generated.h"

/** One-time per-save dialogue shown when Arturo first returns from a completed run. */
UCLASS()
class TFG_API ULobbyReturnDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildInterface();
};
