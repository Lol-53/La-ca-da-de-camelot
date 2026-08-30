#include "UI/MainMenuWidget.h"

#include "Systems/RunPowerPersistenceSubsystem.h"
#include "UI/MainMenuManager.h"
#include "UI/PauseMenuComponent.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "Styling/CoreStyle.h"

namespace MainMenuUI
{
	static UTextBlock* Text(
		UWidgetTree* Tree, const FName Name, const FString& Value, const int32 Size,
		const FLinearColor Color = FLinearColor::White,
		const ETextJustify::Type Justification = ETextJustify::Center)
	{
		UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Result->SetText(FText::FromString(Value));
		Result->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size));
		Result->SetColorAndOpacity(FSlateColor(Color));
		Result->SetJustification(Justification);
		Result->SetAutoWrapText(true);
		Result->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
		Result->SetShadowOffset(FVector2D(2, 2));
		return Result;
	}

	static UButton* Button(
		UWidgetTree* Tree, UVerticalBox* Column, const FName Name,
		const FString& Label, TObjectPtr<UTextBlock>* OutLabel = nullptr,
		const FLinearColor Color = FLinearColor(0.08f, 0.20f, 0.34f, 0.97f))
	{
		UButton* Result = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Result->SetBackgroundColor(Color);
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Result))
		{
			Slot->SetPadding(FMargin(0, 6));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
		UTextBlock* LabelWidget = Text(Tree, *FString::Printf(TEXT("%sText"), *Name.ToString()), Label, 21);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Result->AddChild(LabelWidget)))
		{
			Slot->SetPadding(FMargin(24, 12));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
		if (OutLabel)
		{
			*OutLabel = LabelWidget;
		}
		return Result;
	}

	static UButton* MainButton(
		UWidgetTree* Tree, UVerticalBox* Column, const FName Name, const FString& Label)
	{
		UButton* Result = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);

		FSlateBrush NormalBrush;
		NormalBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		FSlateBrush HoveredBrush;
		HoveredBrush.DrawAs = ESlateBrushDrawType::Box;
		HoveredBrush.TintColor = FSlateColor(FLinearColor(0.88f, 0.89f, 0.88f, 0.94f));
		HoveredBrush.Margin = FMargin(0.08f);
		FSlateBrush PressedBrush = HoveredBrush;
		PressedBrush.TintColor = FSlateColor(FLinearColor(0.72f, 0.74f, 0.73f, 0.98f));

		FButtonStyle Style;
		Style.SetNormal(NormalBrush);
		Style.SetHovered(HoveredBrush);
		Style.SetPressed(PressedBrush);
		Style.SetDisabled(NormalBrush);
		Style.SetNormalForeground(FSlateColor(FLinearColor::White));
		Style.SetHoveredForeground(FSlateColor(FLinearColor::Black));
		Style.SetPressedForeground(FSlateColor(FLinearColor::Black));
		Result->SetStyle(Style);

		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Result))
		{
			Slot->SetPadding(FMargin(0, 15));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
		UTextBlock* LabelWidget = Text(
			Tree, *FString::Printf(TEXT("%sText"), *Name.ToString()), Label, 28);
		LabelWidget->SetColorAndOpacity(FSlateColor::UseForeground());
		LabelWidget->SetShadowColorAndOpacity(FLinearColor::Transparent);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Result->AddChild(LabelWidget)))
		{
			Slot->SetPadding(FMargin(28, 13));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
		return Result;
	}

	static void StyleSaveButton(UButton* Button)
	{
		if (!Button) return;

		FSlateBrush NormalBrush;
		NormalBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		FSlateBrush HoveredBrush;
		HoveredBrush.DrawAs = ESlateBrushDrawType::Box;
		HoveredBrush.TintColor = FSlateColor(FLinearColor(0.86f, 0.87f, 0.84f, 0.95f));
		HoveredBrush.Margin = FMargin(0.08f);
		FSlateBrush PressedBrush = HoveredBrush;
		PressedBrush.TintColor = FSlateColor(FLinearColor(0.68f, 0.70f, 0.68f, 0.98f));

		FButtonStyle Style;
		Style.SetNormal(NormalBrush);
		Style.SetHovered(HoveredBrush);
		Style.SetPressed(PressedBrush);
		Style.SetDisabled(NormalBrush);
		Style.SetNormalForeground(FSlateColor(FLinearColor(0.96f, 0.79f, 0.40f, 1)));
		Style.SetHoveredForeground(FSlateColor(FLinearColor::Black));
		Style.SetPressedForeground(FSlateColor(FLinearColor::Black));
		Button->SetStyle(Style);
	}

	static void Heading(UWidgetTree* Tree, UVerticalBox* Column, const FString& Value)
	{
		UTextBlock* Result = Text(Tree, *FString::Printf(TEXT("%sHeading"), *Value), Value, 34,
			FLinearColor(0.95f, 0.72f, 0.25f, 1));
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Result))
		{
			Slot->SetPadding(FMargin(0, 0, 0, 22));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	static UHorizontalBox* SettingRow(
		UWidgetTree* Tree, UVerticalBox* Column, const FName Name, const FString& Label)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), Name);
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Row))
		{
			Slot->SetPadding(FMargin(0, 5));
		}
		UTextBlock* LabelText = Text(Tree, *FString::Printf(TEXT("%sLabel"), *Name.ToString()), Label, 15,
			FLinearColor(0.82f, 0.87f, 0.94f, 1), ETextJustify::Left);
		if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(LabelText))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Row;
	}
}

void UMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("MainMenuWidgetTree"));
	}
	if (!WidgetTree->RootWidget)
	{
		BuildInterface();
	}
}

void UMainMenuWidget::Configure(
	AMainMenuManager* InManager, UTexture2D* InBackground, const FText& InCredits)
{
	Manager = InManager;
	if (BackgroundImageWidget && InBackground)
	{
		BackgroundImageWidget->SetBrushFromTexture(InBackground, true);
		BackgroundImageWidget->SetColorAndOpacity(FLinearColor::White);
	}
	if (CreditsBody)
	{
		CreditsBody->SetText(InCredits);
	}
	RefreshSaveSlots();
	RefreshSettings();
	ShowMainPage();
}

void UMainMenuWidget::BuildInterface()
{
	using namespace MainMenuUI;
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MainMenuRoot"));
	WidgetTree->RootWidget = Root;

	BackgroundImageWidget = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackgroundImage"));
	BackgroundImageWidget->SetColorAndOpacity(FLinearColor(0.055f, 0.08f, 0.12f, 1));
	if (UOverlaySlot* LayoutSlot = Root->AddChildToOverlay(BackgroundImageWidget))
	{
		LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
		LayoutSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// Portada abierta: solamente fondo, titulo y botones.
	UOverlay* Main = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MainPage"));
	MainPageRoot = Main;
	if (UOverlaySlot* LayoutSlot = Root->AddChildToOverlay(Main))
	{
		LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
		LayoutSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UTextBlock* Title = Text(
		WidgetTree, TEXT("GameTitle"), TEXT("La ca\u00EDda de Camelot"), 78,
		FLinearColor(0.96f, 0.78f, 0.36f, 1));
	if (UOverlaySlot* LayoutSlot = Main->AddChildToOverlay(Title))
	{
		LayoutSlot->SetHorizontalAlignment(HAlign_Center);
		LayoutSlot->SetVerticalAlignment(VAlign_Top);
		LayoutSlot->SetPadding(FMargin(40, 54, 40, 0));
	}

	USizeBox* MainButtonsSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("MainButtonsSize"));
	MainButtonsSize->SetWidthOverride(390);
	if (UOverlaySlot* LayoutSlot = Main->AddChildToOverlay(MainButtonsSize))
	{
		LayoutSlot->SetHorizontalAlignment(HAlign_Left);
		LayoutSlot->SetVerticalAlignment(VAlign_Center);
		LayoutSlot->SetPadding(FMargin(72, 105, 0, 34));
	}
	UVerticalBox* MainButtons = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("MainButtons"));
	MainButtonsSize->AddChild(MainButtons);

	UButton* Play = MainButton(WidgetTree, MainButtons, TEXT("PlayButton"), TEXT("JUGAR"));
	UButton* Settings = MainButton(WidgetTree, MainButtons, TEXT("SettingsButton"), TEXT("AJUSTES"));
	UButton* Credits = MainButton(WidgetTree, MainButtons, TEXT("CreditsButton"), TEXT("CREDITOS"));
	UButton* Exit = MainButton(WidgetTree, MainButtons, TEXT("ExitButton"), TEXT("SALIR"));
	Play->OnClicked.AddDynamic(this, &UMainMenuWidget::ShowSavePage);
	Settings->OnClicked.AddDynamic(this, &UMainMenuWidget::ShowSettingsPage);
	Credits->OnClicked.AddDynamic(this, &UMainMenuWidget::ShowCreditsPage);
	Exit->OnClicked.AddDynamic(this, &UMainMenuWidget::QuitGame);

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSize"));
	SecondaryPanelRoot = PanelSize;
	PanelSize->SetVisibility(ESlateVisibility::Collapsed);
	PanelSize->SetWidthOverride(900);
	PanelSize->SetMaxDesiredHeight(880);
	if (UOverlaySlot* LayoutSlot = Root->AddChildToOverlay(PanelSize))
	{
		LayoutSlot->SetHorizontalAlignment(HAlign_Center);
		LayoutSlot->SetVerticalAlignment(VAlign_Center);
		LayoutSlot->SetPadding(FMargin(24));
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuPanel"));
	Panel->SetBrushColor(FLinearColor::Transparent);
	Panel->SetPadding(FMargin(54, 30));
	PanelSize->AddChild(Panel);

	PageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("MenuPages"));
	Panel->SetContent(PageSwitcher);

	// Principal
	#if 0 // Portada antigua sustituida por la composicion a pantalla completa superior.
	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainPage"));
	PageSwitcher->AddChild(Main);
	UTextBlock* Title = Text(WidgetTree, TEXT("GameTitle"), TEXT("La caída de Camelot"), 47,
		FLinearColor(0.96f, 0.78f, 0.36f, 1));
	if (UVerticalBoxSlot* LayoutSlot = Main->AddChildToVerticalBox(Title))
	{
		LayoutSlot->SetPadding(FMargin(0, 4, 0, 38));
		LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	UButton* Play = Button(WidgetTree, Main, TEXT("PlayButton"), TEXT("JUGAR"), nullptr,
		FLinearColor(0.12f, 0.34f, 0.22f, 1));
	UButton* Settings = Button(WidgetTree, Main, TEXT("SettingsButton"), TEXT("AJUSTES"));
	UButton* Credits = Button(WidgetTree, Main, TEXT("CreditsButton"), TEXT("CREDITOS"));
	UButton* Exit = Button(WidgetTree, Main, TEXT("ExitButton"), TEXT("SALIR"), nullptr,
		FLinearColor(0.40f, 0.09f, 0.12f, 1));
	Play->OnClicked.AddDynamic(this, &UMainMenuWidget::ShowSavePage);
	Settings->OnClicked.AddDynamic(this, &UMainMenuWidget::ShowSettingsPage);
	Credits->OnClicked.AddDynamic(this, &UMainMenuWidget::ShowCreditsPage);
	Exit->OnClicked.AddDynamic(this, &UMainMenuWidget::QuitGame);
	#endif

	// Ranuras
	UVerticalBox* Saves = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SavePage"));
	PageSwitcher->AddChild(Saves);
	Heading(WidgetTree, Saves, TEXT("SELECCIONAR PARTIDA"));
	SaveSlotLabels.SetNum(3);
	SaveDeleteButtons.SetNum(3);
	SaveDeleteLabels.SetNum(3);
	TArray<UButton*> SaveButtons;
	SaveButtons.SetNum(3);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UHorizontalBox* SaveRow = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("SaveRow%d"), Index + 1));
		if (UVerticalBoxSlot* RowSlot = Saves->AddChildToVerticalBox(SaveRow))
		{
			RowSlot->SetPadding(FMargin(0, 15));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		SaveButtons[Index] = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			*FString::Printf(TEXT("SaveSlot%d"), Index + 1));
		StyleSaveButton(SaveButtons[Index]);
		if (UHorizontalBoxSlot* PlaySlot = SaveRow->AddChildToHorizontalBox(SaveButtons[Index]))
		{
			PlaySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			PlaySlot->SetPadding(FMargin(0, 0, 8, 0));
		}
		SaveSlotLabels[Index] = Text(
			WidgetTree,
			*FString::Printf(TEXT("SaveSlot%dText"), Index + 1),
			FString::Printf(TEXT("PARTIDA %d"), Index + 1),
			25);
		SaveSlotLabels[Index]->SetColorAndOpacity(FSlateColor::UseForeground());
		SaveSlotLabels[Index]->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.75f));
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(
			SaveButtons[Index]->AddChild(SaveSlotLabels[Index])))
		{
			ContentSlot->SetPadding(FMargin(26, 19));
			ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		SaveDeleteButtons[Index] = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			*FString::Printf(TEXT("DeleteSlot%d"), Index + 1));
		SaveDeleteButtons[Index]->SetBackgroundColor(FLinearColor(0.48f, 0.07f, 0.09f, 1.0f));
		if (UHorizontalBoxSlot* DeleteSlot = SaveRow->AddChildToHorizontalBox(SaveDeleteButtons[Index]))
		{
			DeleteSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
		SaveDeleteLabels[Index] = Text(
			WidgetTree,
			*FString::Printf(TEXT("DeleteSlot%dText"), Index + 1),
			TEXT("ELIMINAR"),
			16);
		if (UButtonSlot* DeleteContentSlot = Cast<UButtonSlot>(
			SaveDeleteButtons[Index]->AddChild(SaveDeleteLabels[Index])))
		{
			DeleteContentSlot->SetPadding(FMargin(18, 20));
		}
	}
	UButton* Slot1 = SaveButtons[0];
	UButton* Slot2 = SaveButtons[1];
	UButton* Slot3 = SaveButtons[2];
	Slot1->OnClicked.AddDynamic(this, &UMainMenuWidget::ChooseSlot1);
	Slot2->OnClicked.AddDynamic(this, &UMainMenuWidget::ChooseSlot2);
	Slot3->OnClicked.AddDynamic(this, &UMainMenuWidget::ChooseSlot3);
	SaveDeleteButtons[0]->OnClicked.AddDynamic(this, &UMainMenuWidget::DeleteSlot1);
	SaveDeleteButtons[1]->OnClicked.AddDynamic(this, &UMainMenuWidget::DeleteSlot2);
	SaveDeleteButtons[2]->OnClicked.AddDynamic(this, &UMainMenuWidget::DeleteSlot3);
	UButton* BackFromSaves = MainButton(
		WidgetTree, Saves, TEXT("BackFromSaves"), TEXT("VOLVER AL MENU PRINCIPAL"));
	BackFromSaves->OnClicked.AddDynamic(this, &UMainMenuWidget::ShowMainPage);

	// Ajustes
	UVerticalBox* SettingsPage = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsPage"));
	PageSwitcher->AddChild(SettingsPage);
	Heading(WidgetTree, SettingsPage, TEXT("AJUSTES"));
	UHorizontalBox* VolumeRow = SettingRow(WidgetTree, SettingsPage, TEXT("VolumeRow"), TEXT("Volumen general"));
	VolumeSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("VolumeSlider"));
	VolumeSlider->SetValue(1);
	if (UHorizontalBoxSlot* LayoutSlot = VolumeRow->AddChildToHorizontalBox(VolumeSlider)) LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	VolumeValueText = Text(WidgetTree, TEXT("VolumeValue"), TEXT("100%"), 14);
	if (UHorizontalBoxSlot* LayoutSlot = VolumeRow->AddChildToHorizontalBox(VolumeValueText)) { LayoutSlot->SetPadding(FMargin(12,0,0,0)); LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); }
	VolumeSlider->OnValueChanged.AddDynamic(this, &UMainMenuWidget::HandleVolumeChanged);

	UHorizontalBox* ResolutionRow = SettingRow(WidgetTree, SettingsPage, TEXT("ResolutionRow"), TEXT("Resolucion"));
	ResolutionCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("ResolutionCombo"));
	if (UHorizontalBoxSlot* LayoutSlot = ResolutionRow->AddChildToHorizontalBox(ResolutionCombo)) LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox* FullscreenRow = SettingRow(WidgetTree, SettingsPage, TEXT("FullscreenRow"), TEXT("Pantalla completa"));
	FullscreenCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("FullscreenCheck"));
	FullscreenRow->AddChildToHorizontalBox(FullscreenCheckBox);

	UHorizontalBox* SensitivityRow = SettingRow(WidgetTree, SettingsPage, TEXT("SensitivityRow"), TEXT("Sensibilidad del raton"));
	SensitivitySlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("SensitivitySlider"));
	if (UHorizontalBoxSlot* LayoutSlot = SensitivityRow->AddChildToHorizontalBox(SensitivitySlider)) LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SensitivityValueText = Text(WidgetTree, TEXT("SensitivityValue"), TEXT("1.00"), 14);
	if (UHorizontalBoxSlot* LayoutSlot = SensitivityRow->AddChildToHorizontalBox(SensitivityValueText)) { LayoutSlot->SetPadding(FMargin(12,0,0,0)); LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); }
	SensitivitySlider->OnValueChanged.AddDynamic(this, &UMainMenuWidget::HandleSensitivityChanged);
	UHorizontalBox* InvertRow = SettingRow(WidgetTree, SettingsPage, TEXT("InvertRow"), TEXT("Invertir eje Y"));
	InvertYCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("InvertYCheck"));
	InvertRow->AddChildToHorizontalBox(InvertYCheckBox);
	UTextBlock* Controls = Text(WidgetTree, TEXT("ControlsHelp"),
		TEXT("CONTROLES: WASD mover | R poder activo | Shift esprintar | Z/X/C pociones | Esc pausa"), 12,
		FLinearColor(0.66f, 0.72f, 0.80f, 1));
	if (UVerticalBoxSlot* LayoutSlot = SettingsPage->AddChildToVerticalBox(Controls)) LayoutSlot->SetPadding(FMargin(0, 14));
	UButton* Apply = Button(WidgetTree, SettingsPage, TEXT("ApplySettings"), TEXT("APLICAR AJUSTES"), nullptr,
		FLinearColor(0.12f, 0.34f, 0.22f, 1));
	UButton* Defaults = Button(WidgetTree, SettingsPage, TEXT("Defaults"), TEXT("RESTAURAR VALORES"));
	UButton* BackFromSettings = Button(WidgetTree, SettingsPage, TEXT("BackFromSettings"), TEXT("VOLVER AL MENU PRINCIPAL"));
	Apply->OnClicked.AddDynamic(this, &UMainMenuWidget::ApplySettings);
	Defaults->OnClicked.AddDynamic(this, &UMainMenuWidget::RestoreDefaults);
	BackFromSettings->OnClicked.AddDynamic(this, &UMainMenuWidget::ShowMainPage);

	// Creditos
	UVerticalBox* CreditsPage = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CreditsPage"));
	PageSwitcher->AddChild(CreditsPage);
	Heading(WidgetTree, CreditsPage, TEXT("CREDITOS"));
	UScrollBox* CreditsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CreditsScroll"));
	if (UVerticalBoxSlot* LayoutSlot = CreditsPage->AddChildToVerticalBox(CreditsScroll)) LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CreditsBody = Text(WidgetTree, TEXT("CreditsBody"), TEXT("Creditos"), 17,
		FLinearColor(0.88f, 0.90f, 0.94f, 1));
	CreditsScroll->AddChild(CreditsBody);
	UButton* BackFromCredits = Button(WidgetTree, CreditsPage, TEXT("BackFromCredits"), TEXT("VOLVER AL MENU PRINCIPAL"));
	BackFromCredits->OnClicked.AddDynamic(this, &UMainMenuWidget::ShowMainPage);
}

