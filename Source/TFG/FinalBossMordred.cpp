#include "FinalBossMordred.h"

#include "TFGCharacter.h"
#include "Systems/MusicManagerSubsystem.h"
#include "UI/MordredBossHealthWidget.h"
#include "UI/MordredVictoryDialogueWidget.h"
#include "UI/NPCDialogueNameWidget.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogMordredBoss, Log, All);

namespace MordredBossHelpers
{
	static void InvokeNoParam(UObject* Target, const FName FunctionName)
	{
		if (Target)
		{
			if (UFunction* Function = Target->FindFunction(FunctionName))
			{
				Target->ProcessEvent(Function, nullptr);
			}
		}
	}

	static void InvokeText(UObject* Target, const FName FunctionName, const FString& Value)
	{
		UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
		if (Function)
		{
			uint8* Params = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
			FMemory::Memzero(Params, Function->ParmsSize);
			bool bInvoked = false;
			for (TFieldIterator<FProperty> It(Function);
				It && It->HasAnyPropertyFlags(CPF_Parm) && !bInvoked; ++It)
			{
				FProperty* Property = *It;
				if (!Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
				{
					if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
					{
						StringProperty->SetPropertyValue_InContainer(Params, Value);
						bInvoked = true;
					}
					else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
					{
						TextProperty->SetPropertyValue_InContainer(Params, FText::FromString(Value));
						bInvoked = true;
					}
					if (bInvoked)
					{
						Target->ProcessEvent(Function, Params);
					}
				}
			}
		}
	}

	static UObject* ResolvePlayerHud(ACharacter* Player)
	{
		const FObjectPropertyBase* HudProperty = Player
			? FindFProperty<FObjectPropertyBase>(Player->GetClass(), TEXT("InterfazGrafica"))
			: nullptr;
		return HudProperty
			? HudProperty->GetObjectPropertyValue_InContainer(Player)
			: nullptr;
	}

	static bool InvokeNumeric(UObject* Target, const FName FunctionName, const float Value)
	{
		UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
		bool bInvoked = false;
		if (Function)
		{
			uint8* Params = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
			FMemory::Memzero(Params, Function->ParmsSize);
			for (TFieldIterator<FProperty> It(Function);
				It && It->HasAnyPropertyFlags(CPF_Parm) && !bInvoked; ++It)
			{
				FProperty* Property = *It;
				if (!Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
				{
					if (FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
					{
						void* Address = Numeric->ContainerPtrToValuePtr<void>(Params);
						if (Numeric->IsFloatingPoint())
						{
							Numeric->SetFloatingPointPropertyValue(Address, Value);
						}
						else
						{
							Numeric->SetIntPropertyValue(Address, FMath::RoundToInt64(Value));
						}
						Target->ProcessEvent(Function, Params);
						bInvoked = true;
					}
				}
			}
		}
		return bInvoked;
	}
}

AFinalBossMordred::AFinalBossMordred()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AMordredBossAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	EquippedSword = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EquippedSword"));
	EquippedSword->SetupAttachment(GetMesh(), TEXT("Espada1_S"));
	EquippedSword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquippedSword->SetGenerateOverlapEvents(false);

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	// BP_Combate traces melee hits through TraceTypeQuery1 (Visibility).
	// Character's default Pawn profile ignores that channel, so explicitly
	// block it to make sword and kick traces register Mordred as their hit actor.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// Camera booms should retract against walls, not against a boss that is
	// dashing, kicking or standing close behind the player.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 620.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = 520.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2200.0f;
	GetCharacterMovement()->AirControl = 0.35f;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> PlayerMesh(
		TEXT("/Game/ShadowKight/Mesh/ShadowknightUE_SK.ShadowknightUE_SK"));
	if (PlayerMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(PlayerMesh.Object);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -89.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> PlayerSword(
		TEXT("/Game/Fab/Moon_Sword/moon_sword/StaticMeshes/SKM_moon_sword.SKM_moon_sword"));
	if (PlayerSword.Succeeded())
	{
		EquippedSword->SetSkeletalMesh(PlayerSword.Object);
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> PlayerAnimClass(
		TEXT("/Game/MyContent/Animaciones/Arturo/BPAnim_Arturo"));
	if (PlayerAnimClass.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(PlayerAnimClass.Class);
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> SwordMontage(
		TEXT("/Game/MyContent/Animaciones/Arturo/Anim_Viking_attack2_Montage.Anim_Viking_attack2_Montage"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> KickMontage(
		TEXT("/Game/MyContent/Animaciones/Arturo/Anim_Viking_attack3_Montage.Anim_Viking_attack3_Montage"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> HitReaction(
		TEXT("/Game/MyContent/Animaciones/Arturo/Anim_Viking_gethit_Montage.Anim_Viking_gethit_Montage"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DeathSequence(
		TEXT("/Game/MyContent/Animaciones/Arturo/Anim_Viking_death.Anim_Viking_death"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> ProjectileSequence(
		TEXT("/Game/MyContent/Animaciones/Arturo/Anim_Viking_attack1.Anim_Viking_attack1"));
	SwordAttackMontage = SwordMontage.Object;
	KickAttackMontage = KickMontage.Object;
	HitMontage = HitReaction.Object;
	DeathAnimation = DeathSequence.Object;
	ProjectileLaunchAnimation = ProjectileSequence.Object;

	static ConstructorHelpers::FClassFinder<AActor> ProjectileBlueprint(
		TEXT("/Game/MyContent/BluePrints/Actor/BP_Proyectil"));
	if (ProjectileBlueprint.Succeeded())
	{
		ProjectileClass = ProjectileBlueprint.Class;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> HitSound(
		TEXT("/Game/MyContent/Audio/Sounds/Hit.Hit"));
	static ConstructorHelpers::FObjectFinder<USoundBase> HurtSound(
		TEXT("/Game/MyContent/Audio/Sounds/hurt.hurt"));
	static ConstructorHelpers::FObjectFinder<USoundBase> SwordSound(
		TEXT("/Game/MyContent/Audio/Sounds/sword.sword"));
	static ConstructorHelpers::FObjectFinder<USoundBase> FireballSound(
		TEXT("/Game/MyContent/Audio/Sounds/floraphonic-fireball-whoosh-1-179125.floraphonic-fireball-whoosh-1-179125"));
	if (SwordSound.Succeeded())
	{
		AttackSound = SwordSound.Object;
	}
	if (HitSound.Succeeded())
	{
		ImpactSound = HitSound.Object;
	}
	if (HurtSound.Succeeded())
	{
		HitReactionSound = HurtSound.Object;
	}
	if (FireballSound.Succeeded())
	{
		ProjectileLaunchSound = FireballSound.Object;
	}

	IntroDialoguePackages = {
		{
			TEXT("Herencia"),
			{
				TEXT("Mi padre me enseñó que una corona nunca se entrega: se conquista."),
				TEXT("Camelot eligió recordarte como rey y olvidarme como heredero."),
				TEXT("Hoy corregiré ese error con tu sangre.")
			}
		},
		{
			TEXT("Usurpador"),
			{
				TEXT("Has cruzado todo el reino para llegar hasta mí."),
				TEXT("Cada victoria te ha acercado al trono que jamás mereciste."),
				TEXT("Ven, Arturo. Que Camelot contemple a su verdadero rey.")
			}
		},
		{
			TEXT("Destino"),
			{
				TEXT("La Mesa Redonda se quebró mucho antes de que entraras en esta sala."),
				TEXT("Tú todavía luchas por sus ruinas; yo construiré algo nuevo sobre ellas."),
				TEXT("Solo uno de nosotros saldrá de aquí con vida.")
			}
		}
	};
	VictoryDialoguePackages = {
		{
			TEXT("UltimasPalabras"),
			{
				TEXT("Así que este era el final que Camelot había reservado para mí..."),
				TEXT("Has ganado la batalla, Arturo, pero el reino seguirá recordando nuestras heridas."),
				TEXT("Gobierna sobre las ruinas... si todavía puedes llamarlas hogar.")
			}
		},
		{
			TEXT("Derrota"),
			{
				TEXT("No queda fuerza en mis brazos, pero mi voluntad no se arrodilla."),
				TEXT("Hoy recuperas Camelot; mañana descubrirás cuánto te ha costado."),
				TEXT("Termina lo que viniste a hacer, rey de un reino roto.")
			}
		},
		{
			TEXT("Legado"),
			{
				TEXT("Quizá los bardos digan que el bien venció al mal en esta sala."),
				TEXT("Ellos nunca contarán las decisiones que nos trajeron hasta aquí."),
				TEXT("Recuerda mi nombre, Arturo. Mordred también fue hijo de Camelot.")
			}
		}
	};
	Tags.AddUnique(TEXT("Enemy"));
	Tags.AddUnique(TEXT("Boss"));
}

void AFinalBossMordred::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
	CachedPlayer = ResolvePlayerCharacter();
	if (ACharacter* Player = CachedPlayer.Get())
	{
		FVector ToMordred = GetActorLocation() - Player->GetActorLocation();
		ToMordred.Z = 0.0f;
		if (!ToMordred.IsNearlyZero())
		{
			FRotator FacingRotation = ToMordred.Rotation();
			FacingRotation.Pitch = 0.0f;
			FacingRotation.Roll = 0.0f;
			Player->SetActorRotation(FacingRotation);
			if (AController* PlayerController = Player->GetController())
			{
				PlayerController->SetControlRotation(FacingRotation);
			}
			UE_LOG(
				LogMordredBoss,
				Display,
				TEXT("[MORDRED] Jugador orientado hacia el boss al entrar: yaw %.1f."),
				FacingRotation.Yaw);
		}
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		BossHealthWidget = CreateWidget<UMordredBossHealthWidget>(
			PlayerController,
			UMordredBossHealthWidget::StaticClass());
		if (BossHealthWidget)
		{
			BossHealthWidget->AddToViewport(1500);
			BossHealthWidget->SetVisibility(ESlateVisibility::Collapsed);
			UpdateBossHealthBar();
		}
	}
	BindDialogueInput();

	SetPlayerMovementEnabled(false);
	GetWorldTimerManager().SetTimer(
		IntroStartTimer,
		this,
		&AFinalBossMordred::StartIntroDialogue,
		0.65f,
		false);

	UE_LOG(
		LogMordredBoss,
		Display,
		TEXT("[MORDRED] Boss creado con %.0f de vida. IA bloqueada hasta terminar el dialogo."),
		MaxHealth);
}

void AFinalBossMordred::StartIntroDialogue()
{
	if (bDead)
	{
		return;
	}
	bIntroActive = true;
	DialogueIndex = 0;
	ActiveIntroDialogue = SelectDialoguePackage(IntroDialoguePackages, TEXT("entrada"));
	if (ATFGCharacter* Player = Cast<ATFGCharacter>(ResolvePlayerCharacter()))
	{
		Player->EstablecerDialogoActivo(true);
	}
	ShowMordredDialogueName();
	BindDialogueInput();
	if (ActiveIntroDialogue.IsValidIndex(DialogueIndex))
	{
		ShowDialogueLine(ActiveIntroDialogue[DialogueIndex]);
		UE_LOG(
			LogMordredBoss,
			Display,
			TEXT("[MORDRED] Linea de dialogo 1/%d. Esperando pulsacion de E."),
			ActiveIntroDialogue.Num());
	}
	else
	{
		FinishIntroDialogue();
	}
	UE_LOG(LogMordredBoss, Display, TEXT("[MORDRED] Dialogo de entrada iniciado; pulsa E para avanzar."));
}

void AFinalBossMordred::AdvanceIntroDialogue()
{
	if (bVictoryDialogueActive)
	{
		AdvanceVictoryDialogue();
	}
	else if (bIntroActive)
	{
		++DialogueIndex;
		if (ActiveIntroDialogue.IsValidIndex(DialogueIndex))
		{
			ShowDialogueLine(ActiveIntroDialogue[DialogueIndex]);
			UE_LOG(
				LogMordredBoss,
				Display,
				TEXT("[MORDRED] Linea de dialogo %d/%d. Esperando pulsacion de E."),
				DialogueIndex + 1,
				ActiveIntroDialogue.Num());
		}
		else
		{
			FinishIntroDialogue();
		}
	}
}

void AFinalBossMordred::StartVictoryDialogue()
{
	if (!bDead || bVictoryDialogueActive)
	{
		return;
	}

	bVictoryDialogueActive = true;
	VictoryDialogueIndex = 0;
	ActiveVictoryDialogue = SelectDialoguePackage(VictoryDialoguePackages, TEXT("victoria"));
	SetPlayerMovementEnabled(false);
	if (ATFGCharacter* Player = Cast<ATFGCharacter>(ResolvePlayerCharacter()))
	{
		Player->EstablecerDialogoActivo(true);
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		VictoryDialogueWidget = CreateWidget<UMordredVictoryDialogueWidget>(
			PlayerController,
			UMordredVictoryDialogueWidget::StaticClass());
		if (VictoryDialogueWidget)
		{
			VictoryDialogueWidget->AddToViewport(3200);
		}
	}
	BindDialogueInput();

	if (ActiveVictoryDialogue.IsValidIndex(VictoryDialogueIndex))
	{
		if (VictoryDialogueWidget)
		{
			VictoryDialogueWidget->SetDialogueLine(
				ActiveVictoryDialogue[VictoryDialogueIndex],
				VictoryDialogueIndex + 1,
				ActiveVictoryDialogue.Num());
		}
	}
	else
	{
		FinishVictoryDialogue();
	}
	UE_LOG(LogMordredBoss, Display,
		TEXT("[MORDRED] Epilogo de victoria iniciado; pulsa E para avanzar."));
}

void AFinalBossMordred::AdvanceVictoryDialogue()
{
	if (!bVictoryDialogueActive)
	{
		return;
	}
	++VictoryDialogueIndex;
	if (ActiveVictoryDialogue.IsValidIndex(VictoryDialogueIndex))
	{
		if (VictoryDialogueWidget)
		{
			VictoryDialogueWidget->SetDialogueLine(
				ActiveVictoryDialogue[VictoryDialogueIndex],
				VictoryDialogueIndex + 1,
				ActiveVictoryDialogue.Num());
		}
		UE_LOG(LogMordredBoss, Display,
			TEXT("[MORDRED] Epilogo %d/%d."),
			VictoryDialogueIndex + 1, ActiveVictoryDialogue.Num());
	}
	else
	{
		FinishVictoryDialogue();
	}
}

void AFinalBossMordred::FinishVictoryDialogue()
{
	bVictoryDialogueActive = false;
	UnbindDialogueInput();
	if (VictoryDialogueWidget)
	{
		VictoryDialogueWidget->RemoveFromParent();
		VictoryDialogueWidget = nullptr;
	}
	ActiveVictoryDialogue.Reset();
	if (ATFGCharacter* Player = Cast<ATFGCharacter>(ResolvePlayerCharacter()))
	{
		Player->EstablecerDialogoActivo(false);
		Player->PrepararFinRunPorVictoria();
	}
	UE_LOG(LogMordredBoss, Display,
		TEXT("[MORDRED] Epilogo terminado. Volviendo a Lobby_MesaRedonda."));
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/MyContent/Maps/Lobby_MesaRedonda")));
}

void AFinalBossMordred::FinishIntroDialogue()
{
	bIntroActive = false;
	if (ATFGCharacter* Player = Cast<ATFGCharacter>(ResolvePlayerCharacter()))
	{
		Player->EstablecerDialogoActivo(false);
	}
	UnbindDialogueInput();
	bCombatActive = true;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UMusicManagerSubsystem* Music =
			GameInstance->GetSubsystem<UMusicManagerSubsystem>())
		{
			Music->SetMordredCombatActive(true);
		}
	}
	DialogueIndex = INDEX_NONE;
	HideDialogue();
	HideMordredDialogueName();
	ActiveIntroDialogue.Reset();
	SetPlayerMovementEnabled(true);
	if (BossHealthWidget)
	{
		BossHealthWidget->SetVisibility(ESlateVisibility::Visible);
	}
	TimeUntilNextAction = 1.0f;
	UE_LOG(LogMordredBoss, Display, TEXT("[MORDRED] Dialogo terminado. Combate de boss iniciado."));
}

void AFinalBossMordred::BindDialogueInput()
{
	if (!bDialogueInputBound)
	{
		APlayerController* PlayerController =
			UGameplayStatics::GetPlayerController(this, 0);
		if (PlayerController)
		{

	if (!DialogueInputComponent)
	{
		DialogueInputComponent =
			NewObject<UInputComponent>(this, TEXT("MordredDialogueInput"));
		DialogueInputComponent->RegisterComponent();
		DialogueInputComponent->Priority = 30;
		DialogueInputComponent->bBlockInput = false;
		DialogueInputComponent->BindKey(
			EKeys::E,
			IE_Pressed,
			this,
			&AFinalBossMordred::AdvanceIntroDialogue);
	}

			PlayerController->PushInputComponent(DialogueInputComponent);
			bDialogueInputBound = true;
			UE_LOG(
				LogMordredBoss,
				Display,
				TEXT("[MORDRED] Entrada manual E preparada para el dialogo final."));
		}
	}
}

void AFinalBossMordred::UnbindDialogueInput()
{
	if (!bDialogueInputBound)
	{
		return;
	}

	if (APlayerController* PlayerController =
		UGameplayStatics::GetPlayerController(this, 0))
	{
		PlayerController->PopInputComponent(DialogueInputComponent);
	}
	bDialogueInputBound = false;
}

void AFinalBossMordred::ShowDialogueLine(const FString& Line) const
{
	if (ACharacter* Player = ResolvePlayerCharacter())
	{
		MordredBossHelpers::InvokeText(
			MordredBossHelpers::ResolvePlayerHud(Player),
			TEXT("MostrarTexto"),
			Line);
	}
}

void AFinalBossMordred::HideDialogue() const
{
	if (ACharacter* Player = ResolvePlayerCharacter())
	{
		MordredBossHelpers::InvokeNoParam(
			MordredBossHelpers::ResolvePlayerHud(Player),
			TEXT("OcultarTexto"));
	}
}

TArray<FString> AFinalBossMordred::SelectDialoguePackage(
	const TArray<FNPCDialoguePackage>& Packages,
	const TCHAR* DialoguePhase) const
{
	TArray<FString> SelectedLines;
	TArray<int32> ValidPackageIndices;
	for (int32 PackageIndex = 0; PackageIndex < Packages.Num(); ++PackageIndex)
	{
		if (!Packages[PackageIndex].Lineas.IsEmpty())
		{
			ValidPackageIndices.Add(PackageIndex);
		}
	}

	if (ValidPackageIndices.IsEmpty())
	{
		UE_LOG(
			LogMordredBoss,
			Warning,
			TEXT("[MORDRED] No hay paquetes validos para el dialogo de %s."),
			DialoguePhase);
	}
	else
	{
		const int32 SelectedIndex =
			ValidPackageIndices[FMath::RandHelper(ValidPackageIndices.Num())];
		const FNPCDialoguePackage& SelectedPackage = Packages[SelectedIndex];
		SelectedLines = SelectedPackage.Lineas;
		UE_LOG(
			LogMordredBoss,
			Display,
			TEXT("[MORDRED] Paquete de %s seleccionado: '%s' (%d frases de %d paquetes)."),
			DialoguePhase,
			*SelectedPackage.NombrePaquete.ToString(),
			SelectedPackage.Lineas.Num(),
			ValidPackageIndices.Num());
	}
	return SelectedLines;
}

void AFinalBossMordred::ShowMordredDialogueName()
{
	if (DialogueNameWidget)
	{
		return;
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		DialogueNameWidget = CreateWidget<UNPCDialogueNameWidget>(
			PlayerController,
			UNPCDialogueNameWidget::StaticClass());
		if (DialogueNameWidget)
		{
			DialogueNameWidget->SetNPCName(FText::FromString(TEXT("Mordred")));
			DialogueNameWidget->AddToViewport(60);
		}
	}
}

void AFinalBossMordred::HideMordredDialogueName()
{
	if (DialogueNameWidget)
	{
		DialogueNameWidget->RemoveFromParent();
		DialogueNameWidget = nullptr;
	}
}

void AFinalBossMordred::SetPlayerMovementEnabled(const bool bEnabled) const
{
	ACharacter* Player = ResolvePlayerCharacter();
	if (ATFGCharacter* TFGPlayer = Cast<ATFGCharacter>(Player))
	{
		TFGPlayer->MovimientoDesactivado = !bEnabled;
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		PlayerController->SetIgnoreMoveInput(!bEnabled);
		PlayerController->SetIgnoreLookInput(!bEnabled);
	}
}

void AFinalBossMordred::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!EstaCombateActivo() || bActionLocked || GetCharacterMovement()->IsFalling())
	{
		return;
	}

	TimeUntilNextAction -= DeltaSeconds;
	if (TimeUntilNextAction <= 0.0f)
	{
		EvaluateCombatAction();
		TimeUntilNextAction = FMath::FRandRange(MinActionInterval, MaxActionInterval);
	}
}

void AFinalBossMordred::EvaluateCombatAction()
{
	ACharacter* Player = ResolvePlayerCharacter();
	if (!Player)
	{
		return;
	}

	FacePlayerInstantly();
	const float Distance = FVector::Dist2D(GetActorLocation(), Player->GetActorLocation());
	const float Roll = FMath::FRand();
	if (Distance <= 260.0f)
	{
		Roll < 0.48f ? PerformSwordAttack() : PerformKickAttack();
	}
	else if (Distance <= 700.0f)
	{
		if (Roll < 0.30f)
		{
			PerformDash();
		}
		else if (Roll < 0.60f)
		{
			PerformOffensiveLeap();
		}
		else
		{
			PerformProjectileAttack();
		}
	}
	else
	{
		Roll < 0.62f ? PerformProjectileAttack() : PerformDash();
	}
}

void AFinalBossMordred::PerformSwordAttack()
{
	BeginAction(0.95f);
	FacePlayerInstantly();
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Play(SwordAttackMontage, 1.5f);
	}
	FTimerDelegate Impact;
	Impact.BindUObject(this, &AFinalBossMordred::ResolveMeleeImpact, false);
	GetWorldTimerManager().SetTimer(ImpactTimer, Impact, 0.34f, false);
	UE_LOG(LogMordredBoss, Display, TEXT("[MORDRED] Ataque de espada."));
}

void AFinalBossMordred::PerformKickAttack()
{
	BeginAction(0.95f);
	FacePlayerInstantly();
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Play(KickAttackMontage, 1.5f);
	}
	FTimerDelegate Impact;
	Impact.BindUObject(this, &AFinalBossMordred::ResolveMeleeImpact, true);
	GetWorldTimerManager().SetTimer(ImpactTimer, Impact, 0.34f, false);
	UE_LOG(LogMordredBoss, Display, TEXT("[MORDRED] Patada preparada con empuje."));
}

void AFinalBossMordred::ResolveMeleeImpact(const bool bKick)
{
	PlayCombatSound(
		AttackSound,
		bKick ? 0.78f : 0.9f,
		bKick ? TEXT("patada") : TEXT("espada"));
	ACharacter* Player = ResolvePlayerCharacter();
	if (Player)
	{
		const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
		const bool bInsideMeleeArc = ToPlayer.Size2D() <= 285.0f &&
			FVector::DotProduct(GetActorForwardVector(), ToPlayer.GetSafeNormal2D()) >= 0.15f;
		if (bInsideMeleeArc)
		{

	const FVector Direction = ToPlayer.GetSafeNormal2D();
	const FVector Launch = bKick
		? Direction * KickPushStrength + FVector::UpVector * 260.0f
		: Direction * 220.0f;
	DealDamageToPlayer(bKick ? KickDamage : SwordDamage, Launch);
	PlayCombatSound(ImpactSound, bKick ? 0.84f : 0.96f, TEXT("impacto real"));
	UE_LOG(
		LogMordredBoss,
		Display,
		TEXT("[MORDRED] %s impacto al jugador por %.1f."),
		bKick ? TEXT("Patada") : TEXT("Espada"),
			bKick ? KickDamage : SwordDamage);
		}
	}
}

void AFinalBossMordred::PerformDash()
{
	ACharacter* Player = ResolvePlayerCharacter();
	if (!Player)
	{
		return;
	}
	BeginAction(0.62f);
	FacePlayerInstantly();
	const FVector Direction = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	LaunchCharacter(Direction * DashStrength, true, false);
	GetWorldTimerManager().SetTimer(
		ImpactTimer,
		this,
		&AFinalBossMordred::ResolveDashImpact,
		0.28f,
		false);
	UE_LOG(LogMordredBoss, Display, TEXT("[MORDRED] Dash hacia el jugador."));
}

void AFinalBossMordred::ResolveDashImpact()
{
	ACharacter* Player = ResolvePlayerCharacter();
	if (Player && FVector::Dist2D(GetActorLocation(), Player->GetActorLocation()) <= 250.0f)
	{
		const FVector Direction = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		DealDamageToPlayer(DashDamage, Direction * 650.0f + FVector::UpVector * 150.0f);
	}
}

void AFinalBossMordred::PerformProjectileAttack()
{
	ACharacter* Player = ResolvePlayerCharacter();
	if (!Player || !ProjectileClass)
	{
		return;
	}
	BeginAction(0.85f);
	FacePlayerInstantly();
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (ProjectileLaunchAnimation)
		{
			AnimInstance->PlaySlotAnimationAsDynamicMontage(
				ProjectileLaunchAnimation,
				TEXT("DefaultSlot"),
				0.06f,
				0.12f,
				1.65f,
				1,
				0.0f,
				0.0f);
		}
	}
	GetWorldTimerManager().SetTimer(
		ProjectileSpawnTimer,
		this,
		&AFinalBossMordred::SpawnPreparedProjectile,
		0.22f,
		false);
	UE_LOG(LogMordredBoss, Display, TEXT("[MORDRED] Animacion de lanzamiento iniciada."));
}

void AFinalBossMordred::SpawnPreparedProjectile()
{
	ACharacter* Player = ResolvePlayerCharacter();
	if (!Player || !ProjectileClass || bDead)
	{
		return;
	}

	const FVector Direction =
		(Player->GetActorLocation() + FVector(0.0f, 0.0f, 45.0f) -
		 (GetActorLocation() + FVector(0.0f, 0.0f, 80.0f))).GetSafeNormal();
	const FVector SpawnLocation =
		GetActorLocation() + FVector(0.0f, 0.0f, 80.0f) + Direction * 145.0f;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	GetWorld()->SpawnActor<AActor>(
		ProjectileClass,
		SpawnLocation,
		Direction.Rotation(),
		Params);
	PlayCombatSound(ProjectileLaunchSound, 0.68f, TEXT("proyectil"));
	UE_LOG(LogMordredBoss, Display, TEXT("[MORDRED] Proyectil lanzado."));
}

void AFinalBossMordred::PerformOffensiveLeap()
{
	ACharacter* Player = ResolvePlayerCharacter();
	if (!Player || GetCharacterMovement()->IsFalling())
	{
		return;
	}
	BeginAction(2.4f);
	bOffensiveLeapPending = true;
	FacePlayerInstantly();
	const FVector Direction = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	LaunchCharacter(Direction * 760.0f + FVector::UpVector * 880.0f, true, true);
	UE_LOG(LogMordredBoss, Display, TEXT("[MORDRED] Salto ofensivo iniciado."));
}

void AFinalBossMordred::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (!bOffensiveLeapPending)
	{
		return;
	}

	bOffensiveLeapPending = false;
	ACharacter* Player = ResolvePlayerCharacter();
	if (Player && FVector::Dist2D(GetActorLocation(), Player->GetActorLocation()) <= LeapRadius)
	{
		const FVector Direction = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		DealDamageToPlayer(LeapDamage, Direction * 850.0f + FVector::UpVector * 320.0f);
	}
	UnlockAction();
	UE_LOG(LogMordredBoss, Display, TEXT("[MORDRED] Impacto de salto en radio %.0f."), LeapRadius);
}

void AFinalBossMordred::BeginAction(const float Duration)
{
	bActionLocked = true;
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
	GetWorldTimerManager().ClearTimer(ActionUnlockTimer);
	GetWorldTimerManager().SetTimer(
		ActionUnlockTimer,
		this,
		&AFinalBossMordred::UnlockAction,
		Duration,
		false);
}

void AFinalBossMordred::UnlockAction()
{
	if (!bDead)
	{
		bActionLocked = false;
	}
}

ACharacter* AFinalBossMordred::ResolvePlayerCharacter() const
{
	ACharacter* Player = nullptr;
	if (CachedPlayer.IsValid())
	{
		Player = CachedPlayer.Get();
	}
	else
	{
		Player = Cast<ACharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	}
	return Player;
}

void AFinalBossMordred::FacePlayerInstantly()
{
	if (ACharacter* Player = ResolvePlayerCharacter())
	{
		const FVector Direction = Player->GetActorLocation() - GetActorLocation();
		SetActorRotation(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f));
	}
}

void AFinalBossMordred::DealDamageToPlayer(
	const float Damage,
	const FVector& LaunchVelocity)
{
	ACharacter* Player = ResolvePlayerCharacter();
	if (!Player)
	{
		return;
	}
	if (ATFGCharacter* TFGPlayer = Cast<ATFGCharacter>(Player))
	{
		TFGPlayer->RegistrarFuenteDanyo(this);
	}
	MordredBossHelpers::InvokeNumeric(Player, TEXT("RecibirDanyo"), Damage);
	if (!LaunchVelocity.IsNearlyZero())
	{
		Player->LaunchCharacter(LaunchVelocity, true, false);
	}
}

void AFinalBossMordred::RecibirDanyo(const float DanyoARecibir)
{
	if (!EstaCombateActivo() || DanyoARecibir <= 0.0f)
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DanyoARecibir);
	PlayCombatSound(HitReactionSound, 0.74f, TEXT("reaccion al dano"));
	UpdateBossHealthBar();
	UE_LOG(
		LogMordredBoss,
		Display,
		TEXT("[MORDRED] Recibe %.1f de dano. Vida %.1f / %.1f."),
		DanyoARecibir,
		CurrentHealth,
		MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		HandleDeath();
	}
	else if (!bActionLocked)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Play(HitMontage, 2.0f);
		}
	}
}

void AFinalBossMordred::PlayCombatSound(
	USoundBase* Sound,
	const float Pitch,
	const TCHAR* EventName) const
{
	if (!Sound || !GetWorld())
	{
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(
		this,
		Sound,
		GetActorLocation(),
		0.78f,
		Pitch);
	UE_LOG(LogMordredBoss, Verbose,
		TEXT("[AUDIO MORDRED] %s sincronizado."), EventName);
}

void AFinalBossMordred::UpdateBossHealthBar()
{
	if (BossHealthWidget)
	{
		BossHealthWidget->SetBossHealth(CurrentHealth, MaxHealth);
	}
}

void AFinalBossMordred::HandleDeath()
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	bCombatActive = false;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UMusicManagerSubsystem* Music =
			GameInstance->GetSubsystem<UMusicManagerSubsystem>())
		{
			Music->SetMordredCombatActive(false);
		}
	}
	bActionLocked = true;
	bOffensiveLeapPending = false;
	GetWorldTimerManager().ClearTimer(ActionUnlockTimer);
	GetWorldTimerManager().ClearTimer(ImpactTimer);
	GetWorldTimerManager().ClearTimer(ProjectileSpawnTimer);

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
		AI->ClearFocus(EAIFocusPriority::Gameplay);
	}
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (DeathAnimation)
		{
			AnimInstance->PlaySlotAnimationAsDynamicMontage(
				DeathAnimation,
				TEXT("DefaultSlot"),
				0.15f,
				0.25f,
				1.0f);
		}
	}
	if (BossHealthWidget)
	{
		BossHealthWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	const float DelayEpilogo = DeathAnimation
		? FMath::Clamp(DeathAnimation->GetPlayLength() * 0.72f, 1.25f, 3.25f)
		: 1.5f;
	GetWorldTimerManager().SetTimer(
		VictoryStartTimer,
		this,
		&AFinalBossMordred::StartVictoryDialogue,
		DelayEpilogo,
		false);
	UE_LOG(LogMordredBoss, Display,
		TEXT("[MORDRED] Boss derrotado. Epilogo en %.2f s."), DelayEpilogo);
}

