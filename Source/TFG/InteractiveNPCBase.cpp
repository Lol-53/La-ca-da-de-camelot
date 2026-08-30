#include "InteractiveNPCBase.h"

#include "TFGCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/NPCDialogueNameWidget.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPCInteraction, Log, All);

AInteractiveNPCBase::AInteractiveNPCBase()
{
	PrimaryActorTick.bCanEverTick = true;

	InteractionRange = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionRange"));
	InteractionRange->SetupAttachment(GetRootComponent());
	InteractionRange->InitSphereRadius(InteractionRadius);
	InteractionRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionRange->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionRange->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionRange->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionRange->SetGenerateOverlapEvents(true);
	InteractionRange->SetHiddenInGame(true);
	InteractionRange->OnComponentBeginOverlap.AddDynamic(
		this,
		&AInteractiveNPCBase::OnInteractionRangeBegin);
	InteractionRange->OnComponentEndOverlap.AddDynamic(
		this,
		&AInteractiveNPCBase::OnInteractionRangeEnd);

	InteractionPrompt = CreateDefaultSubobject<UTextRenderComponent>(TEXT("InteractionPrompt"));
	InteractionPrompt->SetupAttachment(GetRootComponent());
	InteractionPrompt->SetRelativeLocation(FVector(0.0f, 0.0f, PromptHeight));
	InteractionPrompt->SetText(FText::FromString(TEXT("E-Interactuar")));
	InteractionPrompt->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	InteractionPrompt->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	InteractionPrompt->SetWorldSize(25.0f);
	InteractionPrompt->SetTextRenderColor(FColor(255, 224, 92));
	InteractionPrompt->SetCastShadow(true);
	InteractionPrompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionPrompt->SetVisibility(false);
}

void AInteractiveNPCBase::BeginPlay()
{
	Super::BeginPlay();
	SelectDialoguePackage();
	InteractionRange->SetSphereRadius(InteractionRadius, true);
	InteractionPrompt->SetRelativeLocation(FVector(0.0f, 0.0f, PromptHeight));
	LastObservedDialogueIndex = ReadDialogueIndex();
	UpdatePromptVisibility();
	UE_LOG(
		LogNPCInteraction,
		Display,
		TEXT("[NPC] %s (%s) usara la interaccion unica de BP_LineTrace para avanzar con E."),
		*GetName(),
		*NombreNPC.ToString());
}

void AInteractiveNPCBase::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// BP_LineTrace owns the project's single E binding. Refreshing here keeps
	// the floating prompt synchronized when that Blueprint changes IndexDialogo.
	const int32 DialogueIndex = ReadDialogueIndex();
	if (DialogueIndex != LastObservedDialogueIndex)
	{
		const bool bDialogueWasActive = LastObservedDialogueIndex > 0;
		const bool bDialogueIsActive = DialogueIndex > 0;
		if (bDialogueWasActive != bDialogueIsActive)
		{
			NotifyDialogueState(bDialogueIsActive);
		}
		UE_LOG(
			LogNPCInteraction,
			Display,
			TEXT("[NPC] %s avanzo dialogo: %d -> %d."),
			*GetName(),
			LastObservedDialogueIndex,
			DialogueIndex);
		LastObservedDialogueIndex = DialogueIndex;
	}
	UpdatePromptVisibility();
	if (InteractionPrompt->IsVisible())
	{
		FacePromptToCamera();
	}
}

void AInteractiveNPCBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LastObservedDialogueIndex > 0)
	{
		NotifyDialogueState(false);
	}
	Super::EndPlay(EndPlayReason);
}

void AInteractiveNPCBase::OnInteractionRangeBegin(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (OtherActor != PlayerCharacter)
	{
		return;
	}

	NearbyPlayer = PlayerCharacter;
	bPlayerInRange = true;
	UpdatePromptVisibility();

	UE_LOG(LogNPCInteraction, Display, TEXT("[NPC] Jugador cerca de %s: E-Interactuar."), *GetName());
}

void AInteractiveNPCBase::OnInteractionRangeEnd(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex)
{
	if (OtherActor != NearbyPlayer.Get())
	{
		return;
	}

	NearbyPlayer.Reset();
	bPlayerInRange = false;
	UpdatePromptVisibility();

	UE_LOG(LogNPCInteraction, Display, TEXT("[NPC] Jugador fuera del alcance de %s."), *GetName());
}

void AInteractiveNPCBase::HandleInteractPressed()
{
	if (!bPlayerInRange || !NearbyPlayer.IsValid())
	{
		return;
	}

	UFunction* InteractionFunction = FindFunction(TEXT("Interactuar"));
	if (!InteractionFunction)
	{
		UE_LOG(LogNPCInteraction, Warning, TEXT("[NPC] %s no implementa Interactuar."), *GetName());
		return;
	}

	InteractionPrompt->SetVisibility(false);
	ProcessEvent(InteractionFunction, nullptr);
	UpdatePromptVisibility();

	const int32 DialogueIndex = ReadDialogueIndex();
	if (LastObservedDialogueIndex <= 0 && DialogueIndex > 0)
	{
		NotifyDialogueState(true);
		LastObservedDialogueIndex = DialogueIndex;
	}
	UE_LOG(
		LogNPCInteraction,
		Display,
		TEXT("[NPC] E pulsada en %s. Indice de dialogo: %d."),
		*GetName(),
		DialogueIndex);
}

