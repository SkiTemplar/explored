#include "UI/Widgets/SExploredPauseMenu.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UI/ExploredUIStyle.h"
#include "UI/ScreensLogic.h"
#include "UI/SettingsLogic.h"
#include "UI/Widgets/ExploredUIWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ExploredPauseMenuDetail
{
	/** Página del conmutador (ranuras en el orden de ExploredScreens::EPauseMenuPage). */
	int32 PageIndex(ExploredScreens::EPauseMenuPage Page)
	{
		return static_cast<int32>(Page);
	}
}

void SExploredPauseMenu::Construct(const FArguments& InArgs)
{
	OnResume = InArgs._OnResume;
	OnSettings = InArgs._OnSettings;
	OnSave = InArgs._OnSave;
	OnExitToMenu = InArgs._OnExitToMenu;
	OnQuit = InArgs._OnQuit;
	bConfirmExit = InArgs._bConfirmExit;

	const FExploredUIStyle& Style = FExploredUIStyle::Get();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(Style.BrushWhite())
		// Velo de tinta: el juego queda detrás, apagado, y el panel se lee como lo único activo.
		.BorderBackgroundColor(FLinearColor(Style.ColorInk().R, Style.ColorInk().G, Style.ColorInk().B, 0.45f))
		.Padding(0.0f)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(Style.BrushPanel())
				.Padding(FMargin(40.0f, 32.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 20.0f))
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("ExploredUI", "Paused", "Pausa"))
						.Font(Style.SerifBold(44.0f))
						.ColorAndOpacity(FSlateColor(Style.ColorAccent()))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(Pages, SWidgetSwitcher)
						.WidgetIndex(ExploredPauseMenuDetail::PageIndex(ExploredScreens::EPauseMenuPage::Buttons))
						+ SWidgetSwitcher::Slot()[BuildButtons()]
						+ SWidgetSwitcher::Slot()[BuildConfirm(false)]
						+ SWidgetSwitcher::Slot()[BuildConfirm(true)]
					]
					// Las teclas del juego, fijas y discretas (sin mando: sus botones cambian de un mando a otro).
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 24.0f, 0.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("ExploredUI", "PauseKeysHint", "Tab mochila · J cuaderno · C verbos · Esc pausa"))
						.Font(Style.FontSmall())
						.ColorAndOpacity(FSlateColor(Style.ColorInkDim()))
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SExploredPauseMenu::BuildButtons()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			ExploredUIWidgets::MakeFocusMenuButton(NSLOCTEXT("ExploredUI", "Resume", "Reanudar"),
				FOnClicked::CreateSP(this, &SExploredPauseMenu::HandleResume), true, &InitialFocus)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			ExploredUIWidgets::MakeFocusMenuButton(NSLOCTEXT("ExploredUI", "Settings", "Ajustes"),
				FOnClicked::CreateSP(this, &SExploredPauseMenu::HandleSettings))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			ExploredUIWidgets::MakeFocusMenuButton(NSLOCTEXT("ExploredUI", "SaveGame", "Guardar partida"),
				FOnClicked::CreateSP(this, &SExploredPauseMenu::HandleSave))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			ExploredUIWidgets::MakeFocusMenuButton(NSLOCTEXT("ExploredUI", "ExitToMenu", "Salir al menú"),
				FOnClicked::CreateSP(this, &SExploredPauseMenu::HandleExitToMenu), true, &ExitFocus)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			ExploredUIWidgets::MakeFocusMenuButton(NSLOCTEXT("ExploredUI", "QuitGame", "Salir del juego"),
				FOnClicked::CreateSP(this, &SExploredPauseMenu::HandleQuit), true, &QuitFocus)
		];
}

TSharedRef<SWidget> SExploredPauseMenu::BuildConfirm(bool bQuit)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const FText Question = bQuit
		? NSLOCTEXT("ExploredUI", "QuitConfirm", "¿Salir del juego? Lo que no hayas guardado se perderá.")
		: NSLOCTEXT("ExploredUI", "ExitConfirm", "¿Salir al menú? Lo que no hayas guardado se perderá.");
	const FText Yes = bQuit
		? NSLOCTEXT("ExploredUI", "QuitConfirmYes", "Sí, salir del juego")
		: NSLOCTEXT("ExploredUI", "ExitConfirmYes", "Sí, salir al menú");
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 16.0f))
		[
			SNew(SBox)
			.WidthOverride(420.0f)
			[
				SNew(STextBlock)
				.Text(Question)
				.Font(Style.FontBody())
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(Style.ColorInk()))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			ExploredUIWidgets::MakeFocusMenuButton(NSLOCTEXT("ExploredUI", "ExitConfirmNo", "No, seguir jugando"),
				FOnClicked::CreateSP(this, &SExploredPauseMenu::HandleConfirmNo), true, bQuit ? &QuitConfirmNoFocus : &ConfirmNoFocus)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			ExploredUIWidgets::MakeFocusMenuButton(Yes, FOnClicked::CreateSP(this, &SExploredPauseMenu::HandleConfirmYes))
		];
}

