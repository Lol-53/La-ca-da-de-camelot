#include "UI/RoomPowerUpSelectionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

namespace RoomPowerUpWidget
{
	static UTextBlock* CreateText(
		UWidgetTree* WidgetTree,
		const FName Name,
		const int32 FontSize,
		const FLinearColor Color,
		const ETextJustify::Type Justification)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FontSize));
		Text->SetJustification(Justification);
		Text->SetAutoWrapText(true);
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		Text->SetShadowOffset(FVector2D(1.5f, 1.5f));
		return Text;
	}
}

void URoomPowerUpSelectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("PowerUpWidgetTree"));
	}
	if (!WidgetTree->RootWidget)
	{
		BuildInterface();
	}
	RefreshOptionText();
}

void URoomPowerUpSelectionWidget::Configure(
	const int32 InOptionAId,
	const FText& InOptionATitle,
	const FText& InOptionADescription,
	const int32 InOptionBId,
	const FText& InOptionBTitle,
	const FText& InOptionBDescription)
{
	OptionAId = InOptionAId;
	OptionATitle = InOptionATitle;
	OptionADescription = InOptionADescription;
	OptionBId = InOptionBId;
	OptionBTitle = InOptionBTitle;
	OptionBDescription = InOptionBDescription;
	RefreshOptionText();
}

