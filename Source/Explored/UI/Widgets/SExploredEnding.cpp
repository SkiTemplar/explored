#include "UI/Widgets/SExploredEnding.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "UI/ExploredUIStyle.h"
#include "UI/SettingsLogic.h"
#include "UI/Widgets/ExploredUIWidgets.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace ExploredEndingDetail
{
	/** Azul de la noche en alta mar, el fondo de toda la pantalla. */
	const FLinearColor NightSky(0.012f, 0.025f, 0.07f, 1.0f);
	const FLinearColor StarWhite(0.93f, 0.91f, 0.82f, 1.0f);
	const FLinearColor LemonYellow(1.0f, 0.86f, 0.22f, 1.0f);

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

	TSharedRef<SWidget> ContinueButton = ExploredUIWidgets::MakeMenuButton(
		NSLOCTEXT("ExploredUI", "EndingContinue", "Volver al menú"),
		FOnClicked::CreateSP(this, &SExploredEnding::HandleContinue), false);
	InitialFocus = ContinueButton;

	const FText Days = FText::Format(
		NSLOCTEXT("ExploredUI", "EndingDays", "{0} {0}|plural(one=día,other=días) en el archipiélago."),
		FMath::Max(1, InArgs._DaysSurvived));

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.ColorAndOpacity(FSlateColor(NightSky))
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(760.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 28.0f))
				[
					Line(NSLOCTEXT("ExploredUI", "EndingLine1", "El Limón dejó la laguna con la marea de la noche."), Style.FontSubtitle(), StarWhite)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 28.0f))
				[
					Line(NSLOCTEXT("ExploredUI", "EndingLine2", "Sin brújula ni mapa: solo las estrellas que enseñaron los navegantes."), Style.FontBody(), StarWhite)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 48.0f))
				[
					Line(Days, Style.FontBody(), StarWhite)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
				[
					Line(NSLOCTEXT("ExploredUI", "CreditsTitle", "EXPLORED"), Style.FontHeading(), LemonYellow)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
				[
					Line(NSLOCTEXT("ExploredUI", "CreditsAuthor", "Un juego de Rodrigo Fernández"), Style.FontBody(), StarWhite)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 40.0f))
				[
					Line(NSLOCTEXT("ExploredUI", "Dedication", "Para Almudena, mi Limón"), Style.FontSubtitle(), LemonYellow)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					ContinueButton
				]
			]
		]
	];
}

FReply SExploredEnding::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (ExploredSettingsLogic::IsMenuBackKey(InKeyEvent.GetKey().GetFName()))
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
