#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MordredVictoryDialogueWidget.generated.h"

class UTextBlock;

/** Centered, manually advanced epilogue shown after Mordred is defeated. */
UCLASS()
class TFG_API UMordredVictoryDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetDialogueLine(const FString& Line, int32 CurrentLine, int32 TotalLines);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildInterface();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProgressText;
};
