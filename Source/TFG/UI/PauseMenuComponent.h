#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PauseMenuComponent.generated.h"

class APlayerController;
class UPauseMenuSettingsSaveGame;
class UPauseMenuWidget;

/** Owns pause state, the pause menu and persistent player-facing settings. */
UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class TFG_API UPauseMenuComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPauseMenuComponent();

	UFUNCTION(BlueprintCallable, Category="Interfaz|Pausa")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category="Interfaz|Pausa")
	void OpenPauseMenu();

	UFUNCTION(BlueprintCallable, Category="Interfaz|Pausa")
	void ClosePauseMenu();

	UFUNCTION(BlueprintCallable, Category="Interfaz|Pausa")
	void ApplyMenuSettings(
		float MasterVolume,
		const FString& Resolution,
		bool bFullscreen,
		float MouseSensitivity,
		bool bInvertVerticalLook);

	UFUNCTION(BlueprintPure, Category="Interfaz|Pausa")
	bool IsPauseMenuOpen() const { return ActiveWidget != nullptr; }

	/** Leaves the current game and loads the dedicated main-menu map. */
	UFUNCTION(BlueprintCallable, Category="Interfaz|Pausa")
	void ReturnToMainMenu();

	/** Clears temporary run powers and returns to the Round Table lobby. */
	UFUNCTION(BlueprintCallable, Category="Interfaz|Pausa")
	void ReturnToLobby();

	float GetMouseSensitivity() const;
	float GetMasterVolume() const;
	bool IsVerticalLookInverted() const;
	void QuitGame();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interfaz|Pausa")
	FName MainMenuMap = TEXT("/Game/MyContent/Maps/Menu_Principal");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interfaz|Pausa")
	FName LobbyMap = TEXT("/Game/MyContent/Maps/Lobby_MesaRedonda");

private:
	void LoadSettings();
	void SaveSettings();
	void ApplyMasterVolume(float Volume) const;
	APlayerController* ResolvePlayerController() const;

	static const FString SettingsSlot;

	UPROPERTY(Transient)
	TObjectPtr<UPauseMenuWidget> ActiveWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPauseMenuSettingsSaveGame> SavedSettings;

	TWeakObjectPtr<APlayerController> PausedController;
	bool bOwnsPause = false;
};
