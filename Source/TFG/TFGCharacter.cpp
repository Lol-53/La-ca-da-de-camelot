// Copyright Epic Games, Inc. All Rights Reserved.

#include "TFGCharacter.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "BrainComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Components/ActorComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "UObject/FieldIterator.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Perception/PawnSensingComponent.h"
#include "Sound/SoundBase.h"
#include "Systems/RunPowerPersistenceSubsystem.h"
#include "Systems/MusicManagerSubsystem.h"
#include "TimerManager.h"
#include "UI/PauseMenuComponent.h"
#include "UI/LobbyReturnDialogueWidget.h"
#include "UI/PotionQuickbarWidget.h"
#include "UI/RunStatusWidget.h"
#include "UObject/UnrealType.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);
DEFINE_LOG_CATEGORY(LogCombatPowers);

namespace CombatPowerHelpers
{
	constexpr float StaminaRegeneradaPorSegundo = 8.0f;
	constexpr float StaminaConsumidaSprintPorSegundo = 12.0f;

	FNumericProperty* FindNumericProperty(const UObject* Object, const FName PropertyName)
	{
		return Object ? FindFProperty<FNumericProperty>(Object->GetClass(), PropertyName) : nullptr;
	}

	double ReadNumericProperty(const UObject* Object, FNumericProperty* Property)
	{
		if (!Object || !Property)
		{
			return 0.0;
		}

		const void* ValueAddress = Property->ContainerPtrToValuePtr<void>(Object);
		return Property->IsFloatingPoint()
			? Property->GetFloatingPointPropertyValue(ValueAddress)
			: static_cast<double>(Property->GetSignedIntPropertyValue(ValueAddress));
	}

	void WriteNumericProperty(UObject* Object, FNumericProperty* Property, const double Value)
	{
		if (!Object || !Property)
		{
			return;
		}

		void* ValueAddress = Property->ContainerPtrToValuePtr<void>(Object);
		if (Property->IsFloatingPoint())
		{
			Property->SetFloatingPointPropertyValue(ValueAddress, Value);
		}
		else
		{
			Property->SetIntPropertyValue(ValueAddress, FMath::RoundToInt64(Value));
		}
	}
}

ATFGCharacter::ATFGCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// The imported cape cloth was authored for the package's original motion.
	// With the project's retargeted combo it can stretch through the character,
	// so render it from the animation pose without starting cloth simulation.
	GetMesh()->bAllowClothActors = false;
	GetMesh()->bDisableClothSimulation = true;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	PauseMenuComponent = CreateDefaultSubobject<UPauseMenuComponent>(
		TEXT("PauseMenuComponent"));

	// Keep the weapon independent so it can follow the existing combo while its
	// grip is authored visually with a socket on the right hand.
	DarkKnightSwordVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("DarkKnightSwordVisual"));
	// The socket owns the complete grip transform. Adjust SwordGripSocket in the
	// Skeletal Mesh/Skeleton editor instead of compensating for it in code.
	DarkKnightSwordVisual->SetupAttachment(GetMesh(), TEXT("SwordGripSocket"));
	DarkKnightSwordVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DarkKnightSwordVisual->SetGenerateOverlapEvents(false);
	// Do not add a second transform here: SwordSocket is the single source of
	// truth for the weapon's position and rotation.
	DarkKnightSwordVisual->SetRelativeLocation(FVector::ZeroVector);
	DarkKnightSwordVisual->SetRelativeRotation(FRotator::ZeroRotator);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DarkKnightSwordMesh(
		TEXT("/Game/Dark_Knight/Dark_Knight_Male/Meshes/SM_DKM_Sword.SM_DKM_Sword"));
	if (DarkKnightSwordMesh.Succeeded())
	{
		DarkKnightSwordVisual->SetStaticMesh(DarkKnightSwordMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DarkKnightSwordMaterial(
		TEXT("/Game/MyContent/Materials/DarkKnightWhite/MI_DKM_Sword_White.MI_DKM_Sword_White"));
	if (DarkKnightSwordMaterial.Succeeded())
	{
		DarkKnightSwordVisual->SetMaterial(0, DarkKnightSwordMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DarkKnightDeathAnimation(
		TEXT("/Game/Dark_Knight/Dark_Knight_Male/Animations/Anim_DKM_Death.Anim_DKM_Death"));
	if (DarkKnightDeathAnimation.Succeeded())
	{
		AnimacionMuerte = DarkKnightDeathAnimation.Object;
	}

	static ConstructorHelpers::FClassFinder<UCameraShakeBase> HitEnemyCameraShake(
		TEXT("/Game/Variant_Combat/Blueprints/BP_CameraShake_Hit_Enemy"));
	if (HitEnemyCameraShake.Succeeded())
	{
		CameraShakeGolpeEnemigo = HitEnemyCameraShake.Class;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> HitSound(
		TEXT("/Game/MyContent/Audio/Sounds/Hit.Hit"));
	static ConstructorHelpers::FObjectFinder<USoundBase> HurtSound(
		TEXT("/Game/MyContent/Audio/Sounds/hurt.hurt"));
	static ConstructorHelpers::FObjectFinder<USoundBase> SwordSound(
		TEXT("/Game/MyContent/Audio/Sounds/sword.sword"));
	static ConstructorHelpers::FObjectFinder<USoundBase> FireballSound(
		TEXT("/Game/MyContent/Audio/Sounds/floraphonic-fireball-whoosh-1-179125.floraphonic-fireball-whoosh-1-179125"));
	static ConstructorHelpers::FObjectFinder<USoundBase> DefeatSound(
		TEXT("/Game/MyContent/Audio/Sounds/Derrota_caballeresca.Derrota_caballeresca"));
	if (SwordSound.Succeeded())
	{
		SonidoAtaque = SwordSound.Object;
	}
	if (HitSound.Succeeded())
	{
		SonidoImpacto = HitSound.Object;
	}
	if (HurtSound.Succeeded())
	{
		SonidoRecibirDanyo = HurtSound.Object;
	}
	if (FireballSound.Succeeded())
	{
		SonidoLanzamientoProyectil = FireballSound.Object;
	}
	if (DefeatSound.Succeeded())
	{
		SonidoDerrotaCaballeresca = DefeatSound.Object;
	}
}

void ATFGCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATFGCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ATFGCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ATFGCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ATFGCharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATFGCharacter::Look);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' no tiene Enhanced Input Component."), *GetNameSafe(this));
	}

	// Room rewards are passive except for the single active slot, used with R.
	// The old numeric debug bindings are intentionally removed.
	PlayerInputComponent->BindKey(
		EKeys::R,
		IE_Pressed,
		this,
		&ATFGCharacter::ActivarPowerUpDeSalaEquipado);

	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &ATFGCharacter::IniciarSprint);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &ATFGCharacter::DetenerSprint);
	PlayerInputComponent->BindKey(EKeys::RightShift, IE_Pressed, this, &ATFGCharacter::IniciarSprint);
	PlayerInputComponent->BindKey(EKeys::RightShift, IE_Released, this, &ATFGCharacter::DetenerSprint);
	PlayerInputComponent->BindKey(EKeys::Z, IE_Pressed, this, &ATFGCharacter::UsarPocionVida);
	PlayerInputComponent->BindKey(EKeys::X, IE_Pressed, this, &ATFGCharacter::UsarPocionEnergia);

	// Escape opens the in-game pause menu. PIE must allow the key to reach the game.
	FInputKeyBinding& PauseBinding = PlayerInputComponent->BindKey(
		EKeys::Escape,
		IE_Pressed,
		this,
		&ATFGCharacter::AlternarMenuPausa);
	PauseBinding.bExecuteWhenPaused = true;

#if !WITH_EDITOR
	// These two inputs were authored in BP_ThirdPersonCharacter using
	// K2Node_InputDebugKey. Unreal intentionally disables Debug Key events in
	// Shipping builds, so forward normal key bindings to the generated Blueprint
	// event functions. The projectile uses a regular F key event and therefore
	// does not need this fallback.
	PlayerInputComponent->BindKey(
		EKeys::LeftMouseButton,
		IE_Pressed,
		this,
		&ATFGCharacter::EjecutarAtaquePrincipalEmpaquetado);
	PlayerInputComponent->BindKey(
		EKeys::E,
		IE_Pressed,
		this,
		&ATFGCharacter::EjecutarInteraccionEmpaquetada);
#endif
}

void ATFGCharacter::EjecutarAtaquePrincipalEmpaquetado()
{
	EjecutarEventoEntradaBlueprintEmpaquetado(TEXT("InpActEvt_LeftMouseButton"));
}

void ATFGCharacter::EjecutarInteraccionEmpaquetada()
{
	EjecutarEventoEntradaBlueprintEmpaquetado(TEXT("InpActEvt_E_"));
}

void ATFGCharacter::EjecutarEventoEntradaBlueprintEmpaquetado(
	const TCHAR* PrefijoEvento)
{
	for (TFieldIterator<UFunction> FunctionIt(
		GetClass(), EFieldIterationFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		UFunction* Function = *FunctionIt;
		if (!Function || !Function->GetName().StartsWith(PrefijoEvento))
		{
			continue;
		}

		TArray<uint8> Parameters;
		Parameters.SetNumZeroed(Function->ParmsSize);
		ProcessEvent(Function, Parameters.Num() > 0 ? Parameters.GetData() : nullptr);
		UE_LOG(
			LogCombatPowers,
			Display,
			TEXT("[ENTRADA SHIPPING] Ejecutado evento Blueprint %s."),
			*Function->GetName());
		return;
	}

	UE_LOG(
		LogCombatPowers,
		Warning,
		TEXT("[ENTRADA SHIPPING] No se encontro evento Blueprint con prefijo %s."),
		PrefijoEvento);
}

void ATFGCharacter::BeginPlay()
{
	Super::BeginPlay();

	// A main-menu UIOnly input mode belongs to the viewport and may remain
	// active for the first frame after OpenLevel. Reassert gameplay input once
	// this pawn has been spawned and possessed in the destination map.
	GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (APlayerController* PlayerController =
				Cast<APlayerController>(GetController()))
			{
				PlayerController->SetPause(false);
				PlayerController->ResetIgnoreMoveInput();
				PlayerController->ResetIgnoreLookInput();
				PlayerController->SetShowMouseCursor(false);
				PlayerController->bEnableClickEvents = false;
				PlayerController->bEnableMouseOverEvents = false;
				PlayerController->FlushPressedKeys();
				FInputModeGameOnly InputMode;
				PlayerController->SetInputMode(InputMode);
				EnableInput(PlayerController);
				UE_LOG(LogCombatPowers, Display,
					TEXT("[ENTRADA] Personaje poseido; control de juego restaurado tras el cambio de mapa."));
			}
		}));

	// Native default subobjects added during Live Coding are not injected into
	// Blueprint CDOs that were already loaded. Create a runtime fallback so the
	// sword is visible immediately as well as after a full editor restart.
	if (!IsValid(DarkKnightSwordVisual))
	{
		DarkKnightSwordVisual = NewObject<UStaticMeshComponent>(
			this, TEXT("DarkKnightSwordVisualRuntime"));
		AddInstanceComponent(DarkKnightSwordVisual);
		DarkKnightSwordVisual->SetupAttachment(
			GetMesh(), TEXT("SwordGripSocket"));
		DarkKnightSwordVisual->SetCollisionEnabled(
			ECollisionEnabled::NoCollision);
		DarkKnightSwordVisual->SetGenerateOverlapEvents(false);
		DarkKnightSwordVisual->RegisterComponent();
	}

	if (!DarkKnightSwordVisual->GetStaticMesh())
	{
		DarkKnightSwordVisual->SetStaticMesh(LoadObject<UStaticMesh>(
			nullptr,
			TEXT("/Game/Dark_Knight/Dark_Knight_Male/Meshes/SM_DKM_Sword.SM_DKM_Sword")));
	}
	if (UMaterialInterface* SwordMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/MyContent/Materials/DarkKnightWhite/MI_DKM_Sword_White.MI_DKM_Sword_White")))
	{
		DarkKnightSwordVisual->SetMaterial(0, SwordMaterial);
	}
	DarkKnightSwordVisual->SetRelativeTransform(FTransform::Identity);
	DarkKnightSwordVisual->SetHiddenInGame(false);
	DarkKnightSwordVisual->SetVisibility(true, true);

	GetMesh()->bDisableClothSimulation = true;
	GetMesh()->SuspendClothingSimulation();
	VelocidadBase = GetCharacterMovement()->MaxWalkSpeed;
	// Existing Blueprint defaults may still contain the previous prototype
	// value. Guarantee the longer dash requested for already-created assets.
	FuerzaDash = FMath::Max(FuerzaDash, 3600.0f);
	const bool bEnLobbyMesaRedonda = EsLobbyMesaRedonda();
	if (bEnLobbyMesaRedonda)
	{
		// The lobby is a hard run boundary. Never let the lobby pawn restore or
		// re-save temporary powers after death, victory, abandoning a run, or
		// starting a new PIE session.
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (URunPowerPersistenceSubsystem* Persistence =
				GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>())
			{
				Persistence->ResetPersistentPowers();
			}
		}
		bOmitirGuardadoPoderesEnEndPlay = true;
		UE_LOG(
			LogCombatPowers,
			Display,
			TEXT("[PERSISTENCIA PODERES] Lobby detectado: estado temporal de la run eliminado y guardado de EndPlay bloqueado."));
	}
	else
	{
		RestaurarEstadoPoderesPersistentes();
	}
	AplicarMejorasPermanentes();
	RestaurarVidaPersistente();
	CrearHUDPociones();
	FTimerHandle TimerOcultarEtiquetaVida;
	GetWorldTimerManager().SetTimer(
		TimerOcultarEtiquetaVida,
		this,
		&ATFGCharacter::OcultarEtiquetaVidaActualHUD,
		0.35f,
		false);
	if (bEnLobbyMesaRedonda)
	{
		// The Blueprint creates InterfazGrafica after native BeginPlay. Waiting
		// one frame ensures that only the finished gameplay HUD is removed.
		GetWorldTimerManager().SetTimerForNextTick(
			this, &ATFGCharacter::ConfigurarHUDParaMapaActual);
		GetWorldTimerManager().SetTimer(
			TimerOcultarHUDLobby,
			this,
			&ATFGCharacter::ConfigurarHUDParaMapaActual,
			0.5f,
			false);
		GetWorldTimerManager().SetTimer(
			TimerDialogoPrimerRetorno,
			this,
			&ATFGCharacter::MostrarDialogoPrimerRetornoSiPendiente,
			0.75f,
			false);
	}
	GetWorldTimerManager().SetTimer(TimerRegeneracionMana, this, &ATFGCharacter::RegenerarMana, 1.0f, true, 1.0f);
	UE_LOG(LogCombatPowers, Display, TEXT("Sistema de poderes preparado para %s. Pociones: Z vida, X energia, C mana."), *GetNameSafe(this));
}

void ATFGCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bMuerteEnCurso)
	{
		return;
	}
	ActualizarProyectilesConPoderes(DeltaSeconds);
	ActualizarVisualesProyectiles(DeltaSeconds);

	// C was being consumed by the active input stack before the legacy key
	// binding reached this character. Reading the controller's key state makes
	// the mana potion reliable without firing more than once per press.
	if (IsLocallyControlled())
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if (Now >= SiguienteActualizacionNumeroSala)
		{
			ResolverNumeroSalaActual(true);
			SiguienteActualizacionNumeroSala = Now + 0.5f;
		}

		if (APlayerController* PlayerController =
			Cast<APlayerController>(GetController()))
		{
			// PIE and the editor gameplay debugger can consume Tab before a
			// regular input binding receives it. Polling the controller keeps
			// the run-status overlay reliable in Editor and packaged builds.
			if (PlayerController->WasInputKeyJustPressed(EKeys::Tab))
			{
				AlternarInterfazEstadoRun();
			}

			if (PlayerController->WasInputKeyJustPressed(EKeys::C))
			{
				UE_LOG(
					LogCombatPowers,
					Display,
					TEXT("[POCIONES] Tecla C detectada; intentando usar pocion de mana."));
				UsarPocionMana();
			}
		}
	}
}

void ATFGCharacter::PrepararSalidaVoluntariaAlLobby()
{
	bOmitirGuardadoPoderesEnEndPlay = true;
	CerrarInterfazEstadoRun();
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URunPowerPersistenceSubsystem* Persistence =
			GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>())
		{
			Persistence->ResetPersistentPowers();
		}
	}
	UE_LOG(LogCombatPowers, Display,
		TEXT("[SALIDA RUN] Poderes temporales eliminados antes de volver al lobby; inventario permanente conservado."));
}

