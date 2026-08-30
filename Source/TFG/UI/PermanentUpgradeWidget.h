#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Systems/RunPowerPersistenceSubsystem.h"

#include "PermanentUpgradeWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPermanentUpgradeMenuClosed);

UCLASS()
class TFG_API UPermanentUpgradeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnPermanentUpgradeMenuClosed OnMenuClosed;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> LightText;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> UpgradeTexts;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> UpgradeButtons;

	void BuildInterface();
	void AddUpgradeRow(UVerticalBox* Parent, EPermanentUpgradeType Type);
	void RefreshInterface();
	void TryPurchase(EPermanentUpgradeType Type);
	FText GetUpgradeDescription(EPermanentUpgradeType Type) const;

	UFUNCTION()
	void BuyDamage();

	UFUNCTION()
	void BuySpeed();

	UFUNCTION()
	void BuyHealth();

	UFUNCTION()
	void BuyMana();

	UFUNCTION()
	void BuyHealthRegen();

	UFUNCTION()
	void BuyRevive();

	UFUNCTION()
	void CloseMenu();
};
