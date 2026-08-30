#include "PermanentUpgradeShrine.h"

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "UI/PermanentUpgradeWidget.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPermanentUpgradeShrine, Log, All);

APermanentUpgradeShrine::APermanentUpgradeShrine()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	ShrineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShrineMesh"));
	ShrineMesh->SetupAttachment(SceneRoot);
	ShrineMesh->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShrineMeshAsset(
		TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Gargoyle_Statue_On_Stand.SM_Gargoyle_Statue_On_Stand"));
	if (ShrineMeshAsset.Succeeded())
	{
		ShrineMesh->SetStaticMesh(ShrineMeshAsset.Object);
	}

	LightParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("LightParticles"));
	LightParticles->SetupAttachment(SceneRoot);
	LightParticles->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	LightParticles->SetRelativeScale3D(FVector(0.55f));
	static ConstructorHelpers::FObjectFinder<UParticleSystem> ParticleAsset(
		TEXT("/Game/MedievalDungeon/Particles/P_Pit_Fire.P_Pit_Fire"));
	if (ParticleAsset.Succeeded())
	{
		LightParticles->SetTemplate(ParticleAsset.Object);
	}

	GoldenLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GoldenLight"));
	GoldenLight->SetupAttachment(SceneRoot);
	GoldenLight->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	GoldenLight->SetIntensity(2200.0f);
	GoldenLight->SetAttenuationRadius(500.0f);
	GoldenLight->SetLightColor(FLinearColor(1.0f, 0.42f, 0.08f));
	GoldenLight->SetCastShadows(true);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->SetSphereRadius(220.0f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &APermanentUpgradeShrine::OnInteractionBegin);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &APermanentUpgradeShrine::OnInteractionEnd);

	InteractionPrompt = CreateDefaultSubobject<UTextRenderComponent>(TEXT("InteractionPrompt"));
	InteractionPrompt->SetupAttachment(SceneRoot);
	InteractionPrompt->SetRelativeLocation(FVector(0.0f, 0.0f, 205.0f));
	InteractionPrompt->SetHorizontalAlignment(EHTA_Center);
	InteractionPrompt->SetText(FText::FromString(TEXT("E - MEJORAS PERMANENTES")));
	InteractionPrompt->SetTextRenderColor(FColor(255, 205, 70));
	InteractionPrompt->SetWorldSize(22.0f);
	InteractionPrompt->SetVisibility(false);
}

void APermanentUpgradeShrine::BeginPlay()
{
	Super::BeginPlay();
	SnapToFloor();
	UE_LOG(LogPermanentUpgradeShrine, Display,
		TEXT("[FUENTE DE LUZ] Santuario preparado en %s."), *GetActorLocation().ToCompactString());
}

void APermanentUpgradeShrine::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0))
	{
		const float InteractionDistance = InteractionSphere->GetScaledSphereRadius() + 80.0f;
		const bool bIsNear = FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) <=
			FMath::Square(InteractionDistance);
		if (bIsNear != bPlayerNearby)
		{
			bPlayerNearby = bIsNear;
			InteractionPrompt->SetVisibility(bPlayerNearby && !bMenuOpen);
			UE_LOG(LogPermanentUpgradeShrine, Display, TEXT("[FUENTE DE LUZ] Jugador %s del alcance."),
				bPlayerNearby ? TEXT("dentro") : TEXT("fuera"));
		}
	}

	if (!bPlayerNearby || bMenuOpen)
	{
		return;
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PlayerController->WasInputKeyJustPressed(EKeys::E))
		{
			OpenUpgradeMenu();
		}
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			InteractionPrompt->SetWorldRotation((Camera->GetCameraLocation() - InteractionPrompt->GetComponentLocation()).Rotation());
		}
	}
}

void APermanentUpgradeShrine::OnInteractionBegin(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (OtherActor == UGameplayStatics::GetPlayerCharacter(this, 0))
	{
		bPlayerNearby = true;
		InteractionPrompt->SetVisibility(true);
		UE_LOG(LogPermanentUpgradeShrine, Display, TEXT("[FUENTE DE LUZ] Jugador dentro del alcance."));
	}
}

void APermanentUpgradeShrine::OnInteractionEnd(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	if (OtherActor == UGameplayStatics::GetPlayerCharacter(this, 0))
	{
		bPlayerNearby = false;
		InteractionPrompt->SetVisibility(false);
	}
}

void APermanentUpgradeShrine::OpenUpgradeMenu()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		return;
	}

	UpgradeWidget = CreateWidget<UPermanentUpgradeWidget>(
		PlayerController, UPermanentUpgradeWidget::StaticClass());
	if (!UpgradeWidget)
	{
		return;
	}

	UpgradeWidget->OnMenuClosed.AddDynamic(this, &APermanentUpgradeShrine::CloseUpgradeMenu);
	UpgradeWidget->AddToViewport(150);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(UpgradeWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(true);
	UGameplayStatics::SetGamePaused(this, true);
	bMenuOpen = true;
	InteractionPrompt->SetVisibility(false);
	UE_LOG(LogPermanentUpgradeShrine, Display, TEXT("[FUENTE DE LUZ] Menu abierto."));
}

void APermanentUpgradeShrine::CloseUpgradeMenu()
{
	if (UpgradeWidget)
	{
		UpgradeWidget->RemoveFromParent();
		UpgradeWidget = nullptr;
	}
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->SetShowMouseCursor(false);
	}
	UGameplayStatics::SetGamePaused(this, false);
	bMenuOpen = false;
	InteractionPrompt->SetVisibility(bPlayerNearby);
	UE_LOG(LogPermanentUpgradeShrine, Display, TEXT("[FUENTE DE LUZ] Menu cerrado."));
}

void APermanentUpgradeShrine::SnapToFloor()
{
	FHitResult Hit;
	const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 300.0f);
	const FVector End = GetActorLocation() - FVector(0.0f, 0.0f, 1000.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PermanentUpgradeShrineFloor), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Hit.ImpactPoint.Z));
	}
}