void UMainMenuWidget::ShowMainPage()
{
	if (MainPageRoot) MainPageRoot->SetVisibility(ESlateVisibility::Visible);
	if (SecondaryPanelRoot) SecondaryPanelRoot->SetVisibility(ESlateVisibility::Collapsed);
}

void UMainMenuWidget::ShowSavePage()
{
	RefreshSaveSlots();
	if (MainPageRoot) MainPageRoot->SetVisibility(ESlateVisibility::Collapsed);
	if (SecondaryPanelRoot) SecondaryPanelRoot->SetVisibility(ESlateVisibility::Visible);
	if (PageSwitcher) PageSwitcher->SetActiveWidgetIndex(0);
}

void UMainMenuWidget::ShowSettingsPage()
{
	RefreshSettings();
	if (MainPageRoot) MainPageRoot->SetVisibility(ESlateVisibility::Collapsed);
	if (SecondaryPanelRoot) SecondaryPanelRoot->SetVisibility(ESlateVisibility::Visible);
	if (PageSwitcher) PageSwitcher->SetActiveWidgetIndex(1);
}

void UMainMenuWidget::ShowCreditsPage()
{
	if (MainPageRoot) MainPageRoot->SetVisibility(ESlateVisibility::Collapsed);
	if (SecondaryPanelRoot) SecondaryPanelRoot->SetVisibility(ESlateVisibility::Visible);
	if (PageSwitcher) PageSwitcher->SetActiveWidgetIndex(2);
}