void ATFGCharacter::PrepararFinRunPorVictoria()
{
	bOmitirGuardadoPoderesEnEndPlay = true;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URunPowerPersistenceSubsystem* Persistence =
			GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>())
		{
			Persistence->MarkFirstReturnDialoguePending();
			Persistence->ResetPersistentPowers();
		}
	}
	UE_LOG(LogCombatPowers, Display,
		TEXT("[VICTORIA] Poderes temporales limpiados antes de volver al lobby."));
}

void ATFGCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!bOmitirGuardadoPoderesEnEndPlay)
	{
		GuardarEstadoPoderesPersistentes(TEXT("fin de escena"));
	}
	else
	{
		UE_LOG(LogCombatPowers, Display,
			TEXT("[MUERTE] Guardado de poderes de EndPlay omitido: la run ya fue reiniciada y guardada."));
	}

	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.ClearTimer(TimerEnfriamientoDobleDanyo);
	TimerManager.ClearTimer(TimerSigilo);
	TimerManager.ClearTimer(TimerInversionDanyo);
	TimerManager.ClearTimer(TimerRegeneracionMana);
	TimerManager.ClearTimer(TimerOcultarHUDLobby);
	TimerManager.ClearTimer(TimerDialogoPrimerRetorno);
	TimerManager.ClearTimer(TimerFinalizarMuerte);
	if (bLobbyReturnDialogueInputBound)
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			PlayerController->PopInputComponent(LobbyReturnDialogueInputComponent);
		}
		bLobbyReturnDialogueInputBound = false;
	}
	if (LobbyReturnDialogueWidget)
	{
		LobbyReturnDialogueWidget->RemoveFromParent();
		LobbyReturnDialogueWidget = nullptr;
	}
	CerrarInterfazEstadoRun();

	for (TPair<TWeakObjectPtr<AActor>, FQuemaduraActiva>& Pair : QuemadurasActivas)
	{
		TimerManager.ClearTimer(Pair.Value.Timer);
	}

	TArray<TWeakObjectPtr<AActor>> Debilitados;
	EnemigosDebilitados.GetKeys(Debilitados);
	for (const TWeakObjectPtr<AActor>& Objetivo : Debilitados)
	{
		TerminarDebilitado(Objetivo);
	}

	TArray<TWeakObjectPtr<AActor>> Paralizados;
	EnemigosParalizados.GetKeys(Paralizados);
	for (const TWeakObjectPtr<AActor>& Objetivo : Paralizados)
	{
		TerminarParalisis(Objetivo);
	}

	QuemadurasActivas.Empty();
	if (PotionQuickbarWidget)
	{
		PotionQuickbarWidget->RemoveFromParent();
		PotionQuickbarWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ATFGCharacter::CrearHUDPociones()
{
	if (!IsLocallyControlled() || PotionQuickbarWidget ||
		EsLobbyMesaRedonda())
	{
		if (IsLocallyControlled() && EsLobbyMesaRedonda())
		{
			UE_LOG(
				LogCombatPowers,
				Display,
				TEXT("[HUD] Barra de pociones ocultada en Lobby_MesaRedonda."));
		}
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	PotionQuickbarWidget = CreateWidget<UPotionQuickbarWidget>(
		PlayerController, UPotionQuickbarWidget::StaticClass());
	if (PotionQuickbarWidget)
	{
		PotionQuickbarWidget->SetIconTextures(
			IconoPocionVida,
			IconoPocionEnergia,
			IconoPocionMana);
		PotionQuickbarWidget->AddToViewport(35);
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POCIONES] Barra rapida creada: Z vida, X energia, C mana."));
	}
}

void ATFGCharacter::OcultarEtiquetaVidaActualHUD()
{
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		this, Widgets, UUserWidget::StaticClass(), false);

	int32 EtiquetasOcultadas = 0;
	for (UUserWidget* Widget : Widgets)
	{
		if (!Widget || !Widget->WidgetTree)
		{
			continue;
		}

		TArray<UWidget*> Descendientes;
		Widget->WidgetTree->GetAllWidgets(Descendientes);
		for (UWidget* Descendiente : Descendientes)
		{
			UTextBlock* Texto = Cast<UTextBlock>(Descendiente);
			if (!Texto)
			{
				continue;
			}

			FString TextoNormalizado = Texto->GetText().ToString();
			TextoNormalizado.ReplaceInline(TEXT(" "), TEXT(""));
			if (TextoNormalizado.Equals(TEXT("VidaActual"), ESearchCase::IgnoreCase))
			{
				Texto->SetVisibility(ESlateVisibility::Collapsed);
				++EtiquetasOcultadas;
			}
		}
	}

	UE_LOG(LogCombatPowers, Display,
		TEXT("[HUD] Etiquetas 'VidaActual' ocultadas: %d."), EtiquetasOcultadas);
}

bool ATFGCharacter::EsLobbyMesaRedonda() const
{
	return GetWorld() &&
		GetWorld()->GetMapName().Contains(TEXT("Lobby_MesaRedonda"));
}

void ATFGCharacter::ConfigurarHUDParaMapaActual()
{
	if (!EsLobbyMesaRedonda())
	{
		return;
	}

	bool bHUDPrincipalOcultado = false;
	const FObjectPropertyBase* WidgetProperty =
		FindFProperty<FObjectPropertyBase>(
			GetClass(), TEXT("InterfazGrafica"));
	UObject* WidgetObject = WidgetProperty
		? WidgetProperty->GetObjectPropertyValue_InContainer(this)
		: nullptr;
	if (UUserWidget* GameplayHUD = Cast<UUserWidget>(WidgetObject))
	{
		GameplayHUD->RemoveFromParent();
		GameplayHUD->SetVisibility(ESlateVisibility::Collapsed);
		bHUDPrincipalOcultado = true;
	}

	if (PotionQuickbarWidget)
	{
		PotionQuickbarWidget->RemoveFromParent();
		PotionQuickbarWidget = nullptr;
	}

	UE_LOG(
		LogCombatPowers,
		Display,
		TEXT("[HUD] Lobby_MesaRedonda sin interfaz de combate. HUD principal encontrado=%s; interacciones y menus se mantienen."),
		bHUDPrincipalOcultado ? TEXT("SI") : TEXT("NO"));
}

void ATFGCharacter::MostrarDialogoPrimerRetornoSiPendiente()
{
	if (!EsLobbyMesaRedonda() || !IsLocallyControlled() || LobbyReturnDialogueWidget)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	URunPowerPersistenceSubsystem* Persistence = GameInstance
		? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>()
		: nullptr;
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!Persistence || !Persistence->ShouldShowFirstReturnDialogue() || !PlayerController)
	{
		return;
	}

	LobbyReturnDialogueWidget = CreateWidget<ULobbyReturnDialogueWidget>(
		PlayerController,
		ULobbyReturnDialogueWidget::StaticClass());
	if (!LobbyReturnDialogueWidget)
	{
		return;
	}

	LobbyReturnDialogueWidget->AddToViewport(4000);
	MovimientoDesactivado = true;
	GetCharacterMovement()->StopMovementImmediately();
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);

	if (!LobbyReturnDialogueInputComponent)
	{
		LobbyReturnDialogueInputComponent = NewObject<UInputComponent>(
			this,
			TEXT("LobbyReturnDialogueInput"));
		LobbyReturnDialogueInputComponent->RegisterComponent();
		LobbyReturnDialogueInputComponent->Priority = 100;
		LobbyReturnDialogueInputComponent->bBlockInput = true;
		LobbyReturnDialogueInputComponent->BindKey(
			EKeys::E,
			IE_Pressed,
			this,
			&ATFGCharacter::CerrarDialogoPrimerRetorno);
	}
	PlayerController->PushInputComponent(LobbyReturnDialogueInputComponent);
	bLobbyReturnDialogueInputBound = true;

	UE_LOG(
		LogCombatPowers,
		Display,
		TEXT("[DIALOGO ARTURO] Primer regreso mostrado. Pulsa E para continuar."));
}

void ATFGCharacter::CerrarDialogoPrimerRetorno()
{
	if (!LobbyReturnDialogueWidget)
	{
		return;
	}

	LobbyReturnDialogueWidget->RemoveFromParent();
	LobbyReturnDialogueWidget = nullptr;
	// This callback is executing from the temporary input component itself.
	// Stop it blocking lower-priority Enhanced Input immediately, then remove
	// and destroy it safely on the next frame.
	if (LobbyReturnDialogueInputComponent)
	{
		LobbyReturnDialogueInputComponent->bBlockInput = false;
	}
	MovimientoDesactivado = false;

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URunPowerPersistenceSubsystem* Persistence =
			GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>())
		{
			Persistence->CompleteFirstReturnDialogue();
		}
	}

	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ATFGCharacter::RestaurarControlTrasDialogoPrimerRetorno);

	UE_LOG(LogCombatPowers, Display,
		TEXT("[DIALOGO ARTURO] Dialogo completado; restauracion total preparada para el siguiente frame."));
}

void ATFGCharacter::RestaurarControlTrasDialogoPrimerRetorno()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController)
	{
		if (bLobbyReturnDialogueInputBound && LobbyReturnDialogueInputComponent)
		{
			PlayerController->PopInputComponent(LobbyReturnDialogueInputComponent);
		}
		bLobbyReturnDialogueInputBound = false;

		PlayerController->SetPause(false);
		PlayerController->ResetIgnoreMoveInput();
		PlayerController->ResetIgnoreLookInput();
		PlayerController->SetShowMouseCursor(false);
		PlayerController->bEnableClickEvents = false;
		PlayerController->bEnableMouseOverEvents = false;
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
		EnableInput(PlayerController);
	}

	if (LobbyReturnDialogueInputComponent)
	{
		LobbyReturnDialogueInputComponent->ClearActionBindings();
		LobbyReturnDialogueInputComponent->Deactivate();
		LobbyReturnDialogueInputComponent->UnregisterComponent();
		LobbyReturnDialogueInputComponent->DestroyComponent();
		LobbyReturnDialogueInputComponent = nullptr;
	}

	MovimientoDesactivado = false;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement)
	{
		Movement->SetActive(true);
		Movement->SetComponentTickEnabled(true);
		if (Movement->MovementMode == MOVE_None)
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}

	UE_LOG(
		LogCombatPowers,
		Display,
		TEXT("[DIALOGO ARTURO] Control restaurado: controller=%s, movimiento=%s, modo=%d, ignoreMove=%s, ignoreLook=%s."),
		*GetNameSafe(PlayerController),
		MovimientoDesactivado ? TEXT("BLOQUEADO") : TEXT("ACTIVO"),
		Movement ? static_cast<int32>(Movement->MovementMode) : -1,
		PlayerController && PlayerController->IsMoveInputIgnored() ? TEXT("SI") : TEXT("NO"),
		PlayerController && PlayerController->IsLookInputIgnored() ? TEXT("SI") : TEXT("NO"));
}

void ATFGCharacter::UsarPocionVida()
{
	UsarPocion(EPotionType::Vida);
}

void ATFGCharacter::UsarPocionEnergia()
{
	UsarPocion(EPotionType::Energia);
}

void ATFGCharacter::UsarPocionMana()
{
	UsarPocion(EPotionType::Mana);
}

void ATFGCharacter::EstablecerDialogoActivo(const bool bActivo)
{
	if (bDialogoActivo == bActivo)
	{
		return;
	}

	bDialogoActivo = bActivo;
	if (PotionQuickbarWidget && !EsLobbyMesaRedonda())
	{
		if (bDialogoActivo)
		{
			PotionQuickbarWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			PotionQuickbarWidget->RefreshCounts();
			PotionQuickbarWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	UE_LOG(
		LogCombatPowers,
		Display,
		TEXT("[HUD POCIONES] Dialogo %s: barra de pociones %s."),
		bDialogoActivo ? TEXT("INICIADO") : TEXT("TERMINADO"),
		bDialogoActivo ? TEXT("OCULTA") : TEXT("RESTAURADA"));
}

void ATFGCharacter::UsarPocion(const EPotionType Tipo)
{
	using namespace CombatPowerHelpers;
	UGameInstance* GameInstance = GetGameInstance();
	URunPowerPersistenceSubsystem* Persistence =
		GameInstance ? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>() : nullptr;
	if (!Persistence || Persistence->GetPotionCount(Tipo) <= 0)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
				TEXT("No tienes pociones de ese tipo"));
		}
		UE_LOG(LogCombatPowers, Warning, TEXT("[POCIONES] No hay pociones tipo=%d."),
			static_cast<int32>(Tipo));
		return;
	}

	double Restaurado = 0.0;
	double Anterior = 0.0;
	double Maximo = 0.0;
	FName FuncionHUD = NAME_None;

	if (Tipo == EPotionType::Vida)
	{
		if (UActorComponent* Componente = BuscarComponenteVida())
		{
			FNumericProperty* ActualProperty = FindNumericProperty(Componente, TEXT("VidaActual"));
			FNumericProperty* MaximoProperty = FindNumericProperty(Componente, TEXT("VidaMaxima"));
			Anterior = ReadNumericProperty(Componente, ActualProperty);
			Maximo = ReadNumericProperty(Componente, MaximoProperty);
			Restaurado = CurarPersonaje(
				static_cast<float>(Maximo * PorcentajeRestauradoPocion),
				TEXT("pocion de vida"));
		}
	}
	else
	{
		const bool bEsMana = Tipo == EPotionType::Mana;
		const FName ActualName = bEsMana ? TEXT("ManaActual") : TEXT("StaminaActual");
		const FName MaximoName = bEsMana ? TEXT("ManaMaximo") : TEXT("StaminaMax");
		FuncionHUD = bEsMana ? TEXT("CambiarMana") : TEXT("CambiarStamina");
		if (UActorComponent* Componente = BuscarComponenteRecurso(ActualName, MaximoName))
		{
			FNumericProperty* ActualProperty = FindNumericProperty(Componente, ActualName);
			FNumericProperty* MaximoProperty = FindNumericProperty(Componente, MaximoName);
			Anterior = ReadNumericProperty(Componente, ActualProperty);
			Maximo = ReadNumericProperty(Componente, MaximoProperty);
			const double Nuevo = FMath::Clamp(
				Anterior + Maximo * PorcentajeRestauradoPocion, 0.0, Maximo);
			WriteNumericProperty(Componente, ActualProperty, Nuevo);
			Restaurado = Nuevo - Anterior;
			ActualizarHUDRecurso(FuncionHUD, Maximo > 0.0 ? Nuevo / Maximo : 0.0);
		}
	}

	if (Restaurado <= 0.0)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
				TEXT("Ese recurso ya esta al maximo"));
		}
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POCIONES] Tipo=%d no consumida porque el recurso esta lleno."),
			static_cast<int32>(Tipo));
		return;
	}

	Persistence->ConsumePotion(Tipo);
	if (PotionQuickbarWidget)
	{
		PotionQuickbarWidget->RefreshCounts();
	}
	if (GEngine)
	{
		const FString Message = FString::Printf(TEXT("Pocion usada: +%.0f"), Restaurado);
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green, Message);
	}
	UE_LOG(LogCombatPowers, Display,
		TEXT("[POCIONES] Tipo=%d consumida: +%.1f (%.1f / %.1f), restantes=%d."),
		static_cast<int32>(Tipo), Restaurado, Anterior + Restaurado, Maximo,
		Persistence->GetPotionCount(Tipo));
}

void ATFGCharacter::GuardarEstadoPoderesPersistentes(
	const TCHAR* Motivo) const
{
	UGameInstance* GameInstance = GetGameInstance();
	URunPowerPersistenceSubsystem* Persistence =
		GameInstance
			? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>()
			: nullptr;
	if (!Persistence)
	{
		UE_LOG(
			LogCombatPowers,
			Warning,
			TEXT("[PERSISTENCIA PODERES] No se pudo guardar el estado (%s): GameInstance no disponible."),
			Motivo);
		return;
	}

	FRunPersistentPowerState State;
	State.bDobleDanyoPreparado =
		bDobleDanyoPreparado ||
		PowerUpActivoDeSala == 1 ||
		Persistence->HasPersistentRoomPowerUp(1);
	State.bAtaquesQueman = bAtaquesQueman;
	State.bAtaquesDesplazan = bAtaquesDesplazan;
	State.bAtaquesCriticos = bAtaquesCriticos;
	State.bAtaquesDebilitan = bAtaquesDebilitan;
	State.bVelocidadAumentada = bVelocidadAumentada;
	// The shrine resurrection is permanent progression and must not be
	// serialized or displayed as the temporary Favor del Grial power.
	State.bTieneVidaExtra = bTieneVidaExtra && !bVidaExtraPermanenteAsignada;
	State.bDanyoReducido = bDanyoReducido;
	State.bProyectilesParalizan = bProyectilesParalizan;
	State.bGuardiaDireccionalActiva = bGuardiaDireccionalActiva;
	State.bRoboVidaActivo = bRoboVidaActivo;
	State.bEsquivaActiva = bEsquivaActiva;
	State.bInversionDanyoActiva = bInversionDanyoActiva;
	State.bDashHabilitado = bDashHabilitado;
	State.bAtaqueRapidoActivo = bAtaqueRapidoActivo;
	State.bProyectilesAutoapuntado = bProyectilesAutoapuntado;
	State.bProyectilesRebotan = bProyectilesRebotan;
	State.bRecursosAlquimiaObtenidos = bRecursosAlquimiaObtenidos;
	State.bAtaquesDesvianProyectiles = bAtaquesDesvianProyectiles;
	// Power 1 used to occupy R. Migrate old in-progress runs to the passive
	// ownership flag and free the active slot when their state is stored.
	State.PowerUpActivoDeSala =
		PowerUpActivoDeSala == 1 ? 0 : PowerUpActivoDeSala;
	if (UActorComponent* ComponenteVida = BuscarComponenteVida())
	{
		using namespace CombatPowerHelpers;
		if (FNumericProperty* VidaActualProperty =
			FindNumericProperty(ComponenteVida, TEXT("VidaActual")))
		{
			State.VidaActualGuardada = static_cast<float>(
				FMath::Max(
					0.0,
					ReadNumericProperty(
						ComponenteVida,
						VidaActualProperty)));
			State.bTieneVidaGuardada = true;
		}
	}
	Persistence->StorePowerState(State);

	UE_LOG(
		LogCombatPowers,
		Display,
		TEXT("[PERSISTENCIA PODERES] Estado guardado por %s: %d poderes, vida=%s%.1f."),
		Motivo,
		State.CountActivePowers(),
		State.bTieneVidaGuardada ? TEXT("") : TEXT("NO DISPONIBLE / "),
		State.VidaActualGuardada);
}

