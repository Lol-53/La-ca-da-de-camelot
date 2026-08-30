#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NPCDialogueNameWidget.generated.h"

class UTextBlock;

/**
 * Lightweight nameplate layered over the existing Blueprint dialogue UI.
 * It deliberately does not own dialogue progression, so the current E-key
 * interaction and power-up selection flow remain unchanged.
 */
UCLASS()
class TFG_API UNPCDialogueNameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetNPCName(const FText& InName);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	FText PendingName;
};
