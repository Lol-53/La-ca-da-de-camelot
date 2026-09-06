#include "UI/PauseMenuWidget.h"

#include "UI/PauseMenuComponent.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"

namespace PauseMenuUI
{
	static UTextBlock* CreateText(
		UWidgetTree* Tree,
		const FName Name,
		const FString& Value,
		const int32 FontSize,
		const FLinearColor Color,
		const ETextJustify::Type Justification = ETextJustify::Left)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Value));
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FontSize));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justification);
		Text->SetAutoWrapText(true);
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		return Text;
	}

	static UButton* AddMenuButton(
		UWidgetTree* Tree,
		UVerticalBox* Column,
		const FName Name,
		const FString& Label,
		const FLinearColor Color)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(Color);
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Button))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetPadding(FMargin(0.0f, 7.0f));
		}

		UTextBlock* Text = CreateText(
			Tree,
			*FString::Printf(TEXT("%sLabel"), *Name.ToString()),
			Label,
			22,
			FLinearColor::White,
			ETextJustify::Center);
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button->AddChild(Text)))
		{
			ButtonSlot->SetPadding(FMargin(18.0f, 12.0f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		return Button;
	}

	static void AddSectionTitle(
		UWidgetTree* Tree,
		UVerticalBox* Column,
		const FString& Label)
	{
		UTextBlock* Text = CreateText(
			Tree,
			*FString::Printf(TEXT("%sSection"), *Label),
			Label,
			17,
			FLinearColor(0.96f, 0.72f, 0.24f, 1.0f));
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Text))
		{
			Slot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 5.0f));
		}
	}

	static UHorizontalBox* AddSettingRow(
		UWidgetTree* Tree,
		UVerticalBox* Column,
		const FName Name,
		const FString& Label)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			Name);
		if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.0f, 4.0f));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		UTextBlock* LabelText = CreateText(
			Tree,
			*FString::Printf(TEXT("%sLabel"), *Name.ToString()),
			Label,
			15,
			FLinearColor(0.82f, 0.87f, 0.94f, 1.0f));
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		return Row;
	}
}

void UPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);

	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("PauseMenuWidgetTree"));
	}
	if (!WidgetTree->RootWidget)
	{
		BuildInterface();
	}
}

void UPauseMenuWidget::Configure(
	UPauseMenuComponent* InOwnerComponent,
	const float MasterVolume,
	const FIntPoint& Resolution,
	const bool bFullscreen,
	const float MouseSensitivity,
	const bool bInvertVerticalLook)
{
	OwnerComponent = InOwnerComponent;
	if (VolumeSlider)
	{
		VolumeSlider->SetValue(FMath::Clamp(MasterVolume, 0.0f, 1.0f));
	}
	if (SensitivitySlider)
	{
		SensitivitySlider->SetValue(FMath::GetMappedRangeValueClamped(
			FVector2D(0.1f, 3.0f),
			FVector2D(0.0f, 1.0f),
			MouseSensitivity));
	}
	if (FullscreenCheckBox)
	{
		FullscreenCheckBox->SetIsChecked(bFullscreen);
	}
	if (InvertYCheckBox)
	{
		InvertYCheckBox->SetIsChecked(bInvertVerticalLook);
	}
	PopulateResolutionOptions(Resolution);
	RefreshValueLabels();
}

FReply UPauseMenuWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	FReply Reply = Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	if (InKeyEvent.GetKey() == EKeys::Escape && OwnerComponent.IsValid())
	{
		if (PageSwitcher && PageSwitcher->GetActiveWidgetIndex() != 0)
		{
			ShowMainMenu();
		}
		else
		{
			OwnerComponent->ClosePauseMenu();
		}
		Reply = FReply::Handled();
	}
	return Reply;
}