void ATFGCharacter::RestaurarEstadoPoderesPersistentes()
{
	UGameInstance* GameInstance = GetGameInstance();
	URunPowerPersistenceSubsystem* Persistence =
		GameInstance
			? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>()
			: nullptr;
	if (!Persistence || !Persistence->HasStoredState())
	{
		UE_LOG(
			LogCombatPowers,
			Display,
			TEXT("[PERSISTENCIA PODERES] Primera escena de la partida: no hay poderes anteriores que restaurar."));
		return;
	}

	const FRunPersistentPowerState& State = Persistence->GetPowerState();
	bDobleDanyoPreparado =
		State.bDobleDanyoPreparado || State.PowerUpActivoDeSala == 1;
	bAtaquesQueman = State.bAtaquesQueman;
	bAtaquesDesplazan = State.bAtaquesDesplazan;
	bAtaquesCriticos = State.bAtaquesCriticos;
	bAtaquesDebilitan = State.bAtaquesDebilitan;
	bVelocidadAumentada = State.bVelocidadAumentada;
	bTieneVidaExtra = State.bTieneVidaExtra;
	bDanyoReducido = State.bDanyoReducido;
	bProyectilesParalizan = State.bProyectilesParalizan;
	bGuardiaDireccionalActiva = State.bGuardiaDireccionalActiva;
	bRoboVidaActivo = State.bRoboVidaActivo;
	bEsquivaActiva = State.bEsquivaActiva;
	bInversionDanyoActiva = State.bInversionDanyoActiva;
	bInversionDanyoPreparada = bInversionDanyoActiva;
	bDashHabilitado = State.bDashHabilitado;
	bAtaqueRapidoActivo = State.bAtaqueRapidoActivo;
	bProyectilesAutoapuntado = State.bProyectilesAutoapuntado;
	bProyectilesRebotan = State.bProyectilesRebotan;
	bRecursosAlquimiaObtenidos = State.bRecursosAlquimiaObtenidos;
	bAtaquesDesvianProyectiles = State.bAtaquesDesvianProyectiles;
	PowerUpActivoDeSala =
		State.PowerUpActivoDeSala == 1 ? 0 : State.PowerUpActivoDeSala;
	ActualizarVelocidadMovimiento();

	UE_LOG(
		LogCombatPowers,
		Display,
		TEXT("[PERSISTENCIA PODERES] Restaurados %d poderes en %s despues del cambio de escena."),
		State.CountActivePowers(),
		*GetNameSafe(this));
}

void ATFGCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (bSaltoOfensivoPendiente)
	{
		ResolverSaltoOfensivo();
	}
}

void ATFGCharacter::Move(const FInputActionValue& Value)
{
	if (MovimientoDesactivado)
	{
		return;
	}

	const FVector2D MovementVector = Value.Get<FVector2D>();
	ProcesarEntradaDash(MovementVector);
	DoMove(MovementVector.X, MovementVector.Y);
}

void ATFGCharacter::Look(const FInputActionValue& Value)
{
	if (MovimientoDesactivado)
	{
		return;
	}

	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	const float Sensitivity = PauseMenuComponent
		? PauseMenuComponent->GetMouseSensitivity()
		: 1.0f;
	const float VerticalDirection = PauseMenuComponent &&
		PauseMenuComponent->IsVerticalLookInverted()
			? -1.0f
			: 1.0f;
	DoLook(
		LookAxisVector.X * Sensitivity,
		LookAxisVector.Y * Sensitivity * VerticalDirection);
}

void ATFGCharacter::AlternarMenuPausa()
{
	if (PauseMenuComponent)
	{
		CerrarInterfazEstadoRun();
		PauseMenuComponent->TogglePauseMenu();
	}
}

void ATFGCharacter::AlternarInterfazEstadoRun()
{
	if (!IsLocallyControlled() || bMuerteEnCurso || bDialogoActivo ||
		UGameplayStatics::IsGamePaused(this))
	{
		return;
	}

	if (RunStatusWidget)
	{
		CerrarInterfazEstadoRun();
		return;
	}

	const int32 RoomNumber = ResolverNumeroSalaActual(true);
	if (RoomNumber <= 0)
	{
		UE_LOG(LogCombatPowers, Verbose,
			TEXT("[ESTADO RUN] Tab ignorado fuera de una run."));
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	RunStatusWidget = CreateWidget<URunStatusWidget>(
		PlayerController, URunStatusWidget::StaticClass());
	if (!RunStatusWidget)
	{
		return;
	}

	const TArray<FString> ActivePowers = ObtenerNombresPoderesTemporales();
	RunStatusWidget->Configure(RoomNumber, ActivePowers);
	RunStatusWidget->AddToViewport(1800);
	UE_LOG(LogCombatPowers, Display,
		TEXT("[ESTADO RUN] Sala %d; mostrando %d poderes temporales."),
		RoomNumber, ActivePowers.Num());
}

void ATFGCharacter::CerrarInterfazEstadoRun()
{
	if (RunStatusWidget)
	{
		RunStatusWidget->RemoveFromParent();
		RunStatusWidget = nullptr;
		UE_LOG(LogCombatPowers, Display, TEXT("[ESTADO RUN] Interfaz cerrada."));
	}
}

int32 ATFGCharacter::ResolverNumeroSalaActual(
	const bool bUpdatePersistence) const
{
	URunPowerPersistenceSubsystem* Persistence = nullptr;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		Persistence = GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>();
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->GetClass()->GetName().Contains(TEXT("BP_RunManager")))
		{
			continue;
		}

		if (FNumericProperty* IndexProperty = CombatPowerHelpers::FindNumericProperty(
			Actor, TEXT("CurrentRoomIndex")))
		{
			const int32 RoomNumber = FMath::Max(1, FMath::RoundToInt(
				CombatPowerHelpers::ReadNumericProperty(Actor, IndexProperty)) + 1);
			if (bUpdatePersistence && Persistence)
			{
				Persistence->SetCurrentRunRoomNumber(RoomNumber);
			}
			return RoomNumber;
		}
	}

	const FString MapName = GetWorld() ? GetWorld()->GetMapName() : FString();
	if (MapName.Contains(TEXT("Combate_Final_Mordred")))
	{
		return (Persistence ? Persistence->GetCurrentRunRoomNumber() : 1) + 1;
	}
	return 0;
}

TArray<FString> ATFGCharacter::ObtenerNombresPoderesTemporales() const
{
	static const TCHAR* PowerNames[] =
	{
		TEXT("Doble daño"), TEXT("Ataque ígneo"),
		TEXT("Fuerza descomunal"), TEXT("Golpe crítico"),
		TEXT("Toque debilitador"), TEXT("Reflejos mejorados"),
		TEXT("Favor del Grial"), TEXT("Armadura superior"),
		TEXT("Proyectiles paralizantes"), TEXT("Confianza ciega"),
		TEXT("Paso sigiloso (R)"), TEXT("Daño vampírico"),
		TEXT("Esquiva superior"), TEXT("Daño curativo"),
		TEXT("Esquive direccional"), TEXT("Salto meteórico (R)"),
		TEXT("Sistema motriz mejorado"), TEXT("Bendición de Nunca Falla"),
		TEXT("Rebote arcano"), TEXT("Suministros de alquimia"),
		TEXT("Ataque reflectante")
	};

	TArray<FString> Result;
	for (int32 PowerId = 1; PowerId <= UE_ARRAY_COUNT(PowerNames); ++PowerId)
	{
		if (TienePowerUpDeSala(PowerId))
		{
			Result.Emplace(PowerNames[PowerId - 1]);
		}
	}
	return Result;
}

void ATFGCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ATFGCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ATFGCharacter::DoJumpStart()
{
	if (!MovimientoDesactivado)
	{
		Jump();
	}
}

void ATFGCharacter::DoJumpEnd()
{
	StopJumping();
}

void ATFGCharacter::AlternarPoderNuevo1GuardiaDireccional()
{
	bGuardiaDireccionalActiva = !bGuardiaDireccionalActiva;
	UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 1] Guardia direccional %s: frente %.0f%%, espalda %.0f%% del dano."),
		bGuardiaDireccionalActiva ? TEXT("ACTIVADA") : TEXT("DESACTIVADA"),
		MultiplicadorDanyoFrontal * 100.0f, MultiplicadorDanyoEspalda * 100.0f);
	GuardarEstadoPoderesPersistentes(TEXT("cambio de guardia direccional"));
}

void ATFGCharacter::ActivarPoderNuevo2Sigilo()
{
	bIndetectable = true;
	GetWorldTimerManager().ClearTimer(TimerSigilo);
	GetWorldTimerManager().SetTimer(TimerSigilo, this, &ATFGCharacter::TerminarSigilo, DuracionSigilo, false);

	int32 EnemigosSinAggro = 0;
	int32 SensoresDesactivados = 0;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Enemigo = *It;
		if (!IsValid(Enemigo) || Enemigo == this)
		{
			continue;
		}

		if (AAIController* ControladorIA = Cast<AAIController>(Enemigo->GetController()))
		{
			if (UPawnSensingComponent* Sensor = Enemigo->FindComponentByClass<UPawnSensingComponent>())
			{
				Sensor->SetSensingUpdatesEnabled(false);
				++SensoresDesactivados;
			}
			if (UBlackboardComponent* Blackboard = ControladorIA->GetBlackboardComponent())
			{
				Blackboard->SetValueAsBool(TEXT("Agro"), false);
			}
			ControladorIA->StopMovement();
			ControladorIA->ClearFocus(EAIFocusPriority::Gameplay);
			++EnemigosSinAggro;
		}
	}

	UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 2] INDETECTABLE durante %.1f s; aggro retirado a %d enemigos y %d sensores pausados."),
		DuracionSigilo, EnemigosSinAggro, SensoresDesactivados);
}

void ATFGCharacter::TerminarSigilo()
{
	bIndetectable = false;
	int32 SensoresRestaurados = 0;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Enemigo = *It;
		if (IsValid(Enemigo) && Enemigo != this && Cast<AAIController>(Enemigo->GetController()))
		{
			if (UPawnSensingComponent* Sensor = Enemigo->FindComponentByClass<UPawnSensingComponent>())
			{
				Sensor->SetSensingUpdatesEnabled(true);
				++SensoresRestaurados;
			}
		}
	}
	UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 2] Sigilo terminado; %d sensores restaurados y los enemigos pueden volver a detectar."),
		SensoresRestaurados);
}

void ATFGCharacter::AlternarPoderNuevo3RoboVida()
{
	bRoboVidaActivo = !bRoboVidaActivo;
	UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 3] Robo de vida %s: cura %.0f%% del dano infligido."),
		bRoboVidaActivo ? TEXT("ACTIVADO") : TEXT("DESACTIVADO"), PorcentajeRoboVida * 100.0f);
	GuardarEstadoPoderesPersistentes(TEXT("cambio de robo de vida"));
}

void ATFGCharacter::AlternarPoderNuevo4Esquiva()
{
	bEsquivaActiva = !bEsquivaActiva;
	UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 4] Esquiva %s: %.0f%% de probabilidad."),
		bEsquivaActiva ? TEXT("ACTIVADA") : TEXT("DESACTIVADA"), ProbabilidadEsquiva * 100.0f);
	GuardarEstadoPoderesPersistentes(TEXT("cambio de esquiva"));
}

void ATFGCharacter::AlternarPoderNuevo5InversionDanyo()
{
	bInversionDanyoActiva = !bInversionDanyoActiva;
	GetWorldTimerManager().ClearTimer(TimerInversionDanyo);
	bInversionDanyoPreparada = bInversionDanyoActiva;

	UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 5] Inversion de dano %s%s."),
		bInversionDanyoActiva ? TEXT("ACTIVADA") : TEXT("DESACTIVADA"),
		bInversionDanyoActiva ? TEXT(": el siguiente golpe curara y luego recargara durante 5 s") : TEXT(""));
	GuardarEstadoPoderesPersistentes(TEXT("cambio de inversion de dano"));
}

void ATFGCharacter::PrepararInversionDanyo()
{
	if (!bInversionDanyoActiva)
	{
		return;
	}

	bInversionDanyoPreparada = true;
	UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 5] Recarga terminada: el siguiente dano recibido se convertira en curacion."));
}

void ATFGCharacter::AlternarPoderNuevo6Dash()
{
	bDashHabilitado = !bDashHabilitado;
	UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 6] Dash por doble pulsacion %s; coste %.1f de stamina."),
		bDashHabilitado ? TEXT("ACTIVADO") : TEXT("DESACTIVADO"), CosteStaminaDash);
	GuardarEstadoPoderesPersistentes(TEXT("cambio de dash"));
}

void ATFGCharacter::ActivarPoderNuevo7SaltoOfensivo()
{
	if (bSaltoOfensivoPendiente || GetCharacterMovement()->IsFalling())
	{
		UE_LOG(LogCombatPowers, Warning, TEXT("[NUEVO PODER 7] No se puede iniciar: el personaje ya esta en el aire."));
		return;
	}

	bSaltoOfensivoPendiente = true;
	const FVector Impulso = GetActorForwardVector().GetSafeNormal2D() * FuerzaSaltoOfensivoAdelante
		+ FVector::UpVector * FuerzaSaltoOfensivoVertical;
	LaunchCharacter(Impulso, true, true);
	UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 7] Salto ofensivo iniciado: impulso %s."), *Impulso.ToCompactString());
}

void ATFGCharacter::AlternarPoderNuevo8AtaqueRapido()
{
	bAtaqueRapidoActivo = !bAtaqueRapidoActivo;
	UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 8] Ataques rapidos %s: multiplicador x%.2f."),
		bAtaqueRapidoActivo ? TEXT("ACTIVADOS") : TEXT("DESACTIVADOS"), ObtenerMultiplicadorVelocidadAtaque());
	GuardarEstadoPoderesPersistentes(TEXT("cambio de ataque rapido"));
}

bool ATFGCharacter::TienePowerUpDeSala(const int32 PowerUpId) const
{
	if (EsPowerUpActivoDeSala(PowerUpId))
	{
		return PowerUpActivoDeSala == PowerUpId;
	}

	switch (PowerUpId)
	{
	case 1:
	{
		if (bDobleDanyoPreparado || PowerUpActivoDeSala == 1)
		{
			return true;
		}
		const UGameInstance* GameInstance = GetGameInstance();
		const URunPowerPersistenceSubsystem* Persistence =
			GameInstance
				? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>()
				: nullptr;
		return Persistence && Persistence->HasPersistentRoomPowerUp(1);
	}
	case 2: return bAtaquesQueman;
	case 3: return bAtaquesDesplazan;
	case 4: return bAtaquesCriticos;
	case 5: return bAtaquesDebilitan;
	case 6: return bVelocidadAumentada;
	case 7: return bTieneVidaExtra && !bVidaExtraPermanenteAsignada;
	case 8: return bDanyoReducido;
	case 9: return bProyectilesParalizan;
	case 10: return bGuardiaDireccionalActiva;
	case 12: return bRoboVidaActivo;
	case 13: return bEsquivaActiva;
	case 14: return bInversionDanyoActiva;
	case 15: return bDashHabilitado;
	case 17: return bAtaqueRapidoActivo;
	case 18: return bProyectilesAutoapuntado;
	case 19: return bProyectilesRebotan;
	case 20: return bRecursosAlquimiaObtenidos;
	case 21: return bAtaquesDesvianProyectiles;
	default:
		return false;
	}
}

