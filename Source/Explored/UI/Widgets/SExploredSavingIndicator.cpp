#include "UI/Widgets/SExploredSavingIndicator.h"

#include "UI/ExploredUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

void SExploredSavingIndicator::Construct(const FArguments& InArgs)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	SetVisibility(EVisibility::Collapsed);

	ChildSlot
	[
		SNew(SBox)
		.Padding(FMargin(0.0f, 0.0f, 24.0f, 24.0f))
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		[
			SNew(SBorder)
			.BorderImage(Style.BrushPanelLight())
			.Padding(FMargin(16.0f, 8.0f))
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("ExploredUI", "Saving", "Guardando..."))
				.Font(Style.FontBody())
				.ColorAndOpacity(FSlateColor(Style.ColorInk()))
			]
		]
	];
}

void SExploredSavingIndicator::Show(float Seconds)
{
	RemainingSeconds = Seconds;
	SetVisibility(EVisibility::HitTestInvisible);
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SExploredSavingIndicator::HandleHideTick));
}

EActiveTimerReturnType SExploredSavingIndicator::HandleHideTick(double InCurrentTime, float InDeltaTime)
{
	RemainingSeconds -= InDeltaTime;
	if (RemainingSeconds <= 0.0f)
	{
		SetVisibility(EVisibility::Collapsed);
		return EActiveTimerReturnType::Stop;
	}
	return EActiveTimerReturnType::Continue;
}
