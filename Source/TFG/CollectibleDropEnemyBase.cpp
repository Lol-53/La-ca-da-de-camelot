#include "CollectibleDropEnemyBase.h"

#include "EnemyArcherProjectile.h"
#include "EnemyResourcePickup.h"
#include "Systems/MusicManagerSubsystem.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCollectibleDropEnemy, Log, All);

ACollectibleDropEnemyBase::ACollectibleDropEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<USoundBase> SwordSound(
		TEXT("/Game/MyContent/Audio/Sounds/sword.sword"));
	static ConstructorHelpers::FObjectFinder<USoundBase> FireballSound(
		TEXT("/Game/MyContent/Audio/Sounds/floraphonic-fireball-whoosh-1-179125.floraphonic-fireball-whoosh-1-179125"));
	if (SwordSound.Succeeded())
	{
		CombatSound = SwordSound.Object;
	}
	if (FireballSound.Succeeded())
	{
		ProjectileSound = FireballSound.Object;
	}
}

void ACollectibleDropEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	if (IsGreystoneTank())
	{
		ConfigureGreystoneTankAnimation();
	}
	else if (IsCentaurArcher())
	{
		ConfigureCentaurArcher();
	}
}

void ACollectibleDropEnemyBase::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AccelerateActiveHitReaction();
	if (IsGreystoneTank())
	{
		UpdateGreystoneTankAnimation();
	}
	else if (IsCentaurArcher())
	{
		UpdateCentaurArcher(DeltaSeconds);
	}
	if (!bDropResolved && IsMarkedDead())
	{
		ResolveCollectibleDrop(false);
	}
}

void ACollectibleDropEnemyBase::AccelerateActiveHitReaction() const
{
	if (IsMarkedDead() || HitReactionPlayRate <= 1.0f || !GetMesh())
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	UAnimMontage* ActiveMontage = AnimInstance
		? AnimInstance->GetCurrentActiveMontage()
		: nullptr;
	if (ActiveMontage &&
		ActiveMontage->GetName().Contains(TEXT("Hit"), ESearchCase::IgnoreCase))
	{
		AnimInstance->Montage_SetPlayRate(ActiveMontage, HitReactionPlayRate);
	}
}

bool ACollectibleDropEnemyBase::IsCentaurArcher() const
{
	return ActorHasTag(TEXT("CentaurArcher")) ||
		GetClass()->GetName().Contains(TEXT("ArqueroCentauro"), ESearchCase::IgnoreCase);
}