bool ATFGCharacter::EsPowerUpActivoDeSala(const int32 PowerUpId) const
{
	return PowerUpId == 11 || PowerUpId == 16;
}

void ATFGCharacter::ActivarPowerUpDeSalaEquipado()
{
	switch (PowerUpActivoDeSala)
	{
	case 11:
		ActivarPoderNuevo2Sigilo();
		break;
	case 16:
		ActivarPoderNuevo7SaltoOfensivo();
		break;
	default:
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.0f,
				FColor::Yellow,
				TEXT("No tienes ningun poder activo equipado"));
		}
		UE_LOG(
			LogCombatPowers,
			Display,
			TEXT("[POWER-UP ACTIVO] R pulsada, pero no hay ningun poder activo equipado."));
		break;
	}
}

bool ATFGCharacter::OtorgarPowerUpDeSala(const int32 PowerUpId)
{
	if (EsPowerUpActivoDeSala(PowerUpId) && PowerUpActivoDeSala != 0)
	{
		UE_LOG(
			LogCombatPowers,
			Warning,
			TEXT("[POWER-UP DE SALA] Ya existe un poder activo equipado (%d); no se puede obtener otro durante esta run."),
			PowerUpActivoDeSala);
		return false;
	}

	if (TienePowerUpDeSala(PowerUpId))
	{
		UE_LOG(
			LogCombatPowers,
			Warning,
			TEXT("[POWER-UP DE SALA] La mejora %d ya estaba obtenida; no se ha desactivado."),
			PowerUpId);
		return false;
	}

	switch (PowerUpId)
	{
	case 1:
		bDobleDanyoPreparado = true;
		FinEnfriamientoDobleDanyo = 0.0f;
		GetWorldTimerManager().ClearTimer(TimerEnfriamientoDobleDanyo);
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Doble dano pasivo obtenido; el siguiente ataque hara x2 dano y se recargara automaticamente en %.1f s."),
			EnfriamientoDobleDanyo);
		break;
	case 2:
		bAtaquesQueman = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Quemadura obtenida: %.1f dano en %.1f s."),
			DanyoTotalQuemadura, NumeroTicksQuemadura * IntervaloQuemadura);
		break;
	case 3:
		bAtaquesDesplazan = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Desplazamiento de objetivos obtenido."));
		break;
	case 4:
		bAtaquesCriticos = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Criticos obtenidos: %.0f%% de probabilidad, dano x%.1f."),
			ProbabilidadCritico * 100.0f, MultiplicadorCritico);
		break;
	case 5:
		bAtaquesDebilitan = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Debilitamiento obtenido: enemigos al 50%% durante %.1f s."),
			DuracionDebilitado);
		break;
	case 6:
		bVelocidadAumentada = true;
		ActualizarVelocidadMovimiento();
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Velocidad de movimiento aumentada al 200%%."));
		break;
	case 7:
		ActivarPoder7VidaExtra();
		CurarPersonaje(TNumericLimits<float>::Max(), TEXT("power-up de vida extra"));
		break;
	case 8:
		bDanyoReducido = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Resistencia obtenida: dano recibido reducido un 50%%."));
		break;
	case 9:
		bProyectilesParalizan = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Proyectiles paralizantes obtenidos: pausa y perdida de aggro durante %.1f s."),
			DuracionParalisis);
		break;
	case 10:
		bGuardiaDireccionalActiva = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Guardia direccional obtenida: 25%% frontal y 200%% por la espalda."));
		break;
	case 11:
		PowerUpActivoDeSala = 11;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Sigilo equipado en R: indetectable durante %.1f s."),
			DuracionSigilo);
		break;
	case 12:
		bRoboVidaActivo = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Robo de vida obtenido: cura el 50%% del dano infligido."));
		break;
	case 13:
		bEsquivaActiva = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Esquiva obtenida: 20%% de probabilidad."));
		break;
	case 14:
		bInversionDanyoActiva = true;
		bInversionDanyoPreparada = true;
		GetWorldTimerManager().ClearTimer(TimerInversionDanyo);
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Inversion obtenida: el siguiente golpe cura y recarga cada 5 s."));
		break;
	case 15:
		bDashHabilitado = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Dash obtenido: doble pulsacion, coste %.0f stamina."),
			CosteStaminaDash);
		break;
	case 16:
		PowerUpActivoDeSala = 16;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Salto ofensivo equipado en R: %.0f de dano en area."),
			DanyoSaltoOfensivo);
		break;
	case 17:
		bAtaqueRapidoActivo = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Ataque rapido obtenido: multiplicador de velocidad x%.2f."),
			ObtenerMultiplicadorVelocidadAtaque());
		break;
	case 18:
		bProyectilesAutoapuntado = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Autoapuntado obtenido: radio %.0f, aceleracion %.0f."),
			RadioAutoapuntadoProyectil, AceleracionAutoapuntado);
		break;
	case 19:
		bProyectilesRebotan = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Rebote de proyectiles obtenido: hasta %d saltos en radio %.0f."),
			MaximoRebotesProyectil, RadioReboteProyectil);
		break;
	case 20:
		bRecursosAlquimiaObtenidos = true;
		GuardarRecolectable(EEnemyCollectibleType::Vida, 3);
		GuardarRecolectable(EEnemyCollectibleType::Energia, 3);
		GuardarRecolectable(EEnemyCollectibleType::Mana, 3);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 4.0f, FColor::Cyan,
				TEXT("SUMINISTROS: +3 VIDA, +3 ENERGIA, +3 MANA"));
		}
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Suministros obtenidos: 9 recursos de pociones (3 vida, 3 energia, 3 mana)."));
		break;
	case 21:
		bAtaquesDesvianProyectiles = true;
		UE_LOG(LogCombatPowers, Display,
			TEXT("[POWER-UP DE SALA] Desvio obtenido: ataques reflejan proyectiles en un radio de %.0f."),
			RadioDesvioProyectil);
		break;
	default:
		UE_LOG(
			LogCombatPowers,
			Error,
			TEXT("[POWER-UP DE SALA] Identificador invalido: %d."),
			PowerUpId);
		return false;
	}

	GuardarEstadoPoderesPersistentes(TEXT("power-up de sala obtenido"));
	return true;
}

float ATFGCharacter::ObtenerMultiplicadorVelocidadAtaque() const
{
	return bAtaqueRapidoActivo ? MultiplicadorVelocidadAtaqueNueva : 1.0f;
}

void ATFGCharacter::RegistrarFuenteDanyo(AActor* FuenteDanyo)
{
	FuenteDanyoPendiente = IsValid(FuenteDanyo) && FuenteDanyo != this ? FuenteDanyo : nullptr;
	UE_LOG(LogCombatPowers, Verbose, TEXT("Fuente del siguiente dano registrada: %s."), *GetNameSafe(FuenteDanyoPendiente.Get()));
}

void ATFGCharacter::IniciarSprint()
{
	if (MovimientoDesactivado || bSprintActivo)
	{
		return;
	}
	if (ObtenerStaminaActual() <= 0.0f)
	{
		UE_LOG(LogCombatPowers, Warning, TEXT("[SPRINT] No se puede iniciar: stamina agotada."));
		return;
	}

	bSprintActivo = true;
	ActualizarVelocidadMovimiento();
	UE_LOG(LogCombatPowers, Display, TEXT("[SPRINT] Iniciado con Shift: velocidad %.1f."), GetCharacterMovement()->MaxWalkSpeed);
}

void ATFGCharacter::DetenerSprint()
{
	if (!bSprintActivo)
	{
		return;
	}

	bSprintActivo = false;
	ActualizarVelocidadMovimiento();
	UE_LOG(LogCombatPowers, Display, TEXT("[SPRINT] Terminado: velocidad %.1f."), GetCharacterMovement()->MaxWalkSpeed);
}

void ATFGCharacter::ActualizarVelocidadMovimiento()
{
	const float MultiplicadorPoderAntiguo = bVelocidadAumentada ? MultiplicadorVelocidad : 1.0f;
	const float MultiplicadorSprintActual = bSprintActivo ? MultiplicadorSprint : 1.0f;
	GetCharacterMovement()->MaxWalkSpeed =
		VelocidadBase * MultiplicadorVelocidadPermanente * MultiplicadorPoderAntiguo * MultiplicadorSprintActual;
}

void ATFGCharacter::AplicarMejorasPermanentes()
{
	using namespace CombatPowerHelpers;
	UGameInstance* GameInstance = GetGameInstance();
	URunPowerPersistenceSubsystem* Persistence =
		GameInstance ? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>() : nullptr;
	if (!Persistence)
	{
		return;
	}

	MultiplicadorDanyoPermanente = Persistence->GetPermanentDamageMultiplier();
	MultiplicadorVelocidadPermanente = Persistence->GetPermanentSpeedMultiplier();
	RegeneracionVidaPermanente = Persistence->GetPermanentHealthRegenPerSecond();

	if (UActorComponent* ComponenteVida = BuscarComponenteVida())
	{
		FNumericProperty* ActualProperty = FindNumericProperty(ComponenteVida, TEXT("VidaActual"));
		FNumericProperty* MaximoProperty = FindNumericProperty(ComponenteVida, TEXT("VidaMaxima"));
		const double Bonus = Persistence->GetPermanentHealthBonus();
		const double MaximoNuevo = ReadNumericProperty(ComponenteVida, MaximoProperty) + Bonus;
		const double ActualNuevo = ReadNumericProperty(ComponenteVida, ActualProperty) + Bonus;
		WriteNumericProperty(ComponenteVida, MaximoProperty, MaximoNuevo);
		WriteNumericProperty(ComponenteVida, ActualProperty, FMath::Min(ActualNuevo, MaximoNuevo));
		ActualizarHUDRecurso(TEXT("CambiarVida"), MaximoNuevo > 0.0 ? ActualNuevo / MaximoNuevo : 0.0);
	}

	if (UActorComponent* ComponenteMana = BuscarComponenteRecurso(TEXT("ManaActual"), TEXT("ManaMaximo")))
	{
		FNumericProperty* ActualProperty = FindNumericProperty(ComponenteMana, TEXT("ManaActual"));
		FNumericProperty* MaximoProperty = FindNumericProperty(ComponenteMana, TEXT("ManaMaximo"));
		const double Bonus = Persistence->GetPermanentManaBonus();
		const double MaximoNuevo = ReadNumericProperty(ComponenteMana, MaximoProperty) + Bonus;
		const double ActualNuevo = ReadNumericProperty(ComponenteMana, ActualProperty) + Bonus;
		WriteNumericProperty(ComponenteMana, MaximoProperty, MaximoNuevo);
		WriteNumericProperty(ComponenteMana, ActualProperty, FMath::Min(ActualNuevo, MaximoNuevo));
		ActualizarHUDRecurso(TEXT("CambiarMana"), MaximoNuevo > 0.0 ? ActualNuevo / MaximoNuevo : 0.0);
	}

	if (Persistence->IsPermanentReviveAvailable())
	{
		bTieneVidaExtra = true;
		bVidaExtraPermanenteAsignada = true;
	}

	ActualizarVelocidadMovimiento();
	UE_LOG(LogCombatPowers, Display,
		TEXT("[MEJORAS PERMANENTES] Aplicadas: dano x%.2f, velocidad x%.2f, regen %.1f/s, resurreccion=%s."),
		MultiplicadorDanyoPermanente, MultiplicadorVelocidadPermanente,
		RegeneracionVidaPermanente, bVidaExtraPermanenteAsignada ? TEXT("DISPONIBLE") : TEXT("NO"));
}

void ATFGCharacter::RestaurarVidaPersistente()
{
	using namespace CombatPowerHelpers;
	if (EsLobbyMesaRedonda())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	const URunPowerPersistenceSubsystem* Persistence =
		GameInstance
			? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>()
			: nullptr;
	if (!Persistence || !Persistence->HasStoredState())
	{
		return;
	}

	const FRunPersistentPowerState& State = Persistence->GetPowerState();
	if (!State.bTieneVidaGuardada)
	{
		return;
	}

	UActorComponent* ComponenteVida = BuscarComponenteVida();
	FNumericProperty* VidaActualProperty =
		FindNumericProperty(ComponenteVida, TEXT("VidaActual"));
	FNumericProperty* VidaMaximaProperty =
		FindNumericProperty(ComponenteVida, TEXT("VidaMaxima"));
	if (!ComponenteVida || !VidaActualProperty || !VidaMaximaProperty)
	{
		return;
	}

	const double VidaMaxima =
		ReadNumericProperty(ComponenteVida, VidaMaximaProperty);
	const double VidaRestaurada = FMath::Clamp(
		static_cast<double>(State.VidaActualGuardada),
		1.0,
		VidaMaxima);
	WriteNumericProperty(
		ComponenteVida,
		VidaActualProperty,
		VidaRestaurada);
	ActualizarHUDRecurso(
		TEXT("CambiarVida"),
		VidaMaxima > 0.0 ? VidaRestaurada / VidaMaxima : 0.0);

	UE_LOG(
		LogCombatPowers,
		Display,
		TEXT("[PERSISTENCIA VIDA] Restaurada %.1f / %.1f al entrar en %s."),
		VidaRestaurada,
		VidaMaxima,
		*GetWorld()->GetMapName());
}

void ATFGCharacter::ActivarPoder1DobleDanyo()
{
	if (!TienePowerUpDeSala(1))
	{
		bDobleDanyoPreparado = true;
		FinEnfriamientoDobleDanyo = 0.0f;
		GetWorldTimerManager().ClearTimer(TimerEnfriamientoDobleDanyo);
		GuardarEstadoPoderesPersistentes(TEXT("doble dano pasivo concedido"));
		UE_LOG(LogCombatPowers, Display,
			TEXT("[PODER 1] Pasivo concedido: el siguiente ataque hara x2 dano; recarga automatica %.1f s."),
			EnfriamientoDobleDanyo);
		return;
	}

	if (bDobleDanyoPreparado)
	{
		UE_LOG(LogCombatPowers, Display,
			TEXT("[PODER 1] El pasivo ya esta preparado para el siguiente impacto."));
		return;
	}

	const float Restante = FMath::Max(
		0.0f, FinEnfriamientoDobleDanyo - GetWorld()->GetTimeSeconds());
	UE_LOG(LogCombatPowers, Display,
		TEXT("[PODER 1] Pasivo en recarga automatica: %.2f s restantes."),
		Restante);
}

void ATFGCharacter::DobleDanyoDisponible()
{
	if (!TienePowerUpDeSala(1))
	{
		return;
	}

	bDobleDanyoPreparado = true;
	FinEnfriamientoDobleDanyo = 0.0f;
	UE_LOG(LogCombatPowers, Display,
		TEXT("[PODER 1] Recarga terminada: el siguiente ataque vuelve a tener dano x2."));
}

void ATFGCharacter::AlternarPoder2Quemadura()
{
	bAtaquesQueman = !bAtaquesQueman;
	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 2] Quemadura %s: %.1f dano en %.1f s."),
		bAtaquesQueman ? TEXT("ACTIVADA") : TEXT("DESACTIVADA"), DanyoTotalQuemadura, NumeroTicksQuemadura * IntervaloQuemadura);
	GuardarEstadoPoderesPersistentes(TEXT("cambio de quemadura"));
}

void ATFGCharacter::AlternarPoder3Desplazamiento()
{
	bAtaquesDesplazan = !bAtaquesDesplazan;
	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 3] Desplazamiento %s."), bAtaquesDesplazan ? TEXT("ACTIVADO") : TEXT("DESACTIVADO"));
	GuardarEstadoPoderesPersistentes(TEXT("cambio de desplazamiento"));
}

void ATFGCharacter::AlternarPoder4Critico()
{
	bAtaquesCriticos = !bAtaquesCriticos;
	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 4] Criticos %s: %.0f%% de probabilidad, x%.1f dano."),
		bAtaquesCriticos ? TEXT("ACTIVADOS") : TEXT("DESACTIVADOS"), ProbabilidadCritico * 100.0f, MultiplicadorCritico);
	GuardarEstadoPoderesPersistentes(TEXT("cambio de criticos"));
}

void ATFGCharacter::AlternarPoder5Debilitar()
{
	bAtaquesDebilitan = !bAtaquesDebilitan;
	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 5] Debilitar %s: enemigos hacen %.0f%% de dano durante %.1f s."),
		bAtaquesDebilitan ? TEXT("ACTIVADO") : TEXT("DESACTIVADO"), MultiplicadorDanyoEnemigoDebilitado * 100.0f, DuracionDebilitado);
	GuardarEstadoPoderesPersistentes(TEXT("cambio de debilitado"));
}

