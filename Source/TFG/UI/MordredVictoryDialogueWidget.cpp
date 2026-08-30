#include "UI/MordredVictoryDialogueWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

void UMordredVictoryDialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("VictoryDialogueWidgetTree"));
	}
	if (!WidgetTree->RootWidget)
	{
		BuildInterface();
	}
}

void UMordredVictoryDialogueWidget::BuildInterface()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), TEXT("VictoryRoot"));
	WidgetTree->RootWidget = Root;

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("VictoryPanelSize"));
	PanelSize->SetWidthOverride(900.0f);
	PanelSize->SetMinDesiredHeight(245.0f);
	if (UOverlaySlot* PanelOverlaySlot = Root->AddChildToOverlay(PanelSize))
	{
		PanelOverlaySlot->SetHorizontalAlignment(HAlign_Center);
		PanelOverlaySlot->SetVerticalAlignment(VAlign_Center);
		PanelOverlaySlot->SetPadding(FMargin(35.0f));
	}

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("VictoryFrame"));
	Frame->SetBrushColor(FLinearColor(0.012f, 0.015f, 0.022f, 0.95f));
	Frame->SetPadding(FMargin(42.0f, 28.0f));
	PanelSize->AddChild(Frame);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("VictoryColumn"));
	Frame->SetContent(Column);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("VictoryTitle"));
	Title->SetText(FText::FromString(TEXT("Victoria")));
	Title->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 27));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.72f, 0.24f, 1.0f)));
	Title->SetJustification(ETextJustify::Left);
	if (UVerticalBoxSlot* TitleBoxSlot = Column->AddChildToVerticalBox(Title))
	{
		TitleBoxSlot->SetHorizontalAlignment(HAlign_Fill);
		TitleBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	DialogueText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("VictoryDialogueText"));
	DialogueText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 24));
	DialogueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	DialogueText->SetJustification(ETextJustify::Center);
	DialogueText->SetAutoWrapText(true);
	DialogueText->SetWrapTextAt(790.0f);
	if (UVerticalBoxSlot* DialogueBoxSlot = Column->AddChildToVerticalBox(DialogueText))
	{
		DialogueBoxSlot->SetHorizontalAlignment(HAlign_Fill);
		DialogueBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	ProgressText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("VictoryProgressText"));
	ProgressText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 15));
	ProgressText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.75f, 0.82f, 1.0f)));
	ProgressText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* ProgressBoxSlot = Column->AddChildToVerticalBox(ProgressText))
	{
		ProgressBoxSlot->SetHorizontalAlignment(HAlign_Fill);
		ProgressBoxSlot->SetPadding(FMargin(0.0f, 20.0f, 0.0f, 0.0f));
	}
}

void UMordredVictoryDialogueWidget::SetDialogueLine(
	const FString& Line,
	const int32 CurrentLine,
	const int32 TotalLines)
{
	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(Line));
	}
	if (ProgressText)
	{
		ProgressText->SetText(FText::FromString(FString::Printf(
			TEXT("Pulsa E para continuar   ·   %d/%d"),
			CurrentLine,
			TotalLines)));
	}
}
