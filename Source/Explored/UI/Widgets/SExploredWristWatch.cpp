#include "UI/Widgets/SExploredWristWatch.h"

#include "Styling/SlateColor.h"
#include "UI/ExploredUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SExploredWristWatch::Construct(const FArguments& InArgs)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	Readout = InArgs._Readout;
	Raised = InArgs._Raised;

	// Nunca intercepta el ratón: es un objeto del mundo, no un menú.
	SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SExploredWristWatch::GetWatchVisibility));

	ChildSlot
	[
		SNew(SBox)
		.Padding(FMargin(32.0f, 0.0f, 0.0f, 48.0f))
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		[
			SNew(SBorder)
			.BorderImage(Style.BrushPanel())
			.Padding(FMargin(18.0f, 12.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SExploredWristWatch::GetTimeText)
					.Font(Style.FontHeading())
					.ColorAndOpacity(FSlateColor(Style.ColorInk()))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
				[
					SNew(SVerticalBox)
					.Visibility(this, &SExploredWristWatch::GetNeedsVisibility)
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeNeedRow(NSLOCTEXT("ExploredUI", "WatchWater", "Agua"), &FWristWatchReadout::Thirst)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeNeedRow(NSLOCTEXT("ExploredUI", "WatchFood", "Comida"), &FWristWatchReadout::Hunger)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeNeedRow(NSLOCTEXT("ExploredUI", "WatchRest", "Sueño"), &FWristWatchReadout::Rest)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeNeedRow(NSLOCTEXT("ExploredUI", "WatchWarmth", "Calor"), &FWristWatchReadout::Warmth)
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SExploredWristWatch::MakeNeedRow(const FText& Label, ENeedLevel FWristWatchReadout::* Field)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	TWeakPtr<SExploredWristWatch> WeakSelf = SharedThis(this);
	auto Level = [WeakSelf, Field]()
	{
		const TSharedPtr<SExploredWristWatch> Pinned = WeakSelf.Pin();
		return Pinned.IsValid() ? Pinned->Readout.Get().*Field : ENeedLevel::Good;
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(FMargin(0.0f, 0.0f, 16.0f, 0.0f))
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(Style.FontSmall())
			.ColorAndOpacity(FSlateColor(Style.ColorInkDim()))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text_Lambda([Level]() { return LevelText(Level()); })
			.Font(Style.FontSmall())
			.ColorAndOpacity_Lambda([Level]() { return LevelColor(Level()); })
		];
}

EVisibility SExploredWristWatch::GetWatchVisibility() const
{
	return Raised.Get(false) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

EVisibility SExploredWristWatch::GetNeedsVisibility() const
{
	return Readout.Get().bShowNeeds ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FText SExploredWristWatch::GetTimeText() const
{
	const FWristWatchReadout R = Readout.Get();
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), R.Hour, R.Minute));
}

FText SExploredWristWatch::LevelText(ENeedLevel Level)
{
	switch (Level)
	{
	case ENeedLevel::Good: return NSLOCTEXT("ExploredUI", "NeedGood", "bien");
	case ENeedLevel::Fair: return NSLOCTEXT("ExploredUI", "NeedFair", "regular");
	case ENeedLevel::Low: return NSLOCTEXT("ExploredUI", "NeedLow", "mal");
	default: return NSLOCTEXT("ExploredUI", "NeedCritical", "muy mal");
	}
}

FSlateColor SExploredWristWatch::LevelColor(ENeedLevel Level)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	switch (Level)
	{
	case ENeedLevel::Good: return FSlateColor(Style.ColorInk());
	case ENeedLevel::Fair: return FSlateColor(Style.ColorInkDim());
	case ENeedLevel::Low: return FSlateColor(Style.ColorAccent());
	default: return FSlateColor(FLinearColor(0.85f, 0.25f, 0.18f, 1.0f));
	}
}
