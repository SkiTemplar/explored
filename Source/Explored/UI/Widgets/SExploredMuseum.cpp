#include "UI/Widgets/SExploredMuseum.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "Core/ExploredLocalization.h"
#include "Ruins/MuseumModel.h"
#include "Ruins/RuinsSubsystem.h"
#include "UI/ExploredUIStyle.h"
#include "UI/ScreensLogic.h"
#include "UI/SettingsLogic.h"
#include "UI/Widgets/ExploredUIWidgets.h"

namespace ExploredMuseumDetail
{
	FText ProvenanceText(EArtifactProvenance Provenance)
	{
		switch (Provenance)
		{
		case EArtifactProvenance::Marae: return NSLOCTEXT("ExploredUI", "MuseumFromMarae", "Marae");
		case EArtifactProvenance::RitualCave: return NSLOCTEXT("ExploredUI", "MuseumFromCave", "Cueva ritual");
		case EArtifactProvenance::Shipwreck: return NSLOCTEXT("ExploredUI", "MuseumFromWreck", "Pecio");
		default: return NSLOCTEXT("ExploredUI", "MuseumFromSunken", "Ruina sumergida");
		}
	}

	FText RarityText(EArtifactRarity Rarity)
	{
		switch (Rarity)
		{
		case EArtifactRarity::Rare: return NSLOCTEXT("ExploredUI", "MuseumRare", "raro");
		case EArtifactRarity::Unique: return NSLOCTEXT("ExploredUI", "MuseumUnique", "único");
		default: return NSLOCTEXT("ExploredUI", "MuseumCommon", "común");
		}
	}

	/** Silueta del tesoro: una mancha oscura con «?» si no se conoce; tinta y ámbar si sí. */
	TSharedRef<SWidget> Silhouette(ExploredScreens::EMuseumEntryState State)
	{
		const FExploredUIStyle& Style = FExploredUIStyle::Get();
		using ExploredScreens::EMuseumEntryState;
		const bool bKnown = State != EMuseumEntryState::Unknown;
		const FLinearColor Fill = State == EMuseumEntryState::Exhibited ? Style.ColorAccent()
			: State == EMuseumEntryState::Found ? Style.ColorAccentDim()
			: State == EMuseumEntryState::Photographed ? Style.ColorInkDim()
			: FLinearColor(0.05f, 0.04f, 0.03f, 0.9f);
		return SNew(SBox)
			.WidthOverride(44.0f)
			.HeightOverride(44.0f)
			[
				SNew(SBorder)
				.BorderImage(Style.BrushWhite())
				.BorderBackgroundColor(FSlateColor(Fill))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(bKnown ? FText::GetEmpty() : INVTEXT("?"))
					.Font(Style.FontHeading())
					.ColorAndOpacity(FSlateColor(Style.ColorInkDim()))
				]
			];
	}
}

