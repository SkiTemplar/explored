#include "UI/Widgets/SExploredPauseMenu.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UI/ExploredUIStyle.h"
#include "UI/SettingsLogic.h"
#include "UI/Widgets/ExploredUIWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SExploredPauseMenu::Construct(const FArguments& InArgs)
{
	OnResume = InArgs._OnResume;
	OnSettings = InArgs._OnSettings;
	OnSave = InArgs._OnSave;
	OnExitToMenu = InArgs._OnExitToMenu;
	OnQuit = InArgs._OnQuit;

	const FExploredUIStyle& Style = FExploredUIStyle::Get();

	TSharedRef<SWidget> ResumeButton = ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Resume", "Reanudar"),
		FOnClicked::CreateSP(this, &SExploredPauseMenu::HandleResume));
	InitialFocus = ResumeButton;

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
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ ResumeButton ]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
					[
						ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Settings", "Ajustes"),
							FOnClicked::CreateSP(this, &SExploredPauseMenu::HandleSettings))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
					[
						ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "SaveGame", "Guardar partida"),
							FOnClicked::CreateSP(this, &SExploredPauseMenu::HandleSave))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
					[
						ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "ExitToMenu", "Salir al menú"),
							FOnClicked::CreateSP(this, &SExploredPauseMenu::HandleExitToMenu))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
					[
						ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "QuitGame", "Salir del juego"),
							FOnClicked::CreateSP(this, &SExploredPauseMenu::HandleQuit))
					]
				]
			]
		]
	];
}

FReply SExploredPauseMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FName Key = InKeyEvent.GetKey().GetFName();
	if (ExploredSettingsLogic::IsPauseToggleKey(Key) || ExploredSettingsLogic::IsMenuBackKey(Key))
	{
		return HandleResume();
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

FReply SExploredPauseMenu::HandleExitToMenu()
{
	OnExitToMenu.ExecuteIfBound();
	return FReply::Handled();
}

FReply SExploredPauseMenu::HandleQuit()
{
	OnQuit.ExecuteIfBound();
	return FReply::Handled();
}
