#include "EnemyResourcePickup.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "TFGCharacter.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnemyCollectibles, Log, All);

AEnemyResourcePickup::AEnemyResourcePickup()
{
	PrimaryActorTick.bCanEverTick = true;

	CollectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollectionSphere"));
	SetRootComponent(CollectionSphere);
	CollectionSphere->InitSphereRadius(CollectionRadius);
	CollectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollectionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollectionSphere->SetGenerateOverlapEvents(true);
	CollectionSphere->OnComponentBeginOverlap.AddDynamic(
		this,
		&AEnemyResourcePickup::OnCollectionOverlap);

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CollectibleCore"));
	CoreMesh->SetupAttachment(CollectionSphere);
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreMesh->SetRelativeScale3D(FVector(0.22f));
	CoreMesh->SetCastShadow(false);

	MainParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("MainParticles"));
	MainParticles->SetupAttachment(CollectionSphere);
	MainParticles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MainParticles->bAutoActivate = true;

	AccentParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("AccentParticles"));
	AccentParticles->SetupAttachment(CollectionSphere);
	AccentParticles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AccentParticles->bAutoActivate = true;

	ColoredLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CollectibleLight"));
	ColoredLight->SetupAttachment(CollectionSphere);
	ColoredLight->SetIntensity(1450.0f);
	ColoredLight->SetAttenuationRadius(190.0f);
	ColoredLight->SetCastShadows(false);

	TypeLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CollectibleTypeLabel"));
	TypeLabel->SetupAttachment(CollectionSphere);
	TypeLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 42.0f));
	TypeLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	TypeLabel->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	TypeLabel->SetWorldSize(17.0f);
	TypeLabel->SetCastShadow(false);
	TypeLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		CoreMesh->SetStaticMesh(SphereMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicShapeMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicShapeMaterial.Succeeded())
	{
		ColorableMaterial = BasicShapeMaterial.Object;
		CoreMesh->SetMaterial(0, ColorableMaterial);
	}

	static ConstructorHelpers::FObjectFinder<UParticleSystem> DungeonParticles(
		TEXT("/Game/MedievalDungeon/Particles/P_Pit_Fire.P_Pit_Fire"));
	if (DungeonParticles.Succeeded())
	{
		MainParticles->SetTemplate(DungeonParticles.Object);
		AccentParticles->SetTemplate(DungeonParticles.Object);
	}
}

void AEnemyResourcePickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ConfigureVisuals();
}

void AEnemyResourcePickup::BeginPlay()
{
	Super::BeginPlay();
	bCollectionEnabled = false;
	CollectionSphere->SetGenerateOverlapEvents(false);
	CollectionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollectionSphere->SetSphereRadius(CollectionRadius, true);
	SnapToGround();
	RestLocation = GetActorLocation();
	ConfigureVisuals();

	if (CollectionDelay <= KINDA_SMALL_NUMBER)
	{
		EnableCollection();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			CollectionEnableTimer,
			this,
			&AEnemyResourcePickup::EnableCollection,
			CollectionDelay,
			false);
	}
	UE_LOG(
		LogEnemyCollectibles,
		Display,
		TEXT("[DROP] Recolectable %s visible en %s; se podra recoger en %.2f s."),
		TypeToString(CollectibleType),
		*GetActorLocation().ToCompactString(),
		FMath::Max(0.0f, CollectionDelay));
}

void AEnemyResourcePickup::EnableCollection()
{
	if (bCollected || !IsValid(CollectionSphere))
	{
		return;
	}

	bCollectionEnabled = true;
	CollectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollectionSphere->SetGenerateOverlapEvents(true);
	CollectionSphere->UpdateOverlaps();

	UE_LOG(
		LogEnemyCollectibles,
		Display,
		TEXT("[DROP] %s ya puede recogerse."),
		TypeToString(CollectibleType));
}

void AEnemyResourcePickup::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bCollected)
	{
		return;
	}

	RunningTime += DeltaSeconds;
	FVector NewLocation = RestLocation;
	NewLocation.Z += FMath::Sin(RunningTime * BobSpeed) * BobHeight;
	SetActorLocation(NewLocation, false);

	const float RotationSpeed =
		CollectibleType == EEnemyCollectibleType::Energia ? 125.0f :
		CollectibleType == EEnemyCollectibleType::Mana ? 85.0f : 55.0f;
	AddActorLocalRotation(FRotator(0.0f, RotationSpeed * DeltaSeconds, 0.0f));
	FaceLabelToCamera();
}

void AEnemyResourcePickup::InitializeCollectible(
	const EEnemyCollectibleType NewType)
{
	CollectibleType = NewType;
	ConfigureVisuals();
}

