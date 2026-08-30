#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "RunPowerPersistenceSubsystem.generated.h"

UENUM(BlueprintType)
enum class EEnemyCollectibleType : uint8
{
	Luz UMETA(DisplayName="Luz"),
	Vida UMETA(DisplayName="Vida"),
	Energia UMETA(DisplayName="Energia"),
	Mana UMETA(DisplayName="Mana")
};

UENUM(BlueprintType)
enum class EPotionType : uint8
{
	Vida UMETA(DisplayName="Pocion de vida"),
	Mana UMETA(DisplayName="Pocion de mana"),
	Energia UMETA(DisplayName="Pocion de energia")
};

UENUM(BlueprintType)
enum class EPermanentUpgradeType : uint8
{
	Danyo UMETA(DisplayName="Dano"),
	Velocidad UMETA(DisplayName="Velocidad"),
	Vida UMETA(DisplayName="Vida maxima"),
	Mana UMETA(DisplayName="Mana maximo"),
	RegeneracionVida UMETA(DisplayName="Regeneracion de vida"),
	Resurreccion UMETA(DisplayName="Resurreccion")
};

UCLASS()
class TFG_API UPermanentProgressionSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 StoredLight = 0;

	UPROPERTY()
	int32 StoredHealthResource = 0;

	UPROPERTY()
	int32 StoredEnergyResource = 0;

	UPROPERTY()
	int32 StoredManaResource = 0;

	UPROPERTY()
	int32 StoredHealthPotions = 0;

	UPROPERTY()
	int32 StoredEnergyPotions = 0;

	UPROPERTY()
	int32 StoredManaPotions = 0;

	UPROPERTY()
	int32 DamageLevel = 0;

	UPROPERTY()
	int32 SpeedLevel = 0;

	UPROPERTY()
	int32 HealthLevel = 0;

	UPROPERTY()
	int32 ManaLevel = 0;

	UPROPERTY()
	bool bHealthRegenUnlocked = false;

	UPROPERTY()
	bool bReviveUnlocked = false;

	/** Saved per profile so Arturo's first time-return dialogue is shown only once. */
	UPROPERTY()
	bool bFirstReturnDialogueSeen = false;

	/** Survives the map transition (and an unexpected restart) until the lobby consumes it. */
	UPROPERTY()
	bool bFirstReturnDialoguePending = false;
};

/** Remembers which of the three player profiles is active across launches. */
UCLASS()
class TFG_API USelectedProfileSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 ActiveSlot = 1;
};

USTRUCT(BlueprintType)
struct TFG_API FRunCollectibleInventory
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Recolectables")
	int32 Luz = 0;

	UPROPERTY(BlueprintReadOnly, Category="Recolectables")
	int32 Vida = 0;

	UPROPERTY(BlueprintReadOnly, Category="Recolectables")
	int32 Energia = 0;

	UPROPERTY(BlueprintReadOnly, Category="Recolectables")
	int32 Mana = 0;

	int32 Get(EEnemyCollectibleType Type) const;
	void Add(EEnemyCollectibleType Type, int32 Amount);
	int32 Total() const { return Luz + Vida + Energia + Mana; }
};

USTRUCT(BlueprintType)
struct TFG_API FRunPotionInventory
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Pociones")
	int32 Vida = 0;

	UPROPERTY(BlueprintReadOnly, Category="Pociones")
	int32 Mana = 0;

	UPROPERTY(BlueprintReadOnly, Category="Pociones")
	int32 Energia = 0;

	int32 Get(EPotionType Type) const;
	void Add(EPotionType Type, int32 Amount);
	int32 Total() const { return Vida + Mana + Energia; }
};

/**
 * Durable power switches for the current run. Transient actions such as
 * stealth, sprint, an in-progress jump or the current double-damage cooldown
 * are intentionally excluded.
 */
USTRUCT(BlueprintType)
struct TFG_API FRunPersistentPowerState
{
	GENERATED_BODY()

