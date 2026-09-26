#include "UI/Widgets/SExploredAchievements.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Achievements/AchievementsModel.h"
#include "Achievements/AchievementsSubsystem.h"
#include "UI/ExploredUIStyle.h"
#include "UI/ScreensLogic.h"
#include "UI/SettingsLogic.h"
#include "UI/Widgets/ExploredUIWidgets.h"

void SExploredAchievements::Construct(const FArguments& InArgs)
{
	OnBack = InArgs._OnBack;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const UAchievementsSubsystem* Achievements = InArgs._Achievements.Get();

	TSharedRef<SWidget> BackButton = ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Back", "Volver"),
		FOnClicked::CreateSP(this, &SExploredAchievements::HandleBack), false);
	InitialFocus = BackButton;

	FText Subtitle = NSLOCTEXT("ExploredUI", "AchievementsUnavailable", "Los logros no están disponibles.");
	TSharedRef<SScrollBox> List = SNew(SScrollBox)
		.ScrollBarStyle(&Style.ScrollBarStyle())
		.ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);

	if (Achievements)
	{
		const FAchievementsModel& Model = Achievements->GetModel();
		const ExploredScreens::FAchievementsSummary Summary = ExploredScreens::SummarizeAchievements(Model);
		Subtitle = FText::Format(NSLOCTEXT("ExploredUI", "AchievementsSummary", "Conseguidos: {0} de {1}"),
			FText::AsNumber(Summary.Unlocked), FText::AsNumber(Summary.Total));

		bool bFirst = true;
		for (const ExploredScreens::FAchievementRow& Row : ExploredScreens::BuildAchievementRows(Model))
		{
			const FText Name = Row.bMasked ? INVTEXT("???") : Achievements->GetDisplayName(Row.Id);
			FText Description = Row.bMasked ? NSLOCTEXT("ExploredUI", "AchievementHidden", "Logro oculto: se revela al conseguirlo.")
				: Achievements->GetDescription(Row.Id);
			if (!Row.bAvailableInMode)
			{
				Description = FText::Format(NSLOCTEXT("ExploredUI", "AchievementOtherMode", "{0} (no se puede conseguir en este modo)"), Description);
			}
			const FLinearColor NameColor = Row.bUnlocked ? Style.ColorAccent() : (Row.bAvailableInMode ? Style.ColorInk() : Style.ColorDisabled());
			const float Progress = Row.Progress;

			TSharedRef<SVerticalBox> Content = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(Name)
						.Font(Style.FontBody())
						.ColorAndOpacity(FSlateColor(NameColor))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock)
						.Text(Row.bUnlocked ? NSLOCTEXT("ExploredUI", "AchievementDone", "Conseguido") : FText::GetEmpty())
						.Font(Style.FontSmall())
						.ColorAndOpacity(FSlateColor(Style.ColorAccent()))
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(Description)
					.Font(Style.FontSmall())
					.AutoWrapText(true)
					.ColorAndOpacity(FSlateColor(Style.ColorInkDim()))
				];
			if (!Row.bMasked && !Row.bUnlocked)
			{
				Content->AddSlot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
				[
					ExploredUIWidgets::MakeProgressBar(TAttribute<TOptional<float>>(TOptional<float>(Progress)), 5.0f)
				];
			}

			TSharedRef<SButton> RowButton = SNew(SButton)
				.ButtonStyle(&Style.ButtonStyle())
				.OnClicked_Lambda([]() { return FReply::Handled(); })
				[
					Content
				];
			if (bFirst)
			{
				InitialFocus = RowButton;
				bFirst = false;
			}
			List->AddSlot().Padding(FMargin(0.0f, 2.0f)) [ RowButton ];
		}
	}

	ChildSlot
	[
		ExploredUIWidgets::MakeScreenPanel(NSLOCTEXT("ExploredUI", "AchievementsTitle", "Logros"), Subtitle, List,
			SNew(SBox).HAlign(HAlign_Center) [ BackButton ], 640.0f, 520.0f)
	];
}

FReply SExploredAchievements::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (ExploredSettingsLogic::IsMenuBackKey(InKeyEvent.GetKey().GetFName()))
	{
		return HandleBack();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SExploredAchievements::HandleBack()
{
	OnBack.ExecuteIfBound();
	return FReply::Handled();
}
