#include "AlchemyTable.h"

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
#include "UI/AlchemyCraftingWidget.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogAlchemyTable, Log, All);

AAlchemyTable::AAlchemyTable() {
  PrimaryActorTick.bCanEverTick = true;

  SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
  SetRootComponent(SceneRoot);

  TableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableMesh"));
  TableMesh->SetupAttachment(SceneRoot);
  TableMesh->SetCollisionProfileName(TEXT("BlockAll"));
  static ConstructorHelpers::FObjectFinder<UStaticMesh> TableAsset(
      TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Table.SM_Table"));
  if (TableAsset.Succeeded()) {
    TableMesh->SetStaticMesh(TableAsset.Object);
  }

  static ConstructorHelpers::FObjectFinder<UStaticMesh> HealthPotAsset(
      TEXT("/Game/MedievalDungeon/Meshes/Props/"
           "SM_Pot_A_Complete.SM_Pot_A_Complete"));
  static ConstructorHelpers::FObjectFinder<UStaticMesh> ManaPotAsset(
      TEXT("/Game/MedievalDungeon/Meshes/Props/"
           "SM_Pot_B_Complete.SM_Pot_B_Complete"));
  static ConstructorHelpers::FObjectFinder<UStaticMesh> EnergyPotAsset(
      TEXT("/Game/MedievalDungeon/Meshes/Props/"
           "SM_Pot_C_Complete.SM_Pot_C_Complete"));

  HealthPot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealthPot"));
  HealthPot->SetupAttachment(SceneRoot);
  HealthPot->SetRelativeLocation(FVector(0.0f, -62.0f, 86.0f));
  HealthPot->SetRelativeScale3D(FVector(0.55f));
  HealthPot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  if (HealthPotAsset.Succeeded())
    HealthPot->SetStaticMesh(HealthPotAsset.Object);

  ManaPot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ManaPot"));
  ManaPot->SetupAttachment(SceneRoot);
  ManaPot->SetRelativeLocation(FVector(0.0f, 0.0f, 86.0f));
  ManaPot->SetRelativeScale3D(FVector(0.55f));
  ManaPot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  if (ManaPotAsset.Succeeded())
    ManaPot->SetStaticMesh(ManaPotAsset.Object);

  EnergyPot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnergyPot"));
  EnergyPot->SetupAttachment(SceneRoot);
  EnergyPot->SetRelativeLocation(FVector(0.0f, 62.0f, 86.0f));
  EnergyPot->SetRelativeScale3D(FVector(0.55f));
  EnergyPot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  if (EnergyPotAsset.Succeeded())
    EnergyPot->SetStaticMesh(EnergyPotAsset.Object);

  HealthLight =
      CreateDefaultSubobject<UPointLightComponent>(TEXT("HealthLight"));
  HealthLight->SetupAttachment(SceneRoot);
  HealthLight->SetRelativeLocation(FVector(0.0f, -62.0f, 125.0f));
  HealthLight->SetLightColor(FLinearColor(1.0f, 0.04f, 0.03f));
  HealthLight->SetIntensity(650.0f);
  HealthLight->SetAttenuationRadius(165.0f);

  ManaLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ManaLight"));
  ManaLight->SetupAttachment(SceneRoot);
  ManaLight->SetRelativeLocation(FVector(0.0f, 0.0f, 125.0f));
  ManaLight->SetLightColor(FLinearColor(0.02f, 0.18f, 1.0f));
  ManaLight->SetIntensity(650.0f);
  ManaLight->SetAttenuationRadius(165.0f);

  EnergyLight =
      CreateDefaultSubobject<UPointLightComponent>(TEXT("EnergyLight"));
  EnergyLight->SetupAttachment(SceneRoot);
  EnergyLight->SetRelativeLocation(FVector(0.0f, 62.0f, 125.0f));
  EnergyLight->SetLightColor(FLinearColor(0.03f, 1.0f, 0.16f));
  EnergyLight->SetIntensity(650.0f);
  EnergyLight->SetAttenuationRadius(165.0f);

  InteractionSphere =
      CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
  InteractionSphere->SetupAttachment(SceneRoot);
  InteractionSphere->SetSphereRadius(260.0f);
  InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
  InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

  InteractionPrompt =
      CreateDefaultSubobject<UTextRenderComponent>(TEXT("InteractionPrompt"));
  InteractionPrompt->SetupAttachment(SceneRoot);
  InteractionPrompt->SetRelativeLocation(FVector(0.0f, 0.0f, 170.0f));
  InteractionPrompt->SetHorizontalAlignment(EHTA_Center);
  InteractionPrompt->SetText(FText::FromString(TEXT("E - CREAR POCIONES")));
  InteractionPrompt->SetTextRenderColor(FColor(100, 255, 175));
  InteractionPrompt->SetWorldSize(22.0f);
  InteractionPrompt->SetVisibility(false);
}

void AAlchemyTable::BeginPlay() {
  Super::BeginPlay();
  SnapToFloor();
  UE_LOG(LogAlchemyTable, Display, TEXT("[ALQUIMIA] Mesa preparada en %s."),
         *GetActorLocation().ToCompactString());
}

void AAlchemyTable::Tick(const float DeltaSeconds) {
  Super::Tick(DeltaSeconds);
  ACharacter *Player = UGameplayStatics::GetPlayerCharacter(this, 0);
  if (Player) {
    const float InteractionDistance =
        InteractionSphere->GetScaledSphereRadius() + 70.0f;
    const bool bIsNear =
        FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) <=
        FMath::Square(InteractionDistance);
    if (bIsNear != bPlayerNearby) {
      bPlayerNearby = bIsNear;
      InteractionPrompt->SetVisibility(bPlayerNearby && !bMenuOpen);
      UE_LOG(LogAlchemyTable, Display,
             TEXT("[ALQUIMIA] Jugador %s del alcance."),
             bPlayerNearby ? TEXT("dentro") : TEXT("fuera"));
    }
  }

  if (!bPlayerNearby || bMenuOpen) {
    return;
  }

  if (APlayerController *PlayerController =
          UGameplayStatics::GetPlayerController(this, 0)) {
    if (PlayerController->WasInputKeyJustPressed(EKeys::E)) {
      OpenCraftingMenu();
    }
    if (APlayerCameraManager *Camera = PlayerController->PlayerCameraManager) {
      InteractionPrompt->SetWorldRotation(
          (Camera->GetCameraLocation() -
           InteractionPrompt->GetComponentLocation())
              .Rotation());
    }
  }
}

