#include "UI/Widgets/SExploredModeSelect.h"

#include "UI/ExploredUIStyle.h"
#include "UI/Widgets/ExploredUIWidgets.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SExploredModeSelect::Construct(const FArguments& InArgs)
{
	OnChosen = InArgs._OnChosen;
	OnBack = InArgs._OnBack;

	const FExploredUIStyle& Style = FExploredUIStyle::Get();

	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(Style.BrushPanel())
			.Padding(FMargin(48.0f, 40.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 24.0f))
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("ExploredUI", "ChooseMode", "Elige cómo quieres sobrevivir"))
					.Font(Style.FontHeading())
					.ColorAndOpacity(FSlateColor(Style.ColorInk()))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 6.0f))
				[
					MakeModeRow(EExploredGameplayMode::Explorer,
						NSLOCTEXT("ExploredUI", "ModeExplorer", "Explorador"),
						NSLOCTEXT("ExploredUI", "ModeExplorerDesc", "Las necesidades no matan; la fauna es pacífica. Para disfrutar del mundo y la historia."))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 6.0f))
				[
					MakeModeRow(EExploredGameplayMode::Survivor,
						NSLOCTEXT("ExploredUI", "ModeSurvivor", "Superviviente"),
						NSLOCTEXT("ExploredUI", "ModeSurvivorDesc", "La experiencia prevista."))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 6.0f))
				[
					MakeModeRow(EExploredGameplayMode::Castaway,
						NSLOCTEXT("ExploredUI", "ModeCastaway", "Náufrago"),
						NSLOCTEXT("ExploredUI", "ModeCastawayDesc", "Necesidades más duras y sin reaparición en fogatas."))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 20.0f, 0.0f, 0.0f))
				[
					ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Back", "Volver"),
						FOnClicked::CreateSP(this, &SExploredModeSelect::HandleBack), false)
				]
			]
		]
	];
}

TSharedRef<SWidget> SExploredModeSelect::MakeModeRow(EExploredGameplayMode Mode, const FText& Title, const FText& Description)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	return SNew(SButton)
		.ButtonStyle(&Style.ButtonStyle())
		.OnClicked(FOnClicked::CreateSP(this, &SExploredModeSelect::HandleChoose, Mode))
		.ContentPadding(FMargin(6.0f))
		.HAlign(HAlign_Left)
		[
			SNew(SBox)
			.MinDesiredWidth(420.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(Title)
					.Font(Style.FontHeading())
					.ColorAndOpacity(FSlateColor(Style.ColorInk()))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(Description)
					.Font(Style.FontBody())
					.ColorAndOpacity(FSlateColor(Style.ColorInkDim()))
					.WrapTextAt(400.0f)
				]
			]
		];
}

FReply SExploredModeSelect::HandleChoose(EExploredGameplayMode Mode)
{
	OnChosen.ExecuteIfBound(Mode);
	return FReply::Handled();
}

FReply SExploredModeSelect::HandleBack()
{
	OnBack.ExecuteIfBound();
	return FReply::Handled();
}