void ACollectibleDropEnemyBase::ConfigureCentaurArcher()
{
	ArcherPreviousLocation = GetActorLocation();
	Tags.AddUnique(TEXT("CentaurArcher"));
	ArcherIdleAnimation = LoadObject<UAnimSequenceBase>(nullptr,
		TEXT("/Game/QuadrapedCreatures/Centaur/Animations/ANIM_Centaur_IdleBow.ANIM_Centaur_IdleBow"));
	ArcherWalkAnimation = LoadObject<UAnimSequenceBase>(nullptr,
		TEXT("/Game/QuadrapedCreatures/Centaur/Animations/ANIM_Centaur_WalkBow.ANIM_Centaur_WalkBow"));
	ArcherShootAnimation = LoadObject<UAnimSequenceBase>(nullptr,
		TEXT("/Game/QuadrapedCreatures/Centaur/Animations/ANIM_Centaur_ShootArrowToIdleAiming.ANIM_Centaur_ShootArrowToIdleAiming"));
	ArcherDeathAnimation = LoadObject<UAnimSequenceBase>(nullptr,
		TEXT("/Game/QuadrapedCreatures/Centaur/Animations/ANIM_Centaur_DeathBow.ANIM_Centaur_DeathBow"));

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	USkeletalMesh* CentaurMesh = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/QuadrapedCreatures/Centaur/Meshes/SK_Centaur.SK_Centaur"));
	if (CharacterMesh && CentaurMesh)
	{
		// The source centaur is much larger than the rest of this project's
		// roster. Scale the visual and collision together so hits still match.
		GetCapsuleComponent()->SetCapsuleSize(58.0f, 92.0f);
		CharacterMesh->SetSkeletalMeshAsset(CentaurMesh);
		CharacterMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -92.0f));
		// Centaur assets face local +Y, while ACharacter and its AI use +X as
		// forward. Rotate only the visual so it looks along the actor's aim.
		CharacterMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		CharacterMesh->SetRelativeScale3D(FVector(0.62f));
		CharacterMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		CharacterMesh->PlayAnimation(ArcherIdleAnimation, true);

		auto AddLeaderPosePart = [this, CharacterMesh](const TCHAR* Name, const TCHAR* AssetPath)
			-> USkeletalMeshComponent*
		{
			USkeletalMesh* PartMesh = LoadObject<USkeletalMesh>(nullptr, AssetPath);
			if (!PartMesh)
			{
				return nullptr;
			}
			USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(this, FName(Name));
			Part->SetupAttachment(CharacterMesh);
			Part->SetSkeletalMeshAsset(PartMesh);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->RegisterComponent();
			Part->SetLeaderPoseComponent(CharacterMesh);
			ArcherVisualParts.Add(Part);
			return Part;
		};
		AddLeaderPosePart(TEXT("CentaurBodyArmor"),
			TEXT("/Game/QuadrapedCreatures/Centaur/Meshes/SK_Body_Armor.SK_Body_Armor"));
		AddLeaderPosePart(TEXT("CentaurShoulderPads"),
			TEXT("/Game/QuadrapedCreatures/Centaur/Meshes/SK_Shoulder_Pads.SK_Shoulder_Pads"));
		AddLeaderPosePart(TEXT("CentaurBow"),
			TEXT("/Game/QuadrapedCreatures/Centaur/Meshes/SK_Bow_Action.SK_Bow_Action"));
		AddLeaderPosePart(TEXT("CentaurMane"),
			TEXT("/Game/QuadrapedCreatures/Centaur/Meshes/SK_Mane.SK_Mane"));
		ArcherNockedArrowVisual = AddLeaderPosePart(TEXT("CentaurNockedArrow"),
			TEXT("/Game/QuadrapedCreatures/Centaur/Meshes/SK_Arrow_Action.SK_Arrow_Action"));
		if (ArcherNockedArrowVisual)
		{
			ArcherNockedArrowVisual->SetVisibility(false, true);
		}
		bArcherLocomotionInitialized = true;
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = 315.0f;
		Movement->bOrientRotationToMovement = false;
	}
	NextArcherShotTime = GetWorld() ? GetWorld()->GetTimeSeconds() + 1.0 : 1.0;
	UE_LOG(LogCollectibleDropEnemy, Display,
		TEXT("[ARQUERO] Centauro configurado: arco, idle, caminar, disparo y muerte."));
}

void ACollectibleDropEnemyBase::UpdateCentaurArcher(const float DeltaSeconds)
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh || !GetWorld())
	{
		return;
	}

	AAIController* AI = Cast<AAIController>(GetController());
	if (AI && !bArcherBrainDisabled)
	{
		if (UBrainComponent* Brain = AI->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Centaur archer uses native ranged combat"));
		}
		bArcherBrainDisabled = true;
	}

	if (IsMarkedDead())
	{
		if (AI)
		{
			AI->StopMovement();
		}
		if (!bArcherDeathAnimationStarted && ArcherDeathAnimation)
		{
			GetWorldTimerManager().ClearTimer(ArcherReleaseTimer);
			GetWorldTimerManager().ClearTimer(ArcherShotEndTimer);
			bArcherAttackActive = false;
			bArcherDeathAnimationStarted = true;
			if (ArcherNockedArrowVisual)
			{
				ArcherNockedArrowVisual->SetVisibility(false, true);
			}
			CharacterMesh->PlayAnimation(ArcherDeathAnimation, false);
			UE_LOG(LogCollectibleDropEnemy, Display,
				TEXT("[ARQUERO] Animacion de muerte iniciada para %s."), *GetName());
		}
		return;
	}

	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!Player)
	{
		return;
	}

	FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
	ToPlayer.Z = 0.0f;
	const float Distance = ToPlayer.Size();
	const FVector DirectionToPlayer = ToPlayer.GetSafeNormal();
	if (!DirectionToPlayer.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, DirectionToPlayer.Rotation().Yaw, 0.0f));
	}

	if (!bArcherAttackActive && AI)
	{
		if (Distance > 1750.0f)
		{
			AI->MoveToActor(Player, 1250.0f, true, true, true);
		}
		else if (Distance < 625.0f)
		{
			const FVector RetreatTarget = GetActorLocation() - DirectionToPlayer * 650.0f;
			AI->MoveToLocation(RetreatTarget, 75.0f, true, true, true, false);
		}
		else
		{
			AI->StopMovement();
		}
	}

	if (!bArcherAttackActive && Distance <= 2200.0f && Distance >= 475.0f &&
		GetWorld()->GetTimeSeconds() >= NextArcherShotTime)
	{
		StartCentaurShot();
		return;
	}

	if (bArcherAttackActive)
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const bool bWalking = GetVelocity().SizeSquared2D() > FMath::Square(8.0f) ||
		FVector::DistSquared2D(CurrentLocation, ArcherPreviousLocation) > FMath::Square(0.5f);
	ArcherPreviousLocation = CurrentLocation;
	if (!bArcherLocomotionInitialized || bWalking != bArcherWasWalking)
	{
		if (UAnimSequenceBase* Animation = bWalking ? ArcherWalkAnimation : ArcherIdleAnimation)
		{
			CharacterMesh->SetPlayRate(bWalking ? 0.9f : 1.0f);
			CharacterMesh->PlayAnimation(Animation, true);
		}
		bArcherWasWalking = bWalking;
		bArcherLocomotionInitialized = true;
	}
}

