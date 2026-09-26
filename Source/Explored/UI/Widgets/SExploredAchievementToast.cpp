#include "UI/Widgets/SExploredAchievementToast.h"

#include "UI/ExploredUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SExploredAchievementToast::Construct(const FArguments& InArgs)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	SetVisibility(EVisibility::Collapsed);

	ChildSlot
	[
		SNew(SBox)
		.Padding(FMargin(0.0f, 24.0f, 24.0f, 0.0f))
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.MaxDesiredWidth(420.0f)
			[
				SNew(SBorder)
				.BorderImage(Style.BrushPanelLight())
				.Padding(FMargin(18.0f, 12.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("ExploredUI", "AchievementUnlocked", "Logro conseguido"))
						.Font(Style.FontSmall())
						.ColorAndOpacity(FSlateColor(Style.ColorAccent()))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
					[
						SAssignNew(NameText, STextBlock)
						.Font(Style.FontHeading())
						.ColorAndOpacity(FSlateColor(Style.ColorInk()))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
					[
						SAssignNew(DescriptionText, STextBlock)
						.Font(Style.FontBody())
						.ColorAndOpacity(FSlateColor(Style.ColorInkDim()))
						.AutoWrapText(true)
					]
				]
			]
		]
	];
}

void SExploredAchievementToast::Enqueue(const FText& Name, const FText& Description, float Seconds)
{
	Queue.Add({ Name, Description, Seconds });
	if (!bTimerActive)
	{
		ShowNext();
		bTimerActive = true;
		RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SExploredAchievementToast::HandleToastTick));
	}
}

void SExploredAchievementToast::ShowNext()
{
	if (Queue.IsEmpty())
	{
		SetVisibility(EVisibility::Collapsed);
		return;
	}
	const FPendingToast Next = Queue[0];
	Queue.RemoveAt(0);
	NameText->SetText(Next.Name);
	DescriptionText->SetText(Next.Description);
	RemainingSeconds = Next.Seconds;
	SetVisibility(EVisibility::HitTestInvisible);
}

EActiveTimerReturnType SExploredAchievementToast::HandleToastTick(double InCurrentTime, float InDeltaTime)
{
	RemainingSeconds -= InDeltaTime;
	if (RemainingSeconds > 0.0f)
	{
		return EActiveTimerReturnType::Continue;
	}
	if (Queue.IsEmpty())
	{
		SetVisibility(EVisibility::Collapsed);
		bTimerActive = false;
		return EActiveTimerReturnType::Stop;
	}
	ShowNext();
	return EActiveTimerReturnType::Continue;
}