void ATFGCharacter::AlternarPoder6Velocidad()
{
	bVelocidadAumentada = !bVelocidadAumentada;
	ActualizarVelocidadMovimiento();
	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 6] Velocidad %s: MaxWalkSpeed = %.1f (%.0f%%)."),
		bVelocidadAumentada ? TEXT("ACTIVADA") : TEXT("NORMAL"), GetCharacterMovement()->MaxWalkSpeed,
		(bVelocidadAumentada ? MultiplicadorVelocidad : 1.0f) * 100.0f);
	GuardarEstadoPoderesPersistentes(TEXT("cambio de velocidad aumentada"));
}

void ATFGCharacter::ActivarPoder7VidaExtra()
{
	if (bTieneVidaExtra)
	{
		UE_LOG(LogCombatPowers, Display, TEXT("[PODER 7] Ya hay una vida extra preparada; no se acumulan."));
		return;
	}

	bTieneVidaExtra = true;
	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 7] Vida extra concedida. Revivira con la vida completa al recibir dano mortal."));
	GuardarEstadoPoderesPersistentes(TEXT("vida extra concedida"));
}

void ATFGCharacter::AlternarPoder8Resistencia()
{
	bDanyoReducido = !bDanyoReducido;
	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 8] Resistencia %s: se recibe %.0f%% del dano."),
		bDanyoReducido ? TEXT("ACTIVADA") : TEXT("DESACTIVADA"), bDanyoReducido ? MultiplicadorDanyoRecibido * 100.0f : 100.0f);
	GuardarEstadoPoderesPersistentes(TEXT("cambio de resistencia"));
}

void ATFGCharacter::AlternarPoder9Paralisis()
{
	bProyectilesParalizan = !bProyectilesParalizan;
	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 9] Paralisis de proyectil %s: duracion %.1f s con perdida temporal de aggro."),
		bProyectilesParalizan ? TEXT("ACTIVADA") : TEXT("DESACTIVADA"), DuracionParalisis);
	GuardarEstadoPoderesPersistentes(TEXT("cambio de paralisis"));
}

void ATFGCharacter::AplicarGolpeConPoderes(AActor* Objetivo, float DanyoBase, bool bEsProyectil)
{
	if (!IsValid(Objetivo) || Objetivo == this || !SoportaDanyoBlueprint(Objetivo))
	{
		UE_LOG(LogCombatPowers, Warning, TEXT("Impacto ignorado: %s no es un objetivo valido de RecibirDanyo."), *GetNameSafe(Objetivo));
		return;
	}

	float DanyoFinal = FMath::Max(0.0f, DanyoBase) * MultiplicadorDanyoPermanente;
	if (TienePowerUpDeSala(1) && bDobleDanyoPreparado)
	{
		DanyoFinal *= 2.0f;
		bDobleDanyoPreparado = false;
		FinEnfriamientoDobleDanyo =
			GetWorld()->GetTimeSeconds() + EnfriamientoDobleDanyo;
		GetWorldTimerManager().SetTimer(
			TimerEnfriamientoDobleDanyo,
			this,
			&ATFGCharacter::DobleDanyoDisponible,
			EnfriamientoDobleDanyo,
			false);
		UE_LOG(LogCombatPowers, Display,
			TEXT("[PODER 1] Pasivo consumido contra %s: dano duplicado; recarga automatica %.1f s."),
			*GetNameSafe(Objetivo), EnfriamientoDobleDanyo);
	}

	if (bAtaquesCriticos && FMath::FRand() <= ProbabilidadCritico)
	{
		DanyoFinal *= MultiplicadorCritico;
		UE_LOG(LogCombatPowers, Display, TEXT("[PODER 4] CRITICO contra %s: x%.1f, dano final %.1f."),
			*GetNameSafe(Objetivo), MultiplicadorCritico, DanyoFinal);
	}

	if (!EnviarDanyoBlueprint(Objetivo, DanyoFinal))
	{
		return;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UMusicManagerSubsystem* Music =
			GameInstance->GetSubsystem<UMusicManagerSubsystem>())
		{
			Music->NotifyCombatActivity();
		}
	}

	UE_LOG(LogCombatPowers, Display, TEXT("Impacto %s -> %s: %.1f dano%s."), *GetNameSafe(this), *GetNameSafe(Objetivo),
		DanyoFinal, bEsProyectil ? TEXT(" (proyectil)") : TEXT(""));
	const float AhoraSonido = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (AhoraSonido - UltimoSonidoImpacto >= 0.045f)
	{
		UltimoSonidoImpacto = AhoraSonido;
		ReproducirSonidoCombate(
			SonidoImpacto,
			bEsProyectil ? 1.1f : 1.0f,
			TEXT("contacto con objetivo"));
	}
	ReproducirCameraShakeGolpe(bEsProyectil);

	if (bRoboVidaActivo && DanyoFinal > 0.0f)
	{
		CurarPersonaje(DanyoFinal * PorcentajeRoboVida, TEXT("robo de vida"));
	}

	if (bAtaquesQueman)
	{
		IniciarQuemadura(Objetivo);
	}
	if (bAtaquesDesplazan)
	{
		AplicarDesplazamiento(Objetivo);
	}
	if (bAtaquesDebilitan)
	{
		AplicarDebilitado(Objetivo);
	}
	if (bEsProyectil && bProyectilesParalizan)
	{
		AplicarParalisis(Objetivo);
	}
}

void ATFGCharacter::ReproducirCameraShakeGolpe(const bool bEsProyectil)
{
	if (!IsLocallyControlled() || !CameraShakeGolpeEnemigo || bMuerteEnCurso)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Ahora = World->GetTimeSeconds();
	if (Ahora - UltimoCameraShakeGolpe < IntervaloMinimoCameraShake)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	const float Intensidad = bEsProyectil
		? IntensidadCameraShakeProyectil
		: IntensidadCameraShakeCuerpoACuerpo;
	if (Intensidad <= 0.0f)
	{
		return;
	}

	PlayerController->ClientStartCameraShake(CameraShakeGolpeEnemigo, Intensidad);
	UltimoCameraShakeGolpe = Ahora;
	UE_LOG(LogCombatPowers, Verbose,
		TEXT("[CAMARA] Impacto confirmado: shake %s con intensidad %.2f."),
		bEsProyectil ? TEXT("de proyectil") : TEXT("cuerpo a cuerpo"),
		Intensidad);
}

bool ATFGCharacter::EsActorProyectil(const AActor* Actor) const
{
	return IsValid(Actor) && Actor != this &&
		(Actor->FindComponentByClass<UProjectileMovementComponent>() != nullptr ||
		 Actor->GetClass()->GetName().Contains(TEXT("Proyectil"), ESearchCase::IgnoreCase) ||
		 Actor->GetClass()->GetName().Contains(TEXT("Projectile"), ESearchCase::IgnoreCase));
}

bool ATFGCharacter::EsProyectilDelJugador(const AActor* Proyectil) const
{
	if (!IsValid(Proyectil))
	{
		return false;
	}

	return ProyectilesDelJugador.Contains(Proyectil) ||
		Proyectil->GetOwner() == this || Proyectil->GetInstigator() == this;
}

void ATFGCharacter::MarcarProyectilDelJugador(AActor* Proyectil)
{
	if (!EsActorProyectil(Proyectil))
	{
		return;
	}

	ProyectilesDelJugador.Add(Proyectil);
	Proyectil->SetOwner(this);
	Proyectil->SetInstigator(this);
}

AActor* ATFGCharacter::EncontrarObjetivoParaProyectil(
	const FVector& Origen,
	const AActor* ObjetivoExcluido,
	const TSet<TWeakObjectPtr<AActor>>* ObjetivosExcluidos,
	const float Radio) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AActor* MejorObjetivo = nullptr;
	float MejorDistanciaCuadrada = FMath::Square(FMath::Max(0.0f, Radio));
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidato = *It;
		if (!IsValid(Candidato) || Candidato == this ||
			Candidato == ObjetivoExcluido || Candidato->IsHidden() ||
			EsActorProyectil(Candidato) || !SoportaDanyoBlueprint(Candidato) ||
			(ObjetivosExcluidos && ObjetivosExcluidos->Contains(Candidato)))
		{
			continue;
		}

		const float DistanciaCuadrada = FVector::DistSquared(
			Origen, Candidato->GetActorLocation());
		if (DistanciaCuadrada < MejorDistanciaCuadrada)
		{
			MejorDistanciaCuadrada = DistanciaCuadrada;
			MejorObjetivo = Candidato;
		}
	}

	return MejorObjetivo;
}

void ATFGCharacter::RedirigirProyectil(
	AActor* Proyectil,
	AActor* Objetivo,
	const TCHAR* Motivo)
{
	if (!IsValid(Proyectil) || !IsValid(Objetivo))
	{
		return;
	}

	UProjectileMovementComponent* Movimiento =
		Proyectil->FindComponentByClass<UProjectileMovementComponent>();
	if (!Movimiento)
	{
		return;
	}

	const FVector PuntoObjetivo =
		Objetivo->GetActorLocation() + FVector(0.0f, 0.0f, 45.0f);
	const FVector Direccion =
		(PuntoObjetivo - Proyectil->GetActorLocation()).GetSafeNormal();
	const float Velocidad = FMath::Max(1200.0f, Movimiento->Velocity.Size());
	Movimiento->Velocity = Direccion * Velocidad;
	Movimiento->bRotationFollowsVelocity = true;
	if (bProyectilesAutoapuntado)
	{
		Movimiento->bIsHomingProjectile = true;
		Movimiento->HomingTargetComponent = Objetivo->GetRootComponent();
		Movimiento->HomingAccelerationMagnitude = AceleracionAutoapuntado;
	}
	else
	{
		Movimiento->bIsHomingProjectile = false;
		Movimiento->HomingTargetComponent = nullptr;
	}
	Movimiento->UpdateComponentVelocity();
	Proyectil->SetActorRotation(Direccion.Rotation());

	UE_LOG(LogCombatPowers, Display,
		TEXT("[PROYECTIL %s] %s redirigido hacia %s a velocidad %.0f."),
		Motivo, *GetNameSafe(Proyectil), *GetNameSafe(Objetivo), Velocidad);
}

void ATFGCharacter::ActualizarProyectilesConPoderes(const float DeltaSeconds)
{
	if (!bProyectilesAutoapuntado && !bProyectilesRebotan)
	{
		return;
	}

	AcumuladorActualizacionProyectiles += DeltaSeconds;
	if (AcumuladorActualizacionProyectiles < 0.08f || !GetWorld())
	{
		return;
	}
	AcumuladorActualizacionProyectiles = 0.0f;

	for (auto It = ProyectilesDelJugador.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			ObjetivosPorProyectil.Remove(*It);
			RebotesPorProyectil.Remove(*It);
			It.RemoveCurrent();
		}
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Proyectil = *It;
		if (!EsActorProyectil(Proyectil))
		{
			continue;
		}

		UProjectileMovementComponent* Movimiento =
			Proyectil->FindComponentByClass<UProjectileMovementComponent>();
		if (!Movimiento)
		{
			continue;
		}

		bool bEsDelJugador = EsProyectilDelJugador(Proyectil);
		if (!bEsDelJugador && !IsValid(Proyectil->GetOwner()) &&
			!IsValid(Proyectil->GetInstigator()))
		{
			const FVector VelocidadNormalizada = Movimiento->Velocity.GetSafeNormal();
			const bool bSaleDesdeJugador =
				FVector::DistSquared(Proyectil->GetActorLocation(), GetActorLocation()) <=
				FMath::Square(360.0f) &&
				FVector::DotProduct(VelocidadNormalizada, GetActorForwardVector()) > 0.05f;
			if (bSaleDesdeJugador)
			{
				MarcarProyectilDelJugador(Proyectil);
				bEsDelJugador = true;
			}
		}

		if (!bEsDelJugador || !bProyectilesAutoapuntado ||
			(Movimiento->bIsHomingProjectile &&
			 Movimiento->HomingTargetComponent.IsValid()))
		{
			continue;
		}

		const TSet<TWeakObjectPtr<AActor>>* Excluidos =
			ObjetivosPorProyectil.Find(Proyectil);
		if (AActor* Objetivo = EncontrarObjetivoParaProyectil(
			Proyectil->GetActorLocation(), nullptr, Excluidos,
			RadioAutoapuntadoProyectil))
		{
			RedirigirProyectil(Proyectil, Objetivo, TEXT("AUTOAPUNTADO"));
		}
	}
}

void ATFGCharacter::ActualizarVisualesProyectiles(const float DeltaSeconds)
{
	AcumuladorVisualProyectiles += DeltaSeconds;
	if (AcumuladorVisualProyectiles < 0.045f || !GetWorld())
	{
		return;
	}
	AcumuladorVisualProyectiles = 0.0f;

	for (auto It = ProyectilesVisualesConfigurados.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Proyectil = *It;
		if (!EsActorProyectil(Proyectil) ||
			ProyectilesVisualesConfigurados.Contains(Proyectil))
		{
			continue;
		}

		AActor* Lanzador = Proyectil->GetInstigator();
		if (!IsValid(Lanzador))
		{
			Lanzador = Proyectil->GetOwner();
		}

		bool bEsArturo = Lanzador == this;
		if (!IsValid(Lanzador))
		{
			const UProjectileMovementComponent* Movimiento =
				Proyectil->FindComponentByClass<UProjectileMovementComponent>();
			const FVector Direccion = Movimiento
				? Movimiento->Velocity.GetSafeNormal()
				: Proyectil->GetActorForwardVector();
			bEsArturo =
				FVector::DistSquared(Proyectil->GetActorLocation(), GetActorLocation()) <=
				FMath::Square(425.0f) &&
				FVector::DotProduct(Direccion, GetActorForwardVector()) > -0.15f;
			if (bEsArturo)
			{
				MarcarProyectilDelJugador(Proyectil);
				Lanzador = this;
			}
		}

		const bool bEsMordred = IsValid(Lanzador) &&
			Lanzador->GetClass()->GetName().Contains(TEXT("Mordred"), ESearchCase::IgnoreCase);
		if (!bEsArturo && !bEsMordred)
		{
			continue;
		}

		ProyectilesVisualesConfigurados.Add(Proyectil);
		if (bEsArturo)
		{
			const float Ahora = GetWorld()->GetTimeSeconds();
			if (Ahora - UltimoProyectilArturoAceptado < EnfriamientoProyectilArturo)
			{
				ProyectilesDelJugador.Remove(Proyectil);
				ObjetivosPorProyectil.Remove(Proyectil);
				RebotesPorProyectil.Remove(Proyectil);
				UE_LOG(LogCombatPowers, Display,
					TEXT("[ARTURO] Proyectil cancelado: cooldown %.2f s."),
					EnfriamientoProyectilArturo);
				Proyectil->Destroy();
				continue;
			}
			UltimoProyectilArturoAceptado = Ahora;
			ConfigurarDestelloProyectil(
				Proyectil,
				FLinearColor(1.0f, 0.76f, 0.22f, 1.0f),
				TEXT("ARTURO"));
			ReproducirAnimacionLanzamiento();
		}
		else
		{
			ConfigurarDestelloProyectil(
				Proyectil,
				FLinearColor(1.0f, 0.025f, 0.01f, 1.0f),
				TEXT("MORDRED"));
		}
	}
}

void ATFGCharacter::ConfigurarDestelloProyectil(
	AActor* Proyectil,
	const FLinearColor& Color,
	const TCHAR* Lanzador)
{
	if (!IsValid(Proyectil) || !Proyectil->GetRootComponent())
	{
		return;
	}

	const bool bEsMordred = Lanzador &&
		FCString::Stricmp(Lanzador, TEXT("MORDRED")) == 0;

	if (bEsMordred)
	{
		static UMaterialInterface* MaterialNucleoNegro = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/ObsidianGoldOrnates_YV/Materials/M_Cast_Iron.M_Cast_Iron"));
		if (MaterialNucleoNegro)
		{
			TInlineComponentArray<UStaticMeshComponent*> Mallas(Proyectil);
			for (UStaticMeshComponent* Malla : Mallas)
			{
				if (Malla && Malla->GetName().Contains(TEXT("Esfera"), ESearchCase::IgnoreCase))
				{
					Malla->SetMaterial(0, MaterialNucleoNegro);
					Malla->SetCastShadow(false);
					UE_LOG(LogCombatPowers, Display,
						TEXT("[PROYECTIL MORDRED] Nucleo convertido en bola negra."));
				}
			}
		}
	}

	UPointLightComponent* Destello = NewObject<UPointLightComponent>(
		Proyectil,
		MakeUniqueObjectName(Proyectil, UPointLightComponent::StaticClass(), TEXT("DestelloLanzamiento")));
	Destello->SetMobility(EComponentMobility::Movable);
	Destello->SetLightColor(Color);
	Destello->SetIntensity(bEsMordred
		? 1750.0f
		: 1350.0f);
	Destello->SetAttenuationRadius(285.0f);
	Destello->SetSourceRadius(14.0f);
	Destello->SetSoftSourceRadius(32.0f);
	Destello->SetupAttachment(Proyectil->GetRootComponent());
	Destello->RegisterComponent();
	if (bEsMordred)
	{
		UPointLightComponent* HaloExterior = NewObject<UPointLightComponent>(
			Proyectil,
			MakeUniqueObjectName(Proyectil, UPointLightComponent::StaticClass(), TEXT("HaloRojoExterior")));
		HaloExterior->SetMobility(EComponentMobility::Movable);
		HaloExterior->SetLightColor(FLinearColor(0.8f, 0.0f, 0.0f, 1.0f));
		HaloExterior->SetIntensity(620.0f);
		HaloExterior->SetAttenuationRadius(430.0f);
		HaloExterior->SetSourceRadius(28.0f);
		HaloExterior->SetSoftSourceRadius(65.0f);
		HaloExterior->SetupAttachment(Proyectil->GetRootComponent());
		HaloExterior->RegisterComponent();
	}

	UE_LOG(LogCombatPowers, Display,
		TEXT("[PROYECTIL %s] Destello ligero configurado en %s."),
		Lanzador, *GetNameSafe(Proyectil));
}

