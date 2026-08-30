#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UPauseMenuComponent;
class USlider;
class UTextBlock;
class UWidgetSwitcher;

/** Native pause/settings screen used without requiring a Widget Blueprint. */
UCLASS()
class TFG_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Configure(
		UPauseMenuComponent* InOwnerComponent,
		float MasterVolume,
		const FIntPoint& Resolution,
		bool bFullscreen,
		float MouseSensitivity,
		bool bInvertVerticalLook);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;

private:
	void BuildInterface();
	void PopulateResolutionOptions(const FIntPoint& CurrentResolution);
	void RefreshValueLabels();

	UFUNCTION()
	void ContinueGame();

	UFUNCTION()
	void ShowSettings();

	UFUNCTION()
	void ShowMainMenu();

	UFUNCTION()
	void ReturnToMainMenu();

	UFUNCTION()
	void ReturnToLobby();

	UFUNCTION()
	void ShowLobbyConfirmation();

	UFUNCTION()
	void CancelLobbyReturn();

	UFUNCTION()
	void ApplySettings();

	UFUNCTION()
	void RestoreDefaults();

	UFUNCTION()
	void ExitGame();

	UFUNCTION()
	void HandleVolumeChanged(float Value);

	UFUNCTION()
	void HandleSensitivityChanged(float Value);

	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> PageSwitcher;

	UPROPERTY(Transient)
	TObjectPtr<USlider> VolumeSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> VolumeValueText;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> ResolutionCombo;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> FullscreenCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<USlider> SensitivitySlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SensitivityValueText;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> InvertYCheckBox;

	TWeakObjectPtr<UPauseMenuComponent> OwnerComponent;
};
