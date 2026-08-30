#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoomPowerUpSelectionComponent.generated.h"

class APlayerController;
class ATFGCharacter;
class URoomPowerUpSelectionWidget;

/**
 * Owns the room reward interface, input mode and application of the selected
 * persistent player power.
 */
UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class TFG_API URoomPowerUpSelectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnSelectionFinished);
	FOnSelectionFinished OnSelectionFinished;

	/**
	 * Shows two power-up cards restricted to the knight identified by NpcIndex:
	 * Lanzarote=0, Gawain=1, Galahad=2, Palomides=3, Tristan=4,
	 * Perceval=5 and Bedevere=6.
	 */
	UFUNCTION(BlueprintCallable, Category="Power Ups|Seleccion")
	bool ShowSelection(int32 SelectionSeed, int32 NpcIndex = -1);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandlePowerUpChosen(int32 PowerUpId);
	void RestoreGameInput();

	UPROPERTY(Transient)
	TObjectPtr<URoomPowerUpSelectionWidget> ActiveWidget;

	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<ATFGCharacter> PlayerCharacter;
	bool bOwnsGameInput = false;
	bool bSelectionCommitted = false;
	int32 OptionAId = INDEX_NONE;
	int32 OptionBId = INDEX_NONE;
};
