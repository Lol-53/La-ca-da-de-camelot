#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RoomPowerUpSelectionWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * Native two-card interface shown after the room NPC finishes speaking.
 * It is intentionally built in C++ so every procedural room can use it
 * without requiring a hand-authored Widget Blueprint instance.
 */
UCLASS()
class TFG_API URoomPowerUpSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPowerUpChosen, int32);
	FOnPowerUpChosen OnPowerUpChosen;

	void Configure(
		int32 InOptionAId,
		const FText& InOptionATitle,
		const FText& InOptionADescription,
		int32 InOptionBId,
		const FText& InOptionBTitle,
		const FText& InOptionBDescription);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildInterface();
	void RefreshOptionText();

	UFUNCTION()
	void ChooseOptionA();

	UFUNCTION()
	void ChooseOptionB();

	UPROPERTY(Transient)
	TObjectPtr<UButton> OptionAButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> OptionBButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OptionATitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OptionADescriptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OptionBTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OptionBDescriptionText;

	int32 OptionAId = INDEX_NONE;
	int32 OptionBId = INDEX_NONE;
	FText OptionATitle;
	FText OptionADescription;
	FText OptionBTitle;
	FText OptionBDescription;
};
