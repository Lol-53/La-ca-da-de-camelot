#include "UI/MordredBossHealthWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

void UMordredBossHealthWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("MordredBossWidgetTree"));
	}
	if (!WidgetTree->RootWidget)
	{
		BuildInterface();
	}
}

void UMordredBossHealthWidget::BuildInterface()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		TEXT("BossHealthRoot"));
	WidgetTree->RootWidget = Root;

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("BossHealthPanelSize"));
	PanelSize->SetWidthOverride(980.0f);
	PanelSize->SetHeightOverride(112.0f);
	if (UOverlaySlot* PanelOverlaySlot = Root->AddChildToOverlay(PanelSize))
	{
		PanelOverlaySlot->SetHorizontalAlignment(HAlign_Center);
		PanelOverlaySlot->SetVerticalAlignment(VAlign_Bottom);
		PanelOverlaySlot->SetPadding(FMargin(30.0f, 0.0f, 30.0f, 42.0f));
	}

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("BossHealthFrame"));
	Frame->SetBrushColor(FLinearColor(0.018f, 0.022f, 0.030f, 0.96f));
	Frame->SetPadding(FMargin(20.0f, 10.0f));
	PanelSize->AddChild(Frame);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("BossHealthColumn"));
	Frame->SetContent(Column);

	UTextBlock* BossName = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("BossName"));
	BossName->SetText(FText::FromString(TEXT("MORDRED")));
	BossName->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24));
	BossName->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.68f, 0.18f, 1.0f)));
	BossName->SetJustification(ETextJustify::Center);
	BossName->SetShadowColorAndOpacity(FLinearColor::Black);
	BossName->SetShadowOffset(FVector2D(1.5f, 1.5f));
	if (UVerticalBoxSlot* NameSlot = Column->AddChildToVerticalBox(BossName))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Fill);
		NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	HealthProgress = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(),
		TEXT("BossHealthProgress"));
	HealthProgress->SetPercent(1.0f);
	HealthProgress->SetFillColorAndOpacity(FLinearColor(0.72f, 0.025f, 0.035f, 1.0f));
	if (UVerticalBoxSlot* ProgressSlot = Column->AddChildToVerticalBox(HealthProgress))
	{
		ProgressSlot->SetHorizontalAlignment(HAlign_Fill);
		ProgressSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
	}

	HealthValueText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("BossHealthValue"));
	HealthValueText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14));
	HealthValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	HealthValueText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* ValueSlot = Column->AddChildToVerticalBox(HealthValueText))
	{
		ValueSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void UMordredBossHealthWidget::SetBossHealth(
	const float CurrentHealth,
	const float MaxHealth)
{
	const float Percentage = MaxHealth > 0.0f
		? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f)
		: 0.0f;
	if (HealthProgress)
	{
		HealthProgress->SetPercent(Percentage);
	}
	if (HealthValueText)
	{
		HealthValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%.0f / %.0f"),
			FMath::Max(0.0f, CurrentHealth),
			FMath::Max(0.0f, MaxHealth))));
	}
}