AEnemyResourcePickup* AEnemyResourcePickup::SpawnRandomDrop(
	AActor* DefeatedEnemy,
	const float DropChance)
{
	if (!IsValid(DefeatedEnemy) || !DefeatedEnemy->GetWorld())
	{
		return nullptr;
	}

	const float ClampedChance = FMath::Clamp(DropChance, 0.0f, 1.0f);
	const float Roll = FMath::FRand();
	if (Roll >= ClampedChance)
	{
		UE_LOG(
			LogEnemyCollectibles,
			Display,
			TEXT("[DROP] %s no genero objeto (tirada %.3f, probabilidad %.0f%%)."),
			*DefeatedEnemy->GetName(),
			Roll,
			ClampedChance * 100.0f);
		return nullptr;
	}

	const EEnemyCollectibleType RandomType =
		static_cast<EEnemyCollectibleType>(FMath::RandRange(0, 3));
	FTransform SpawnTransform = DefeatedEnemy->GetActorTransform();
	SpawnTransform.SetScale3D(FVector::OneVector);
	SpawnTransform.AddToTranslation(FVector(0.0f, 0.0f, 20.0f));

	AEnemyResourcePickup* Pickup =
		DefeatedEnemy->GetWorld()->SpawnActorDeferred<AEnemyResourcePickup>(
			StaticClass(),
			SpawnTransform,
			DefeatedEnemy,
			DefeatedEnemy->GetInstigator(),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		return nullptr;
	}

	Pickup->InitializeCollectible(RandomType);
	UGameplayStatics::FinishSpawningActor(Pickup, SpawnTransform);
	UE_LOG(
		LogEnemyCollectibles,
		Display,
		TEXT("[DROP] %s genero %s (tirada %.3f, probabilidad %.0f%%)."),
		*DefeatedEnemy->GetName(),
		TypeToString(RandomType),
		Roll,
		ClampedChance * 100.0f);
	return Pickup;
}

const TCHAR* AEnemyResourcePickup::TypeToString(
	const EEnemyCollectibleType Type)
{
	switch (Type)
	{
	case EEnemyCollectibleType::Luz:
		return TEXT("LUZ");
	case EEnemyCollectibleType::Vida:
		return TEXT("VIDA");
	case EEnemyCollectibleType::Energia:
		return TEXT("ENERGIA");
	case EEnemyCollectibleType::Mana:
		return TEXT("MANA");
	default:
		return TEXT("DESCONOCIDO");
	}
}

void AEnemyResourcePickup::OnCollectionOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	ATFGCharacter* PlayerCharacter = Cast<ATFGCharacter>(OtherActor);
	if (bCollected || !bCollectionEnabled || !PlayerCharacter)
	{
		return;
	}

	bCollected = true;
	CollectionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayerCharacter->GuardarRecolectable(CollectibleType, 1);

	UE_LOG(
		LogEnemyCollectibles,
		Display,
		TEXT("[RECOGIDA] %s recogio %s al pasar por encima. Cantidad=%d."),
		*PlayerCharacter->GetName(),
		TypeToString(CollectibleType),
		PlayerCharacter->ObtenerCantidadRecolectable(CollectibleType));

	if (GEngine)
	{
		const int32 TypeAmount =
			PlayerCharacter->ObtenerCantidadRecolectable(CollectibleType);
		const int32 TotalAmount =
			PlayerCharacter->ObtenerTotalRecolectables();
		GEngine->AddOnScreenDebugMessage(
			-1,
			4.5f,
			GetTypeColor().ToFColor(true),
			FString::Printf(
				TEXT("RECOGIDO: +1 %s  |  Tienes: %d  |  Total: %d"),
				TypeToString(CollectibleType),
				TypeAmount,
				TotalAmount));
	}

	SetActorHiddenInGame(true);
	Destroy();
}

