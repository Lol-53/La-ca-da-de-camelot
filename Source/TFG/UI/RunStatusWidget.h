#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RunStatusWidget.generated.h"

class UTextBlock;
class UVerticalBox;

/** Read-only overlay that summarizes the current procedural run. */
UCLASS()
class TFG_API URunStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Configure(int32 RoomNumber, const TArray<FString>& ActivePowerNames);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildInterface();
	void RefreshPowerList(const TArray<FString>& ActivePowerNames);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RoomText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PowerList;
};
