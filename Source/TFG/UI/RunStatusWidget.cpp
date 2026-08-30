#include "UI/RunStatusWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

namespace RunStatusUI
{
	static UTextBlock* CreateText(
		UWidgetTree* Tree,
		const FName Name,
		const FString& Value,
		const int32 FontSize,
		const FLinearColor Color,
		const ETextJustify::Type Justification = ETextJustify::Left)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Value));
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FontSize));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justification);
		Text->SetAutoWrapText(true);
		Text->SetWrapTextAt(650.0f);
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		return Text;
	}
}

void URunStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("RunStatusWidgetTree"));
	}
	if (!WidgetTree->RootWidget)
	{
		BuildInterface();
	}
}

void URunStatusWidget::Configure(
	const int32 RoomNumber,
	const TArray<FString>& ActivePowerNames)
{
	if (RoomText)
	{
		RoomText->SetText(FText::FromString(FString::Printf(
			TEXT("SALA %d"), FMath::Max(1, RoomNumber))));
	}
	RefreshPowerList(ActivePowerNames);
}

void URunStatusWidget::BuildInterface()
{
	using namespace RunStatusUI;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), TEXT("RunStatusRoot"));
	WidgetTree->RootWidget = Root;

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("RunStatusPanelSize"));
	PanelSize->SetWidthOverride(720.0f);
	if (UOverlaySlot* PanelOverlaySlot = Root->AddChildToOverlay(PanelSize))
	{
		PanelOverlaySlot->SetHorizontalAlignment(HAlign_Center);
		PanelOverlaySlot->SetVerticalAlignment(VAlign_Center);
		PanelOverlaySlot->SetPadding(FMargin(30.0f));
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("RunStatusPanel"));
	Panel->SetBrushColor(FLinearColor(0.025f, 0.04f, 0.07f, 0.94f));
	Panel->SetPadding(FMargin(42.0f, 32.0f));
	PanelSize->AddChild(Panel);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("RunStatusColumn"));
	Panel->SetContent(Column);

	RoomText = CreateText(
		WidgetTree, TEXT("RoomNumber"), TEXT("SALA 1"), 40,
		FLinearColor(0.96f, 0.72f, 0.24f, 1.0f), ETextJustify::Center);
	if (UVerticalBoxSlot* RoomSlot = Column->AddChildToVerticalBox(RoomText))
	{
		RoomSlot->SetHorizontalAlignment(HAlign_Fill);
		RoomSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	UTextBlock* Heading = CreateText(
		WidgetTree, TEXT("PowersHeading"), TEXT("PODERES TEMPORALES ACTIVOS"), 22,
		FLinearColor(0.82f, 0.87f, 0.96f, 1.0f), ETextJustify::Center);
	if (UVerticalBoxSlot* HeadingSlot = Column->AddChildToVerticalBox(Heading))
	{
		HeadingSlot->SetHorizontalAlignment(HAlign_Fill);
		HeadingSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}

	PowerList = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("ActivePowerList"));
	if (UVerticalBoxSlot* PowerListSlot = Column->AddChildToVerticalBox(PowerList))
	{
		PowerListSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	UTextBlock* Hint = CreateText(
		WidgetTree, TEXT("RunStatusHint"), TEXT("TAB: CERRAR"), 14,
		FLinearColor(0.55f, 0.62f, 0.72f, 1.0f), ETextJustify::Center);
	if (UVerticalBoxSlot* HintSlot = Column->AddChildToVerticalBox(Hint))
	{
		HintSlot->SetHorizontalAlignment(HAlign_Fill);
		HintSlot->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));
	}
}

void URunStatusWidget::RefreshPowerList(
	const TArray<FString>& ActivePowerNames)
{
	if (!PowerList || !WidgetTree)
	{
		return;
	}

	PowerList->ClearChildren();
	if (ActivePowerNames.IsEmpty())
	{
		UTextBlock* EmptyText = RunStatusUI::CreateText(
			WidgetTree, TEXT("NoActivePowers"), TEXT("Todavia no tienes poderes temporales."),
			18, FLinearColor(0.68f, 0.72f, 0.78f, 1.0f), ETextJustify::Center);
		PowerList->AddChildToVerticalBox(EmptyText);
		return;
	}

	for (int32 Index = 0; Index < ActivePowerNames.Num(); ++Index)
	{
		UTextBlock* PowerText = RunStatusUI::CreateText(
			WidgetTree,
			*FString::Printf(TEXT("ActivePower_%d"), Index),
			FString::Printf(TEXT("• %s"), *ActivePowerNames[Index]),
			18,
			FLinearColor::White);
		if (UVerticalBoxSlot* PowerSlot = PowerList->AddChildToVerticalBox(PowerText))
		{
			PowerSlot->SetPadding(FMargin(8.0f, 4.0f));
		}
	}
}