void UPauseMenuWidget::BuildInterface()
{
	using namespace PauseMenuUI;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		TEXT("PauseRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Shade = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("PauseShade"));
	Shade->SetBrushColor(FLinearColor(0.008f, 0.012f, 0.022f, 0.84f));
	if (UOverlaySlot* ShadeSlot = Root->AddChildToOverlay(Shade))
	{
		ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
		ShadeSlot->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("PausePanelSize"));
	PanelSize->SetWidthOverride(690.0f);
	if (UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelSize))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
		PanelSlot->SetPadding(FMargin(24.0f));
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("PausePanel"));
	Panel->SetBrushColor(FLinearColor(0.028f, 0.046f, 0.075f, 0.98f));
	Panel->SetPadding(FMargin(46.0f, 30.0f));
	PanelSize->AddChild(Panel);

	PageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(
		UWidgetSwitcher::StaticClass(),
		TEXT("PausePages"));
	Panel->SetContent(PageSwitcher);

	UVerticalBox* MainPage = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("MainPage"));
	PageSwitcher->AddChild(MainPage);

	UTextBlock* MainTitle = CreateText(
		WidgetTree,
		TEXT("PauseTitle"),
		TEXT("PARTIDA EN PAUSA"),
		36,
		FLinearColor(0.96f, 0.72f, 0.24f, 1.0f),
		ETextJustify::Center);
	if (UVerticalBoxSlot* MainTitleSlot = MainPage->AddChildToVerticalBox(MainTitle))
	{
		MainTitleSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 28.0f));
		MainTitleSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	UButton* ContinueButton = AddMenuButton(
		WidgetTree, MainPage, TEXT("ContinueButton"), TEXT("CONTINUAR PARTIDA"),
		FLinearColor(0.10f, 0.36f, 0.24f, 1.0f));
	UButton* SettingsButton = AddMenuButton(
		WidgetTree, MainPage, TEXT("SettingsButton"), TEXT("AJUSTES"),
		FLinearColor(0.09f, 0.24f, 0.46f, 1.0f));
	UButton* LobbyButton = AddMenuButton(
		WidgetTree, MainPage, TEXT("LobbyButton"), TEXT("VOLVER AL LOBBY"),
		FLinearColor(0.12f, 0.29f, 0.36f, 1.0f));
	UButton* MainMenuButton = AddMenuButton(
		WidgetTree, MainPage, TEXT("MainMenuButton"), TEXT("VOLVER AL MENU PRINCIPAL"),
		FLinearColor(0.24f, 0.18f, 0.09f, 1.0f));
	UButton* ExitButton = AddMenuButton(
		WidgetTree, MainPage, TEXT("ExitButton"), TEXT("SALIR"),
		FLinearColor(0.42f, 0.10f, 0.13f, 1.0f));
	ContinueButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::ContinueGame);
	SettingsButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::ShowSettings);
	LobbyButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::ShowLobbyConfirmation);
	MainMenuButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::ReturnToMainMenu);
	ExitButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::ExitGame);

	UTextBlock* Hint = CreateText(
		WidgetTree,
		TEXT("PauseHint"),
		TEXT("Pulsa Esc para volver a la partida"),
		13,
		FLinearColor(0.58f, 0.65f, 0.74f, 1.0f),
		ETextJustify::Center);
	if (UVerticalBoxSlot* HintSlot = MainPage->AddChildToVerticalBox(Hint))
	{
		HintSlot->SetPadding(FMargin(0.0f, 22.0f, 0.0f, 4.0f));
		HintSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	UVerticalBox* SettingsPage = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("SettingsPage"));
	PageSwitcher->AddChild(SettingsPage);

	UTextBlock* SettingsTitle = CreateText(
		WidgetTree,
		TEXT("SettingsTitle"),
		TEXT("AJUSTES"),
		30,
		FLinearColor(0.96f, 0.72f, 0.24f, 1.0f),
		ETextJustify::Center);
	if (UVerticalBoxSlot* SettingsTitleSlot = SettingsPage->AddChildToVerticalBox(SettingsTitle))
	{
		SettingsTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		SettingsTitleSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	AddSectionTitle(WidgetTree, SettingsPage, TEXT("AUDIO"));
	UHorizontalBox* VolumeRow = AddSettingRow(
		WidgetTree, SettingsPage, TEXT("VolumeRow"), TEXT("Volumen general"));
	VolumeSlider = WidgetTree->ConstructWidget<USlider>(
		USlider::StaticClass(), TEXT("VolumeSlider"));
	VolumeSlider->SetStepSize(0.05f);
	if (UHorizontalBoxSlot* VolumeSliderSlot = VolumeRow->AddChildToHorizontalBox(VolumeSlider))
	{
		VolumeSliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		VolumeSliderSlot->SetPadding(FMargin(14.0f, 0.0f));
		VolumeSliderSlot->SetVerticalAlignment(VAlign_Center);
	}
	VolumeValueText = CreateText(
		WidgetTree, TEXT("VolumeValue"), TEXT("100%"), 14,
		FLinearColor::White, ETextJustify::Right);
	if (UHorizontalBoxSlot* VolumeValueSlot = VolumeRow->AddChildToHorizontalBox(VolumeValueText))
	{
		VolumeValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		VolumeValueSlot->SetVerticalAlignment(VAlign_Center);
	}
	VolumeSlider->OnValueChanged.AddDynamic(this, &UPauseMenuWidget::HandleVolumeChanged);

	AddSectionTitle(WidgetTree, SettingsPage, TEXT("PANTALLA"));
	UHorizontalBox* ResolutionRow = AddSettingRow(
		WidgetTree, SettingsPage, TEXT("ResolutionRow"), TEXT("Resolucion"));
	ResolutionCombo = WidgetTree->ConstructWidget<UComboBoxString>(
		UComboBoxString::StaticClass(), TEXT("ResolutionCombo"));
	if (UHorizontalBoxSlot* ResolutionComboSlot = ResolutionRow->AddChildToHorizontalBox(ResolutionCombo))
	{
		ResolutionComboSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ResolutionComboSlot->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));
	}

	UHorizontalBox* FullscreenRow = AddSettingRow(
		WidgetTree, SettingsPage, TEXT("FullscreenRow"), TEXT("Pantalla completa"));
	FullscreenCheckBox = WidgetTree->ConstructWidget<UCheckBox>(
		UCheckBox::StaticClass(), TEXT("FullscreenCheckBox"));
	if (UHorizontalBoxSlot* FullscreenCheckSlot = FullscreenRow->AddChildToHorizontalBox(FullscreenCheckBox))
	{
		FullscreenCheckSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		FullscreenCheckSlot->SetPadding(FMargin(14.0f, 0.0f));
		FullscreenCheckSlot->SetVerticalAlignment(VAlign_Center);
	}

	AddSectionTitle(WidgetTree, SettingsPage, TEXT("CONTROLES"));
	UHorizontalBox* SensitivityRow = AddSettingRow(
		WidgetTree, SettingsPage, TEXT("SensitivityRow"), TEXT("Sensibilidad de camara"));
	SensitivitySlider = WidgetTree->ConstructWidget<USlider>(
		USlider::StaticClass(), TEXT("SensitivitySlider"));
	SensitivitySlider->SetStepSize(0.025f);
	if (UHorizontalBoxSlot* SensitivitySliderSlot = SensitivityRow->AddChildToHorizontalBox(SensitivitySlider))
	{
		SensitivitySliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SensitivitySliderSlot->SetPadding(FMargin(14.0f, 0.0f));
		SensitivitySliderSlot->SetVerticalAlignment(VAlign_Center);
	}
	SensitivityValueText = CreateText(
		WidgetTree, TEXT("SensitivityValue"), TEXT("1.00"), 14,
		FLinearColor::White, ETextJustify::Right);
	if (UHorizontalBoxSlot* SensitivityValueSlot = SensitivityRow->AddChildToHorizontalBox(SensitivityValueText))
	{
		SensitivityValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		SensitivityValueSlot->SetVerticalAlignment(VAlign_Center);
	}
	SensitivitySlider->OnValueChanged.AddDynamic(
		this, &UPauseMenuWidget::HandleSensitivityChanged);

	UHorizontalBox* InvertRow = AddSettingRow(
		WidgetTree, SettingsPage, TEXT("InvertRow"), TEXT("Invertir eje vertical"));
	InvertYCheckBox = WidgetTree->ConstructWidget<UCheckBox>(
		UCheckBox::StaticClass(), TEXT("InvertYCheckBox"));
	if (UHorizontalBoxSlot* InvertCheckSlot = InvertRow->AddChildToHorizontalBox(InvertYCheckBox))
	{
		InvertCheckSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		InvertCheckSlot->SetPadding(FMargin(14.0f, 0.0f));
		InvertCheckSlot->SetVerticalAlignment(VAlign_Center);
	}

	UTextBlock* ControlsHelp = CreateText(
		WidgetTree,
		TEXT("ControlsHelp"),
		TEXT("WASD: mover  |  Raton: camara  |  Espacio: saltar  |  Shift: esprintar\n"
			"Clic izquierdo: combo de espada y patada  |  F: lanzar proyectil\n"
			"E: interactuar / avanzar dialogo  |  R: activar poder equipado\n"
			"Doble pulsacion de WASD: dash (si esta desbloqueado)\n"
			"Z: pocion de vida  |  X: pocion de energia  |  C: pocion de mana\n"
			"Tab: estado de la run  |  Esc: pausa"),
		12,
		FLinearColor(0.62f, 0.70f, 0.80f, 1.0f),
		ETextJustify::Center);
	ControlsHelp->SetAutoWrapText(true);
	ControlsHelp->SetWrapTextAt(820.0f);
	if (UVerticalBoxSlot* ControlsHelpSlot = SettingsPage->AddChildToVerticalBox(ControlsHelp))
	{
		ControlsHelpSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 8.0f));
		ControlsHelpSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("SettingsActions"));
	if (UVerticalBoxSlot* ActionsSlot = SettingsPage->AddChildToVerticalBox(Actions))
	{
		ActionsSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		ActionsSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	auto AddActionButton = [this, Actions](
		const FName Name,
		const FString& Label,
		const FLinearColor Color)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(Color);
		if (UHorizontalBoxSlot* ActionButtonSlot = Actions->AddChildToHorizontalBox(Button))
		{
			ActionButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ActionButtonSlot->SetPadding(FMargin(5.0f));
		}
		UTextBlock* Text = PauseMenuUI::CreateText(
			WidgetTree,
			*FString::Printf(TEXT("%sLabel"), *Name.ToString()),
			Label,
			14,
			FLinearColor::White,
			ETextJustify::Center);
		if (UButtonSlot* ActionContentSlot = Cast<UButtonSlot>(Button->AddChild(Text)))
		{
			ActionContentSlot->SetPadding(FMargin(8.0f, 9.0f));
			ActionContentSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		return Button;
	};

	UButton* BackButton = AddActionButton(
		TEXT("BackButton"), TEXT("VOLVER"),
		FLinearColor(0.18f, 0.21f, 0.27f, 1.0f));
	UButton* DefaultsButton = AddActionButton(
		TEXT("DefaultsButton"), TEXT("RESTABLECER"),
		FLinearColor(0.28f, 0.20f, 0.09f, 1.0f));
	UButton* ApplyButton = AddActionButton(
		TEXT("ApplyButton"), TEXT("APLICAR"),
		FLinearColor(0.08f, 0.34f, 0.22f, 1.0f));
	BackButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::ShowMainMenu);
	DefaultsButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::RestoreDefaults);
	ApplyButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::ApplySettings);

	UVerticalBox* LobbyConfirmationPage =
		WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("LobbyConfirmationPage"));
	PageSwitcher->AddChild(LobbyConfirmationPage);

	UTextBlock* ConfirmationTitle = CreateText(
		WidgetTree, TEXT("LobbyConfirmationTitle"),
		TEXT("¿VOLVER AL LOBBY?"), 32,
		FLinearColor(0.96f, 0.72f, 0.24f, 1.0f), ETextJustify::Center);
	if (UVerticalBoxSlot* ConfirmationTitleSlot =
		LobbyConfirmationPage->AddChildToVerticalBox(ConfirmationTitle))
	{
		ConfirmationTitleSlot->SetHorizontalAlignment(HAlign_Fill);
		ConfirmationTitleSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 22.0f));
	}

	UTextBlock* ConfirmationMessage = CreateText(
		WidgetTree, TEXT("LobbyConfirmationMessage"),
		TEXT("Abandonaras la run actual y perderas todos sus poderes temporales.\n\n"
			"Conservaras los recursos, las pociones y las mejoras permanentes."),
		19, FLinearColor(0.88f, 0.90f, 0.94f, 1.0f), ETextJustify::Center);
	ConfirmationMessage->SetWrapTextAt(570.0f);
	if (UVerticalBoxSlot* ConfirmationMessageSlot =
		LobbyConfirmationPage->AddChildToVerticalBox(ConfirmationMessage))
	{
		ConfirmationMessageSlot->SetHorizontalAlignment(HAlign_Fill);
		ConfirmationMessageSlot->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 26.0f));
	}

	UButton* ConfirmLobbyButton = AddMenuButton(
		WidgetTree, LobbyConfirmationPage, TEXT("ConfirmLobbyButton"),
		TEXT("SI, VOLVER AL LOBBY"),
		FLinearColor(0.48f, 0.12f, 0.13f, 1.0f));
	UButton* CancelLobbyButton = AddMenuButton(
		WidgetTree, LobbyConfirmationPage, TEXT("CancelLobbyButton"),
		TEXT("CANCELAR"), FLinearColor(0.16f, 0.25f, 0.34f, 1.0f));
	ConfirmLobbyButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::ReturnToLobby);
	CancelLobbyButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::CancelLobbyReturn);

	PageSwitcher->SetActiveWidgetIndex(0);
}

