#include "LobbyRunDoor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Systems/RunPowerPersistenceSubsystem.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogLobbyRunDoor, Log, All);

ALobbyRunDoor::ALobbyRunDoor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	DoorFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorFrame"));
	DoorFrame->SetupAttachment(SceneRoot);
	DoorFrame->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FrameAsset(
		TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_DoorWay.SM_DoorWay"));
	if (FrameAsset.Succeeded())
	{
		DoorFrame->SetStaticMesh(FrameAsset.Object);
	}

	LeftDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftDoor"));
	LeftDoor->SetupAttachment(SceneRoot);
	LeftDoor->SetRelativeLocation(FVector(0.0f, -265.0f, 0.0f));
	LeftDoor->SetRelativeScale3D(FVector(2.0f));
	LeftDoor->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LeftDoorAsset(
		TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_DoorWay_Large_Door_Left.SM_DoorWay_Large_Door_Left"));
	if (LeftDoorAsset.Succeeded())
	{
		LeftDoor->SetStaticMesh(LeftDoorAsset.Object);
	}

	RightDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightDoor"));
	RightDoor->SetupAttachment(SceneRoot);
	RightDoor->SetRelativeLocation(FVector(0.0f, 265.0f, 0.0f));
	RightDoor->SetRelativeScale3D(FVector(2.0f));
	RightDoor->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RightDoorAsset(
		TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_DoorWay_Large_Door_Right.SM_DoorWay_Large_Door_Right"));
	if (RightDoorAsset.Succeeded())
	{
		RightDoor->SetStaticMesh(RightDoorAsset.Object);
	}

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->SetRelativeLocation(FVector(-180.0f, 0.0f, 170.0f));
	InteractionSphere->SetSphereRadius(310.0f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(
		this, &ALobbyRunDoor::OnInteractionBegin);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(
		this, &ALobbyRunDoor::OnInteractionEnd);

	InteractionPrompt = CreateDefaultSubobject<UTextRenderComponent>(TEXT("InteractionPrompt"));
	InteractionPrompt->SetupAttachment(SceneRoot);
	InteractionPrompt->SetRelativeLocation(FVector(-45.0f, 0.0f, 390.0f));
	InteractionPrompt->SetHorizontalAlignment(EHTA_Center);
	InteractionPrompt->SetVerticalAlignment(EVRTA_TextCenter);
	InteractionPrompt->SetText(FText::FromString(TEXT("E - INICIAR RUN")));
	InteractionPrompt->SetTextRenderColor(FColor(255, 210, 80));
	InteractionPrompt->SetWorldSize(28.0f);
	InteractionPrompt->SetCastShadow(true);
	InteractionPrompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionPrompt->SetVisibility(false);
}

void ALobbyRunDoor::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(
		LogLobbyRunDoor,
		Display,
		TEXT("[PUERTA DE RUN] Preparada en %s. Destino: %s."),
		*GetActorLocation().ToCompactString(),
		*RunMapName.ToString());
}

void ALobbyRunDoor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0))
	{
		const float InteractionDistance =
			InteractionSphere->GetScaledSphereRadius() + 80.0f;
		const bool bIsNear = FVector::DistSquared(
			Player->GetActorLocation(),
			InteractionSphere->GetComponentLocation()) <=
			FMath::Square(InteractionDistance);
		if (bIsNear != bPlayerNearby)
		{
			bPlayerNearby = bIsNear;
			InteractionPrompt->SetVisibility(
				bPlayerNearby && !bTransitionStarted);
			UE_LOG(
				LogLobbyRunDoor,
				Display,
				TEXT("[PUERTA DE RUN] Jugador %s del alcance."),
				bPlayerNearby ? TEXT("dentro") : TEXT("fuera"));
		}
	}

	if (!bPlayerNearby || bTransitionStarted)
	{
		return;
	}

	FacePromptToCamera();
	if (APlayerController* Controller =
		UGameplayStatics::GetPlayerController(this, 0))
	{
		if (Controller->WasInputKeyJustPressed(EKeys::E))
		{
			StartRun();
		}
	}
}

void ALobbyRunDoor::StartRun()
{
	if (bTransitionStarted)
	{
		return;
	}
	bTransitionStarted = true;
	InteractionPrompt->SetVisibility(false);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URunPowerPersistenceSubsystem* Persistence =
			GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>())
		{
			Persistence->ResetPersistentPowers();
		}
	}

	UE_LOG(
		LogLobbyRunDoor,
		Display,
		TEXT("[PUERTA DE RUN] E pulsada. Nueva run iniciada; cargando %s."),
		*RunMapName.ToString());
	UGameplayStatics::OpenLevel(this, RunMapName);
}

void ALobbyRunDoor::OnInteractionBegin(
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
		UE_LOG(
			LogLobbyRunDoor,
			Display,
			TEXT("[PUERTA DE RUN] Jugador dentro del alcance: E - INICIAR RUN."));
	}
}

void ALobbyRunDoor::OnInteractionEnd(
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

void ALobbyRunDoor::FacePromptToCamera() const
{
	if (APlayerController* Controller =
		UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APlayerCameraManager* Camera = Controller->PlayerCameraManager)
		{
			InteractionPrompt->SetWorldRotation(
				(Camera->GetCameraLocation() -
					InteractionPrompt->GetComponentLocation()).Rotation());
		}
	}
}
