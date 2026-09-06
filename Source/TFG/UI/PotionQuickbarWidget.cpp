#include "UI/PotionQuickbarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Systems/RunPowerPersistenceSubsystem.h"

namespace PotionQuickbarUI
{
	FSlateFontInfo Font(const int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size);
	}
}

void UPotionQuickbarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree->RootWidget)
	{
		BuildInterface();
	}
}

void UPotionQuickbarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshCounts();
}

void UPotionQuickbarWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.2f)
	{
		RefreshAccumulator = 0.0f;
		RefreshCounts();
	}
}

void UPotionQuickbarWidget::BuildInterface()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PotionHUDRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PotionHUDFrame"));
	Frame->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.03f, 0.86f));
	Frame->SetPadding(FMargin(12.0f, 10.0f));
	UOverlaySlot* FrameSlot = Root->AddChildToOverlay(Frame);
	FrameSlot->SetHorizontalAlignment(HAlign_Left);
	FrameSlot->SetVerticalAlignment(VAlign_Bottom);
	const bool bFinalArena = GetWorld() &&
		GetWorld()->GetMapName().Contains(TEXT("Combate_Final_Mordred"));
	FrameSlot->SetPadding(FMargin(
		26.0f,
		0.0f,
		0.0f,
		bFinalArena ? 180.0f : 24.0f));

	UVerticalBox* PotionPanel = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("PotionPanel"));
	Frame->SetContent(PotionPanel);

	UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PotionHeader"));
	Header->SetText(FText::FromString(TEXT("POCIONES")));
	Header->SetFont(PotionQuickbarUI::Font(16));
	Header->SetJustification(ETextJustify::Center);
	Header->SetColorAndOpacity(FLinearColor(1.0f, 0.82f, 0.28f, 1.0f));
	PotionPanel->AddChildToVerticalBox(Header)->SetPadding(
		FMargin(0.0f, 0.0f, 0.0f, 4.0f));

	UHorizontalBox* PotionRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("PotionRow"));
	PotionPanel->AddChildToVerticalBox(PotionRow);

	AddPotionSlot(PotionRow, FText::FromString(TEXT("Z")), FText::FromString(TEXT("V")),
		FLinearColor(0.65f, 0.035f, 0.025f, 1.0f));
	AddPotionSlot(PotionRow, FText::FromString(TEXT("X")), FText::FromString(TEXT("E")),
		FLinearColor(0.03f, 0.48f, 0.12f, 1.0f));
	AddPotionSlot(PotionRow, FText::FromString(TEXT("C")), FText::FromString(TEXT("M")),
		FLinearColor(0.025f, 0.18f, 0.72f, 1.0f));
}

void UPotionQuickbarWidget::AddPotionSlot(
	UHorizontalBox* Parent,
	const FText& Key,
	const FText& Placeholder,
	const FLinearColor& Color)
{
	UVerticalBox* SlotPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBoxSlot* PanelSlot = Parent->AddChildToHorizontalBox(SlotPanel);
	PanelSlot->SetPadding(FMargin(5.0f));

	UBorder* IconBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	IconBorder->SetBrushColor(Color);
	IconBorder->SetPadding(FMargin(3.0f));
	SlotPanel->AddChildToVerticalBox(IconBorder);

	USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconSize->SetWidthOverride(64.0f);
	IconSize->SetHeightOverride(64.0f);
	IconBorder->SetContent(IconSize);

	UOverlay* IconOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	IconSize->SetContent(IconOverlay);

	UTextBlock* PlaceholderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	PlaceholderText->SetText(Placeholder);
	PlaceholderText->SetFont(PotionQuickbarUI::Font(30));
	PlaceholderText->SetJustification(ETextJustify::Center);
	UOverlaySlot* PlaceholderSlot = IconOverlay->AddChildToOverlay(PlaceholderText);
	PlaceholderSlot->SetHorizontalAlignment(HAlign_Center);
	PlaceholderSlot->SetVerticalAlignment(VAlign_Center);

	UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	IconImage->SetVisibility(ESlateVisibility::Hidden);
	UOverlaySlot* ImageSlot = IconOverlay->AddChildToOverlay(IconImage);
	ImageSlot->SetHorizontalAlignment(HAlign_Fill);
	ImageSlot->SetVerticalAlignment(VAlign_Fill);
	IconImages.Add(IconImage);

	UTextBlock* KeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	KeyText->SetText(Key);
	KeyText->SetFont(PotionQuickbarUI::Font(20));
	KeyText->SetJustification(ETextJustify::Center);
	KeyText->SetColorAndOpacity(FLinearColor(1.0f, 0.82f, 0.28f, 1.0f));
	SlotPanel->AddChildToVerticalBox(KeyText)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));

	UTextBlock* CountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CountText->SetText(FText::FromString(TEXT("x 0")));
	CountText->SetFont(PotionQuickbarUI::Font(20));
	CountText->SetJustification(ETextJustify::Center);
	CountText->SetColorAndOpacity(FLinearColor::White);
	CountText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	CountText->SetShadowColorAndOpacity(FLinearColor::Black);
	SlotPanel->AddChildToVerticalBox(CountText);
	CountTexts.Add(CountText);
}

void UPotionQuickbarWidget::SetIconTextures(
	UTexture2D* HealthTexture,
	UTexture2D* EnergyTexture,
	UTexture2D* ManaTexture)
{
	const TArray<UTexture2D*> Textures = { HealthTexture, EnergyTexture, ManaTexture };
	for (int32 Index = 0; Index < IconImages.Num() && Index < Textures.Num(); ++Index)
	{
		if (Textures[Index])
		{
			IconImages[Index]->SetBrushFromTexture(Textures[Index], true);
			IconImages[Index]->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			IconImages[Index]->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UPotionQuickbarWidget::RefreshCounts()
{
	if (CountTexts.Num() >= 3)
	{
		UGameInstance* GameInstance = GetGameInstance();
		const URunPowerPersistenceSubsystem* Persistence =
			GameInstance ? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>() : nullptr;
		if (Persistence)
		{
			CountTexts[0]->SetText(FText::FromString(
				FString::Printf(TEXT("x %d"), Persistence->GetPotionCount(EPotionType::Vida))));
			CountTexts[1]->SetText(FText::FromString(
				FString::Printf(TEXT("x %d"), Persistence->GetPotionCount(EPotionType::Energia))));
			CountTexts[2]->SetText(FText::FromString(
				FString::Printf(TEXT("x %d"), Persistence->GetPotionCount(EPotionType::Mana))));
		}
	}
}