void AInteractiveNPCBase::UpdatePromptVisibility()
{
	const bool bShouldShow = bPlayerInRange && NearbyPlayer.IsValid() && ReadDialogueIndex() == 0;
	InteractionPrompt->SetVisibility(bShouldShow, true);
	if (bShouldShow)
	{
		FacePromptToCamera();
	}
}

void AInteractiveNPCBase::FacePromptToCamera() const
{
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager || !InteractionPrompt)
	{
		return;
	}

	FRotator LookRotation =
		(CameraManager->GetCameraLocation() - InteractionPrompt->GetComponentLocation()).Rotation();
	LookRotation.Roll = 0.0f;
	InteractionPrompt->SetWorldRotation(LookRotation);
}

int32 AInteractiveNPCBase::ReadDialogueIndex() const
{
	const FNumericProperty* IndexProperty =
		FindFProperty<FNumericProperty>(GetClass(), TEXT("IndexDialogo"));
	if (!IndexProperty)
	{
		return 0;
	}

	const void* ValueAddress = IndexProperty->ContainerPtrToValuePtr<void>(this);
	return static_cast<int32>(IndexProperty->GetSignedIntPropertyValue(ValueAddress));
}

void AInteractiveNPCBase::SelectDialoguePackage()
{
	TArray<int32> ValidPackageIndices;
	for (int32 Index = 0; Index < PaquetesDialogo.Num(); ++Index)
	{
		if (!PaquetesDialogo[Index].Lineas.IsEmpty())
		{
			ValidPackageIndices.Add(Index);
		}
	}

	if (ValidPackageIndices.IsEmpty())
	{
		UE_LOG(
			LogNPCInteraction,
			Display,
			TEXT("[NPC] %s no tiene paquetes configurados; se conserva el Dialogo heredado."),
			*NombreNPC.ToString());
		return;
	}

	const int32 PackageIndex =
		ValidPackageIndices[FMath::RandHelper(ValidPackageIndices.Num())];
	const FNPCDialoguePackage& SelectedPackage = PaquetesDialogo[PackageIndex];

	FArrayProperty* DialogueProperty =
		FindFProperty<FArrayProperty>(GetClass(), TEXT("Dialogo"));
	FStrProperty* DialogueLineProperty =
		DialogueProperty ? CastField<FStrProperty>(DialogueProperty->Inner) : nullptr;
	FTextProperty* DialogueTextProperty =
		DialogueProperty ? CastField<FTextProperty>(DialogueProperty->Inner) : nullptr;
	if (!DialogueProperty || (!DialogueLineProperty && !DialogueTextProperty))
	{
		UE_LOG(
			LogNPCInteraction,
			Error,
			TEXT("[NPC] %s no puede aplicar el paquete porque Dialogo no es un array de Text o String."),
			*NombreNPC.ToString());
		return;
	}

	void* DialogueAddress = DialogueProperty->ContainerPtrToValuePtr<void>(this);
	FScriptArrayHelper DialogueArray(DialogueProperty, DialogueAddress);
	DialogueArray.EmptyAndAddValues(SelectedPackage.Lineas.Num());
	for (int32 LineIndex = 0; LineIndex < SelectedPackage.Lineas.Num(); ++LineIndex)
	{
		void* DialogueLineAddress = DialogueArray.GetRawPtr(LineIndex);
		if (DialogueTextProperty)
		{
			DialogueTextProperty->SetPropertyValue(
				DialogueLineAddress,
				FText::FromString(SelectedPackage.Lineas[LineIndex]));
		}
		else
		{
			DialogueLineProperty->SetPropertyValue(
				DialogueLineAddress,
				SelectedPackage.Lineas[LineIndex]);
		}
	}

	UE_LOG(
		LogNPCInteraction,
		Display,
		TEXT("[NPC] %s ha elegido el paquete '%s' (%d frases de %d paquetes disponibles)."),
		*NombreNPC.ToString(),
		*SelectedPackage.NombrePaquete.ToString(),
		SelectedPackage.Lineas.Num(),
		ValidPackageIndices.Num());
}

void AInteractiveNPCBase::NotifyDialogueState(const bool bActive)
{
	if (ATFGCharacter* Player =
		Cast<ATFGCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		Player->EstablecerDialogoActivo(bActive);
	}

	if (bActive)
	{
		if (!DialogueNameWidget)
		{
			if (APlayerController* OwningController =
				UGameplayStatics::GetPlayerController(this, 0))
			{
				DialogueNameWidget = CreateWidget<UNPCDialogueNameWidget>(
					OwningController,
					UNPCDialogueNameWidget::StaticClass());
				if (DialogueNameWidget)
				{
					DialogueNameWidget->SetNPCName(NombreNPC);
					DialogueNameWidget->AddToViewport(60);
				}
			}
		}
		UE_LOG(
			LogNPCInteraction,
			Display,
			TEXT("[NPC] Dialogo iniciado con %s."),
			*NombreNPC.ToString());
	}
	else
	{
		if (DialogueNameWidget)
		{
			DialogueNameWidget->RemoveFromParent();
			DialogueNameWidget = nullptr;
		}
		UE_LOG(
			LogNPCInteraction,
			Display,
			TEXT("[NPC] Dialogo finalizado con %s."),
			*NombreNPC.ToString());
	}
}