void SExploredMuseum::Construct(const FArguments& InArgs)
{
	using namespace ExploredMuseumDetail;
	using ExploredScreens::EMuseumEntryState;

	OnBack = InArgs._OnBack;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const URuinsSubsystem* Ruins = InArgs._Ruins.Get();
	const bool bEnglish = ExploredLocalization::IsEnglishCulture(ExploredUIWidgets::CurrentCultureName());

	TSharedRef<SWidget> BackButton = ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Back", "Volver"),
		FOnClicked::CreateSP(this, &SExploredMuseum::HandleBack), false);
	InitialFocus = BackButton;

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	FText Subtitle = NSLOCTEXT("ExploredUI", "MuseumUnavailable", "El catálogo no está disponible en este mundo.");

	if (Ruins)
	{
		const FMuseumModel& Museum = Ruins->GetMuseum();
		const ExploredScreens::FMuseumSummary Summary = ExploredScreens::SummarizeMuseum(Museum);
		Subtitle = FText::Format(NSLOCTEXT("ExploredUI", "MuseumSummary", "Catálogo: {0} de {1} registrados · {2} hallados"),
			FText::AsNumber(Summary.Registered), FText::AsNumber(Summary.Total), FText::AsNumber(Summary.Found));

		const float CollectorProgress = Summary.CollectorProgress;
		const float CatalogProgress = Summary.CatalogProgress;
		Body->AddSlot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
		[
			SNew(STextBlock)
			.Text(FText::Format(Summary.bCollectorDone
					? NSLOCTEXT("ExploredUI", "MuseumCollectorDone", "Coleccionista: {0} tesoros expuestos (conseguido)")
					: NSLOCTEXT("ExploredUI", "MuseumCollector", "Coleccionista: {0} de {1} tesoros expuestos"),
				FText::AsNumber(Summary.Exhibited), FText::AsNumber(Summary.CollectorTarget)))
			.Font(Style.FontBody())
			.ColorAndOpacity(FSlateColor(Summary.bCollectorDone ? Style.ColorAccent() : Style.ColorInk()))
		];
		Body->AddSlot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
		[
			ExploredUIWidgets::MakeProgressBar(TAttribute<TOptional<float>>(TOptional<float>(CollectorProgress)))
		];
		Body->AddSlot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 14.0f))
		[
			ExploredUIWidgets::MakeProgressBar(TAttribute<TOptional<float>>(TOptional<float>(CatalogProgress)), 4.0f)
		];

		TSharedRef<SScrollBox> List = SNew(SScrollBox)
			.ScrollBarStyle(&Style.ScrollBarStyle())
			.ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
		bool bFirst = true;
		for (const ExploredScreens::FMuseumEntry& Entry : ExploredScreens::BuildMuseumEntries(Museum))
		{
			const FText Name = Entry.bShowName ? FText::FromString(Ruins->GetDisplayName(Entry.ArtifactId, bEnglish)) : INVTEXT("???");
			FText State;
			switch (Entry.State)
			{
			case EMuseumEntryState::Exhibited: State = NSLOCTEXT("ExploredUI", "MuseumExhibited", "Expuesto en el museo"); break;
			case EMuseumEntryState::Found: State = NSLOCTEXT("ExploredUI", "MuseumFound", "Hallado"); break;
			case EMuseumEntryState::Photographed: State = NSLOCTEXT("ExploredUI", "MuseumPhotographed", "Fotografiado, sin recoger"); break;
			default: State = NSLOCTEXT("ExploredUI", "MuseumUnknown", "Sin descubrir"); break;
			}
			// Lugar del hallazgo: solo si el nombre existe en ruins.json (GetDisplayName devuelve el id si no).
			if (!Entry.FoundAt.IsNone())
			{
				const FString Place = Ruins->GetDisplayName(Entry.FoundAt, bEnglish);
				if (Place != Entry.FoundAt.ToString())
				{
					State = FText::Format(NSLOCTEXT("ExploredUI", "MuseumStateAt", "{0} · {1}"), State, FText::FromString(Place));
				}
			}
			const FText Provenance = FText::Format(NSLOCTEXT("ExploredUI", "MuseumProvenance", "Procedencia: {0} · {1}"),
				ProvenanceText(Entry.Provenance), RarityText(Entry.Rarity));

			TSharedRef<SButton> Row = SNew(SButton)
				.ButtonStyle(&Style.ButtonStyle())
				.OnClicked_Lambda([]() { return FReply::Handled(); })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
					[
						Silhouette(Entry.State)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(Name)
							.Font(Style.FontBody())
							.ColorAndOpacity(FSlateColor(Entry.bShowName ? Style.ColorInk() : Style.ColorInkDim()))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(State)
							.Font(Style.FontSmall())
							.ColorAndOpacity(FSlateColor(Entry.State == EMuseumEntryState::Exhibited ? Style.ColorAccent() : Style.ColorInkDim()))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(Provenance)
							.Font(Style.FontSmall())
							.ColorAndOpacity(FSlateColor(Style.ColorInkDim()))
						]
					]
				];
			if (bFirst)
			{
				InitialFocus = Row;
				bFirst = false;
			}
			List->AddSlot().Padding(FMargin(0.0f, 2.0f)) [ Row ];
		}
		Body->AddSlot().FillHeight(1.0f) [ List ];
	}

	ChildSlot
	[
		ExploredUIWidgets::MakeScreenPanel(NSLOCTEXT("ExploredUI", "MuseumTitle", "Museo"), Subtitle, Body,
			SNew(SBox).HAlign(HAlign_Center) [ BackButton ], 640.0f, 520.0f)
	];
}

FReply SExploredMuseum::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (ExploredSettingsLogic::IsMenuBackKey(InKeyEvent.GetKey().GetFName()))
	{
		return HandleBack();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SExploredMuseum::HandleBack()
{
	OnBack.ExecuteIfBound();
	return FReply::Handled();
}
