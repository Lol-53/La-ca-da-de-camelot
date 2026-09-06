#include "UI/AlchemyCraftingWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Styling/CoreStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogAlchemyCraftingUI, Log, All);

namespace AlchemyCraftingUI
{
	FSlateFontInfo Font(const int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
	}

	FLinearColor Gold()
	{
		return FLinearColor(0.95f, 0.67f, 0.18f, 1.0f);
	}
}

void UAlchemyCraftingWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (!WidgetTree->RootWidget)
	{
		BuildInterface();
	}
}

void UAlchemyCraftingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshInterface();
}

void UAlchemyCraftingWidget::BuildInterface()
{
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AlchemyBackground"));
	Background->SetBrushColor(FLinearColor(0.012f, 0.025f, 0.025f, 0.82f));
	Background->SetPadding(FMargin(38.0f));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Background;

	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AlchemyPanel"));
	Background->SetContent(Panel);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("MESA DE ALQUIMIA")));
	Title->SetFont(AlchemyCraftingUI::Font(34));
	Title->SetColorAndOpacity(AlchemyCraftingUI::Gold());
	Title->SetJustification(ETextJustify::Center);
	Panel->AddChildToVerticalBox(Title)->SetPadding(FMargin(10.0f));

	UTextBlock* Subtitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Subtitle->SetText(FText::FromString(TEXT("TRANSFORMA LOS ELEMENTOS DE LOS ENEMIGOS EN POCIONES")));
	Subtitle->SetFont(AlchemyCraftingUI::Font(17));
	Subtitle->SetColorAndOpacity(FLinearColor(0.72f, 0.82f, 0.78f, 1.0f));
	Subtitle->SetJustification(ETextJustify::Center);
	Panel->AddChildToVerticalBox(Subtitle)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	ResourceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AlchemyResources"));
	ResourceText->SetFont(AlchemyCraftingUI::Font(20));
	ResourceText->SetJustification(ETextJustify::Center);
	ResourceText->SetColorAndOpacity(FLinearColor::White);
	Panel->AddChildToVerticalBox(ResourceText)->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 16.0f));

	AddRecipeRow(Panel, EPotionType::Vida);
	AddRecipeRow(Panel, EPotionType::Mana);
	AddRecipeRow(Panel, EPotionType::Energia);

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("AlchemyCloseButton"));
	CloseButton->SetBackgroundColor(FLinearColor(0.30f, 0.08f, 0.07f, 1.0f));
	CloseButton->OnClicked.AddDynamic(this, &UAlchemyCraftingWidget::CloseMenu);
	UTextBlock* CloseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CloseText->SetText(FText::FromString(TEXT("CERRAR")));
	CloseText->SetFont(AlchemyCraftingUI::Font(20));
	CloseText->SetJustification(ETextJustify::Center);
	CloseButton->AddChild(CloseText);
	Panel->AddChildToVerticalBox(CloseButton)->SetPadding(FMargin(90.0f, 14.0f, 90.0f, 0.0f));
}

void UAlchemyCraftingWidget::AddRecipeRow(UVerticalBox* Parent, const EPotionType Type)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(FLinearColor(0.08f, 0.15f, 0.13f, 1.0f));

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(AlchemyCraftingUI::Font(19));
	Text->SetJustification(ETextJustify::Center);
	Text->SetAutoWrapText(true);
	Button->AddChild(Text);

	switch (Type)
	{
	case EPotionType::Vida: Button->OnClicked.AddDynamic(this, &UAlchemyCraftingWidget::CraftHealthPotion); break;
	case EPotionType::Mana: Button->OnClicked.AddDynamic(this, &UAlchemyCraftingWidget::CraftManaPotion); break;
	case EPotionType::Energia: Button->OnClicked.AddDynamic(this, &UAlchemyCraftingWidget::CraftEnergyPotion); break;
	default: break;
	}

	RecipeButtons.Add(Button);
	RecipeTexts.Add(Text);
	Parent->AddChildToVerticalBox(Button)->SetPadding(FMargin(5.0f));
}

FText UAlchemyCraftingWidget::GetRecipeDescription(const EPotionType Type) const
{
	FText Description;
	switch (Type)
	{
	case EPotionType::Vida: Description = FText::FromString(TEXT("POCION DE VIDA")); break;
	case EPotionType::Mana: Description = FText::FromString(TEXT("POCION DE MANA")); break;
	case EPotionType::Energia: Description = FText::FromString(TEXT("POCION DE ENERGIA")); break;
	default: Description = FText::GetEmpty(); break;
	}
	return Description;
}

void UAlchemyCraftingWidget::RefreshInterface()
{
	UGameInstance* GameInstance = GetGameInstance();
	URunPowerPersistenceSubsystem* Persistence =
		GameInstance ? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>() : nullptr;
	if (!Persistence || !ResourceText)
	{
		return;
	}

	ResourceText->SetText(FText::FromString(FString::Printf(
		TEXT("ELEMENTOS   Vida: %d     Mana: %d     Energia: %d"),
		Persistence->GetCollectibleCount(EEnemyCollectibleType::Vida),
		Persistence->GetCollectibleCount(EEnemyCollectibleType::Mana),
		Persistence->GetCollectibleCount(EEnemyCollectibleType::Energia))));

	for (int32 Index = 0; Index < RecipeButtons.Num(); ++Index)
	{
		const EPotionType Type = static_cast<EPotionType>(Index);
		RecipeTexts[Index]->SetText(FText::FromString(FString::Printf(
			TEXT("%s\nCoste: %d elementos     En inventario: %d"),
			*GetRecipeDescription(Type).ToString(),
			Persistence->GetPotionCraftCost(Type),
			Persistence->GetPotionCount(Type))));
		RecipeButtons[Index]->SetBackgroundColor(
			Persistence->CanCraftPotion(Type)
				? FLinearColor(0.12f, 0.30f, 0.18f, 1.0f)
				: FLinearColor(0.12f, 0.13f, 0.14f, 1.0f));
	}
}

void UAlchemyCraftingWidget::TryCraft(const EPotionType Type)
{
	UGameInstance* GameInstance = GetGameInstance();
	URunPowerPersistenceSubsystem* Persistence =
		GameInstance ? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>() : nullptr;
	const bool bCrafted = Persistence && Persistence->CraftPotion(Type);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 3.0f,
			bCrafted ? FColor::Green : FColor::Red,
			bCrafted
				? TEXT("Pocion fabricada y guardada en el inventario")
				: TEXT("No tienes suficientes elementos para esta pocion"));
	}
	UE_LOG(LogAlchemyCraftingUI, Display, TEXT("[ALQUIMIA] Fabricacion tipo=%d: %s."),
		static_cast<int32>(Type), bCrafted ? TEXT("CORRECTA") : TEXT("RECHAZADA"));
	RefreshInterface();
}

void UAlchemyCraftingWidget::CraftHealthPotion() { TryCraft(EPotionType::Vida); }
void UAlchemyCraftingWidget::CraftManaPotion() { TryCraft(EPotionType::Mana); }
void UAlchemyCraftingWidget::CraftEnergyPotion() { TryCraft(EPotionType::Energia); }

void UAlchemyCraftingWidget::CloseMenu()
{
	OnMenuClosed.Broadcast();
}