void AEnemyResourcePickup::ConfigureVisuals()
{
	const FLinearColor TypeColor = GetTypeColor();
	if (ColoredLight)
	{
		ColoredLight->SetLightColor(TypeColor);
	}
	if (TypeLabel)
	{
		TypeLabel->SetText(FText::FromString(TypeToString(CollectibleType)));
		TypeLabel->SetTextRenderColor(TypeColor.ToFColor(true));
	}

	if (CoreMesh && ColorableMaterial)
	{
		UMaterialInstanceDynamic* DynamicMaterial =
			CoreMesh->CreateDynamicMaterialInstance(0, ColorableMaterial);
		if (DynamicMaterial)
		{
			DynamicMaterial->SetVectorParameterValue(TEXT("Color"), TypeColor);
			DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), TypeColor);
		}
	}

	const float MainScale =
		CollectibleType == EEnemyCollectibleType::Luz ? 0.15f :
		CollectibleType == EEnemyCollectibleType::Vida ? 0.12f :
		CollectibleType == EEnemyCollectibleType::Energia ? 0.09f : 0.17f;
	const float AccentScale =
		CollectibleType == EEnemyCollectibleType::Luz ? 0.07f :
		CollectibleType == EEnemyCollectibleType::Vida ? 0.10f :
		CollectibleType == EEnemyCollectibleType::Energia ? 0.065f : 0.12f;

	if (MainParticles)
	{
		MainParticles->SetRelativeScale3D(FVector(MainScale));
		MainParticles->CustomTimeDilation =
			CollectibleType == EEnemyCollectibleType::Energia ? 1.45f :
			CollectibleType == EEnemyCollectibleType::Mana ? 0.65f : 1.0f;
		MainParticles->SetColorParameter(TEXT("Color"), TypeColor);
		MainParticles->SetColorParameter(TEXT("ParticleColor"), TypeColor);
	}
	if (AccentParticles)
	{
		AccentParticles->SetRelativeScale3D(FVector(AccentScale));
		AccentParticles->SetRelativeLocation(
			CollectibleType == EEnemyCollectibleType::Energia
				? FVector(18.0f, 0.0f, -5.0f)
				: FVector(0.0f, 0.0f, 18.0f));
		AccentParticles->SetRelativeRotation(
			CollectibleType == EEnemyCollectibleType::Mana
				? FRotator(0.0f, 90.0f, 180.0f)
				: FRotator::ZeroRotator);
		AccentParticles->CustomTimeDilation =
			CollectibleType == EEnemyCollectibleType::Energia ? 1.8f :
			CollectibleType == EEnemyCollectibleType::Luz ? 0.75f : 1.15f;
		AccentParticles->SetColorParameter(TEXT("Color"), TypeColor);
		AccentParticles->SetColorParameter(TEXT("ParticleColor"), TypeColor);
	}

	if (CoreMesh)
	{
		const float CoreScale =
			CollectibleType == EEnemyCollectibleType::Luz ? 0.26f :
			CollectibleType == EEnemyCollectibleType::Vida ? 0.21f :
			CollectibleType == EEnemyCollectibleType::Energia ? 0.18f : 0.23f;
		CoreMesh->SetRelativeScale3D(FVector(CoreScale));
	}
}

FLinearColor AEnemyResourcePickup::GetTypeColor() const
{
	switch (CollectibleType)
	{
	case EEnemyCollectibleType::Luz:
		return FLinearColor(1.0f, 0.78f, 0.08f, 1.0f);
	case EEnemyCollectibleType::Vida:
		return FLinearColor(1.0f, 0.025f, 0.035f, 1.0f);
	case EEnemyCollectibleType::Energia:
		return FLinearColor(0.04f, 1.0f, 0.18f, 1.0f);
	case EEnemyCollectibleType::Mana:
		return FLinearColor(0.12f, 0.28f, 1.0f, 1.0f);
	default:
		return FLinearColor::White;
	}
}

void AEnemyResourcePickup::SnapToGround()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector TraceStart = CurrentLocation + FVector(0.0f, 0.0f, 260.0f);
	const FVector TraceEnd = CurrentLocation - FVector(0.0f, 0.0f, 1200.0f);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CollectibleGroundTrace), false);
	QueryParams.AddIgnoredActor(this);
	if (AActor* OwningEnemy = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwningEnemy);
	}

	FHitResult GroundHit;
	if (World->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams))
	{
		FVector GroundedLocation = CurrentLocation;
		GroundedLocation.Z = GroundHit.ImpactPoint.Z + GroundOffset;
		SetActorLocation(GroundedLocation, false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(
			LogEnemyCollectibles,
			Display,
			TEXT("[DROP] %s colocado en el suelo: Z %.1f (suelo %.1f)."),
			TypeToString(CollectibleType),
			GroundedLocation.Z,
			GroundHit.ImpactPoint.Z);
	}
	else
	{
		UE_LOG(
			LogEnemyCollectibles,
			Warning,
			TEXT("[DROP] No se encontro suelo bajo %s; se conserva la posicion %s."),
			TypeToString(CollectibleType),
			*CurrentLocation.ToCompactString());
	}
}

void AEnemyResourcePickup::FaceLabelToCamera() const
{
	if (!TypeLabel)
	{
		return;
	}

	const APlayerCameraManager* CameraManager =
		UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager)
	{
		return;
	}

	FRotator LookRotation =
		(CameraManager->GetCameraLocation() - TypeLabel->GetComponentLocation()).Rotation();
	LookRotation.Roll = 0.0f;
	TypeLabel->SetWorldRotation(LookRotation);
}
