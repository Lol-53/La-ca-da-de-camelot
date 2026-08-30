#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InteractiveNPCBase.generated.h"

class USphereComponent;
class UTextRenderComponent;
class UNPCDialogueNameWidget;

/** A complete linear conversation. Only one package is selected per NPC spawn. */
USTRUCT(BlueprintType)
struct FNPCDialoguePackage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogo")
	FName NombrePaquete = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogo")
	TArray<FString> Lineas;
};

/**
 * Reusable base for dialogue NPCs.
 * The dialogue content remains in Blueprint; this class provides proximity,
 * a floating prompt and reliable manual E-key interaction.
 */
UCLASS(Blueprintable)
class TFG_API AInteractiveNPCBase : public ACharacter
{
	GENERATED_BODY()

public:
	AInteractiveNPCBase();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC|Interaccion")
	TObjectPtr<USphereComponent> InteractionRange;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC|Interaccion")
	TObjectPtr<UTextRenderComponent> InteractionPrompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC|Interaccion", meta=(ClampMin="50.0"))
	float InteractionRadius = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC|Interaccion")
	float PromptHeight = 180.0f;

	/** Displayed in the upper-left corner of the dialogue panel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NPC|Dialogo")
	FText NombreNPC = FText::FromString(TEXT("NPC"));

	/**
	 * Alternative complete conversations for this NPC. At BeginPlay one package
	 * is selected randomly and copied into the legacy Dialogo array.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NPC|Dialogo",
		meta=(TitleProperty="NombrePaquete"))
	TArray<FNPCDialoguePackage> PaquetesDialogo;

private:
	UFUNCTION()
	void OnInteractionRangeBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractionRangeEnd(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	UFUNCTION(BlueprintCallable, Category = "NPC|Interaccion")
	void HandleInteractPressed();
	void UpdatePromptVisibility();
	void FacePromptToCamera() const;
	int32 ReadDialogueIndex() const;
	void SelectDialoguePackage();
	void NotifyDialogueState(bool bActive);

	TWeakObjectPtr<ACharacter> NearbyPlayer;
	UPROPERTY(Transient)
	TObjectPtr<UNPCDialogueNameWidget> DialogueNameWidget;
	bool bPlayerInRange = false;
	int32 LastObservedDialogueIndex = 0;
};
