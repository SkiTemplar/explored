#include "UI/Widgets/SExploredMainMenu.h"

#include "UI/ExploredUIStyle.h"
#include "UI/Widgets/ExploredUIWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SExploredMainMenu::Construct(const FArguments& InArgs)
{
	OnContinue = InArgs._OnContinue;
	OnNewGame = InArgs._OnNewGame;
	OnLoad = InArgs._OnLoad;
	OnSettings = InArgs._OnSettings;
	OnAchievements = InArgs._OnAchievements;
	OnCredits = InArgs._OnCredits;
	OnQuit = InArgs._OnQuit;
	const bool bCanContinue = InArgs._bCanContinue;

	const FExploredUIStyle& Style = FExploredUIStyle::Get();

	TSharedRef<SWidget> ContinueButton = ExploredUIWidgets::MakeMenuButton(
		NSLOCTEXT("ExploredUI", "Continue", "Continuar"), FOnClicked::CreateSP(this, &SExploredMainMenu::HandleContinue));
	ContinueButton->SetEnabled(bCanContinue);
	TSharedRef<SWidget> NewGameButton = ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "NewGame", "Nueva partida"),
		FOnClicked::CreateSP(this, &SExploredMainMenu::HandleNewGame));
	// «Cargar» solo tiene sentido con alguna partida legible (la misma condición que «Continuar»).
	TSharedRef<SWidget> LoadButton = ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "LoadGame", "Cargar"),
		FOnClicked::CreateSP(this, &SExploredMainMenu::HandleLoad));
	LoadButton->SetEnabled(bCanContinue);
	// Un botón deshabilitado no puede recibir el foco.
	InitialFocus = bCanContinue ? ContinueButton : NewGameButton;

	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(90.0f, 0.0f, 0.0f, 90.0f))
		[
			SNew(SBorder)
			.BorderImage(Style.BrushPanel())
			.Padding(FMargin(40.0f, 32.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("ExploredUI", "Title", "EXPLORED"))
					.Font(Style.FontTitle())
					.ColorAndOpacity(FSlateColor(Style.ColorInk()))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("ExploredUI", "Subtitle", "Un archipiélago que no aparece en ningún mapa"))
					.Font(Style.FontSubtitle())
					.ColorAndOpacity(FSlateColor(Style.ColorInkDim()))
				]
				// Dedicatoria (GDD §15): idéntica en los dos idiomas.
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(4.0f, 6.0f, 0.0f, 28.0f))
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("ExploredUI", "Dedication", "Para Almudena, mi Limón"))
					.Font(Style.FontSmall())
					.ColorAndOpacity(FSlateColor(Style.ColorAccent()))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ ContinueButton ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ NewGameButton ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ LoadButton ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
				[
					ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Settings", "Ajustes"),
						FOnClicked::CreateSP(this, &SExploredMainMenu::HandleSettings))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
				[
					ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Achievements", "Logros"),
						FOnClicked::CreateSP(this, &SExploredMainMenu::HandleAchievements))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
				[
					ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Credits", "Créditos"),
						FOnClicked::CreateSP(this, &SExploredMainMenu::HandleCredits))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
				[
					ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Quit", "Salir"),
						FOnClicked::CreateSP(this, &SExploredMainMenu::HandleQuit))
				]
			]
		]
	];
}

FReply SExploredMainMenu::HandleContinue()
{
	OnContinue.ExecuteIfBound();
	return FReply::Handled();
}

FReply SExploredMainMenu::HandleNewGame()
{
	OnNewGame.ExecuteIfBound();
	return FReply::Handled();
}

FReply SExploredMainMenu::HandleLoad()
{
	OnLoad.ExecuteIfBound();
	return FReply::Handled();
}

FReply SExploredMainMenu::HandleAchievements()
{
	OnAchievements.ExecuteIfBound();
	return FReply::Handled();
}

FReply SExploredMainMenu::HandleSettings()
{
	OnSettings.ExecuteIfBound();
	return FReply::Handled();
}

FReply SExploredMainMenu::HandleCredits()
{
	OnCredits.ExecuteIfBound();
	return FReply::Handled();
}

FReply SExploredMainMenu::HandleQuit()
{
	OnQuit.ExecuteIfBound();
	return FReply::Handled();
}