void UMainMenuWidget::RefreshSaveSlots()
{
	URunPowerPersistenceSubsystem* Persistence = Manager.IsValid() && Manager->GetGameInstance()
		? Manager->GetGameInstance()->GetSubsystem<URunPowerPersistenceSubsystem>() : nullptr;
	for (int32 Index = 0; Index < SaveSlotLabels.Num(); ++Index)
	{
		if (SaveSlotLabels[Index])
		{
			const bool bExists = Persistence && Persistence->DoesSaveSlotExist(Index + 1);
			SaveSlotLabels[Index]->SetText(FText::FromString(FString::Printf(
				TEXT("PARTIDA %d   -   %s"), Index + 1, bExists ? TEXT("CONTINUAR") : TEXT("NUEVA PARTIDA"))));
			if (SaveDeleteButtons.IsValidIndex(Index) && SaveDeleteButtons[Index])
			{
				SaveDeleteButtons[Index]->SetIsEnabled(bExists);
			}
			if (SaveDeleteLabels.IsValidIndex(Index) && SaveDeleteLabels[Index])
			{
				SaveDeleteLabels[Index]->SetText(FText::FromString(
					PendingDeleteSlot == Index + 1 && bExists
						? TEXT("CONFIRMAR")
						: TEXT("ELIMINAR")));
			}
		}
	}
}