void AFinalBossMordred::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(IntroStartTimer);
	GetWorldTimerManager().ClearTimer(ActionUnlockTimer);
	GetWorldTimerManager().ClearTimer(ImpactTimer);
	GetWorldTimerManager().ClearTimer(ProjectileSpawnTimer);
	GetWorldTimerManager().ClearTimer(VictoryStartTimer);
	UnbindDialogueInput();
	if (bIntroActive || bVictoryDialogueActive)
	{
		HideDialogue();
		SetPlayerMovementEnabled(true);
		if (ATFGCharacter* Player = Cast<ATFGCharacter>(ResolvePlayerCharacter()))
		{
			Player->EstablecerDialogoActivo(false);
		}
	}
	HideMordredDialogueName();
	if (BossHealthWidget)
	{
		BossHealthWidget->RemoveFromParent();
		BossHealthWidget = nullptr;
	}
	if (VictoryDialogueWidget)
	{
		VictoryDialogueWidget->RemoveFromParent();
		VictoryDialogueWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

AMordredBossAIController::AMordredBossAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bAttachToPawn = true;
}

void AMordredBossAIController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AFinalBossMordred* Boss = Cast<AFinalBossMordred>(GetPawn());
	ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Boss || !Player || !Boss->EstaCombateActivo())
	{
		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
	}
	else
	{
		SetFocus(Player);
		if (Boss->EstaEjecutandoAccion())
		{
			StopMovement();
		}
		else
		{
			const float Distance = FVector::Dist2D(Boss->GetActorLocation(), Player->GetActorLocation());
			if (Distance > 235.0f)
			{
				MoveToActor(Player, 205.0f, true, true, true, nullptr, true);
			}
			else
			{
				StopMovement();
			}
		}
	}
}
