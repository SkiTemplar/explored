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
	OnCredits = InArgs._OnCredits;
	OnQuit = InArgs._OnQuit;
	const bool bCanContinue = InArgs._bCanContinue;
	const FText Notice = InArgs._Notice;

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
		.Padding(FMargin(110.0f, 0.0f, 0.0f, 80.0f))
		[
			SNew(SVerticalBox)
			// El título va sobre la escena del atardecer, en papel con sombra de tinta.
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("ExploredUI", "Title", "Baliza Siete"))
				.Font(Style.SerifBold(104.0f))
				.ColorAndOpacity(FSlateColor(Style.ColorOnScene()))
				.ShadowOffset(FVector2D(3.0, 3.0))
				.ShadowColorAndOpacity(Style.ColorInk() * FLinearColor(1.0f, 1.0f, 1.0f, 0.55f))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(8.0f, 0.0f, 0.0f, 26.0f))
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("ExploredUI", "Subtitle", "Isla de Sal"))
				.Font(Style.SerifItalic(34.0f))
				.ColorAndOpacity(FSlateColor(Style.ColorOnScene()))
				.ShadowOffset(FVector2D(2.0, 2.0))
				.ShadowColorAndOpacity(Style.ColorInk() * FLinearColor(1.0f, 1.0f, 1.0f, 0.55f))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
			[
			SNew(SBorder)
			.BorderImage(Style.BrushPanel())
			.Padding(FMargin(24.0f, 18.0f))
			[
				SNew(SVerticalBox)
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
					ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Credits", "Créditos"),
						FOnClicked::CreateSP(this, &SExploredMainMenu::HandleCredits))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
				[
					ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Quit", "Salir"),
						FOnClicked::CreateSP(this, &SExploredMainMenu::HandleQuit))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f))
				[
					SNew(SBox)
					.WidthOverride(420.0f)
					.Visibility(Notice.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
					[
						SNew(STextBlock)
						.Text(Notice)
						.Font(Style.FontBody())
						.AutoWrapText(true)
						.ColorAndOpacity(FSlateColor(Style.ColorWarning()))
					]
				]
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