void ACollectibleDropEnemyBase::StartCentaurShot()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UMusicManagerSubsystem* Music =
			GameInstance->GetSubsystem<UMusicManagerSubsystem>())
		{
			Music->NotifyCombatActivity();
		}
	}
	if (bArcherAttackActive || IsMarkedDead() || !ArcherShootAnimation || !GetMesh())
	{
		return;
	}
	bArcherAttackActive = true;
	bArcherLocomotionInitialized = false;
	if (ArcherNockedArrowVisual)
	{
		ArcherNockedArrowVisual->SetVisibility(true, true);
	}
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
	GetMesh()->SetPlayRate(1.0f);
	GetMesh()->PlayAnimation(ArcherShootAnimation, false);
	const float Duration = FMath::Max(0.8f, ArcherShootAnimation->GetPlayLength());
	// Source sequence: 36 frames at 30 fps. The string hand releases around
	// frame 10, so spawn on that exact visual beat rather than near frame 15.
	const float ReleaseTime = FMath::Min(Duration * 0.29f, 10.0f / 30.0f);
	GetWorldTimerManager().SetTimer(ArcherReleaseTimer, this,
		&ACollectibleDropEnemyBase::SpawnCentaurArrow, ReleaseTime, false);
	GetWorldTimerManager().SetTimer(ArcherShotEndTimer, this,
		&ACollectibleDropEnemyBase::FinishCentaurShot, Duration, false);
	UE_LOG(LogCollectibleDropEnemy, Display,
		TEXT("[ARQUERO] %s prepara un disparo."), *GetName());
}

void ACollectibleDropEnemyBase::SpawnCentaurArrow()
{
	if (IsMarkedDead() || !GetWorld())
	{
		return;
	}
	if (ArcherNockedArrowVisual)
	{
		ArcherNockedArrowVisual->SetVisibility(false, true);
	}
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!Player)
	{
		return;
	}
	// ArrowAction is the actual nocked-arrow bone from the centaur skeleton.
	// Using it keeps the projectile attached to the animated hand until release.
	FVector SpawnLocation = GetMesh()->GetSocketLocation(TEXT("ArrowAction"));
	if (SpawnLocation.IsNearlyZero())
	{
		SpawnLocation = GetActorLocation() +
			GetActorForwardVector() * 95.0f + FVector::UpVector * 82.0f;
	}
	SpawnLocation += GetActorForwardVector() * 24.0f;
	const FVector AimPoint = Player->GetActorLocation() + FVector::UpVector * 55.0f;
	const FRotator SpawnRotation = (AimPoint - SpawnLocation).Rotation();
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<AEnemyArcherProjectile>(
		AEnemyArcherProjectile::StaticClass(), SpawnLocation, SpawnRotation, Params);
	PlaySynchronizedCombatSound(ProjectileSound, 1.18f, TEXT("disparo de flecha"));
	UE_LOG(LogCollectibleDropEnemy, Display,
		TEXT("[ARQUERO] %s dispara una flecha hacia %s."),
		*GetName(), *GetNameSafe(Player));
}

void ACollectibleDropEnemyBase::FinishCentaurShot()
{
	if (ArcherNockedArrowVisual)
	{
		ArcherNockedArrowVisual->SetVisibility(false, true);
	}
	bArcherAttackActive = false;
	bArcherLocomotionInitialized = false;
	NextArcherShotTime = GetWorld() ? GetWorld()->GetTimeSeconds() + 2.15 : 2.15;
}