void ATFGCharacter::ReproducirAnimacionLanzamiento()
{
	const float Ahora = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Ahora - UltimaAnimacionLanzamiento < 0.22f)
	{
		return;
	}
	UltimaAnimacionLanzamiento = Ahora;

	static UAnimSequenceBase* AnimacionLanzamiento = LoadObject<UAnimSequenceBase>(
		nullptr,
		TEXT("/Game/MyContent/Animaciones/Arturo/Anim_Viking_attack1.Anim_Viking_attack1"));
	if (AnimacionLanzamiento)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->PlaySlotAnimationAsDynamicMontage(
				AnimacionLanzamiento,
				TEXT("DefaultSlot"),
				0.06f,
				0.12f,
				1.75f,
				1,
				0.0f,
				0.0f);
		}
	}
	ReproducirSonidoCombate(
		SonidoLanzamientoProyectil,
		1.22f,
		TEXT("lanzamiento de proyectil"));
	UE_LOG(LogCombatPowers, Display, TEXT("[ARTURO] Animacion de lanzamiento reproducida."));
}

void ATFGCharacter::ReproducirSonidoCombate(
	USoundBase* Sonido,
	const float Tono,
	const TCHAR* Evento) const
{
	if (!Sonido || !GetWorld())
	{
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(
		this,
		Sonido,
		GetActorLocation(),
		VolumenSonidosCombate,
		Tono);
	UE_LOG(LogCombatPowers, Verbose,
		TEXT("[AUDIO] %s sincronizado para %s."), Evento, *GetName());
}

void ATFGCharacter::DesviarProyectilesEnAtaque(
	const FVector& Inicio,
	const FVector& Fin)
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Proyectil = *It;
		if (!EsActorProyectil(Proyectil) || EsProyectilDelJugador(Proyectil) ||
			FMath::PointDistToSegmentSquared(
				Proyectil->GetActorLocation(), Inicio, Fin) >
			FMath::Square(RadioDesvioProyectil))
		{
			continue;
		}

		UProjectileMovementComponent* Movimiento =
			Proyectil->FindComponentByClass<UProjectileMovementComponent>();
		if (!Movimiento)
		{
			continue;
		}

		MarcarProyectilDelJugador(Proyectil);
		AActor* Objetivo = EncontrarObjetivoParaProyectil(
			Proyectil->GetActorLocation(), nullptr, nullptr,
			RadioAutoapuntadoProyectil);
		if (Objetivo)
		{
			RedirigirProyectil(Proyectil, Objetivo, TEXT("DESVIO"));
		}
		else
		{
			const float Velocidad = FMath::Max(1200.0f, Movimiento->Velocity.Size());
			Movimiento->Velocity = GetActorForwardVector().GetSafeNormal() * Velocidad;
			Movimiento->bRotationFollowsVelocity = true;
			Movimiento->UpdateComponentVelocity();
			Proyectil->SetActorRotation(Movimiento->Velocity.Rotation());
		}

		UE_LOG(LogCombatPowers, Display,
			TEXT("[DESVIO] Ataque reflejo el proyectil %s."),
			*GetNameSafe(Proyectil));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 1.2f, FColor::Cyan, TEXT("PROYECTIL DESVIADO"));
		}
	}
}

void ATFGCharacter::RealizarTrazaAtaqueJugador(const bool bGolpeConCuerpo)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float TiempoSonido = World->GetTimeSeconds();
	if (TiempoSonido - UltimoSonidoAtaque >= 0.12f)
	{
		UltimoSonidoAtaque = TiempoSonido;
		ReproducirSonidoCombate(
			SonidoAtaque,
			bGolpeConCuerpo ? 0.88f : 1.0f,
			bGolpeConCuerpo ? TEXT("impacto de patada") : TEXT("impacto de espada"));
	}

	USkeletalMeshComponent* ComponenteTraza = GetMesh();
	if (!bGolpeConCuerpo)
	{
		TInlineComponentArray<USkeletalMeshComponent*> Mallas(this);
		for (USkeletalMeshComponent* Malla : Mallas)
		{
			if (Malla && Malla->GetFName() == TEXT("ArmaEquipada"))
			{
				ComponenteTraza = Malla;
				break;
			}
		}
	}

	const FVector Adelante = GetActorForwardVector().GetSafeNormal2D();
	const FVector OrigenFallback = GetActorLocation() + FVector(0.0f, 0.0f, 58.0f);
	FVector Inicio = OrigenFallback + Adelante * 35.0f;
	FVector Fin = OrigenFallback + Adelante * (bGolpeConCuerpo ? 145.0f : 205.0f);

	if (ComponenteTraza &&
		ComponenteTraza->DoesSocketExist(TEXT("Start")) &&
		ComponenteTraza->DoesSocketExist(TEXT("End")))
	{
		const FVector InicioSocket = ComponenteTraza->GetSocketLocation(TEXT("Start"));
		const FVector FinSocket = ComponenteTraza->GetSocketLocation(TEXT("End"));
		if (!InicioSocket.Equals(FinSocket, 1.0f))
		{
			Inicio = InicioSocket;
			Fin = FinSocket;
		}
	}

	if (bAtaquesDesvianProyectiles)
	{
		DesviarProyectilesEnAtaque(Inicio, Fin);
	}

	FCollisionQueryParams ParametrosTraza(SCENE_QUERY_STAT(AtaqueJugador), false, this);
	ParametrosTraza.AddIgnoredActor(this);

	TArray<FHitResult> Impactos;
	const float RadioTraza = bGolpeConCuerpo ? 34.0f : 28.0f;
	World->SweepMultiByChannel(
		Impactos,
		Inicio,
		Fin,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(RadioTraza),
		ParametrosTraza);

	const float Ahora = World->GetTimeSeconds();
	for (auto It = UltimosImpactosAtaque.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || Ahora - It.Value() > 1.5f)
		{
			It.RemoveCurrent();
		}
	}

	TSet<TWeakObjectPtr<AActor>> ObjetivosDeEstaTraza;
	for (const FHitResult& Impacto : Impactos)
	{
		AActor* Objetivo = Impacto.GetActor();
		if (!IsValid(Objetivo) || Objetivo == this || ObjetivosDeEstaTraza.Contains(Objetivo) ||
			!SoportaDanyoBlueprint(Objetivo))
		{
			continue;
		}

		ObjetivosDeEstaTraza.Add(Objetivo);
		if (const float* UltimoImpacto = UltimosImpactosAtaque.Find(Objetivo);
			UltimoImpacto && Ahora - *UltimoImpacto < 0.55f)
		{
			continue;
		}

		UltimosImpactosAtaque.Add(Objetivo, Ahora);
		const float DanyoBase = bGolpeConCuerpo ? 10.0f : 25.0f;
		AplicarGolpeConPoderes(Objetivo, DanyoBase, false);
		UE_LOG(
			LogCombatPowers,
			Display,
			TEXT("[ATAQUE] Traza %s impacto a %s por %.1f."),
			bGolpeConCuerpo ? TEXT("cuerpo/patada") : TEXT("espada"),
			*GetNameSafe(Objetivo),
			DanyoBase);
	}
}

float ATFGCharacter::ProcesarDanyoRecibido(float DanyoBase)
{
	if (bMuerteEnCurso)
	{
		return 0.0f;
	}

	const float DanyoSeguro = FMath::Max(0.0f, DanyoBase);
	float DanyoProcesado = DanyoSeguro;
	AActor* FuenteDanyo = FuenteDanyoPendiente.Get();
	FuenteDanyoPendiente.Reset();
	if (!IsValid(FuenteDanyo))
	{
		FuenteDanyo = BuscarFuenteDanyoCercana();
	}

	if (bGuardiaDireccionalActiva && IsValid(FuenteDanyo) && DanyoProcesado > 0.0f)
	{
		const FVector HaciaFuente = (FuenteDanyo->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		const float ProductoEscalar = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), HaciaFuente);
		const bool bGolpeFrontal = ProductoEscalar >= 0.0f;
		const float MultiplicadorDireccion = bGolpeFrontal ? MultiplicadorDanyoFrontal : MultiplicadorDanyoEspalda;
		const float DanyoAntesDireccion = DanyoProcesado;
		DanyoProcesado *= MultiplicadorDireccion;
		UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 1] Golpe por %s de %s: %.1f -> %.1f dano (dot %.2f)."),
			bGolpeFrontal ? TEXT("DELANTE") : TEXT("LA ESPALDA"), *GetNameSafe(FuenteDanyo),
			DanyoAntesDireccion, DanyoProcesado, ProductoEscalar);
	}

	if (bDanyoReducido)
	{
		const float DanyoAntesResistencia = DanyoProcesado;
		DanyoProcesado *= MultiplicadorDanyoRecibido;
		if (DanyoProcesado > 0.0f)
		{
			UE_LOG(LogCombatPowers, Display, TEXT("[PODER ANTIGUO 8] Dano reducido de %.1f a %.1f."), DanyoAntesResistencia, DanyoProcesado);
		}
	}

	if (bEsquivaActiva && DanyoProcesado > 0.0f && FMath::FRand() <= ProbabilidadEsquiva)
	{
		UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 4] ESQUIVA: golpe de %.1f dano anulado."), DanyoProcesado);
		return 0.0f;
	}

	if (bInversionDanyoActiva && bInversionDanyoPreparada && DanyoProcesado > 0.0f)
	{
		bInversionDanyoPreparada = false;
		const float CuracionReal = CurarPersonaje(DanyoProcesado, TEXT("inversion de dano"));
		GetWorldTimerManager().SetTimer(TimerInversionDanyo, this, &ATFGCharacter::PrepararInversionDanyo,
			EnfriamientoInversionDanyo, false);
		UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 5] Golpe de %.1f convertido en %.1f de curacion; recarga %.1f s."),
			DanyoProcesado, CuracionReal, EnfriamientoInversionDanyo);
		return 0.0f;
	}

	if (bTieneVidaExtra && ConsumirVidaExtraSiEsMortal(DanyoProcesado))
	{
		return 0.0f;
	}

	if (DanyoProcesado > 0.0f)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UMusicManagerSubsystem* Music =
				GameInstance->GetSubsystem<UMusicManagerSubsystem>())
			{
				Music->NotifyCombatActivity();
			}
		}
		ReproducirSonidoCombate(
			SonidoRecibirDanyo,
			0.82f,
			TEXT("reaccion al dano"));
		using namespace CombatPowerHelpers;
		if (UActorComponent* ComponenteVida = BuscarComponenteVida())
		{
			FNumericProperty* VidaActualProperty =
				FindNumericProperty(ComponenteVida, TEXT("VidaActual"));
			const double VidaActual =
				ReadNumericProperty(ComponenteVida, VidaActualProperty);
			if (VidaActual > 0.0 && DanyoProcesado >= VidaActual)
			{
				// QuitarVida and the legacy hit reaction still execute in the
				// Blueprint this frame. Start death next tick so its montage wins.
				bMuerteEnCurso = true;
				GetWorldTimerManager().SetTimerForNextTick(
					this,
					&ATFGCharacter::IniciarMuerteDefinitiva);
				UE_LOG(LogCombatPowers, Display,
					TEXT("[MUERTE] Golpe mortal detectado: vida %.1f, dano final %.1f."),
					VidaActual,
					DanyoProcesado);
			}
		}
	}

	UE_LOG(LogCombatPowers, Display, TEXT("Dano recibido por %s: %.1f."), *GetNameSafe(this), DanyoProcesado);
	return DanyoProcesado;
}

void ATFGCharacter::IniciarMuerteDefinitiva()
{
	if (!bMuerteEnCurso || bAnimacionMuerteIniciada || !GetWorld())
	{
		return;
	}
	bAnimacionMuerteIniciada = true;
	ReproducirSonidoCombate(
		SonidoDerrotaCaballeresca,
		1.0f,
		TEXT("derrota caballeresca de Arturo"));
	bSprintActivo = false;
	bSaltoOfensivoPendiente = false;
	MovimientoDesactivado = true;

	GetWorldTimerManager().ClearTimer(TimerRegeneracionMana);
	GetWorldTimerManager().ClearTimer(TimerSigilo);
	GetWorldTimerManager().ClearTimer(TimerInversionDanyo);
	GetWorldTimerManager().ClearTimer(TimerEnfriamientoDobleDanyo);

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (APlayerController* PlayerController =
		Cast<APlayerController>(GetController()))
	{
		PlayerController->ResetIgnoreMoveInput();
		PlayerController->ResetIgnoreLookInput();
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
		PlayerController->SetShowMouseCursor(false);
		DisableInput(PlayerController);
	}

	float Duracion = DuracionMuerteFallback;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.08f);
		if (AnimacionMuerte)
		{
			AnimInstance->PlaySlotAnimationAsDynamicMontage(
				AnimacionMuerte,
				TEXT("DefaultSlot"),
				0.08f,
				0.0f,
				1.0f,
				1,
				-1.0f,
				0.0f);
			Duracion = FMath::Max(0.25f, AnimacionMuerte->GetPlayLength());
		}
	}

	GetWorldTimerManager().SetTimer(
		TimerFinalizarMuerte,
		this,
		&ATFGCharacter::FinalizarMuerteYVolverLobby,
		Duracion,
		false);
	UE_LOG(LogCombatPowers, Display,
		TEXT("[MUERTE] Animacion iniciada; regreso al lobby en %.2f segundos."),
		Duracion);
}

void ATFGCharacter::FinalizarMuerteYVolverLobby()
{
	if (!bMuerteEnCurso)
	{
		return;
	}

	bOmitirGuardadoPoderesEnEndPlay = true;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URunPowerPersistenceSubsystem* Persistence =
			GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>())
		{
			// Clears only the current run. Resources, potions and permanent
			// upgrades are written to the selected save slot and retained.
			Persistence->MarkFirstReturnDialoguePending();
			Persistence->ResetPersistentPowers();
		}
	}

	UE_LOG(LogCombatPowers, Display,
		TEXT("[MUERTE] Animacion terminada. Run guardada y poderes temporales eliminados; volviendo al lobby."));
	UGameplayStatics::OpenLevel(
		this,
		FName(TEXT("/Game/MyContent/Maps/Lobby_MesaRedonda")));
}

