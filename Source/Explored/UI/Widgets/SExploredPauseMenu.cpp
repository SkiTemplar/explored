#include "UI/Widgets/SExploredPauseMenu.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UI/ExploredUIStyle.h"
#include "UI/SettingsLogic.h"
#include "UI/Widgets/ExploredUIWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ExploredPauseMenuDetail
{
	/** Página del conmutador: los botones o la pregunta de «Salir al menú» o de «Salir del juego». */
	constexpr int32 ButtonsPage = 0;
	constexpr int32 ConfirmPage = 1;
	constexpr int32 QuitConfirmPage = 2;
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
						.WidgetIndex(ExploredPauseMenuDetail::ButtonsPage)
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
	const bool bConfirming = Pages.IsValid() && Pages->GetActiveWidgetIndex() != ExploredPauseMenuDetail::ButtonsPage;
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

bool SExploredPauseMenu::AskConfirm(int32 Page, const TSharedPtr<SWidget>& NoFocus, FReply& OutReply)
{
	if (!bConfirmExit || !Pages.IsValid() || !NoFocus.IsValid())
	{
		return false;
	}
	Pages->SetActiveWidgetIndex(Page);
	OutReply = FReply::Handled().SetUserFocus(NoFocus.ToSharedRef(), EFocusCause::SetDirectly);
	return true;
}

FReply SExploredPauseMenu::HandleExitToMenu()
{
	FReply Reply = FReply::Handled();
	if (AskConfirm(ExploredPauseMenuDetail::ConfirmPage, ConfirmNoFocus, Reply))
	{
		return Reply;
	}
	OnExitToMenu.ExecuteIfBound();
	return FReply::Handled();
}

FReply SExploredPauseMenu::HandleConfirmYes()
{
	const bool bQuit = Pages.IsValid() && Pages->GetActiveWidgetIndex() == ExploredPauseMenuDetail::QuitConfirmPage;
	(bQuit ? OnQuit : OnExitToMenu).ExecuteIfBound();
	return FReply::Handled();
}

FReply SExploredPauseMenu::HandleConfirmNo()
{
	const bool bQuit = Pages.IsValid() && Pages->GetActiveWidgetIndex() == ExploredPauseMenuDetail::QuitConfirmPage;
	if (Pages.IsValid())
	{
		Pages->SetActiveWidgetIndex(ExploredPauseMenuDetail::ButtonsPage);
	}
	FReply Reply = FReply::Handled();
	const TSharedPtr<SWidget>& Back = bQuit ? QuitFocus : ExitFocus;
	if (Back.IsValid())
	{
		Reply.SetUserFocus(Back.ToSharedRef(), EFocusCause::SetDirectly);
	}
	return Reply;
}

FReply SExploredPauseMenu::HandleQuit()
{
	FReply Reply = FReply::Handled();
	if (AskConfirm(ExploredPauseMenuDetail::QuitConfirmPage, QuitConfirmNoFocus, Reply))
	{
		return Reply;
	}
	OnQuit.ExecuteIfBound();
	return FReply::Handled();
}