void UPauseMenuWidget::PopulateResolutionOptions(
	const FIntPoint& CurrentResolution)
{
	if (!ResolutionCombo)
	{
		return;
	}

	ResolutionCombo->ClearOptions();
	const TArray<FIntPoint> StandardResolutions = {
		FIntPoint(1280, 720),
		FIntPoint(1600, 900),
		FIntPoint(1920, 1080),
		FIntPoint(2560, 1440),
		FIntPoint(3840, 2160)
	};

	const FString Current = FString::Printf(
		TEXT("%d x %d"), CurrentResolution.X, CurrentResolution.Y);
	ResolutionCombo->AddOption(Current);
	for (const FIntPoint& Option : StandardResolutions)
	{
		const FString Label = FString::Printf(TEXT("%d x %d"), Option.X, Option.Y);
		if (Label != Current)
		{
			ResolutionCombo->AddOption(Label);
		}
	}
	ResolutionCombo->SetSelectedOption(Current);
}

void UPauseMenuWidget::RefreshValueLabels()
{
	if (VolumeSlider && VolumeValueText)
	{
		VolumeValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%.0f%%"), VolumeSlider->GetValue() * 100.0f)));
	}
	if (SensitivitySlider && SensitivityValueText)
	{
		const float Sensitivity = FMath::GetMappedRangeValueClamped(
			FVector2D(0.0f, 1.0f),
			FVector2D(0.1f, 3.0f),
			SensitivitySlider->GetValue());
		SensitivityValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%.2f"), Sensitivity)));
	}
}