bool ATFGCharacter::ProcesarImpactoProyectil(AActor* Proyectil, AActor* Objetivo, float DanyoBase)
{
	if (!IsValid(Proyectil) || !IsValid(Objetivo) || Objetivo == Proyectil)
	{
		return false;
	}

	AActor* Fuente = Proyectil->GetInstigator();
	if (!IsValid(Fuente))
	{
		Fuente = Proyectil->GetOwner();
	}

	// Ignore the projectile while it still overlaps its owner at spawn.
	if (Objetivo == Fuente || (Fuente == this && Objetivo == this))
	{
		return false;
	}

	if (Objetivo == this)
	{
		if (Fuente == this)
		{
			return false;
		}

		RegistrarFuenteDanyo(IsValid(Fuente) ? Fuente : Proyectil);
		const bool bAplicado = EnviarDanyoBlueprint(this, DanyoBase);
		if (bAplicado)
		{
			UE_LOG(LogCombatPowers, Display, TEXT("Proyectil enemigo %s impacto al jugador por %.1f dano base."), *GetNameSafe(Proyectil), DanyoBase);
		}
		return bAplicado;
	}

	// BP_Proyectil is also used by Blueprint spawn paths that do not always set
	// Owner or Instigator. The overlap itself is authoritative here: every
	// non-player actor that implements RecibirDanyo is a valid enemy target.
	if (SoportaDanyoBlueprint(Objetivo))
	{
		const bool bProyectilJugador = Fuente == this ||
			EsProyectilDelJugador(Proyectil) || !IsValid(Fuente);
		if (bProyectilJugador)
		{
			MarcarProyectilDelJugador(Proyectil);
			TSet<TWeakObjectPtr<AActor>>& ObjetivosGolpeados =
				ObjetivosPorProyectil.FindOrAdd(Proyectil);
			if (ObjetivosGolpeados.Contains(Objetivo))
			{
				return false;
			}
			ObjetivosGolpeados.Add(Objetivo);
		}

		UE_LOG(LogCombatPowers, Display, TEXT("[PROYECTIL] %s impacto a %s: aplicando %.1f dano base."),
			*GetNameSafe(Proyectil), *GetNameSafe(Objetivo), DanyoBase);
		AplicarGolpeConPoderes(Objetivo, DanyoBase, true);

		if (bProyectilJugador && bProyectilesRebotan)
		{
			int32& NumeroRebotes = RebotesPorProyectil.FindOrAdd(Proyectil);
			const TSet<TWeakObjectPtr<AActor>>& ObjetivosGolpeados =
				ObjetivosPorProyectil.FindChecked(Proyectil);
			if (NumeroRebotes < MaximoRebotesProyectil)
			{
				if (AActor* SiguienteObjetivo = EncontrarObjetivoParaProyectil(
					Objetivo->GetActorLocation(),
					Objetivo,
					&ObjetivosGolpeados,
					RadioReboteProyectil))
				{
					++NumeroRebotes;
					RedirigirProyectil(
						Proyectil, SiguienteObjetivo, TEXT("rebote"));
					if (UProjectileMovementComponent* Movimiento =
						Proyectil->FindComponentByClass<UProjectileMovementComponent>())
					{
						Proyectil->AddActorWorldOffset(
							Movimiento->Velocity.GetSafeNormal() * 75.0f,
							false);
					}
					return false;
				}
			}
			UE_LOG(LogCombatPowers, Display,
				TEXT("[REBOTE] %s termina su cadena tras %d rebotes: no quedan objetivos validos."),
				*GetNameSafe(Proyectil), NumeroRebotes);
		}
		return true;
	}

	UE_LOG(LogCombatPowers, Verbose, TEXT("[PROYECTIL] %s impacto a %s, pero el objetivo no implementa RecibirDanyo."),
		*GetNameSafe(Proyectil), *GetNameSafe(Objetivo));

	return false;
}

void ATFGCharacter::IniciarQuemadura(AActor* Objetivo)
{
	const TWeakObjectPtr<AActor> ObjetivoDebil(Objetivo);
	if (FQuemaduraActiva* Existente = QuemadurasActivas.Find(ObjetivoDebil))
	{
		GetWorldTimerManager().ClearTimer(Existente->Timer);
	}

	FQuemaduraActiva& Estado = QuemadurasActivas.FindOrAdd(ObjetivoDebil);
	Estado.TicksRestantes = FMath::Max(1, NumeroTicksQuemadura);
	FTimerDelegate Delegate;
	Delegate.BindUObject(this, &ATFGCharacter::TickQuemadura, ObjetivoDebil);
	GetWorldTimerManager().SetTimer(Estado.Timer, Delegate, IntervaloQuemadura, true, IntervaloQuemadura);

	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 2] %s empieza a arder: %.1f dano en %d ticks."),
		*GetNameSafe(Objetivo), DanyoTotalQuemadura, Estado.TicksRestantes);
}

void ATFGCharacter::TickQuemadura(TWeakObjectPtr<AActor> Objetivo)
{
	FQuemaduraActiva* Estado = QuemadurasActivas.Find(Objetivo);
	if (!Estado)
	{
		return;
	}

	AActor* ActorObjetivo = Objetivo.Get();
	if (!IsValid(ActorObjetivo))
	{
		GetWorldTimerManager().ClearTimer(Estado->Timer);
		QuemadurasActivas.Remove(Objetivo);
		return;
	}

	const float DanyoTick = DanyoTotalQuemadura / FMath::Max(1, NumeroTicksQuemadura);
	const bool bDanyoAplicado = EnviarDanyoBlueprint(ActorObjetivo, DanyoTick);
	if (bDanyoAplicado && bRoboVidaActivo)
	{
		CurarPersonaje(DanyoTick * PorcentajeRoboVida, TEXT("robo de vida por quemadura"));
	}
	--Estado->TicksRestantes;
	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 2] Tick de quemadura a %s: %.1f dano, %d restantes."),
		*GetNameSafe(ActorObjetivo), DanyoTick, FMath::Max(0, Estado->TicksRestantes));

	if (Estado->TicksRestantes <= 0)
	{
		GetWorldTimerManager().ClearTimer(Estado->Timer);
		QuemadurasActivas.Remove(Objetivo);
		UE_LOG(LogCombatPowers, Display, TEXT("[PODER 2] Quemadura terminada en %s."), *GetNameSafe(ActorObjetivo));
	}
}

void ATFGCharacter::AplicarDesplazamiento(AActor* Objetivo) const
{
	// Push strictly away from the point the hit came from. Keeping the vector
	// horizontal prevents the old power from launching enemies into the air.
	FVector Direccion = Objetivo->GetActorLocation() - GetActorLocation();
	Direccion.Z = 0.0f;
	Direccion = Direccion.GetSafeNormal();
	if (Direccion.IsNearlyZero())
	{
		Direccion = GetActorForwardVector().GetSafeNormal2D();
	}

	FVector Impulso = Direccion * FuerzaDesplazamiento;
	// A small controlled lift frees the capsule from floor friction and low
	// obstacles, allowing the horizontal knockback to travel its full distance.
	Impulso.Z = FuerzaVerticalDesplazamiento;
	if (ACharacter* PersonajeObjetivo = Cast<ACharacter>(Objetivo))
	{
		PersonajeObjetivo->LaunchCharacter(Impulso, true, true);
	}
	else if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Objetivo->GetRootComponent()))
	{
		if (Primitive->IsSimulatingPhysics())
		{
			Primitive->AddImpulse(Impulso * Primitive->GetMass());
		}
	}

	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 3] %s desplazado con impulso %s."), *GetNameSafe(Objetivo), *Impulso.ToCompactString());
}

void ATFGCharacter::AplicarDebilitado(AActor* Objetivo)
{
	using namespace CombatPowerHelpers;
	FNumericProperty* Property = FindNumericProperty(Objetivo, TEXT("DanyoAtaqueBase"));
	if (!Property)
	{
		UE_LOG(LogCombatPowers, Warning, TEXT("[PODER 5] %s no expone DanyoAtaqueBase; no se pudo debilitar."), *GetNameSafe(Objetivo));
		return;
	}

	const TWeakObjectPtr<AActor> ObjetivoDebil(Objetivo);
	FDebilitadoActivo* Existente = EnemigosDebilitados.Find(ObjetivoDebil);
	const double DanyoOriginal = Existente ? Existente->DanyoOriginal : ReadNumericProperty(Objetivo, Property);
	if (Existente)
	{
		GetWorldTimerManager().ClearTimer(Existente->Timer);
	}

	FDebilitadoActivo& Estado = EnemigosDebilitados.FindOrAdd(ObjetivoDebil);
	Estado.DanyoOriginal = DanyoOriginal;
	WriteNumericProperty(Objetivo, Property, DanyoOriginal * MultiplicadorDanyoEnemigoDebilitado);

	FTimerDelegate Delegate;
	Delegate.BindUObject(this, &ATFGCharacter::TerminarDebilitado, ObjetivoDebil);
	GetWorldTimerManager().SetTimer(Estado.Timer, Delegate, DuracionDebilitado, false);

	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 5] %s debilitado durante %.1f s: dano %.1f -> %.1f."),
		*GetNameSafe(Objetivo), DuracionDebilitado, DanyoOriginal, DanyoOriginal * MultiplicadorDanyoEnemigoDebilitado);
}

void ATFGCharacter::TerminarDebilitado(TWeakObjectPtr<AActor> Objetivo)
{
	using namespace CombatPowerHelpers;
	FDebilitadoActivo* Estado = EnemigosDebilitados.Find(Objetivo);
	if (!Estado)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(Estado->Timer);
	if (AActor* ActorObjetivo = Objetivo.Get())
	{
		if (FNumericProperty* Property = FindNumericProperty(ActorObjetivo, TEXT("DanyoAtaqueBase")))
		{
			WriteNumericProperty(ActorObjetivo, Property, Estado->DanyoOriginal);
		}
		UE_LOG(LogCombatPowers, Display, TEXT("[PODER 5] Debilitado terminado en %s; dano restaurado."), *GetNameSafe(ActorObjetivo));
	}
	EnemigosDebilitados.Remove(Objetivo);
}

void ATFGCharacter::AplicarParalisis(AActor* Objetivo)
{
	ACharacter* PersonajeObjetivo = Cast<ACharacter>(Objetivo);
	AAIController* AIController = PersonajeObjetivo ? Cast<AAIController>(PersonajeObjetivo->GetController()) : nullptr;
	if (!PersonajeObjetivo || !AIController)
	{
		UE_LOG(LogCombatPowers, Warning, TEXT("[PODER 9] %s no es un personaje controlado por IA."), *GetNameSafe(Objetivo));
		return;
	}

	const TWeakObjectPtr<AActor> ObjetivoDebil(Objetivo);
	FParalisisActiva* Existente = EnemigosParalizados.Find(ObjetivoDebil);
	const float VelocidadOriginal = Existente ? Existente->VelocidadOriginal : PersonajeObjetivo->GetCharacterMovement()->MaxWalkSpeed;
	bool bTeniaAggro = Existente ? Existente->bTeniaAggro : false;

	if (Existente)
	{
		GetWorldTimerManager().ClearTimer(Existente->Timer);
	}
	else if (UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent())
	{
		bTeniaAggro = Blackboard->GetValueAsBool(TEXT("Agro"));
	}

	FParalisisActiva& Estado = EnemigosParalizados.FindOrAdd(ObjetivoDebil);
	Estado.VelocidadOriginal = VelocidadOriginal;
	Estado.bTeniaAggro = bTeniaAggro;

	PersonajeObjetivo->GetCharacterMovement()->StopMovementImmediately();
	PersonajeObjetivo->GetCharacterMovement()->MaxWalkSpeed = 0.0f;
	PersonajeObjetivo->StopAnimMontage();
	AIController->StopMovement();
	AIController->ClearFocus(EAIFocusPriority::Gameplay);

	if (UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent())
	{
		Blackboard->SetValueAsBool(TEXT("Agro"), false);
	}
	if (UBrainComponent* Brain = AIController->GetBrainComponent())
	{
		Brain->PauseLogic(TEXT("Paralisis del poder 9"));
	}

	FTimerDelegate Delegate;
	Delegate.BindUObject(this, &ATFGCharacter::TerminarParalisis, ObjetivoDebil);
	GetWorldTimerManager().SetTimer(Estado.Timer, Delegate, DuracionParalisis, false);
	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 9] %s paralizado y sin aggro durante %.1f s."), *GetNameSafe(Objetivo), DuracionParalisis);
}

void ATFGCharacter::TerminarParalisis(TWeakObjectPtr<AActor> Objetivo)
{
	FParalisisActiva* Estado = EnemigosParalizados.Find(Objetivo);
	if (!Estado)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(Estado->Timer);
	if (ACharacter* PersonajeObjetivo = Cast<ACharacter>(Objetivo.Get()))
	{
		PersonajeObjetivo->GetCharacterMovement()->MaxWalkSpeed = Estado->VelocidadOriginal;
		if (AAIController* AIController = Cast<AAIController>(PersonajeObjetivo->GetController()))
		{
			if (UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent())
			{
				Blackboard->SetValueAsBool(TEXT("Agro"), Estado->bTeniaAggro);
			}
			if (UBrainComponent* Brain = AIController->GetBrainComponent())
			{
				Brain->ResumeLogic(TEXT("Fin de paralisis del poder 9"));
			}
		}
		UE_LOG(LogCombatPowers, Display, TEXT("[PODER 9] %s recupera movimiento y estado de aggro."), *GetNameSafe(PersonajeObjetivo));
	}
	EnemigosParalizados.Remove(Objetivo);
}

void ATFGCharacter::ProcesarEntradaDash(const FVector2D& MovementVector)
{
	const bool bAdelanteAhora = MovementVector.Y > 0.5f;
	const bool bAtrasAhora = MovementVector.Y < -0.5f;
	const bool bDerechaAhora = MovementVector.X > 0.5f;
	const bool bIzquierdaAhora = MovementVector.X < -0.5f;

	if (bDashHabilitado)
	{
		if (bAdelanteAhora && !bAdelantePulsado)
		{
			RegistrarPulsacionDash(FVector2D(0.0f, 1.0f), UltimaPulsacionAdelante);
		}
		if (bAtrasAhora && !bAtrasPulsado)
		{
			RegistrarPulsacionDash(FVector2D(0.0f, -1.0f), UltimaPulsacionAtras);
		}
		if (bDerechaAhora && !bDerechaPulsada)
		{
			RegistrarPulsacionDash(FVector2D(1.0f, 0.0f), UltimaPulsacionDerecha);
		}
		if (bIzquierdaAhora && !bIzquierdaPulsada)
		{
			RegistrarPulsacionDash(FVector2D(-1.0f, 0.0f), UltimaPulsacionIzquierda);
		}
	}

	bAdelantePulsado = bAdelanteAhora;
	bAtrasPulsado = bAtrasAhora;
	bDerechaPulsada = bDerechaAhora;
	bIzquierdaPulsada = bIzquierdaAhora;
}

void ATFGCharacter::RegistrarPulsacionDash(const FVector2D& DireccionLocal, float& UltimaPulsacion)
{
	const float Ahora = GetWorld()->GetTimeSeconds();
	if (Ahora - UltimaPulsacion <= VentanaDoblePulsacion && Ahora - UltimoDash > 0.15f)
	{
		UltimaPulsacion = -100.0f;
		EjecutarDash(DireccionLocal);
		return;
	}

	UltimaPulsacion = Ahora;
	UE_LOG(LogCombatPowers, Verbose, TEXT("[DASH] Primera pulsacion registrada para direccion (%.0f, %.0f)."),
		DireccionLocal.X, DireccionLocal.Y);
}

void ATFGCharacter::EjecutarDash(const FVector2D& DireccionLocal)
{
	if (!ConsumirStamina(CosteStaminaDash))
	{
		UE_LOG(LogCombatPowers, Warning, TEXT("[DASH] Cancelado: stamina insuficiente para gastar %.1f."), CosteStaminaDash);
		return;
	}

	const FRotator RotacionControl = GetController() ? GetController()->GetControlRotation() : GetActorRotation();
	const FRotator RotacionYaw(0.0f, RotacionControl.Yaw, 0.0f);
	const FVector DireccionAdelante = FRotationMatrix(RotacionYaw).GetUnitAxis(EAxis::X);
	const FVector DireccionDerecha = FRotationMatrix(RotacionYaw).GetUnitAxis(EAxis::Y);
	const FVector DireccionMundo = (DireccionDerecha * DireccionLocal.X + DireccionAdelante * DireccionLocal.Y).GetSafeNormal2D();

	UltimoDash = GetWorld()->GetTimeSeconds();
	UCharacterMovementComponent* Movimiento = GetCharacterMovement();
	const bool bDashAereo = Movimiento && Movimiento->IsFalling();
	if (Movimiento && !bDashAereo)
	{
		Movimiento->StopMovementImmediately();
	}

	const float FuerzaAplicada = FuerzaDash * (bDashAereo ? MultiplicadorDashAereo : 1.0f);
	// XY is replaced by the dash while Z is preserved. In the air this avoids
	// cancelling gravity/jump momentum, which previously lengthened the flight.
	LaunchCharacter(DireccionMundo * FuerzaAplicada, true, false);
	UE_LOG(
		LogCombatPowers,
		Display,
		TEXT("[DASH] Ejecutado en %s hacia %s; fuerza %.1f, coste %.1f, stamina restante %.1f."),
		bDashAereo ? TEXT("AIRE") : TEXT("TIERRA"),
		*DireccionMundo.ToCompactString(),
		FuerzaAplicada,
		CosteStaminaDash,
		ObtenerStaminaActual());
}

