#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "EnemyArcherProjectile.generated.h"

class UProjectileMovementComponent;
class USkeletalMeshComponent;
class USphereComponent;

/** Physical arrow fired by the Centaur archer. It also supports player deflection. */
UCLASS()
class TFG_API AEnemyArcherProjectile : public AActor
{
	GENERATED_BODY()

public:
	AEnemyArcherProjectile();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleImpact(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> ArrowVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	TWeakObjectPtr<AActor> OriginalShooter;
	float Damage = 16.0f;
};