void URoomPowerUpSelectionWidget::BuildInterface()
{
	using namespace RoomPowerUpWidget;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		TEXT("PowerUpSelectionRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* ScreenShade = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("ScreenShade"));
	ScreenShade->SetBrushColor(FLinearColor::Transparent);
	if (UOverlaySlot* ShadeSlot = Root->AddChildToOverlay(ScreenShade))
	{
		ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
		ShadeSlot->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox* ContentSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("SelectionContentSize"));
	ContentSize->SetWidthOverride(1120.0f);
	ContentSize->SetHeightOverride(540.0f);
	if (UOverlaySlot* ContentOverlaySlot = Root->AddChildToOverlay(ContentSize))
	{
		ContentOverlaySlot->SetHorizontalAlignment(HAlign_Center);
		ContentOverlaySlot->SetVerticalAlignment(VAlign_Center);
		ContentOverlaySlot->SetPadding(FMargin(30.0f));
	}

	UBorder* ContentFrame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("SelectionContentFrame"));
	ContentFrame->SetBrushColor(FLinearColor::Transparent);
	ContentFrame->SetPadding(FMargin(50.0f, 34.0f));
	ContentSize->AddChild(ContentFrame);

	UVerticalBox* MainColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("SelectionMainColumn"));
	ContentFrame->SetContent(MainColumn);

	UTextBlock* Heading = CreateText(
		WidgetTree,
		TEXT("SelectionHeading"),
		38,
		FLinearColor(0.96f, 0.78f, 0.28f, 1.0f),
		ETextJustify::Center);
	Heading->SetText(FText::FromString(TEXT("ELIGE UN POWER-UP")));
	if (UVerticalBoxSlot* HeadingSlot = MainColumn->AddChildToVerticalBox(Heading))
	{
		HeadingSlot->SetHorizontalAlignment(HAlign_Fill);
		HeadingSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 34.0f));
	}

	UHorizontalBox* CardsRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("PowerUpCards"));
	if (UVerticalBoxSlot* CardsSlot = MainColumn->AddChildToVerticalBox(CardsRow))
	{
		CardsSlot->SetHorizontalAlignment(HAlign_Center);
	}

	auto BuildCard = [this, CardsRow](
		const FName Prefix,
		const FLinearColor ButtonColor,
		const FLinearColor TextColor,
		TObjectPtr<UButton>& OutButton,
		TObjectPtr<UTextBlock>& OutTitle,
		TObjectPtr<UTextBlock>& OutDescription)
	{
		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("%sSize"), *Prefix.ToString()));
		CardSize->SetWidthOverride(455.0f);
		CardSize->SetHeightOverride(340.0f);
		if (UHorizontalBoxSlot* CardRowSlot = CardsRow->AddChildToHorizontalBox(CardSize))
		{
			CardRowSlot->SetPadding(FMargin(14.0f, 0.0f));
			CardRowSlot->SetVerticalAlignment(VAlign_Fill);
		}

		OutButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			*FString::Printf(TEXT("%sButton"), *Prefix.ToString()));
		OutButton->SetBackgroundColor(ButtonColor);
		CardSize->AddChild(OutButton);

		UVerticalBox* CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			*FString::Printf(TEXT("%sColumn"), *Prefix.ToString()));
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(
			OutButton->AddChild(CardColumn)))
		{
			ButtonSlot->SetPadding(FMargin(30.0f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UTextBlock* ChoiceLabel = RoomPowerUpWidget::CreateText(
			WidgetTree,
			*FString::Printf(TEXT("%sChoiceLabel"), *Prefix.ToString()),
			16,
			TextColor,
			ETextJustify::Center);
		ChoiceLabel->SetShadowColorAndOpacity(FLinearColor::Transparent);
		ChoiceLabel->SetText(FText::FromString(TEXT("SELECCIONAR")));
		if (UVerticalBoxSlot* ChoiceSlot = CardColumn->AddChildToVerticalBox(ChoiceLabel))
		{
			ChoiceSlot->SetHorizontalAlignment(HAlign_Fill);
			ChoiceSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 22.0f));
		}

		OutTitle = RoomPowerUpWidget::CreateText(
			WidgetTree,
			*FString::Printf(TEXT("%sTitle"), *Prefix.ToString()),
			27,
			TextColor,
			ETextJustify::Center);
		OutTitle->SetShadowColorAndOpacity(FLinearColor::Transparent);
		if (UVerticalBoxSlot* TitleSlot = CardColumn->AddChildToVerticalBox(OutTitle))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Fill);
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
		}

		OutDescription = RoomPowerUpWidget::CreateText(
			WidgetTree,
			*FString::Printf(TEXT("%sDescription"), *Prefix.ToString()),
			18,
			TextColor,
			ETextJustify::Center);
		OutDescription->SetShadowColorAndOpacity(FLinearColor::Transparent);
		if (UVerticalBoxSlot* DescriptionSlot =
			CardColumn->AddChildToVerticalBox(OutDescription))
		{
			DescriptionSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	};

	BuildCard(
		TEXT("OptionA"),
		FLinearColor(0.94f, 0.94f, 0.91f, 1.0f),
		FLinearColor(0.055f, 0.06f, 0.07f, 1.0f),
		OptionAButton,
		OptionATitleText,
		OptionADescriptionText);
	BuildCard(
		TEXT("OptionB"),
		FLinearColor(0.82f, 0.60f, 0.17f, 1.0f),
		FLinearColor(0.055f, 0.045f, 0.025f, 1.0f),
		OptionBButton,
		OptionBTitleText,
		OptionBDescriptionText);

	OptionAButton->OnClicked.AddDynamic(this, &URoomPowerUpSelectionWidget::ChooseOptionA);
	OptionBButton->OnClicked.AddDynamic(this, &URoomPowerUpSelectionWidget::ChooseOptionB);
}

void URoomPowerUpSelectionWidget::RefreshOptionText()
{
	if (OptionATitleText)
	{
		OptionATitleText->SetText(OptionATitle);
	}
	if (OptionADescriptionText)
	{
		OptionADescriptionText->SetText(OptionADescription);
	}
	if (OptionBTitleText)
	{
		OptionBTitleText->SetText(OptionBTitle);
	}
	if (OptionBDescriptionText)
	{
		OptionBDescriptionText->SetText(OptionBDescription);
	}
}

void URoomPowerUpSelectionWidget::ChooseOptionA()
{
	if (OptionAId != INDEX_NONE)
	{
		OnPowerUpChosen.Broadcast(OptionAId);
	}
}

void URoomPowerUpSelectionWidget::ChooseOptionB()
{
	if (OptionBId != INDEX_NONE)
	{
		OnPowerUpChosen.Broadcast(OptionBId);
	}
}