FReply SExploredPauseMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FName Key = InKeyEvent.GetKey().GetFName();
	const bool bConfirming = CurrentPage() != ExploredScreens::EPauseMenuPage::Buttons;
	if (bConfirming && ExploredSettingsLogic::IsMenuBackKey(Key))
	{
		// Escape en la pregunta es «No»: vuelve a los botones, no reanuda.
		return HandleConfirmNo();
	}
	if (ExploredSettingsLogic::IsPauseToggleKey(Key) || ExploredSettingsLogic::IsMenuBackKey(Key))
	{
		return HandleResume();
	}
	// W y S mueven el foco como las flechas (Slate ya navega con flechas y mando; Enter pulsa).
	const int32 Step = ExploredSettingsLogic::MenuNavigationStep(Key);
	if (Step != 0)
	{
		return FReply::Handled().SetNavigation(Step < 0 ? EUINavigation::Up : EUINavigation::Down, ENavigationGenesis::Keyboard);
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SExploredPauseMenu::HandleResume()
{
	OnResume.ExecuteIfBound();
	return FReply::Handled();
}

FReply SExploredPauseMenu::HandleSettings()
{
	OnSettings.ExecuteIfBound();
	return FReply::Handled();
}

FReply SExploredPauseMenu::HandleSave()
{
	OnSave.ExecuteIfBound();
	return FReply::Handled();
}

ExploredScreens::EPauseMenuPage SExploredPauseMenu::CurrentPage() const
{
	const int32 Index = Pages.IsValid() ? Pages->GetActiveWidgetIndex() : 0;
	return Index == ExploredPauseMenuDetail::PageIndex(ExploredScreens::EPauseMenuPage::ConfirmQuit) ? ExploredScreens::EPauseMenuPage::ConfirmQuit
		: Index == ExploredPauseMenuDetail::PageIndex(ExploredScreens::EPauseMenuPage::ConfirmExit) ? ExploredScreens::EPauseMenuPage::ConfirmExit
		: ExploredScreens::EPauseMenuPage::Buttons;
}

FReply SExploredPauseMenu::ApplyStep(const ExploredScreens::FPauseMenuStep& Step)
{
	using namespace ExploredScreens;
	if (Pages.IsValid())
	{
		Pages->SetActiveWidgetIndex(ExploredPauseMenuDetail::PageIndex(Step.Page));
	}
	switch (Step.Action)
	{
	case EPauseMenuAction::Quit: OnQuit.ExecuteIfBound(); break;
	case EPauseMenuAction::ExitToMenu: OnExitToMenu.ExecuteIfBound(); break;
	default: break;
	}
	TSharedPtr<SWidget> Focus;
	switch (Step.Focus)
	{
	case EPauseMenuFocus::ConfirmNo: Focus = Step.Page == EPauseMenuPage::ConfirmQuit ? QuitConfirmNoFocus : ConfirmNoFocus; break;
	case EPauseMenuFocus::ExitButton: Focus = ExitFocus; break;
	case EPauseMenuFocus::QuitButton: Focus = QuitFocus; break;
	default: break;
	}
	FReply Reply = FReply::Handled();
	if (Focus.IsValid())
	{
		Reply.SetUserFocus(Focus.ToSharedRef(), EFocusCause::SetDirectly);
	}
	return Reply;
}

FReply SExploredPauseMenu::HandleExitToMenu()
{
	return ApplyStep(ExploredScreens::PauseRequestExit(false, bConfirmExit));
}

FReply SExploredPauseMenu::HandleQuit()
{
	return ApplyStep(ExploredScreens::PauseRequestExit(true, bConfirmExit));
}

FReply SExploredPauseMenu::HandleConfirmYes()
{
	return ApplyStep(ExploredScreens::PauseConfirmYes(CurrentPage()));
}

FReply SExploredPauseMenu::HandleConfirmNo()
{
	return ApplyStep(ExploredScreens::PauseConfirmNo(CurrentPage()));
}