void UMainMenuWidget::ChooseSlot1() { if (Manager.IsValid()) Manager->StartSaveSlot(1); }
void UMainMenuWidget::ChooseSlot2() { if (Manager.IsValid()) Manager->StartSaveSlot(2); }
void UMainMenuWidget::ChooseSlot3() { if (Manager.IsValid()) Manager->StartSaveSlot(3); }
void UMainMenuWidget::DeleteSlot1() { RequestDeleteSlot(1); }
void UMainMenuWidget::DeleteSlot2() { RequestDeleteSlot(2); }
void UMainMenuWidget::DeleteSlot3() { RequestDeleteSlot(3); }

void UMainMenuWidget::RequestDeleteSlot(const int32 SlotIndex)
{
	URunPowerPersistenceSubsystem* Persistence = Manager.IsValid() && Manager->GetGameInstance()
		? Manager->GetGameInstance()->GetSubsystem<URunPowerPersistenceSubsystem>() : nullptr;
	if (!Persistence || !Persistence->DoesSaveSlotExist(SlotIndex))
	{
		PendingDeleteSlot = 0;
		RefreshSaveSlots();
		return;
	}

	if (PendingDeleteSlot != SlotIndex)
	{
		PendingDeleteSlot = SlotIndex;
		RefreshSaveSlots();
		return;
	}

	const bool bDeleted = Persistence->DeleteSaveSlot(SlotIndex);
	PendingDeleteSlot = 0;
	RefreshSaveSlots();
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 4.0f,
			bDeleted ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("Partida %d: %s"), SlotIndex,
				bDeleted ? TEXT("datos eliminados") : TEXT("no se pudo eliminar")));
	}
}
void UMainMenuWidget::QuitGame() { if (Manager.IsValid()) Manager->QuitGame(); }

