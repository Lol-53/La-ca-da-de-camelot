#include "UI/MainMenuManager.h"

#include "Systems/RunPowerPersistenceSubsystem.h"
#include "UI/MainMenuWidget.h"
#include "UI/PauseMenuComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogMainMenu, Log, All);

AMainMenuManager::AMainMenuManager()
{
	PrimaryActorTick.bCanEverTick = false;
	SettingsComponent = CreateDefaultSubobject<UPauseMenuComponent>(TEXT("SettingsComponent"));
	BackgroundImage = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/MyContent/Visuals/T_Fondo_Menu_Principal.T_Fondo_Menu_Principal")));
	CreditsText = FText::FromString(
		TEXT("LA CAIDA DE CAMELOT\n\n")
		TEXT("Direccion y desarrollo\nJavier\n\n")
		TEXT("Diseno, programacion y arte\nEquipo de La caida de Camelot\n\n")
		TEXT("Creado con Unreal Engine 5\n\n")
		TEXT("Edita este texto seleccionando MainMenuManager en el mapa Menu_Principal."));
}

void AMainMenuManager::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	if (Controller)
	{
		if (APawn* Pawn = Controller->GetPawn())
		{
			Pawn->SetActorHiddenInGame(true);
			Pawn->DisableInput(Controller);
		}

		MenuWidget = CreateWidget<UMainMenuWidget>(Controller, UMainMenuWidget::StaticClass());
		if (MenuWidget)
		{

	// This is the authored project background and deliberately takes priority
	// over the legacy T_MainMenu_Camelot override stored in the placed actor.
	UTexture2D* ResolvedBackground = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/MyContent/Visuals/T_Fondo_Menu_Principal.T_Fondo_Menu_Principal"));
	if (!ResolvedBackground)
	{
		ResolvedBackground = BackgroundImage.LoadSynchronous();
	}
	MenuWidget->Configure(this, ResolvedBackground, CreditsText);
	MenuWidget->AddToViewport(5000);
	Controller->SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Controller->SetInputMode(InputMode);

			UE_LOG(LogMainMenu, Display, TEXT("[MENU PRINCIPAL] Interfaz inicial abierta."));
		}
		else
		{
			UE_LOG(LogMainMenu, Error, TEXT("[MENU PRINCIPAL] No se pudo crear la interfaz."));
		}
	}
	else
	{
		UE_LOG(LogMainMenu, Error, TEXT("[MENU PRINCIPAL] No se encontro PlayerController."));
	}
}

void AMainMenuManager::StartSaveSlot(const int32 SlotIndex)
{
	// UIOnly focus and cursor state can survive the map travel at viewport
	// level. Remove the menu and hand control back before opening gameplay.
	if (MenuWidget)
	{
		MenuWidget->RemoveFromParent();
		MenuWidget = nullptr;
	}
	RestoreGameplayInput();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URunPowerPersistenceSubsystem* Persistence =
			GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>())
		{
			Persistence->SelectSaveSlot(SlotIndex);
			Persistence->ResetPersistentPowers();
		}
	}

	UE_LOG(LogMainMenu, Display,
		TEXT("[MENU PRINCIPAL] Jugar: Partida %d -> %s."), SlotIndex, *FirstGameplayMap.ToString());
	UGameplayStatics::OpenLevel(this, FirstGameplayMap);
}

void AMainMenuManager::RestoreGameplayInput() const
{
	if (APlayerController* Controller =
		UGameplayStatics::GetPlayerController(this, 0))
	{
		Controller->SetPause(false);
		Controller->ResetIgnoreMoveInput();
		Controller->ResetIgnoreLookInput();
		Controller->SetShowMouseCursor(false);
		Controller->bEnableClickEvents = false;
		Controller->bEnableMouseOverEvents = false;
		Controller->FlushPressedKeys();
		FInputModeGameOnly InputMode;
		Controller->SetInputMode(InputMode);
		UE_LOG(LogMainMenu, Display,
			TEXT("[MENU PRINCIPAL] Foco de entrada devuelto al juego antes de viajar."));
	}
}

void AMainMenuManager::QuitGame()
{
	UE_LOG(LogMainMenu, Display, TEXT("[MENU PRINCIPAL] Saliendo del juego."));
	UKismetSystemLibrary::QuitGame(
		this,
		UGameplayStatics::GetPlayerController(this, 0),
		EQuitPreference::Quit,
		false);
}

void AMainMenuManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MenuWidget)
	{
		MenuWidget->RemoveFromParent();
		MenuWidget = nullptr;
	}
	RestoreGameplayInput();
	Super::EndPlay(EndPlayReason);
}
