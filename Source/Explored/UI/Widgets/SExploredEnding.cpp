#include "UI/Widgets/SExploredEnding.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Math/RandomStream.h"
#include "Narrative/FinaleSequenceModel.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/ExploredUIPalette.h"
#include "UI/ExploredUIStyle.h"
#include "UI/Widgets/ExploredUIWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace ExploredEndingDetail
{
	/** Azul de la noche en alta mar, el fondo de toda la pantalla. */
	const FLinearColor NightSky = ExploredPalette::Linear(ExploredPalette::NocheCenit);
	const FLinearColor StarWhite = ExploredPalette::Linear(ExploredPalette::Papel);
	const FLinearColor BeaconAmber = ExploredPalette::Linear(ExploredPalette::Laton);
	constexpr int32 StarCount = 160;
	constexpr int32 StarSeed = 1974;

	TSharedRef<SWidget> Line(const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(Font)
			.ColorAndOpacity(FSlateColor(Color))
			.Justification(ETextJustify::Center)
			.AutoWrapText(true);
	}
}

void SExploredEnding::Construct(const FArguments& InArgs)
{
	using namespace ExploredEndingDetail;

	OnContinue = InArgs._OnContinue;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();

	TSharedRef<SWidget> ContinueButton = ExploredUIWidgets::MakeSceneButton(
		NSLOCTEXT("ExploredUI", "EndingContinue", "Continuar"),
		FOnClicked::CreateSP(this, &SExploredEnding::HandleContinue));
	InitialFocus = ContinueButton;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(760.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 28.0f))
				[
					Line(InArgs._EpilogueLine, Style.FontSubtitle(), StarWhite)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 48.0f))
				[
					Line(InArgs._CommonLine, Style.FontBody(), StarWhite)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
				[
					Line(NSLOCTEXT("ExploredUI", "CreditsTitle", "BALIZA SIETE"), Style.FontHeading(), BeaconAmber)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 40.0f))
				[
					Line(NSLOCTEXT("ExploredUI", "CreditsAuthor", "Un juego de Rodrigo Fernández"), Style.FontBody(), StarWhite)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					ContinueButton
				]
			]
		]
	];
}

int32 SExploredEnding::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// Fondo en la capa base, estrellas encima y el texto y el botón después.
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), White, ESlateDrawEffect::None,
		ExploredEndingDetail::NightSky);
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const double Now = FPlatformTime::Seconds();
	FRandomStream Rng(ExploredEndingDetail::StarSeed);
	for (int32 I = 0; I < ExploredEndingDetail::StarCount; ++I)
	{
		const FVector2D Position(Rng.FRand() * Size.X, Rng.FRand() * Size.Y * 0.85f);
		const float Radius = Rng.FRandRange(1.0f, 2.6f);
		const float Phase = Rng.FRand() * 6.28f;
		const float Twinkle = 0.55f + 0.45f * FMath::Sin(static_cast<float>(Now) * Rng.FRandRange(0.6f, 2.2f) + Phase);
		FLinearColor Color = ExploredEndingDetail::StarWhite;
		Color.A = Twinkle;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2f(Radius, Radius), FSlateLayoutTransform(FVector2f(Position))),
			White, ESlateDrawEffect::None, Color);
	}
	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle, bParentEnabled);
}

FReply SExploredEnding::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (FFinaleSequenceModel::IsEpilogueContinueKey(InKeyEvent.GetKey().GetFName()))
	{
		return HandleContinue();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SExploredEnding::HandleContinue()
{
	OnContinue.ExecuteIfBound();
	return FReply::Handled();
}