void ATFGCharacter::ResolverSaltoOfensivo()
{
	bSaltoOfensivoPendiente = false;
	TArray<FOverlapResult> Solapamientos;
	FCollisionObjectQueryParams TiposObjeto;
	TiposObjeto.AddObjectTypesToQuery(ECC_Pawn);
	TiposObjeto.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams ParametrosConsulta(SCENE_QUERY_STAT(SaltoOfensivo), false, this);
	GetWorld()->OverlapMultiByObjectType(Solapamientos, GetActorLocation(), FQuat::Identity, TiposObjeto,
		FCollisionShape::MakeSphere(RadioSaltoOfensivo), ParametrosConsulta);

	TSet<AActor*> ObjetivosUnicos;
	for (const FOverlapResult& Solapamiento : Solapamientos)
	{
		AActor* Objetivo = Solapamiento.GetActor();
		if (IsValid(Objetivo) && Objetivo != this && SoportaDanyoBlueprint(Objetivo))
		{
			ObjetivosUnicos.Add(Objetivo);
		}
	}

	for (AActor* Objetivo : ObjetivosUnicos)
	{
		AplicarGolpeConPoderes(Objetivo, DanyoSaltoOfensivo, false);
	}

	UE_LOG(LogCombatPowers, Display, TEXT("[NUEVO PODER 7] ATERRIZAJE: %.1f dano en radio %.1f a %d enemigos."),
		DanyoSaltoOfensivo, RadioSaltoOfensivo, ObjetivosUnicos.Num());
}

void ATFGCharacter::RegenerarMana()
{
	using namespace CombatPowerHelpers;
	if (RegeneracionVidaPermanente > 0.0f)
	{
		CurarPersonaje(RegeneracionVidaPermanente, TEXT("regeneracion de vida permanente"));
	}

	UActorComponent* ComponenteMana = BuscarComponenteRecurso(TEXT("ManaActual"), TEXT("ManaMaximo"));
	if (ComponenteMana)
	{
		FNumericProperty* ActualProperty = FindNumericProperty(ComponenteMana, TEXT("ManaActual"));
		FNumericProperty* MaximoProperty = FindNumericProperty(ComponenteMana, TEXT("ManaMaximo"));
		const double ManaAnterior = ReadNumericProperty(ComponenteMana, ActualProperty);
		const double ManaMaximo = ReadNumericProperty(ComponenteMana, MaximoProperty);
		const double ManaNuevo = FMath::Clamp(ManaAnterior + ManaRegeneradoPorSegundo, 0.0, ManaMaximo);
		if (!FMath::IsNearlyEqual(ManaAnterior, ManaNuevo))
		{
			WriteNumericProperty(ComponenteMana, ActualProperty, ManaNuevo);
			ActualizarHUDRecurso(TEXT("CambiarMana"), ManaMaximo > 0.0 ? ManaNuevo / ManaMaximo : 0.0f);
			UE_LOG(LogCombatPowers, Display, TEXT("[MANA] Regeneracion: %.1f -> %.1f / %.1f."), ManaAnterior, ManaNuevo, ManaMaximo);
		}
	}

	UActorComponent* ComponenteStamina = BuscarComponenteRecurso(TEXT("StaminaActual"), TEXT("StaminaMax"));
	if (ComponenteStamina)
	{
		FNumericProperty* ActualProperty = FindNumericProperty(ComponenteStamina, TEXT("StaminaActual"));
		FNumericProperty* MaximoProperty = FindNumericProperty(ComponenteStamina, TEXT("StaminaMax"));
		const double StaminaAnterior = ReadNumericProperty(ComponenteStamina, ActualProperty);
		const double StaminaMaxima = ReadNumericProperty(ComponenteStamina, MaximoProperty);
		const double CambioStamina = bSprintActivo
			? -StaminaConsumidaSprintPorSegundo
			: StaminaRegeneradaPorSegundo;
		const double StaminaNueva = FMath::Clamp(StaminaAnterior + CambioStamina, 0.0, StaminaMaxima);
		if (!FMath::IsNearlyEqual(StaminaAnterior, StaminaNueva))
		{
			WriteNumericProperty(ComponenteStamina, ActualProperty, StaminaNueva);
			ActualizarHUDRecurso(TEXT("CambiarStamina"), StaminaMaxima > 0.0 ? StaminaNueva / StaminaMaxima : 0.0f);
			UE_LOG(LogCombatPowers, Display, TEXT("[STAMINA] %s: %.1f -> %.1f / %.1f."),
				bSprintActivo ? TEXT("Consumo de sprint") : TEXT("Regeneracion"),
				StaminaAnterior, StaminaNueva, StaminaMaxima);
		}

		if (bSprintActivo && StaminaNueva <= 0.0)
		{
			UE_LOG(LogCombatPowers, Warning, TEXT("[SPRINT] Stamina agotada; sprint detenido automaticamente."));
			DetenerSprint();
		}
	}
}

float ATFGCharacter::ObtenerManaActual() const
{
	using namespace CombatPowerHelpers;
	if (UActorComponent* Componente = BuscarComponenteRecurso(TEXT("ManaActual"), TEXT("ManaMaximo")))
	{
		return static_cast<float>(ReadNumericProperty(Componente, FindNumericProperty(Componente, TEXT("ManaActual"))));
	}
	return 0.0f;
}

float ATFGCharacter::ObtenerStaminaActual() const
{
	using namespace CombatPowerHelpers;
	if (UActorComponent* Componente = BuscarComponenteRecurso(TEXT("StaminaActual"), TEXT("StaminaMax")))
	{
		return static_cast<float>(ReadNumericProperty(Componente, FindNumericProperty(Componente, TEXT("StaminaActual"))));
	}
	return 0.0f;
}

void ATFGCharacter::GuardarRecolectable(
	const EEnemyCollectibleType Tipo,
	const int32 Cantidad)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URunPowerPersistenceSubsystem* Persistence =
			GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>())
		{
			Persistence->AddCollectible(Tipo, Cantidad);
			UE_LOG(
				LogCombatPowers,
				Display,
				TEXT("[PERSONAJE] Recolectable guardado. Tipo=%d, cantidad tipo=%d, total=%d."),
				static_cast<int32>(Tipo),
				Persistence->GetCollectibleCount(Tipo),
				Persistence->GetTotalCollectibleCount());
		}
	}
}

void ATFGCharacter::DebugDarLuz()
{
	OtorgarRecursoDebug(EEnemyCollectibleType::Luz, TEXT("LUZ"));
}

void ATFGCharacter::DebugDarVida()
{
	OtorgarRecursoDebug(EEnemyCollectibleType::Vida, TEXT("VIDA"));
}

void ATFGCharacter::DebugDarEnergia()
{
	OtorgarRecursoDebug(EEnemyCollectibleType::Energia, TEXT("ENERGIA"));
}

void ATFGCharacter::DebugDarMana()
{
	OtorgarRecursoDebug(EEnemyCollectibleType::Mana, TEXT("MANA"));
}

void ATFGCharacter::OtorgarRecursoDebug(
	const EEnemyCollectibleType Tipo,
	const TCHAR* Nombre)
{
	GuardarRecolectable(Tipo, 1);
	const int32 CantidadActual = ObtenerCantidadRecolectable(Tipo);
	const FString Mensaje = FString::Printf(
		TEXT("[PRUEBA] +1 %s (total: %d)"),
		Nombre,
		CantidadActual);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.5f,
			FColor::Cyan,
			Mensaje);
	}
	UE_LOG(
		LogCombatPowers,
		Display,
		TEXT("%s. Teclas temporales: 1 Luz, 2 Vida, 3 Energia, 4 Mana."),
		*Mensaje);
}

int32 ATFGCharacter::ObtenerCantidadRecolectable(
	const EEnemyCollectibleType Tipo) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const URunPowerPersistenceSubsystem* Persistence =
		GameInstance
			? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>()
			: nullptr;
	return Persistence ? Persistence->GetCollectibleCount(Tipo) : 0;
}

int32 ATFGCharacter::ObtenerTotalRecolectables() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const URunPowerPersistenceSubsystem* Persistence =
		GameInstance
			? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>()
			: nullptr;
	return Persistence ? Persistence->GetTotalCollectibleCount() : 0;
}

bool ATFGCharacter::ConsumirStamina(float Cantidad)
{
	using namespace CombatPowerHelpers;
	UActorComponent* Componente = BuscarComponenteRecurso(TEXT("StaminaActual"), TEXT("StaminaMax"));
	if (!Componente)
	{
		UE_LOG(LogCombatPowers, Warning, TEXT("[STAMINA] No se encontro BP_BarraStamina."));
		return false;
	}

	FNumericProperty* ActualProperty = FindNumericProperty(Componente, TEXT("StaminaActual"));
	FNumericProperty* MaximoProperty = FindNumericProperty(Componente, TEXT("StaminaMax"));
	const double Actual = ReadNumericProperty(Componente, ActualProperty);
	const double Maximo = ReadNumericProperty(Componente, MaximoProperty);
	if (Actual < Cantidad)
	{
		return false;
	}

	const double Nuevo = FMath::Max(0.0, Actual - Cantidad);
	WriteNumericProperty(Componente, ActualProperty, Nuevo);
	ActualizarHUDRecurso(TEXT("CambiarStamina"), Maximo > 0.0 ? Nuevo / Maximo : 0.0f);
	return true;
}

float ATFGCharacter::CurarPersonaje(float Cantidad, const TCHAR* Motivo)
{
	using namespace CombatPowerHelpers;
	UActorComponent* ComponenteVida = BuscarComponenteVida();
	if (!ComponenteVida || Cantidad <= 0.0f)
	{
		return 0.0f;
	}

	FNumericProperty* ActualProperty = FindNumericProperty(ComponenteVida, TEXT("VidaActual"));
	FNumericProperty* MaximaProperty = FindNumericProperty(ComponenteVida, TEXT("VidaMaxima"));
	const double VidaAnterior = ReadNumericProperty(ComponenteVida, ActualProperty);
	const double VidaMaxima = ReadNumericProperty(ComponenteVida, MaximaProperty);
	const double VidaNueva = FMath::Clamp(VidaAnterior + Cantidad, 0.0, VidaMaxima);
	WriteNumericProperty(ComponenteVida, ActualProperty, VidaNueva);

	if (FBoolProperty* MuertoProperty = FindFProperty<FBoolProperty>(ComponenteVida->GetClass(), TEXT("EstoyMuerto")))
	{
		MuertoProperty->SetPropertyValue_InContainer(ComponenteVida, false);
	}

	ActualizarHUDRecurso(TEXT("CambiarVida"), VidaMaxima > 0.0 ? VidaNueva / VidaMaxima : 0.0f);
	const float CuracionReal = static_cast<float>(VidaNueva - VidaAnterior);
	UE_LOG(LogCombatPowers, Display, TEXT("[CURACION] %s: +%.1f de vida (%.1f -> %.1f / %.1f)."),
		Motivo, CuracionReal, VidaAnterior, VidaNueva, VidaMaxima);
	return CuracionReal;
}

AActor* ATFGCharacter::BuscarFuenteDanyoCercana() const
{
	AActor* FuenteMasCercana = nullptr;
	float DistanciaCuadradaMinima = FMath::Square(600.0f);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidato = *It;
		if (!IsValid(Candidato) || Candidato == this ||
			!FindFProperty<FNumericProperty>(Candidato->GetClass(), TEXT("DanyoAtaqueBase")))
		{
			continue;
		}

		const float DistanciaCuadrada = FVector::DistSquared2D(GetActorLocation(), Candidato->GetActorLocation());
		if (DistanciaCuadrada < DistanciaCuadradaMinima)
		{
			DistanciaCuadradaMinima = DistanciaCuadrada;
			FuenteMasCercana = Candidato;
		}
	}
	return FuenteMasCercana;
}

void ATFGCharacter::ActualizarHUDRecurso(FName Funcion, float Porcentaje) const
{
	const FObjectPropertyBase* WidgetProperty = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("InterfazGrafica"));
	UObject* Widget = WidgetProperty ? WidgetProperty->GetObjectPropertyValue_InContainer(this) : nullptr;
	UFunction* FuncionHUD = Widget ? Widget->FindFunction(Funcion) : nullptr;
	if (!FuncionHUD)
	{
		return;
	}

	uint8* Parametros = static_cast<uint8*>(FMemory_Alloca(FuncionHUD->ParmsSize));
	FMemory::Memzero(Parametros, FuncionHUD->ParmsSize);
	for (TFieldIterator<FProperty> It(FuncionHUD); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Property = *It;
		if (Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
		{
			continue;
		}
		if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
		{
			void* DireccionValor = NumericProperty->ContainerPtrToValuePtr<void>(Parametros);
			NumericProperty->SetFloatingPointPropertyValue(DireccionValor, Porcentaje);
			Widget->ProcessEvent(FuncionHUD, Parametros);
			return;
		}
	}
}

bool ATFGCharacter::EnviarDanyoBlueprint(AActor* Objetivo, float Danyo) const
{
	if (!IsValid(Objetivo))
	{
		return false;
	}

	UFunction* FuncionDanyo = Objetivo->FindFunction(TEXT("RecibirDanyo"));
	if (!FuncionDanyo)
	{
		return false;
	}

	uint8* Parametros = static_cast<uint8*>(FMemory_Alloca(FuncionDanyo->ParmsSize));
	FMemory::Memzero(Parametros, FuncionDanyo->ParmsSize);

	bool bParametroAsignado = false;
	for (TFieldIterator<FProperty> It(FuncionDanyo); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Property = *It;
		if (Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
		{
			continue;
		}

		if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
		{
			void* ValueAddress = NumericProperty->ContainerPtrToValuePtr<void>(Parametros);
			if (NumericProperty->IsFloatingPoint())
			{
				NumericProperty->SetFloatingPointPropertyValue(ValueAddress, Danyo);
			}
			else
			{
				NumericProperty->SetIntPropertyValue(ValueAddress, FMath::RoundToInt64(Danyo));
			}
			bParametroAsignado = true;
			break;
		}
	}

	if (!bParametroAsignado)
	{
		UE_LOG(LogCombatPowers, Error, TEXT("RecibirDanyo en %s no tiene un parametro numerico compatible."), *GetNameSafe(Objetivo));
		return false;
	}

	Objetivo->ProcessEvent(FuncionDanyo, Parametros);
	return true;
}

bool ATFGCharacter::SoportaDanyoBlueprint(const AActor* Objetivo) const
{
	return IsValid(Objetivo) && Objetivo->FindFunction(TEXT("RecibirDanyo")) != nullptr;
}

UActorComponent* ATFGCharacter::BuscarComponenteVida() const
{
	TInlineComponentArray<UActorComponent*> Componentes(const_cast<ATFGCharacter*>(this));
	for (UActorComponent* Componente : Componentes)
	{
		if (Componente && FindFProperty<FNumericProperty>(Componente->GetClass(), TEXT("VidaActual")) &&
			FindFProperty<FNumericProperty>(Componente->GetClass(), TEXT("VidaMaxima")))
		{
			return Componente;
		}
	}
	return nullptr;
}

UActorComponent* ATFGCharacter::BuscarComponenteRecurso(FName PropiedadActual, FName PropiedadMaxima) const
{
	TInlineComponentArray<UActorComponent*> Componentes(const_cast<ATFGCharacter*>(this));
	for (UActorComponent* Componente : Componentes)
	{
		if (Componente && FindFProperty<FNumericProperty>(Componente->GetClass(), PropiedadActual) &&
			FindFProperty<FNumericProperty>(Componente->GetClass(), PropiedadMaxima))
		{
			return Componente;
		}
	}
	return nullptr;
}

bool ATFGCharacter::ConsumirVidaExtraSiEsMortal(float DanyoProcesado)
{
	using namespace CombatPowerHelpers;
	UActorComponent* ComponenteVida = BuscarComponenteVida();
	if (!ComponenteVida)
	{
		UE_LOG(LogCombatPowers, Warning, TEXT("[PODER 7] No se encontro BP_BarraVida para comprobar el dano mortal."));
		return false;
	}

	FNumericProperty* VidaActualProperty = FindNumericProperty(ComponenteVida, TEXT("VidaActual"));
	FNumericProperty* VidaMaximaProperty = FindNumericProperty(ComponenteVida, TEXT("VidaMaxima"));
	const double VidaActual = ReadNumericProperty(ComponenteVida, VidaActualProperty);
	if (DanyoProcesado < VidaActual)
	{
		return false;
	}

	const double VidaMaxima = ReadNumericProperty(ComponenteVida, VidaMaximaProperty);
	WriteNumericProperty(ComponenteVida, VidaActualProperty, VidaMaxima);
	if (FBoolProperty* MuertoProperty = FindFProperty<FBoolProperty>(ComponenteVida->GetClass(), TEXT("EstoyMuerto")))
	{
		MuertoProperty->SetPropertyValue_InContainer(ComponenteVida, false);
	}

	bTieneVidaExtra = false;
	if (bVidaExtraPermanenteAsignada)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (URunPowerPersistenceSubsystem* Persistence =
				GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>())
			{
				Persistence->ConsumePermanentRevive();
			}
		}
		bVidaExtraPermanenteAsignada = false;
	}
	GuardarEstadoPoderesPersistentes(TEXT("vida extra consumida"));
	UE_LOG(LogCombatPowers, Display, TEXT("[PODER 7] VIDA EXTRA CONSUMIDA: dano mortal %.1f anulado y vida restaurada a %.1f."),
		DanyoProcesado, VidaMaxima);
	return true;
}