void UMainMenuWidget::PopulateResolutionOptions(const FIntPoint& Resolution)
{
	if (!ResolutionCombo) return;
	ResolutionCombo->ClearOptions();
	const TArray<FString> Options = { TEXT("1280 x 720"), TEXT("1600 x 900"), TEXT("1920 x 1080"), TEXT("2560 x 1440"), TEXT("3840 x 2160") };
	for (const FString& Option : Options) ResolutionCombo->AddOption(Option);
	const FString Current = FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y);
	if (!Options.Contains(Current)) ResolutionCombo->AddOption(Current);
	ResolutionCombo->SetSelectedOption(Current);
}

void UMainMenuWidget::RefreshSettings()
{
	UPauseMenuComponent* Settings = Manager.IsValid() ? Manager->GetSettingsComponent() : nullptr;
	if (VolumeSlider) VolumeSlider->SetValue(Settings ? Settings->GetMasterVolume() : 1.0f);
	if (SensitivitySlider)
	{
		const float Sensitivity = Settings ? Settings->GetMouseSensitivity() : 1.0f;
		SensitivitySlider->SetValue(FMath::GetMappedRangeValueClamped(FVector2D(0.1f, 3.0f), FVector2D(0, 1), Sensitivity));
	}
	if (InvertYCheckBox) InvertYCheckBox->SetIsChecked(Settings && Settings->IsVerticalLookInverted());
	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		PopulateResolutionOptions(UserSettings->GetScreenResolution());
		if (FullscreenCheckBox) FullscreenCheckBox->SetIsChecked(UserSettings->GetFullscreenMode() != EWindowMode::Windowed);
	}
	RefreshSettingLabels();
}