void UPauseMenuWidget::ContinueGame()
{
	if (OwnerComponent.IsValid())
	{
		OwnerComponent->ClosePauseMenu();
	}
}

void UPauseMenuWidget::ShowSettings()
{
	if (PageSwitcher)
	{
		PageSwitcher->SetActiveWidgetIndex(1);
	}
}

void UPauseMenuWidget::ShowMainMenu()
{
	if (PageSwitcher)
	{
		PageSwitcher->SetActiveWidgetIndex(0);
	}
}

void UPauseMenuWidget::ReturnToMainMenu()
{
	if (OwnerComponent.IsValid())
	{
		OwnerComponent->ReturnToMainMenu();
	}
}

void UPauseMenuWidget::ReturnToLobby()
{
	if (OwnerComponent.IsValid())
	{
		OwnerComponent->ReturnToLobby();
	}
}

void UPauseMenuWidget::ShowLobbyConfirmation()
{
	if (PageSwitcher)
	{
		PageSwitcher->SetActiveWidgetIndex(2);
	}
}

void UPauseMenuWidget::CancelLobbyReturn()
{
	ShowMainMenu();
}

void UPauseMenuWidget::ApplySettings()
{
	if (!OwnerComponent.IsValid() || !VolumeSlider || !ResolutionCombo ||
		!FullscreenCheckBox || !SensitivitySlider || !InvertYCheckBox)
	{
		return;
	}

	const float Sensitivity = FMath::GetMappedRangeValueClamped(
		FVector2D(0.0f, 1.0f),
		FVector2D(0.1f, 3.0f),
		SensitivitySlider->GetValue());
	OwnerComponent->ApplyMenuSettings(
		VolumeSlider->GetValue(),
		ResolutionCombo->GetSelectedOption(),
		FullscreenCheckBox->IsChecked(),
		Sensitivity,
		InvertYCheckBox->IsChecked());
	ShowMainMenu();
}

void UPauseMenuWidget::RestoreDefaults()
{
	if (VolumeSlider)
	{
		VolumeSlider->SetValue(1.0f);
	}
	if (SensitivitySlider)
	{
		SensitivitySlider->SetValue(FMath::GetMappedRangeValueClamped(
			FVector2D(0.1f, 3.0f),
			FVector2D(0.0f, 1.0f),
			1.0f));
	}
	if (FullscreenCheckBox)
	{
		FullscreenCheckBox->SetIsChecked(false);
	}
	if (InvertYCheckBox)
	{
		InvertYCheckBox->SetIsChecked(false);
	}
	if (ResolutionCombo)
	{
		ResolutionCombo->SetSelectedOption(TEXT("1920 x 1080"));
	}
	RefreshValueLabels();
}

void UPauseMenuWidget::ExitGame()
{
	if (OwnerComponent.IsValid())
	{
		OwnerComponent->QuitGame();
	}
}

void UPauseMenuWidget::HandleVolumeChanged(const float Value)
{
	RefreshValueLabels();
}

void UPauseMenuWidget::HandleSensitivityChanged(const float Value)
{
	RefreshValueLabels();
}
