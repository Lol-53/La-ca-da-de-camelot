#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "InteractiveNPCBase.h"
#include "GameFramework/Character.h"
#include "FinalBossMordred.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class UInputComponent;
class UMordredBossHealthWidget;
class UMordredVictoryDialogueWidget;
class UNPCDialogueNameWidget;
class USkeletalMeshComponent;
class USoundBase;

/**
 * Final boss that reuses Arturo's mesh, AnimBP and attack montages.
 * A Blueprint child exposes all combat tuning while native code provides
 * reliable navigation, damage, dialogue and boss-HUD integration.
 */
UCLASS(Blueprintable)
class TFG_API AFinalBossMordred : public ACharacter
{
	GENERATED_BODY()

public:
	AFinalBossMordred();

	/** Project damage interface entry point used by BP_Combate and projectiles. */
	UFUNCTION(BlueprintCallable, Category="Mordred|Combate")
	void RecibirDanyo(float DanyoARecibir);

	UFUNCTION(BlueprintPure, Category="Mordred|Estado")
	bool EstaCombateActivo() const { return bCombatActive && !bDead; }

	UFUNCTION(BlueprintPure, Category="Mordred|Estado")
	bool EstaEjecutandoAccion() const { return bActionLocked; }

	UFUNCTION(BlueprintPure, Category="Mordred|Vida")
	float ObtenerVidaActual() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="Mordred|Vida")
	float ObtenerVidaMaxima() const { return MaxHealth; }

	/** Advances exactly one intro phrase. Bound to E while the intro is active. */
	UFUNCTION(BlueprintCallable, Category="Mordred|Dialogo")
	void AdvanceIntroDialogue();

	UFUNCTION(BlueprintPure, Category="Mordred|Dialogo")
	bool EstaIntroduccionActiva() const { return bIntroActive; }

	UFUNCTION(BlueprintPure, Category="Mordred|Dialogo")
	int32 ObtenerIndiceDialogo() const
	{
		return bVictoryDialogueActive ? VictoryDialogueIndex : DialogueIndex;
	}

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Same equipped sword and hand socket used by the playable character. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mordred|Equipo")
	TObjectPtr<USkeletalMeshComponent> EquippedSword;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Vida", meta=(ClampMin="1.0"))
	float MaxHealth = 600.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mordred|Vida")
	float CurrentHealth = 600.0f;

	/** Kept for compatibility with the player's weakening power. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mordred|Combate")
	float DanyoAtaqueBase = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Combate")
	float SwordDamage = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Combate")
	float KickDamage = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Combate")
	float LeapDamage = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Combate")
	float DashDamage = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Combate")
	float KickPushStrength = 1250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Combate")
	float DashStrength = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Combate")
	float LeapRadius = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|IA")
	float MinActionInterval = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|IA")
	float MaxActionInterval = 2.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Animacion")
	TObjectPtr<UAnimMontage> SwordAttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Animacion")
	TObjectPtr<UAnimMontage> KickAttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Animacion")
	TObjectPtr<UAnimMontage> HitMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Animacion")
	TObjectPtr<UAnimSequenceBase> DeathAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Animacion")
	TObjectPtr<UAnimSequenceBase> ProjectileLaunchAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Proyectil")
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Audio")
	TObjectPtr<USoundBase> AttackSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Audio")
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Audio")
	TObjectPtr<USoundBase> HitReactionSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Audio")
	TObjectPtr<USoundBase> ProjectileLaunchSound;

	/** Complete alternative conversations used when the player enters the arena. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Dialogo|Entrada",
		meta=(TitleProperty="NombrePaquete"))
	TArray<FNPCDialoguePackage> IntroDialoguePackages;

	/** Complete alternative conversations used after Mordred is defeated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Dialogo|Victoria",
		meta=(TitleProperty="NombrePaquete"))
	TArray<FNPCDialoguePackage> VictoryDialoguePackages;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mordred|Dialogo", meta=(ClampMin="0.5"))
	float DialogueLineDuration = 2.4f;

private:
	void StartIntroDialogue();
	void FinishIntroDialogue();
	void StartVictoryDialogue();
	void AdvanceVictoryDialogue();
	void FinishVictoryDialogue();
	TArray<FString> SelectDialoguePackage(
		const TArray<FNPCDialoguePackage>& Packages,
		const TCHAR* DialoguePhase) const;
	void ShowDialogueLine(const FString& Line) const;
	void HideDialogue() const;
	void ShowMordredDialogueName();
	void HideMordredDialogueName();
	void SetPlayerMovementEnabled(bool bEnabled) const;
	void BindDialogueInput();
	void UnbindDialogueInput();

	void EvaluateCombatAction();
	void PerformSwordAttack();
	void PerformKickAttack();
	void PerformDash();
	void PerformProjectileAttack();
	void SpawnPreparedProjectile();
	void PerformOffensiveLeap();
	void ResolveMeleeImpact(bool bKick);
	void ResolveDashImpact();
	void UnlockAction();
	void BeginAction(float Duration);

	ACharacter* ResolvePlayerCharacter() const;
	void FacePlayerInstantly();
	void DealDamageToPlayer(float Damage, const FVector& LaunchVelocity);
	void UpdateBossHealthBar();
	void HandleDeath();
	void PlayCombatSound(USoundBase* Sound, float Pitch, const TCHAR* EventName) const;

	UPROPERTY(Transient)
	TObjectPtr<UMordredBossHealthWidget> BossHealthWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNPCDialogueNameWidget> DialogueNameWidget;

	UPROPERTY(Transient)
	TObjectPtr<UMordredVictoryDialogueWidget> VictoryDialogueWidget;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> DialogueInputComponent;

	TWeakObjectPtr<ACharacter> CachedPlayer;
	FTimerHandle IntroStartTimer;
	FTimerHandle ActionUnlockTimer;
	FTimerHandle ImpactTimer;
	FTimerHandle ProjectileSpawnTimer;
	FTimerHandle VictoryStartTimer;

	float TimeUntilNextAction = 1.0f;
	TArray<FString> ActiveIntroDialogue;
	TArray<FString> ActiveVictoryDialogue;
	int32 DialogueIndex = INDEX_NONE;
	int32 VictoryDialogueIndex = INDEX_NONE;
	bool bIntroActive = false;
	bool bVictoryDialogueActive = false;
	bool bCombatActive = false;
	bool bActionLocked = false;
	bool bOffensiveLeapPending = false;
	bool bDead = false;
	bool bDialogueInputBound = false;
};

/** Navigation and focus layer for the Mordred boss. */
UCLASS()
class TFG_API AMordredBossAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMordredBossAIController();

protected:
	virtual void Tick(float DeltaSeconds) override;
};