bool ACollectibleDropEnemyBase::IsGreystoneTank() const
{
	return ActorHasTag(TEXT("GreystoneTank")) ||
		GetClass()->GetName().Contains(TEXT("GreystoneTank"), ESearchCase::IgnoreCase);
}

void ACollectibleDropEnemyBase::ConfigureGreystoneTankAnimation()
{
	GreystonePreviousLocation = GetActorLocation();
	GreystoneIdleAnimation = LoadObject<UAnimSequenceBase>(
		nullptr,
		TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Idle.Idle"));
	GreystoneWalkAnimation = LoadObject<UAnimSequenceBase>(
		nullptr,
		TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Jog_Fwd.Jog_Fwd"));
	GreystoneAttackAnimation = LoadObject<UAnimSequenceBase>(
		nullptr,
		TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Attack_PrimaryA.Attack_PrimaryA"));
	GreystoneDeathAnimation = LoadObject<UAnimSequenceBase>(
		nullptr,
		TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Death.Death"));

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		// A single-node controller is intentional here. The package AnimBP binds
		// attack delegates to GreystonePlayerCharacter and leaves AI subclasses
		// sliding in its reference pose.
		CharacterMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		if (GreystoneIdleAnimation)
		{
			CharacterMesh->SetPlayRate(1.0f);
			CharacterMesh->PlayAnimation(GreystoneIdleAnimation, true);
			bGreystoneLocomotionInitialized = true;
		}
	}

	UE_LOG(LogCollectibleDropEnemy, Display,
		TEXT("[GREYSTONE TANQUE] Animaciones configuradas: idle=%s, caminar=%s, ataque=%s, muerte=%s."),
		GreystoneIdleAnimation ? TEXT("OK") : TEXT("ERROR"),
		GreystoneWalkAnimation ? TEXT("OK") : TEXT("ERROR"),
		GreystoneAttackAnimation ? TEXT("OK") : TEXT("ERROR"),
		GreystoneDeathAnimation ? TEXT("OK") : TEXT("ERROR"));
}

void ACollectibleDropEnemyBase::UpdateGreystoneTankAnimation()
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh)
	{
		return;
	}

	if (IsMarkedDead())
	{
		SetBlueprintAttackWindow(false);
		if (!bGreystoneDeathAnimationStarted && GreystoneDeathAnimation)
		{
			GetWorldTimerManager().ClearTimer(GreystoneAttackWindowTimer);
			GetWorldTimerManager().ClearTimer(GreystoneAttackEndTimer);
			bGreystoneAttackActive = false;
			bGreystoneDeathAnimationStarted = true;
			CharacterMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			CharacterMesh->SetPlayRate(1.0f);
			CharacterMesh->PlayAnimation(GreystoneDeathAnimation, false);
			UE_LOG(LogCollectibleDropEnemy, Display,
				TEXT("[GREYSTONE TANQUE] Animacion de derrota iniciada para %s."),
				*GetName());
		}
		return;
	}

	if (bGreystoneAttackActive)
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const float DistanceMovedSquared = FVector::DistSquared2D(
		CurrentLocation,
		GreystonePreviousLocation);
	GreystonePreviousLocation = CurrentLocation;
	const bool bIsWalking =
		GetVelocity().SizeSquared2D() > FMath::Square(8.0f) ||
		DistanceMovedSquared > FMath::Square(0.5f);
	if (!bGreystoneLocomotionInitialized || bIsWalking != bGreystoneWasWalking)
	{
		UAnimSequenceBase* LocomotionAnimation = bIsWalking
			? GreystoneWalkAnimation
			: GreystoneIdleAnimation;
		if (LocomotionAnimation)
		{
			// The slower playback gives the tank a heavy gait without changing
			// its actual navigation speed.
			CharacterMesh->SetPlayRate(bIsWalking ? 0.72f : 1.0f);
			CharacterMesh->PlayAnimation(LocomotionAnimation, true);
		}
		bGreystoneWasWalking = bIsWalking;
		bGreystoneLocomotionInitialized = true;
	}
}

void ACollectibleDropEnemyBase::ReproducirAtaqueGreystoneTanque()
{
	if (!IsGreystoneTank() || IsMarkedDead() || bGreystoneAttackActive ||
		!GreystoneAttackAnimation || !GetMesh())
	{
		return;
	}

	bGreystoneAttackActive = true;
	bGreystoneLocomotionInitialized = false;
	SetBlueprintAttackWindow(false);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	GetMesh()->SetPlayRate(0.82f);
	GetMesh()->PlayAnimation(GreystoneAttackAnimation, false);

	const float AttackDuration = FMath::Max(
		0.6f,
		GreystoneAttackAnimation->GetPlayLength() / 0.82f);
	GetWorldTimerManager().SetTimer(
		GreystoneAttackWindowTimer,
		this,
		&ACollectibleDropEnemyBase::AbrirVentanaAtaqueGreystone,
		AttackDuration * 0.28f,
		false);
	GetWorldTimerManager().SetTimer(
		GreystoneAttackEndTimer,
		this,
		&ACollectibleDropEnemyBase::FinalizarAtaqueGreystone,
		AttackDuration,
		false);

	UE_LOG(LogCollectibleDropEnemy, Display,
		TEXT("[GREYSTONE TANQUE] Ataque iniciado por %s (duracion %.2f s)."),
		*GetName(), AttackDuration);
}

void ACollectibleDropEnemyBase::EjecutarAtaqueIA()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UMusicManagerSubsystem* Music =
			GameInstance->GetSubsystem<UMusicManagerSubsystem>())
		{
			Music->NotifyCombatActivity();
		}
	}
	if (IsCentaurArcher())
	{
		StartCentaurShot();
		return;
	}
	if (IsGreystoneTank())
	{
		UE_LOG(LogCollectibleDropEnemy, Display,
			TEXT("[GREYSTONE TANQUE] Orden de ataque recibida desde el Behavior Tree por %s."),
			*GetName());
		ReproducirAtaqueGreystoneTanque();
		return;
	}

	// Existing enemies keep their Blueprint Atacar implementation. Calling it
	// by reflection lets the shared BT task work with both the original
	// Blueprint enemy and sibling enemy Blueprints such as Greystone.
	static const FName AttackFunctionName(TEXT("Atacar"));
	if (UFunction* AttackFunction = FindFunction(AttackFunctionName))
	{
		// The Blueprint starts its attack montage inside Atacar, so this marks
		// the same first animation frame for the weapon movement sound.
		PlaySynchronizedCombatSound(CombatSound, 1.0f, TEXT("ataque cuerpo a cuerpo"));
		ProcessEvent(AttackFunction, nullptr);
	}
	else
	{
		UE_LOG(LogCollectibleDropEnemy, Warning,
			TEXT("[ENEMIGO IA] %s no implementa la funcion Atacar."),
			*GetName());
	}
}

