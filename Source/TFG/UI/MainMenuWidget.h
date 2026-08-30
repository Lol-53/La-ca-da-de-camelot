#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class AMainMenuManager;
class UButton;
class UCheckBox;
class UComboBoxString;
class UImage;
class USlider;
class UTextBlock;
class UTexture2D;
class UWidget;
class UWidgetSwitcher;

/** Native, Blueprintable main menu with main, save, settings and credits pages. */
UCLASS(Blueprintable)
class TFG_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Configure(AMainMenuManager* InManager, UTexture2D* InBackground, const FText& InCredits);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildInterface();
	void RefreshSaveSlots();
	void RefreshSettings();
	void PopulateResolutionOptions(const FIntPoint& Resolution);
	void RefreshSettingLabels();
	void RequestDeleteSlot(int32 SlotIndex);

	UFUNCTION() void ShowMainPage();
	UFUNCTION() void ShowSavePage();
	UFUNCTION() void ShowSettingsPage();
	UFUNCTION() void ShowCreditsPage();
	UFUNCTION() void ChooseSlot1();
	UFUNCTION() void ChooseSlot2();
	UFUNCTION() void ChooseSlot3();
	UFUNCTION() void DeleteSlot1();
	UFUNCTION() void DeleteSlot2();
	UFUNCTION() void DeleteSlot3();
	UFUNCTION() void ApplySettings();
	UFUNCTION() void RestoreDefaults();
	UFUNCTION() void QuitGame();
	UFUNCTION() void HandleVolumeChanged(float Value);
	UFUNCTION() void HandleSensitivityChanged(float Value);

	UPROPERTY(Transient) TObjectPtr<UImage> BackgroundImageWidget;
	UPROPERTY(Transient) TObjectPtr<UWidget> MainPageRoot;
	UPROPERTY(Transient) TObjectPtr<UWidget> SecondaryPanelRoot;
	UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher> PageSwitcher;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> SaveSlotLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> SaveDeleteButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> SaveDeleteLabels;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CreditsBody;
	UPROPERTY(Transient) TObjectPtr<USlider> VolumeSlider;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> VolumeValueText;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> ResolutionCombo;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> FullscreenCheckBox;
	UPROPERTY(Transient) TObjectPtr<USlider> SensitivitySlider;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SensitivityValueText;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> InvertYCheckBox;

	TWeakObjectPtr<AMainMenuManager> Manager;
	int32 PendingDeleteSlot = 0;
};
