#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PermanentUpgradeShrine.generated.h"

class UParticleSystemComponent;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UPermanentUpgradeWidget;

UCLASS()
class TFG_API APermanentUpgradeShrine : public AActor
{
	GENERATED_BODY()

public:
	APermanentUpgradeShrine();
	virtual void Tick(float DeltaSeconds) override;

	/** Opens the permanent-upgrade interface. Also callable from Blueprint. */
	UFUNCTION(BlueprintCallable, Category="Fuente de Luz")
	void OpenUpgradeMenu();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ShrineMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UParticleSystemComponent> LightParticles;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> GoldenLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> InteractionPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UPermanentUpgradeWidget> UpgradeWidget;

	bool bPlayerNearby = false;
	bool bMenuOpen = false;

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

	UFUNCTION()
	void CloseUpgradeMenu();

	void SnapToFloor();
};
