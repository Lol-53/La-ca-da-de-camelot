#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "LobbyRunDoor.generated.h"

class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Interactive lobby entrance that starts a fresh procedural run.
 * Permanent progression is preserved by the run persistence subsystem.
 */
UCLASS()
class TFG_API ALobbyRunDoor : public AActor
{
	GENERATED_BODY()

public:
	ALobbyRunDoor();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Lobby|Run")
	void StartRun();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> DoorFrame;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> LeftDoor;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> RightDoor;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> InteractionPrompt;

	UPROPERTY(EditAnywhere, Category="Lobby|Run")
	FName RunMapName = TEXT("Run_Container");

	bool bPlayerNearby = false;
	bool bTransitionStarted = false;

	UFUNCTION()
	void OnInteractionBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractionEnd(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	void FacePromptToCamera() const;
};
