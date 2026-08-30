#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Systems/RunPowerPersistenceSubsystem.h"

#include "EnemyResourcePickup.generated.h"

class UMaterialInterface;
class UParticleSystemComponent;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Physical room drop collected by walking through it. Its type is stored in
 * the player's persistent run inventory; it is never attached as visible gear.
 */
UCLASS(Blueprintable)
class TFG_API AEnemyResourcePickup : public AActor
{
	GENERATED_BODY()

public:
	AEnemyResourcePickup();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Recolectable")
	void InitializeCollectible(EEnemyCollectibleType NewType);

	UFUNCTION(BlueprintPure, Category="Recolectable")
	EEnemyCollectibleType GetCollectibleType() const { return CollectibleType; }

	static AEnemyResourcePickup* SpawnRandomDrop(
		AActor* DefeatedEnemy,
		float DropChance = 0.5f);

	static const TCHAR* TypeToString(EEnemyCollectibleType Type);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Recolectable")
	TObjectPtr<USphereComponent> CollectionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Recolectable")
	TObjectPtr<UStaticMeshComponent> CoreMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Recolectable")
	TObjectPtr<UParticleSystemComponent> MainParticles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Recolectable")
	TObjectPtr<UParticleSystemComponent> AccentParticles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Recolectable")
	TObjectPtr<UPointLightComponent> ColoredLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Recolectable")
	TObjectPtr<UTextRenderComponent> TypeLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recolectable")
	EEnemyCollectibleType CollectibleType = EEnemyCollectibleType::Luz;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recolectable", meta=(ClampMin="20.0"))
	float CollectionRadius = 72.0f;

	/** Time the drop must remain visible before the player can collect it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recolectable", meta=(ClampMin="0.0", Units="s"))
	float CollectionDelay = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recolectable")
	float BobHeight = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recolectable")
	float BobSpeed = 2.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recolectable", meta=(ClampMin="5.0"))
	float GroundOffset = 20.0f;

private:
	UFUNCTION()
	void OnCollectionOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void ConfigureVisuals();
	void EnableCollection();
	FLinearColor GetTypeColor() const;
	void SnapToGround();
	void FaceLabelToCamera() const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ColorableMaterial;

	FVector RestLocation = FVector::ZeroVector;
	float RunningTime = 0.0f;
	bool bCollectionEnabled = false;
	bool bCollected = false;
	FTimerHandle CollectionEnableTimer;
};
