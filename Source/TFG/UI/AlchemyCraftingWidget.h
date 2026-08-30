#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Systems/RunPowerPersistenceSubsystem.h"

#include "AlchemyCraftingWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAlchemyMenuClosed);

UCLASS()
class TFG_API UAlchemyCraftingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnAlchemyMenuClosed OnMenuClosed;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> ResourceText;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> RecipeTexts;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> RecipeButtons;

	void BuildInterface();
	void AddRecipeRow(UVerticalBox* Parent, EPotionType Type);
	void RefreshInterface();
	void TryCraft(EPotionType Type);
	FText GetRecipeDescription(EPotionType Type) const;

	UFUNCTION()
	void CraftHealthPotion();

	UFUNCTION()
	void CraftManaPotion();

	UFUNCTION()
	void CraftEnergyPotion();

	UFUNCTION()
	void CloseMenu();
};