void ACollectibleDropEnemyBase::AbrirVentanaAtaqueGreystone()
{
	if (bGreystoneAttackActive && !IsMarkedDead())
	{
		PlaySynchronizedCombatSound(CombatSound, 0.72f, TEXT("ataque de Greystone"));
		SetBlueprintAttackWindow(true);
	}
}

void ACollectibleDropEnemyBase::PlaySynchronizedCombatSound(
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
		CombatSoundVolume,
		Pitch);
	UE_LOG(LogCollectibleDropEnemy, Verbose,
		TEXT("[AUDIO ENEMIGO] %s sincronizado para %s."), EventName, *GetName());
}

void ACollectibleDropEnemyBase::FinalizarAtaqueGreystone()
{
	SetBlueprintAttackWindow(false);
	bGreystoneAttackActive = false;
	bGreystoneLocomotionInitialized = false;
}

void ACollectibleDropEnemyBase::SetBlueprintAttackWindow(const bool bActive)
{
	if (FBoolProperty* AttackProperty =
		FindFProperty<FBoolProperty>(GetClass(), TEXT("EstoyAtacando")))
	{
		AttackProperty->SetPropertyValue_InContainer(this, bActive);
	}
}

bool ACollectibleDropEnemyBase::ResolveCollectibleDrop(const bool bForceDrop)
{
	if (bDropResolved)
	{
		return false;
	}

	bDropResolved = true;
	const float Chance = bForceDrop ? 1.0f : CollectibleDropChance;
	const bool bSpawned =
		AEnemyResourcePickup::SpawnRandomDrop(this, Chance) != nullptr;
	UE_LOG(
		LogCollectibleDropEnemy,
		Display,
		TEXT("[ENEMIGO DROP] %s resolvio su drop una sola vez: %s."),
		*GetName(),
		bSpawned ? TEXT("OBJETO CREADO") : TEXT("SIN OBJETO"));
	return bSpawned;
}

bool ACollectibleDropEnemyBase::IsMarkedDead() const
{
	const FBoolProperty* DeathProperty =
		FindFProperty<FBoolProperty>(GetClass(), DeathFlagName);
	return DeathProperty &&
		DeathProperty->GetPropertyValue_InContainer(this);
}
