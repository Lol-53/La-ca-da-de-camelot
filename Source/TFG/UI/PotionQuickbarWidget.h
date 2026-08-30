#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "PotionQuickbarWidget.generated.h"

class UHorizontalBox;
class UImage;
class UTextBlock;
class UTexture2D;

UCLASS()
class TFG_API UPotionQuickbarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetIconTextures(UTexture2D* HealthTexture, UTexture2D* EnergyTexture, UTexture2D* ManaTexture);
	void RefreshCounts();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TArray<TObjectPtr<UImage>> IconImages;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> CountTexts;

	float RefreshAccumulator = 0.0f;

	void BuildInterface();
	void AddPotionSlot(
		UHorizontalBox* Parent,
		const FText& Key,
		const FText& Placeholder,
		const FLinearColor& Color);
};