	/** Legacy field name: stores ownership of the passive double-damage power. */
	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bDobleDanyoPreparado = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bAtaquesQueman = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bAtaquesDesplazan = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bAtaquesCriticos = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bAtaquesDebilitan = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bVelocidadAumentada = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bTieneVidaExtra = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bDanyoReducido = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bProyectilesParalizan = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bGuardiaDireccionalActiva = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bRoboVidaActivo = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bEsquivaActiva = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bInversionDanyoActiva = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bDashHabilitado = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bAtaqueRapidoActivo = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bProyectilesAutoapuntado = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bProyectilesRebotan = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bRecursosAlquimiaObtenidos = false;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bAtaquesDesvianProyectiles = false;

	/** Single non-passive power equipped for this run: 11, 16, or zero. */
	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	int32 PowerUpActivoDeSala = 0;

	/** Health carried between rooms of the current run. */
	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	float VidaActualGuardada = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Poderes|Persistencia")
	bool bTieneVidaGuardada = false;

	int32 CountActivePowers() const;
};

/**
 * Lives inside the GameInstance, so it survives OpenLevel and other map
 * transitions while the current run remains active.
 */
UCLASS()
class TFG_API URunPowerPersistenceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Selects and loads one of the three permanent-progression profiles. */
	UFUNCTION(BlueprintCallable, Category="Guardado")
	void SelectSaveSlot(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category="Guardado")
	int32 GetActiveSaveSlot() const { return ActiveSaveSlot; }

	UFUNCTION(BlueprintPure, Category="Guardado")
	bool DoesSaveSlotExist(int32 SlotIndex) const;

	/** Permanently removes all progression associated with one profile. */
	UFUNCTION(BlueprintCallable, Category="Guardado")
	bool DeleteSaveSlot(int32 SlotIndex);

	void StorePowerState(const FRunPersistentPowerState& NewState);
	const FRunPersistentPowerState& GetPowerState() const { return PowerState; }
	bool HasStoredState() const { return bHasStoredState; }

	UFUNCTION(BlueprintPure, Category="Poderes|Persistencia")
	int32 GetPersistentPowerCount() const;

	UFUNCTION(BlueprintPure, Category="Poderes|Persistencia")
	bool HasPersistentRoomPowerUp(int32 PowerUpId) const;

	UFUNCTION(BlueprintCallable, Category="Recolectables|Persistencia")
	void AddCollectible(EEnemyCollectibleType Type, int32 Amount = 1);

	UFUNCTION(BlueprintPure, Category="Recolectables|Persistencia")
	int32 GetCollectibleCount(EEnemyCollectibleType Type) const;

	UFUNCTION(BlueprintPure, Category="Recolectables|Persistencia")
	int32 GetTotalCollectibleCount() const { return CollectibleInventory.Total(); }

	UFUNCTION(BlueprintPure, Category="Recolectables|Persistencia")
	FRunCollectibleInventory GetCollectibleInventory() const { return CollectibleInventory; }

	UFUNCTION(BlueprintPure, Category="Pociones|Alquimia")
	int32 GetPotionCount(EPotionType Type) const;

	UFUNCTION(BlueprintPure, Category="Pociones|Alquimia")
	int32 GetPotionCraftCost(EPotionType Type) const;

	UFUNCTION(BlueprintPure, Category="Pociones|Alquimia")
	bool CanCraftPotion(EPotionType Type) const;

	UFUNCTION(BlueprintCallable, Category="Pociones|Alquimia")
	bool CraftPotion(EPotionType Type);

	UFUNCTION(BlueprintCallable, Category="Pociones|Alquimia")
	bool ConsumePotion(EPotionType Type);

	UFUNCTION(BlueprintPure, Category="Pociones|Alquimia")
	FRunPotionInventory GetPotionInventory() const { return PotionInventory; }

	UFUNCTION(BlueprintPure, Category="Mejoras permanentes")
	int32 GetLightCount() const;

	UFUNCTION(BlueprintPure, Category="Mejoras permanentes")
	int32 GetPermanentUpgradeLevel(EPermanentUpgradeType Type) const;

	UFUNCTION(BlueprintPure, Category="Mejoras permanentes")
	int32 GetPermanentUpgradeMaxLevel(EPermanentUpgradeType Type) const;

	UFUNCTION(BlueprintPure, Category="Mejoras permanentes")
	int32 GetPermanentUpgradeCost(EPermanentUpgradeType Type) const;

	UFUNCTION(BlueprintPure, Category="Mejoras permanentes")
	bool IsPermanentUpgradeUnlocked(EPermanentUpgradeType Type) const;

	UFUNCTION(BlueprintPure, Category="Mejoras permanentes")
	bool CanPurchasePermanentUpgrade(EPermanentUpgradeType Type) const;

	UFUNCTION(BlueprintCallable, Category="Mejoras permanentes")
	bool PurchasePermanentUpgrade(EPermanentUpgradeType Type);

	UFUNCTION(BlueprintPure, Category="Mejoras permanentes")
	float GetPermanentDamageMultiplier() const;

	UFUNCTION(BlueprintPure, Category="Mejoras permanentes")
	float GetPermanentSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category="Mejoras permanentes")
	float GetPermanentHealthBonus() const;

	UFUNCTION(BlueprintPure, Category="Mejoras permanentes")
	float GetPermanentManaBonus() const;

	UFUNCTION(BlueprintPure, Category="Mejoras permanentes")
	float GetPermanentHealthRegenPerSecond() const;

	UFUNCTION(BlueprintPure, Category="Mejoras permanentes")
	bool IsPermanentReviveAvailable() const;

	UFUNCTION(BlueprintCallable, Category="Mejoras permanentes")
	void ConsumePermanentRevive();

	/** Marks the first completed run, whether it ended in death or victory. */
	void MarkFirstReturnDialoguePending();

	/** Returns whether the active save still needs to show Arturo's first-return dialogue. */
	bool ShouldShowFirstReturnDialogue() const;

	/** Persists completion only after the player advances the dialogue with E. */
	void CompleteFirstReturnDialogue();

	/** Call this when starting a completely new run. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Persistencia")
	void ResetPersistentPowers();

	/** Last procedural room reached during this run (one-based). */
	UFUNCTION(BlueprintPure, Category="Run")
	int32 GetCurrentRunRoomNumber() const { return CurrentRunRoomNumber; }

	/** Updates the room shown by the run-status interface. */
	void SetCurrentRunRoomNumber(int32 RoomNumber);

	/**
	 * Returns an NPC index that has not appeared during the current run.
	 * Once every available NPC has appeared, a new non-repeating cycle begins.
	 */
	int32 ChooseUniqueNpcIndex(int32 SelectionSeed, int32 NpcCount);

private:
	UPROPERTY(Transient)
	int32 CurrentRunRoomNumber = 1;

	UPROPERTY(Transient)
	FRunPersistentPowerState PowerState;

	UPROPERTY(Transient)
	bool bHasStoredState = false;

	UPROPERTY(Transient)
	FRunCollectibleInventory CollectibleInventory;

	UPROPERTY(Transient)
	FRunPotionInventory PotionInventory;

	UPROPERTY(Transient)
	TArray<int32> UsedNpcIndices;

	UPROPERTY(Transient)
	TObjectPtr<UPermanentProgressionSaveGame> PermanentProgression;

	bool bPermanentReviveAvailable = false;

	int32 ActiveSaveSlot = 1;

	void LoadPermanentProgression();
	void SavePermanentProgression();
	FString GetProgressionSlotName(int32 SlotIndex) const;
	void SaveSelectedProfile() const;

	static const FString LegacyProgressionSlot;
	static const FString SelectedProfileSlot;
};
