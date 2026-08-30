#include "UI/PauseMenuComponent.h"

#include "UI/PauseMenuSettingsSaveGame.h"
#include "UI/PauseMenuWidget.h"
#include "TFGCharacter.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogPauseMenu, Log, All);

const FString UPauseMenuComponent::SettingsSlot(TEXT("TFG_PlayerSettings"));

UPauseMenuComponent::UPauseMenuComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPauseMenuComponent::BeginPlay()
{
	Super::BeginPlay();
	LoadSettings();
	ApplyMasterVolume(SavedSettings ? SavedSettings->MasterVolume : 1.0f);

	UE_LOG(
		LogPauseMenu,
		Display,
		TEXT("[MENU PAUSA] Configuracion cargada: volumen %.0f%%, sensibilidad %.2f, eje Y invertido=%s."),
		(SavedSettings ? SavedSettings->MasterVolume : 1.0f) * 100.0f,
		GetMouseSensitivity(),
		IsVerticalLookInverted() ? TEXT("si") : TEXT("no"));
}

void UPauseMenuComponent::TogglePauseMenu()
{
	if (ActiveWidget)
	{
		ClosePauseMenu();
	}
	else
	{
		OpenPauseMenu();
	}
}

void UPauseMenuComponent::OpenPauseMenu()
{
	if (ActiveWidget || !GetWorld())
	{
		return;
	}

	APlayerController* Controller = ResolvePlayerController();
	if (!Controller)
	{
		UE_LOG(LogPauseMenu, Error, TEXT("[MENU PAUSA] No se encontro el controlador del jugador."));
		return;
	}

	if (!SavedSettings)
	{
		LoadSettings();
	}

	ActiveWidget = CreateWidget<UPauseMenuWidget>(
		Controller,
		UPauseMenuWidget::StaticClass());
	if (!ActiveWidget)
	{
		UE_LOG(LogPauseMenu, Error, TEXT("[MENU PAUSA] No se pudo crear la interfaz."));
		return;
	}

	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const FIntPoint Resolution = UserSettings
		? UserSettings->GetScreenResolution()
		: FIntPoint(1920, 1080);
	const bool bFullscreen = UserSettings &&
		UserSettings->GetFullscreenMode() != EWindowMode::Windowed;

	ActiveWidget->Configure(
		this,
		SavedSettings ? SavedSettings->MasterVolume : 1.0f,
		Resolution,
		bFullscreen,
		GetMouseSensitivity(),
		IsVerticalLookInverted());
	ActiveWidget->AddToViewport(2500);

	PausedController = Controller;
	bOwnsPause = true;
	Controller->SetPause(true);
	Controller->SetShowMouseCursor(true);

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(ActiveWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	Controller->SetInputMode(InputMode);

	UE_LOG(LogPauseMenu, Display, TEXT("[MENU PAUSA] Partida pausada; menu abierto con Escape."));
}

void UPauseMenuComponent::ClosePauseMenu()
{
	if (ActiveWidget)
	{
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;
	}

	if (bOwnsPause)
	{
		bOwnsPause = false;
		if (PausedController.IsValid())
		{
			PausedController->SetPause(false);
			PausedController->SetShowMouseCursor(false);
			FInputModeGameOnly InputMode;
			PausedController->SetInputMode(InputMode);
		}
	}

	PausedController.Reset();
	UE_LOG(LogPauseMenu, Display, TEXT("[MENU PAUSA] Menu cerrado; partida reanudada."));
}

void UPauseMenuComponent::ApplyMenuSettings(
	const float MasterVolume,
	const FString& Resolution,
	const bool bFullscreen,
	const float MouseSensitivity,
	const bool bInvertVerticalLook)
{
	if (!SavedSettings)
	{
		LoadSettings();
	}

	SavedSettings->MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	SavedSettings->MouseSensitivity = FMath::Clamp(MouseSensitivity, 0.1f, 3.0f);
	SavedSettings->bInvertVerticalLook = bInvertVerticalLook;
	ApplyMasterVolume(SavedSettings->MasterVolume);
	SaveSettings();

	int32 Width = 0;
	int32 Height = 0;
	FString Left;
	FString Right;
	if (Resolution.Split(TEXT("x"), &Left, &Right))
	{
		Width = FCString::Atoi(*Left.TrimStartAndEnd());
		Height = FCString::Atoi(*Right.TrimStartAndEnd());
	}

	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		if (Width >= 640 && Height >= 480)
		{
			UserSettings->SetScreenResolution(FIntPoint(Width, Height));
		}
		UserSettings->SetFullscreenMode(
			bFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
		UserSettings->ApplySettings(false);
		UserSettings->SaveSettings();
	}

	UE_LOG(
		LogPauseMenu,
		Display,
		TEXT("[MENU PAUSA] Ajustes aplicados: volumen %.0f%%, resolucion %s, pantalla completa=%s, sensibilidad %.2f, invertir Y=%s."),
		SavedSettings->MasterVolume * 100.0f,
		*Resolution,
		bFullscreen ? TEXT("si") : TEXT("no"),
		SavedSettings->MouseSensitivity,
		SavedSettings->bInvertVerticalLook ? TEXT("si") : TEXT("no"));
}

float UPauseMenuComponent::GetMouseSensitivity() const
{
	return SavedSettings
		? FMath::Clamp(SavedSettings->MouseSensitivity, 0.1f, 3.0f)
		: 1.0f;
}

float UPauseMenuComponent::GetMasterVolume() const
{
	return SavedSettings ? FMath::Clamp(SavedSettings->MasterVolume, 0.0f, 1.0f) : 1.0f;
}

bool UPauseMenuComponent::IsVerticalLookInverted() const
{
	return SavedSettings && SavedSettings->bInvertVerticalLook;
}

void UPauseMenuComponent::ReturnToMainMenu()
{
	if (!GetWorld() || MainMenuMap.IsNone())
	{
		UE_LOG(LogPauseMenu, Error,
			TEXT("[MENU PAUSA] No se puede volver al menu principal: mapa no configurado."));
		return;
	}

	// Remove the pause widget and restore an unpaused world before travelling.
	// This prevents the cursor/focus/pause state from leaking into the menu map.
	ClosePauseMenu();
	UE_LOG(LogPauseMenu, Display,
		TEXT("[MENU PAUSA] Volviendo al menu principal: %s."),
		*MainMenuMap.ToString());
	UGameplayStatics::OpenLevel(this, MainMenuMap);
}

void UPauseMenuComponent::ReturnToLobby()
{
	if (!GetWorld() || LobbyMap.IsNone())
	{
		UE_LOG(LogPauseMenu, Error,
			TEXT("[MENU PAUSA] No se puede volver al lobby: mapa no configurado."));
		return;
	}

	// Clear the run before travel and prevent the character's EndPlay from
	// restoring the discarded powers into the GameInstance subsystem.
	if (ATFGCharacter* Character = Cast<ATFGCharacter>(GetOwner()))
	{
		Character->PrepararSalidaVoluntariaAlLobby();
	}
	ClosePauseMenu();
	UE_LOG(LogPauseMenu, Display,
		TEXT("[MENU PAUSA] Run abandonada, poderes temporales eliminados; volviendo al lobby: %s."),
		*LobbyMap.ToString());
	UGameplayStatics::OpenLevel(this, LobbyMap);
}

void UPauseMenuComponent::QuitGame()
{
	APlayerController* Controller = PausedController.IsValid()
		? PausedController.Get()
		: ResolvePlayerController();
	UE_LOG(LogPauseMenu, Display, TEXT("[MENU PAUSA] Salir seleccionado; cerrando la partida."));
	UKismetSystemLibrary::QuitGame(
		this,
		Controller,
		EQuitPreference::Quit,
		false);
}

void UPauseMenuComponent::LoadSettings()
{
	SavedSettings = Cast<UPauseMenuSettingsSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SettingsSlot, 0));
	if (!SavedSettings)
	{
		SavedSettings = Cast<UPauseMenuSettingsSaveGame>(
			UGameplayStatics::CreateSaveGameObject(
				UPauseMenuSettingsSaveGame::StaticClass()));
	}
}

void UPauseMenuComponent::SaveSettings()
{
	if (!SavedSettings ||
		!UGameplayStatics::SaveGameToSlot(SavedSettings, SettingsSlot, 0))
	{
		UE_LOG(LogPauseMenu, Warning, TEXT("[MENU PAUSA] No se pudo guardar la configuracion."));
	}
}

void UPauseMenuComponent::ApplyMasterVolume(const float Volume) const
{
	if (UWorld* World = GetWorld())
	{
		if (FAudioDevice* AudioDevice = World->GetAudioDeviceRaw())
		{
			AudioDevice->SetTransientPrimaryVolume(FMath::Clamp(Volume, 0.0f, 1.0f));
		}
	}
}

APlayerController* UPauseMenuComponent::ResolvePlayerController() const
{
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* Controller = Cast<APlayerController>(
			OwnerPawn->GetController()))
		{
			return Controller;
		}
	}
	return GetWorld() ? UGameplayStatics::GetPlayerController(GetWorld(), 0) : nullptr;
}

void UPauseMenuComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ActiveWidget)
	{
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;
	}
	bOwnsPause = false;
	PausedController.Reset();
	Super::EndPlay(EndPlayReason);
}
