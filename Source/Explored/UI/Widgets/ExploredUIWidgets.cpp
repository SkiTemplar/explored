#include "UI/Widgets/ExploredUIWidgets.h"

#include "Core/ExploredLocalization.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "UI/ExploredUIPalette.h"
#include "UI/ExploredUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"

TSharedRef<SWidget> ExploredUIWidgets::MakeMenuButton(const FText& Label, FOnClicked OnClicked, bool bLarge)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	return SNew(SButton)
		.ButtonStyle(&Style.ButtonStyle())
		.OnClicked(OnClicked)
		.ContentPadding(FMargin(0.0f))
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(bLarge ? Style.FontHeading() : Style.FontBody())
			.ColorAndOpacity(FSlateColor(Style.ColorInk()))
		];
}

TSharedRef<SWidget> ExploredUIWidgets::MakeFocusMenuButton(const FText& Label, FOnClicked OnClicked, bool bEnabled,
	TSharedPtr<SWidget>* OutFocusable)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const FLinearColor Ink = Style.ColorInk();
	const FLinearColor Dim(Ink.R, Ink.G, Ink.B, 0.35f);
	const FLinearColor Focus = ExploredPalette::Linear(ExploredPalette::CieloOcaso);
	TSharedPtr<SButton> Button;
	// La pastilla y el triángulo preguntan al botón si tiene el foco (puntero débil: sin ciclos).
	TSharedRef<TWeakPtr<SButton>> Weak = MakeShared<TWeakPtr<SButton>>();
	const auto HasFocus = [Weak]()
	{
		const TSharedPtr<SButton> Pinned = Weak->Pin();
		return Pinned.IsValid() && Pinned->HasKeyboardFocus();
	};
	TSharedRef<SWidget> Row = SNew(SBorder)
		.BorderImage(Style.BrushRound(2))
		.BorderBackgroundColor_Lambda([HasFocus, Focus]() { return HasFocus() ? Focus : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f); })
		.Padding(0.0f)
		[
			SAssignNew(Button, SButton)
			.ButtonStyle(&Style.ButtonStyle())
			.OnClicked(OnClicked)
			.IsEnabled(bEnabled)
			.IsFocusable(bEnabled)
			.ContentPadding(FMargin(0.0f))
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("›")))  // loc: ignorar (Lora no trae el triángulo)
					.Font(Style.FontHeading())
					.ColorAndOpacity(FSlateColor(Ink))
					.Visibility_Lambda([HasFocus]() { return HasFocus() ? EVisibility::Visible : EVisibility::Hidden; })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Label)
					.Font(Style.FontHeading())
					.ColorAndOpacity(FSlateColor(bEnabled ? Ink : Dim))
				]
			]
		];
	*Weak = Button;
	if (OutFocusable)
	{
		*OutFocusable = Button;
	}
	return Row;
}

TSharedRef<SWidget> ExploredUIWidgets::MakeTabButton(const FText& Label, bool bActive, FOnClicked OnClicked)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	return SNew(SButton)
		.ButtonStyle(&Style.TabButtonStyle(bActive))
		.OnClicked(OnClicked)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(Style.FontBody())
			.ColorAndOpacity(FSlateColor(bActive ? Style.ColorAccent() : Style.ColorInkDim()))
		];
}

FString ExploredUIWidgets::CurrentCultureName()
{
	return FInternationalization::Get().GetCurrentCulture()->GetName();
}

FText ExploredUIWidgets::PickLocalized(const FString& Es, const FString& En)
{
	return FText::FromString(ExploredLocalization::Pick(Es, En, CurrentCultureName()));
}

TSharedRef<SWidget> ExploredUIWidgets::MakeScreenPanel(const FText& Title, TAttribute<FText> Subtitle, TSharedRef<SWidget> Body,
	TSharedRef<SWidget> Footer, float Width, float Height)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(Style.BrushPanel())
			.Padding(FMargin(40.0f, 28.0f))
			[
				SNew(SBox)
				.WidthOverride(Width)
				.HeightOverride(Height)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(Title)
						.Font(Style.FontHeading())
						.ColorAndOpacity(FSlateColor(Style.ColorInk()))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 16.0f))
					[
						SNew(STextBlock)
						.Text(Subtitle)
						.Font(Style.FontBody())
						.ColorAndOpacity(FSlateColor(Style.ColorInkDim()))
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						Body
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f))
					[
						Footer
					]
				]
			]
		];
}

TSharedRef<SWidget> ExploredUIWidgets::MakeProgressBar(TAttribute<TOptional<float>> Percent, float Height)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	return SNew(SBox)
		.HeightOverride(Height)
		[
			SNew(SProgressBar)
			.Style(&Style.ProgressBarStyle())
			.Percent(Percent)
			.FillColorAndOpacity(FSlateColor(FLinearColor::White))
		];
}

TSharedRef<SWidget> ExploredUIWidgets::MakeSceneButton(const FText& Label, FOnClicked OnClicked)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	return SNew(SButton)
		.ButtonStyle(&Style.SceneButtonStyle())
		.OnClicked(OnClicked)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(Style.SerifBold(30.0f))
			.ColorAndOpacity(FSlateColor(Style.ColorOnScene()))
			.ShadowOffset(FVector2D(2.0, 2.0))
			.ShadowColorAndOpacity(Style.ColorInk() * FLinearColor(1.0f, 1.0f, 1.0f, 0.6f))
		];
}
