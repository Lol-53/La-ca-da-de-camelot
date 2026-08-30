#include "UI/PermanentUpgradeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Styling/CoreStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogPermanentUpgradeUI, Log, All);

namespace PermanentUpgradeUI
{
	FSlateFontInfo Font(const int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
	}

	FLinearColor Gold()
	{
		return FLinearColor(1.0f, 0.68f, 0.12f, 1.0f);
	}
}

void UPermanentUpgradeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (!WidgetTree->RootWidget)
	{
		BuildInterface();
	}
}

void UPermanentUpgradeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshInterface();
}

void UPermanentUpgradeWidget::BuildInterface()
{
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PermanentUpgradeBackground"));
	Background->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.035f, 0.82f));
	Background->SetPadding(FMargin(34.0f));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Background;

	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("UpgradePanel"));
	Background->SetContent(Panel);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("FUENTE DE LUZ")));
	Title->SetFont(PermanentUpgradeUI::Font(34));
	Title->SetColorAndOpacity(PermanentUpgradeUI::Gold());
	Title->SetJustification(ETextJustify::Center);
	Panel->AddChildToVerticalBox(Title)->SetPadding(FMargin(12.0f));

	UTextBlock* Subtitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Subtitle->SetText(FText::FromString(TEXT("MEJORAS PERMANENTES")));
	Subtitle->SetFont(PermanentUpgradeUI::Font(19));
	Subtitle->SetColorAndOpacity(FLinearColor(0.78f, 0.82f, 0.9f, 1.0f));
	Subtitle->SetJustification(ETextJustify::Center);
	Panel->AddChildToVerticalBox(Subtitle)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	LightText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LightCount"));
	LightText->SetFont(PermanentUpgradeUI::Font(24));
	LightText->SetColorAndOpacity(PermanentUpgradeUI::Gold());
	LightText->SetJustification(ETextJustify::Center);
	Panel->AddChildToVerticalBox(LightText)->SetPadding(FMargin(6.0f, 0.0f, 6.0f, 16.0f));

	AddUpgradeRow(Panel, EPermanentUpgradeType::Danyo);
	AddUpgradeRow(Panel, EPermanentUpgradeType::Velocidad);
	AddUpgradeRow(Panel, EPermanentUpgradeType::Vida);
	AddUpgradeRow(Panel, EPermanentUpgradeType::Mana);
	AddUpgradeRow(Panel, EPermanentUpgradeType::RegeneracionVida);
	AddUpgradeRow(Panel, EPermanentUpgradeType::Resurreccion);

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	CloseButton->SetBackgroundColor(FLinearColor(0.32f, 0.08f, 0.07f, 1.0f));
	CloseButton->OnClicked.AddDynamic(this, &UPermanentUpgradeWidget::CloseMenu);
	UTextBlock* CloseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CloseText->SetText(FText::FromString(TEXT("CERRAR")));
	CloseText->SetFont(PermanentUpgradeUI::Font(20));
	CloseText->SetJustification(ETextJustify::Center);
	CloseButton->AddChild(CloseText);
	Panel->AddChildToVerticalBox(CloseButton)->SetPadding(FMargin(90.0f, 20.0f, 90.0f, 0.0f));
}

void UPermanentUpgradeWidget::AddUpgradeRow(UVerticalBox* Parent, const EPermanentUpgradeType Type)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(FLinearColor(0.10f, 0.13f, 0.19f, 1.0f));

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(PermanentUpgradeUI::Font(18));
	Text->SetColorAndOpacity(FLinearColor::White);
	Text->SetJustification(ETextJustify::Center);
	Text->SetAutoWrapText(true);
	Button->AddChild(Text);

	switch (Type)
	{
	case EPermanentUpgradeType::Danyo: Button->OnClicked.AddDynamic(this, &UPermanentUpgradeWidget::BuyDamage); break;
	case EPermanentUpgradeType::Velocidad: Button->OnClicked.AddDynamic(this, &UPermanentUpgradeWidget::BuySpeed); break;
	case EPermanentUpgradeType::Vida: Button->OnClicked.AddDynamic(this, &UPermanentUpgradeWidget::BuyHealth); break;
	case EPermanentUpgradeType::Mana: Button->OnClicked.AddDynamic(this, &UPermanentUpgradeWidget::BuyMana); break;
	case EPermanentUpgradeType::RegeneracionVida: Button->OnClicked.AddDynamic(this, &UPermanentUpgradeWidget::BuyHealthRegen); break;
	case EPermanentUpgradeType::Resurreccion: Button->OnClicked.AddDynamic(this, &UPermanentUpgradeWidget::BuyRevive); break;
	default: break;
	}

	UpgradeButtons.Add(Button);
	UpgradeTexts.Add(Text);
	Parent->AddChildToVerticalBox(Button)->SetPadding(FMargin(5.0f));
}

