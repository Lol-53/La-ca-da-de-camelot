#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MordredBossHealthWidget.generated.h"

class UProgressBar;
class UTextBlock;

/** Full-width final boss health bar shown at the bottom of the screen. */
UCLASS()
class TFG_API UMordredBossHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetBossHealth(float CurrentHealth, float MaxHealth);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildInterface();

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthProgress;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthValueText;
};