void AAlchemyTable::OpenCraftingMenu() {
  APlayerController *PlayerController =
      UGameplayStatics::GetPlayerController(this, 0);
  if (PlayerController && !bMenuOpen) {
    CraftingWidget = CreateWidget<UAlchemyCraftingWidget>(
        PlayerController, UAlchemyCraftingWidget::StaticClass());
    if (CraftingWidget) {
      CraftingWidget->OnMenuClosed.AddDynamic(
          this, &AAlchemyTable::CloseCraftingMenu);
      CraftingWidget->AddToViewport(150);
      FInputModeUIOnly InputMode;
      InputMode.SetWidgetToFocus(CraftingWidget->TakeWidget());
      InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
      PlayerController->SetInputMode(InputMode);
      PlayerController->SetShowMouseCursor(true);
      UGameplayStatics::SetGamePaused(this, true);
      bMenuOpen = true;
      InteractionPrompt->SetVisibility(false);
      UE_LOG(LogAlchemyTable, Display,
             TEXT("[ALQUIMIA] Menu de fabricacion abierto."));
    }
  }
}

void AAlchemyTable::CloseCraftingMenu() {
  if (CraftingWidget) {
    CraftingWidget->RemoveFromParent();
    CraftingWidget = nullptr;
  }
  if (APlayerController *PlayerController =
          UGameplayStatics::GetPlayerController(this, 0)) {
    PlayerController->SetInputMode(FInputModeGameOnly());
    PlayerController->SetShowMouseCursor(false);
  }
  UGameplayStatics::SetGamePaused(this, false);
  bMenuOpen = false;
  InteractionPrompt->SetVisibility(bPlayerNearby);
  UE_LOG(LogAlchemyTable, Display,
         TEXT("[ALQUIMIA] Menu de fabricacion cerrado."));
}

void AAlchemyTable::SnapToFloor() {
  FHitResult Hit;
  const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 300.0f);
  const FVector End = GetActorLocation() - FVector(0.0f, 0.0f, 1000.0f);
  FCollisionQueryParams Params(SCENE_QUERY_STAT(AlchemyTableFloor), false,
                               this);
  if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility,
                                           Params)) {
    SetActorLocation(
        FVector(GetActorLocation().X, GetActorLocation().Y, Hit.ImpactPoint.Z));
  }
}
