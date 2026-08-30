#include "UI/LobbyReturnDialogueWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

void ULobbyReturnDialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("LobbyReturnDialogueWidgetTree"));
	}
	if (!WidgetTree->RootWidget)
	{
		BuildInterface();
	}
}

void ULobbyReturnDialogueWidget::BuildInterface()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), TEXT("ReturnDialogueRoot"));
	WidgetTree->RootWidget = Root;

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("ReturnDialoguePanelSize"));
	PanelSize->SetWidthOverride(1200.0f);
	PanelSize->SetMinDesiredHeight(210.0f);
	if (UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelSize))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Bottom);
		PanelSlot->SetPadding(FMargin(45.0f, 35.0f, 45.0f, 55.0f));
	}

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("ReturnDialogueFrame"));
	Frame->SetBrushColor(FLinearColor(0.018f, 0.016f, 0.013f, 0.94f));
	Frame->SetPadding(FMargin(34.0f, 22.0f, 34.0f, 18.0f));
	PanelSize->AddChild(Frame);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("ReturnDialogueColumn"));
	Frame->SetContent(Column);

	UTextBlock* Speaker = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ReturnDialogueSpeaker"));
	Speaker->SetText(FText::FromString(TEXT("Arturo")));
	Speaker->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 27));
	Speaker->SetColorAndOpacity(
		FSlateColor(FLinearColor(1.0f, 0.78f, 0.26f, 1.0f)));
	if (UVerticalBoxSlot* SpeakerSlot = Column->AddChildToVerticalBox(Speaker))
	{
		SpeakerSlot->SetHorizontalAlignment(HAlign_Fill);
		SpeakerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	UTextBlock* Dialogue = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ReturnDialogueText"));
	Dialogue->SetText(FText::FromString(TEXT(
		"Gracias, Excalibur, por traerme atrás en el tiempo y permitirme enmendar mis errores.")));
	Dialogue->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 24));
	Dialogue->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Dialogue->SetAutoWrapText(true);
	Dialogue->SetWrapTextAt(1080.0f);
	if (UVerticalBoxSlot* DialogueSlot = Column->AddChildToVerticalBox(Dialogue))
	{
		DialogueSlot->SetHorizontalAlignment(HAlign_Fill);
		DialogueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ReturnDialogueHint"));
	Hint->SetText(FText::FromString(TEXT("Pulsa E para continuar")));
	Hint->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 15));
	Hint->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.62f, 0.64f, 0.68f, 1.0f)));
	Hint->SetJustification(ETextJustify::Right);
	if (UVerticalBoxSlot* HintSlot = Column->AddChildToVerticalBox(Hint))
	{
		HintSlot->SetHorizontalAlignment(HAlign_Fill);
		HintSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));
	}
}