void UMainMenuWidget::RefreshSettingLabels()
{
	if (VolumeValueText && VolumeSlider) VolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), VolumeSlider->GetValue() * 100)));
	if (SensitivityValueText && SensitivitySlider)
	{
		const float Value = FMath::GetMappedRangeValueClamped(FVector2D(0,1), FVector2D(0.1f,3.0f), SensitivitySlider->GetValue());
		SensitivityValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Value)));
	}
}

void UMainMenuWidget::HandleVolumeChanged(float Value) { RefreshSettingLabels(); }
void UMainMenuWidget::HandleSensitivityChanged(float Value) { RefreshSettingLabels(); }

void UMainMenuWidget::ApplySettings()
{
	UPauseMenuComponent* Settings = Manager.IsValid() ? Manager->GetSettingsComponent() : nullptr;
	if (!Settings || !VolumeSlider || !ResolutionCombo || !FullscreenCheckBox || !SensitivitySlider || !InvertYCheckBox) return;
	const float Sensitivity = FMath::GetMappedRangeValueClamped(FVector2D(0,1), FVector2D(0.1f,3.0f), SensitivitySlider->GetValue());
	Settings->ApplyMenuSettings(VolumeSlider->GetValue(), ResolutionCombo->GetSelectedOption(), FullscreenCheckBox->IsChecked(), Sensitivity, InvertYCheckBox->IsChecked());
}

void UMainMenuWidget::RestoreDefaults()
{
	if (VolumeSlider) VolumeSlider->SetValue(1);
	if (SensitivitySlider) SensitivitySlider->SetValue(FMath::GetMappedRangeValueClamped(FVector2D(0.1f,3.0f), FVector2D(0,1), 1.0f));
	if (FullscreenCheckBox) FullscreenCheckBox->SetIsChecked(true);
	if (InvertYCheckBox) InvertYCheckBox->SetIsChecked(false);
	if (ResolutionCombo) ResolutionCombo->SetSelectedOption(TEXT("1920 x 1080"));
	RefreshSettingLabels();
}
