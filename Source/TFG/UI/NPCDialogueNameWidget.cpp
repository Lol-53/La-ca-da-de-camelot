#include "UI/NPCDialogueNameWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UNPCDialogueNameWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* RootCanvas =
		WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* NameBorder =
		WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NameBorder"));
	NameBorder->SetBrushColor(FLinearColor(0.025f, 0.02f, 0.015f, 0.92f));
	NameBorder->SetPadding(FMargin(14.0f, 5.0f, 14.0f, 5.0f));

	NameText =
		WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NPCName"));
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.78f, 0.26f, 1.0f)));
	NameText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 27));
	NameText->SetShadowOffset(FVector2D(1.5f, 1.5f));
	NameText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
	NameText->SetText(PendingName);
	NameBorder->SetContent(NameText);

	if (UCanvasPanelSlot* NameSlot = RootCanvas->AddChildToCanvas(NameBorder))
	{
		// The existing dialogue border begins at X=72 and Y=796 on its
		// 1920x1080 design canvas. Anchoring from the lower-left keeps this
		// label aligned with its upper-left edge at other resolutions.
		NameSlot->SetAnchors(FAnchors(0.0f, 1.0f));
		// Keep the name above the dialogue text instead of sharing its first
		// line. The extra vertical separation also leaves room for larger fonts.
		NameSlot->SetPosition(FVector2D(88.0f, -337.0f));
		NameSlot->SetSize(FVector2D(520.0f, 48.0f));
		NameSlot->SetZOrder(1);
	}

	UTextBlock* ContinueHint =
		WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ContinueHint"));
	ContinueHint->SetText(FText::FromString(TEXT("Pulsa E para continuar")));
	ContinueHint->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.62f, 0.64f, 0.68f, 1.0f)));
	ContinueHint->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 15));
	ContinueHint->SetJustification(ETextJustify::Right);
	ContinueHint->SetShadowOffset(FVector2D(1.0f, 1.0f));
	ContinueHint->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
	if (UCanvasPanelSlot* HintSlot = RootCanvas->AddChildToCanvas(ContinueHint))
	{
		HintSlot->SetAnchors(FAnchors(1.0f, 1.0f));
		HintSlot->SetAlignment(FVector2D(1.0f, 1.0f));
		HintSlot->SetPosition(FVector2D(-88.0f, -54.0f));
		HintSlot->SetSize(FVector2D(360.0f, 28.0f));
		HintSlot->SetZOrder(1);
	}
}

void UNPCDialogueNameWidget::SetNPCName(const FText& InName)
{
	PendingName = InName;
	if (NameText)
	{
		NameText->SetText(PendingName);
	}
}