FText UPermanentUpgradeWidget::GetUpgradeDescription(const EPermanentUpgradeType Type) const
{
	switch (Type)
	{
	case EPermanentUpgradeType::Danyo: return FText::FromString(TEXT("DANO  +5% por nivel"));
	case EPermanentUpgradeType::Velocidad: return FText::FromString(TEXT("VELOCIDAD  +3% por nivel"));
	case EPermanentUpgradeType::Vida: return FText::FromString(TEXT("VIDA MAXIMA  +10 por nivel"));
	case EPermanentUpgradeType::Mana: return FText::FromString(TEXT("MANA MAXIMO  +10 por nivel"));
	case EPermanentUpgradeType::RegeneracionVida: return FText::FromString(TEXT("REGENERACION  +2 vida por segundo"));
	case EPermanentUpgradeType::Resurreccion: return FText::FromString(TEXT("RESURRECCION  una vez por partida"));
	default: return FText::GetEmpty();
	}
}

void UPermanentUpgradeWidget::RefreshInterface()
{
	UGameInstance* GameInstance = GetGameInstance();
	URunPowerPersistenceSubsystem* Persistence =
		GameInstance ? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>() : nullptr;
	if (!Persistence || !LightText)
	{
		return;
	}

	LightText->SetText(FText::FromString(FString::Printf(TEXT("LUZ DISPONIBLE: %d"), Persistence->GetLightCount())));

	for (int32 Index = 0; Index < UpgradeButtons.Num(); ++Index)
	{
		const EPermanentUpgradeType Type = static_cast<EPermanentUpgradeType>(Index);
		const int32 Level = Persistence->GetPermanentUpgradeLevel(Type);
		const int32 MaxLevel = Persistence->GetPermanentUpgradeMaxLevel(Type);
		const bool bCompleted = Level >= MaxLevel;
		const FString State = bCompleted
			? TEXT("DESBLOQUEADO / MAXIMO")
			: FString::Printf(TEXT("Nivel %d/%d  -  Coste: %d Luz"),
				Level, MaxLevel, Persistence->GetPermanentUpgradeCost(Type));
		UpgradeTexts[Index]->SetText(FText::FromString(
			FString::Printf(TEXT("%s\n%s"), *GetUpgradeDescription(Type).ToString(), *State)));
		UpgradeButtons[Index]->SetIsEnabled(!bCompleted);
		UpgradeButtons[Index]->SetBackgroundColor(
			Persistence->CanPurchasePermanentUpgrade(Type)
				? FLinearColor(0.16f, 0.29f, 0.20f, 1.0f)
				: FLinearColor(0.10f, 0.13f, 0.19f, 1.0f));
	}
}

void UPermanentUpgradeWidget::TryPurchase(const EPermanentUpgradeType Type)
{
	UGameInstance* GameInstance = GetGameInstance();
	URunPowerPersistenceSubsystem* Persistence =
		GameInstance ? GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>() : nullptr;
	const bool bPurchased = Persistence && Persistence->PurchasePermanentUpgrade(Type);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 3.0f,
			bPurchased ? FColor::Green : FColor::Red,
			bPurchased
				? TEXT("Mejora permanente comprada")
				: TEXT("No tienes suficiente Luz o la mejora ya esta completa"));
	}
	UE_LOG(LogPermanentUpgradeUI, Display, TEXT("[FUENTE DE LUZ] Compra tipo=%d: %s."),
		static_cast<int32>(Type), bPurchased ? TEXT("CORRECTA") : TEXT("RECHAZADA"));
	RefreshInterface();
}

void UPermanentUpgradeWidget::BuyDamage() { TryPurchase(EPermanentUpgradeType::Danyo); }
void UPermanentUpgradeWidget::BuySpeed() { TryPurchase(EPermanentUpgradeType::Velocidad); }
void UPermanentUpgradeWidget::BuyHealth() { TryPurchase(EPermanentUpgradeType::Vida); }
void UPermanentUpgradeWidget::BuyMana() { TryPurchase(EPermanentUpgradeType::Mana); }
void UPermanentUpgradeWidget::BuyHealthRegen() { TryPurchase(EPermanentUpgradeType::RegeneracionVida); }
void UPermanentUpgradeWidget::BuyRevive() { TryPurchase(EPermanentUpgradeType::Resurreccion); }

void UPermanentUpgradeWidget::CloseMenu()
{
	OnMenuClosed.Broadcast();
}
