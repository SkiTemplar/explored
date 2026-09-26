#include "UI/Widgets/SExploredCredits.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UI/ExploredUIStyle.h"
#include "UI/SettingsLogic.h"
#include "UI/Widgets/ExploredUIWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// Espacio de nombres con nombre (no anónimo) para no chocar en el Unity build.
namespace ExploredCreditsDetail
{
	TSharedRef<SWidget> Line(const FText& Text, bool bHeading = false)
	{
		const FExploredUIStyle& Style = FExploredUIStyle::Get();
		return SNew(STextBlock)
			.Text(Text)
			.Font(bHeading ? Style.FontHeading() : Style.FontBody())
			.ColorAndOpacity(FSlateColor(bHeading ? Style.ColorAccent() : Style.ColorInk()))
			.Justification(ETextJustify::Center);
	}
}

void SExploredCredits::Construct(const FArguments& InArgs)
{
	using ExploredCreditsDetail::Line;

	OnBack = InArgs._OnBack;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();

	TSharedRef<SWidget> BackButton = ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Back", "Volver"),
		FOnClicked::CreateSP(this, &SExploredCredits::HandleBack), false);
	InitialFocus = BackButton;

	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(Style.BrushPanel())
			.Padding(FMargin(48.0f, 36.0f))
			[
				SNew(SBox)
				.WidthOverride(620.0f)
				.HeightOverride(420.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SNew(SScrollBox)
						.ScrollBarStyle(&Style.ScrollBarStyle())
						+ SScrollBox::Slot().Padding(FMargin(0.0f, 8.0f)) [ Line(NSLOCTEXT("ExploredUI", "CreditsTitle", "EXPLORED"), true) ]
						+ SScrollBox::Slot().Padding(FMargin(0.0f, 24.0f, 0.0f, 8.0f)) [ Line(NSLOCTEXT("ExploredUI", "CreditsAuthor", "Un juego de Rodrigo Fernández")) ]
						+ SScrollBox::Slot().Padding(FMargin(0.0f, 8.0f)) [ Line(NSLOCTEXT("ExploredUI", "CreditsClaude", "Desarrollado con Claude (Anthropic)")) ]
						+ SScrollBox::Slot().Padding(FMargin(0.0f, 8.0f)) [ Line(NSLOCTEXT("ExploredUI", "CreditsEngine", "Motor: Unreal Engine 5.6")) ]
						+ SScrollBox::Slot().Padding(FMargin(0.0f, 8.0f, 0.0f, 24.0f))
						[
							Line(NSLOCTEXT("ExploredUI", "CreditsArt", "Todo el arte, sonido y música generados por código"))
						]
						// Dedicatoria (GDD §15): la misma clave que el menú principal, idéntica en los dos idiomas.
						+ SScrollBox::Slot().Padding(FMargin(0.0f, 8.0f, 0.0f, 24.0f))
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("ExploredUI", "Dedication", "Para Almudena, mi Limón"))
							.Font(Style.FontSubtitle())
							.ColorAndOpacity(FSlateColor(Style.ColorAccent()))
							.Justification(ETextJustify::Center)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f))
					[
						BackButton
					]
				]
			]
		]
	];
}

FReply SExploredCredits::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (ExploredSettingsLogic::IsMenuBackKey(InKeyEvent.GetKey().GetFName()))
	{
		return HandleBack();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SExploredCredits::HandleBack()
{
	OnBack.ExecuteIfBound();
	return FReply::Handled();
}
