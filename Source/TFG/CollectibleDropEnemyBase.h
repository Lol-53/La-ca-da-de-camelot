#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "CollectibleDropEnemyBase.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;
class USoundBase;

/**
 * Reusable parent for the project's Blueprint enemies. It observes the
 * existing Blueprint death flag and resolves one 50% drop roll per enemy.
 */
UCLASS(Blueprintable)
class TFG_API ACollectibleDropEnemyBase : public ACharacter
{
	GENERATED_BODY()

public:
	ACollectibleDropEnemyBase();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	/** Can also be called explicitly by future enemy death Blueprints. */
	UFUNCTION(BlueprintCallable, Category="Enemigo|Drop")
	bool ResolveCollectibleDrop(bool bForceDrop = false);

	/** Called by the Greystone tank Blueprint when its behavior tree orders an attack. */
	UFUNCTION(BlueprintCallable, Category="Enemigo|Greystone")
	void ReproducirAtaqueGreystoneTanque();

	/** Common entry point used by the Behavior Tree for every enemy type. */
	UFUNCTION(BlueprintCallable, Category="Enemigo|IA")
	void EjecutarAtaqueIA();

protected:
	/** Shared placeholder combat sound; replace per Blueprint when more sounds are imported. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemigo|Audio")
	TObjectPtr<USoundBase> CombatSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemigo|Audio")
	TObjectPtr<USoundBase> ProjectileSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemigo|Audio", meta=(ClampMin="0.0"))
	float CombatSoundVolume = 0.68f;

	/** Speeds up non-lethal hit reactions so enemies regain visual responsiveness sooner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemigo|Combate", meta=(ClampMin="1.0"))
	float HitReactionPlayRate = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemigo|Drop", meta=(ClampMin="0.0", ClampMax="1.0"))
	float CollectibleDropChance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemigo|Drop")
	FName DeathFlagName = TEXT("Muerto");

private:
	bool IsMarkedDead() const;
	bool IsGreystoneTank() const;
	bool IsCentaurArcher() const;
	void ConfigureGreystoneTankAnimation();
	void UpdateGreystoneTankAnimation();
	void ConfigureCentaurArcher();
	void UpdateCentaurArcher(float DeltaSeconds);
	void StartCentaurShot();
	void SpawnCentaurArrow();
	void FinishCentaurShot();
	void SetBlueprintAttackWindow(bool bActive);
	void AbrirVentanaAtaqueGreystone();
	void FinalizarAtaqueGreystone();
	void PlaySynchronizedCombatSound(USoundBase* Sound, float Pitch, const TCHAR* EventName) const;
	void AccelerateActiveHitReaction() const;

	bool bDropResolved = false;
	bool bGreystoneDeathAnimationStarted = false;
	bool bGreystoneAttackActive = false;
	bool bGreystoneLocomotionInitialized = false;
	bool bGreystoneWasWalking = false;
	FVector GreystonePreviousLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> GreystoneIdleAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> GreystoneWalkAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> GreystoneAttackAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> GreystoneDeathAnimation;

	FTimerHandle GreystoneAttackWindowTimer;
	FTimerHandle GreystoneAttackEndTimer;

	bool bArcherBrainDisabled = false;
	bool bArcherAttackActive = false;
	bool bArcherDeathAnimationStarted = false;
	bool bArcherLocomotionInitialized = false;
	bool bArcherWasWalking = false;
	double NextArcherShotTime = 0.0;
	FVector ArcherPreviousLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ArcherIdleAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ArcherWalkAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ArcherShootAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ArcherDeathAnimation;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> ArcherVisualParts;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> ArcherNockedArrowVisual;

	FTimerHandle ArcherReleaseTimer;
	FTimerHandle ArcherShotEndTimer;
};
