#include "UI/Widgets/SExploredMapInHands.h"

#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "Cartography/CartographyComponent.h"
#include "Cartography/CartographyModel.h"
#include "Core/ExploredLocalization.h"
#include "Crafting/CraftingTypes.h"
#include "Items/ItemRegistrySubsystem.h"
#include "Items/ItemTypes.h"
#include "UI/ExploredGameUserSettings.h"
#include "UI/ExploredUIStyle.h"
#include "UI/Widgets/ExploredUIWidgets.h"
#include "UI/Widgets/SExploredDoodle.h"
#include "UI/Widgets/SExploredMapSheet.h"

namespace ExploredMapInHandsDetail
{
	/** Segundos que tarda la hoja en subir a las manos. */
	constexpr float RaiseSeconds = 0.3f;
	/** Inclinación de la hoja sostenida (grados) y recorrido de la subida (unidades de Slate). */
	constexpr float TiltDegrees = -1.5f;
	constexpr float RaiseTravel = 760.0f;
	/** Lado de la hoja y ancho del cuaderno del margen. */
	constexpr float SheetSide = 640.0f;
	constexpr float NotebookWidth = 300.0f;

	enum ETab : int32
	{
		TabMarks = 0,
		TabWayfinding = 1,
		TabRecipes = 2
	};

	FText TechniqueName(EWayfindingTechnique Technique)
	{
		switch (Technique)
		{
		case EWayfindingTechnique::StarPath: return NSLOCTEXT("ExploredUI", "MapTechniqueStarPath", "Camino de estrellas");
		case EWayfindingTechnique::SwellReading: return NSLOCTEXT("ExploredUI", "MapTechniqueSwell", "Lectura del oleaje");
		case EWayfindingTechnique::BirdsAtDusk: return NSLOCTEXT("ExploredUI", "MapTechniqueBirds", "Aves al atardecer");
		case EWayfindingTechnique::FixedClouds: return NSLOCTEXT("ExploredUI", "MapTechniqueClouds", "Nubes fijas");
		default: return NSLOCTEXT("ExploredUI", "MapTechniqueWater", "Color del agua");
		}
	}

	FText MarkSourceName(EMapMarkSource Source)
	{
		switch (Source)
		{
		case EMapMarkSource::Spyglass: return NSLOCTEXT("ExploredUI", "MapMarkSpyglass", "vista con el catalejo");
		case EMapMarkSource::Sextant: return NSLOCTEXT("ExploredUI", "MapMarkSextant", "situada con el sextante");
		default: return NSLOCTEXT("ExploredUI", "MapMarkHand", "dibujada a mano");
		}
	}
}

void SExploredMapInHands::Construct(const FArguments& InArgs)
{
	using namespace ExploredMapInHandsDetail;

	Cartography = InArgs._Cartography;
	WorldContextObject = InArgs._WorldContextObject;
	Annotations = InArgs._Annotations;
	OnClose = InArgs._OnClose;
	OnOpenCollection = InArgs._OnOpenCollection;

	const UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get();
	bReduceMotion = Settings && Settings->GetReduceMotion();
	RaiseAlpha = bReduceMotion ? 1.0f : 0.0f;

	LoadStampLabels();
	SelectedStamp = Stamps.Num() > 0 ? Stamps[0].Id : FName();
	StatusText = NSLOCTEXT("ExploredUI", "MapHintPlace", "Pulsa en la hoja para poner la marca.");

	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const TWeakPtr<SExploredMapInHands> WeakSelf = SharedThis(this);

	Sheet = SNew(SExploredMapSheet)
		.Cartography(Cartography)
		.bInteractive(true)
		.ViewCenter_Lambda([WeakSelf]()
		{
			const TSharedPtr<SExploredMapInHands> Pinned = WeakSelf.Pin();
			return Pinned.IsValid() ? Pinned->View.Center : FVector2D(0.5, 0.5);
		})
		.ViewSize_Lambda([WeakSelf]()
		{
			const TSharedPtr<SExploredMapInHands> Pinned = WeakSelf.Pin();
			return Pinned.IsValid() ? static_cast<float>(Pinned->View.Size) : 1.0f;
		})
		.ShowWayfinding_Lambda([WeakSelf]()
		{
			const TSharedPtr<SExploredMapInHands> Pinned = WeakSelf.Pin();
			return Pinned.IsValid() && Pinned->bShowWayfinding;
		})
		.SelectedMark_Lambda([WeakSelf]()
		{
			const TSharedPtr<SExploredMapInHands> Pinned = WeakSelf.Pin();
			return Pinned.IsValid() ? Pinned->SelectedMark : int32(INDEX_NONE);
		})
		.OnViewRequested(FOnMapSheetViewRequested::CreateSP(this, &SExploredMapInHands::HandleViewRequested))
		.OnSheetClicked(FOnMapSheetClicked::CreateSP(this, &SExploredMapInHands::HandleSheetClicked));
	Sheet->SetAnnotations(Annotations);

	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(24.0f, 24.0f, 24.0f, 36.0f))
		[
			SNew(SOverlay)
			.RenderTransform(this, &SExploredMapInHands::GetPaperTransform)
			.RenderTransformPivot(FVector2D(0.5, 1.0))
			// La hoja con su cuaderno al margen.
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(Style.BrushSheet())
				.Padding(FMargin(16.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(SheetSide)
						.HeightOverride(SheetSide)
						[
							Sheet.ToSharedRef()
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(16.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(SBox)
						.WidthOverride(NotebookWidth)
						.HeightOverride(SheetSide)
						[
							BuildNotebook()
						]
					]
				]
			]
			// Pulgares que sujetan el papel por abajo.
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(60.0f, 0.0f, 0.0f, -34.0f))
			[
				SNew(SBox)
				.WidthOverride(48.0f)
				.HeightOverride(78.0f)
				.Visibility(EVisibility::HitTestInvisible)
				[
					SNew(SBorder).BorderImage(Style.BrushThumb())
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(0.0f, 0.0f, 60.0f, -34.0f))
			[
				SNew(SBox)
				.WidthOverride(48.0f)
				.HeightOverride(78.0f)
				.Visibility(EVisibility::HitTestInvisible)
				[
					SNew(SBorder).BorderImage(Style.BrushThumb())
				]
			]
		]
	];

	RefreshStampButtons();
}

void SExploredMapInHands::LoadStampLabels()
{
	FString Json;
	FString Error;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data/story_es.json");
	if (!FFileHelper::LoadFileToString(Json, *Path) || !ExploredMapView::ParseMapStampLabels(Json, Stamps, Error) || Stamps.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Explored] Mapa: no se pudieron leer los sellos de %s (%s); se usan los ids"), *Path, *Error);
		Stamps = ExploredMapView::FallbackStampLabels();
	}
}

FText SExploredMapInHands::StampLabel(FName StampId) const
{
	for (const ExploredMapView::FMapStampLabel& Stamp : Stamps)
	{
		if (Stamp.Id == StampId)
		{
			return ExploredUIWidgets::PickLocalized(Stamp.LabelEs, Stamp.LabelEn);
		}
	}
	return StampId.IsNone() ? NSLOCTEXT("ExploredUI", "MapStampNote", "Nota") : FText::FromName(StampId);
}

FText SExploredMapInHands::RecipeLabel(FName RecipeId) const
{
	const UItemRegistrySubsystem* Registry = UItemRegistrySubsystem::Resolve(WorldContextObject.Get());
	if (Registry)
	{
		for (const FCraftingTemplateDef& Template : Registry->GetTemplates())
		{
			if (Template.Id == RecipeId && !Template.NameEs.IsEmpty())
			{
				return Template.NameEs;
			}
		}
		FItemDefinition Definition;
		if (Registry->FindDefinition(RecipeId, Definition))
		{
			const bool bEnglish = ExploredLocalization::IsEnglishCulture(ExploredUIWidgets::CurrentCultureName());
			return (bEnglish && !Definition.NameEn.IsEmpty()) ? Definition.NameEn : Definition.NameEs;
		}
	}
	return FText::FromName(RecipeId);
}

TSharedRef<SWidget> SExploredMapInHands::BuildNotebook()
{
	using namespace ExploredMapInHandsDetail;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();

	TSharedRef<SWidget> CollectionButton = SNew(SButton)
		.ButtonStyle(&Style.TabButtonStyle(false))
		.OnClicked(this, &SExploredMapInHands::HandleOpenCollection)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ExploredUI", "MapCollection", "Colección"))
			.Font(Style.FontBody())
			.ColorAndOpacity(FSlateColor(Style.ColorSheetInk()))
		];

	TSharedRef<SWidget> CloseButton = SNew(SButton)
		.ButtonStyle(&Style.TabButtonStyle(false))
		.OnClicked(this, &SExploredMapInHands::HandleClose)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ExploredUI", "MapPutAway", "Guardar el mapa"))
			.Font(Style.FontBody())
			.ColorAndOpacity(FSlateColor(Style.ColorSheetInk()))
		];

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ExploredUI", "MapTitle", "Mapa"))
			.Font(Style.FontHeading())
			.ColorAndOpacity(FSlateColor(Style.ColorSheetInk()))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 2.0f, 0.0f, 8.0f))
		[
			SNew(STextBlock)
			.Text(this, &SExploredMapInHands::GetWetnessText)
			.Font(Style.FontSmall())
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.30f, 0.45f, 1.0f)))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f))
			[
				MakeTabButton(TabMarks, NSLOCTEXT("ExploredUI", "MapTabMarks", "Marcas"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(2.0f, 0.0f))
			[
				MakeTabButton(TabWayfinding, NSLOCTEXT("ExploredUI", "MapTabWayfinding", "Rumbos"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(2.0f, 0.0f, 0.0f, 0.0f))
			[
				MakeTabButton(TabRecipes, NSLOCTEXT("ExploredUI", "MapTabRecipes", "Recetas"))
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(FMargin(0.0f, 10.0f))
		[
			SAssignNew(Pages, SWidgetSwitcher)
			.WidgetIndex_Lambda([WeakSelf = TWeakPtr<SExploredMapInHands>(SharedThis(this))]()
			{
				const TSharedPtr<SExploredMapInHands> Pinned = WeakSelf.Pin();
				return Pinned.IsValid() ? Pinned->ActiveTab : 0;
			})
			+ SWidgetSwitcher::Slot() [ BuildMarksPage() ]
			+ SWidgetSwitcher::Slot() [ BuildWayfindingPage() ]
			+ SWidgetSwitcher::Slot() [ BuildRecipesPage() ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
		[
			CollectionButton
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			CloseButton
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ExploredUI", "MapControlsHint", "Arrastra o usa las flechas para moverte; rueda o LB/RB para acercar. Y cambia entre la hoja y el cuaderno."))
			.Font(Style.FontSmall())
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(Style.ColorSheetInk() * FLinearColor(1.0f, 1.0f, 1.0f, 0.6f)))
		];
}

TSharedRef<SWidget> SExploredMapInHands::MakeTabButton(int32 Tab, const FText& Label)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	TSharedPtr<SButton> Button;
	SAssignNew(Button, SButton)
		.ButtonStyle(&Style.TabButtonStyle(Tab == ActiveTab))
		.OnClicked(this, &SExploredMapInHands::HandleSelectTab, Tab)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(Style.FontSmall())
			.ColorAndOpacity(FSlateColor(Style.ColorSheetInk()))
		];
	TabButtons.Add(Button);
	if (Tab == 0)
	{
		NotebookFirstFocus = Button;
	}
	return Button.ToSharedRef();
}

TSharedRef<SWidget> SExploredMapInHands::MakeStampButton(FName StampId, const FText& Label)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	TSharedPtr<SButton> Button;
	SAssignNew(Button, SButton)
		.ButtonStyle(&Style.TabButtonStyle(false))
		.OnClicked(this, &SExploredMapInHands::HandleSelectStamp, StampId)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(Style.FontSmall())
			.ColorAndOpacity(FSlateColor(Style.ColorSheetInk()))
		];
	StampButtons.Add(Button);
	StampButtonIds.Add(StampId);
	return Button.ToSharedRef();
}

TSharedRef<SWidget> SExploredMapInHands::BuildMarksPage()
{
	using namespace ExploredMapInHandsDetail;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();

	TSharedRef<SWrapBox> StampBox = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.0, 4.0));
	for (const ExploredMapView::FMapStampLabel& Stamp : Stamps)
	{
		StampBox->AddSlot()
		[
			MakeStampButton(Stamp.Id, ExploredUIWidgets::PickLocalized(Stamp.LabelEs, Stamp.LabelEn))
		];
	}
	StampBox->AddSlot()
	[
		MakeStampButton(NAME_None, NSLOCTEXT("ExploredUI", "MapStampNote", "Nota"))
	];

	TSharedRef<SWidget> MarkHereButton = SNew(SButton)
		.ButtonStyle(&Style.TabButtonStyle(false))
		.OnClicked(this, &SExploredMapInHands::HandleMarkHere)
		.IsEnabled(this, &SExploredMapInHands::CanCommit)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ExploredUI", "MapMarkHere", "Marcar donde estoy"))
			.Font(Style.FontSmall())
			.ColorAndOpacity(FSlateColor(Style.ColorSheetInk()))
		];

	return SNew(SScrollBox)
		.ScrollBarStyle(&Style.ScrollBarStyle())
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("ExploredUI", "MapStamp", "Sello"))
				.Font(Style.FontSmall())
				.ColorAndOpacity(FSlateColor(Style.ColorSheetInk()))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				StampBox
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 4.0f))
			[
				SAssignNew(MarkTextBox, SEditableTextBox)
				.Style(&Style.EditableTextBoxStyle())
				.HintText(NSLOCTEXT("ExploredUI", "MapMarkTextHint", "Texto corto (opcional)"))
				.OnTextChanged(this, &SExploredMapInHands::HandleTextChanged)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(this, &SExploredMapInHands::GetRemainingText)
				.Font(Style.FontSmall())
				.ColorAndOpacity(FSlateColor(Style.ColorSheetInk() * FLinearColor(1.0f, 1.0f, 1.0f, 0.6f)))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 6.0f))
			[
				MarkHereButton
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
			[
				SNew(STextBlock)
				.Text(this, &SExploredMapInHands::GetStatusText)
				.Font(Style.FontSmall())
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(Style.ColorSheetInk()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(this, &SExploredMapInHands::GetSelectedMarkText)
				.Font(Style.FontSmall())
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(Style.ColorAccentDim()))
			]
		];
}

TSharedRef<SWidget> SExploredMapInHands::BuildWayfindingPage()
{
	using namespace ExploredMapInHandsDetail;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const TWeakPtr<SExploredMapInHands> WeakSelf = SharedThis(this);

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (const FWayfindingAnnotation& Annotation : Annotations)
	{
		const bool bRing = Annotation.Technique == EWayfindingTechnique::FixedClouds || Annotation.Technique == EWayfindingTechnique::WaterColour
			|| Annotation.From.Equals(Annotation.To, 1.0);
		const FText Detail = bRing
			? FText::Format(NSLOCTEXT("ExploredUI", "MapAnnotationRadius", "Radio de {0} m"),
				FText::AsNumber(FMath::RoundToInt(Annotation.DistanceMeters)))
			: FText::Format(NSLOCTEXT("ExploredUI", "MapAnnotationBearing", "Rumbo {0}°, {1} m"),
				FText::AsNumber(FMath::RoundToInt(Annotation.BearingDegrees)), FText::AsNumber(FMath::RoundToInt(Annotation.DistanceMeters)));
		List->AddSlot().AutoHeight().Padding(FMargin(0.0f, 3.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(TechniqueName(Annotation.Technique))
				.Font(Style.FontSmall())
				.ColorAndOpacity(FSlateColor(Style.ColorSheetInk()))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Detail)
				.Font(Style.FontSmall())
				.ColorAndOpacity(FSlateColor(Style.ColorSheetInk() * FLinearColor(1.0f, 1.0f, 1.0f, 0.65f)))
			]
		];
	}
	if (Annotations.Num() == 0)
	{
		List->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ExploredUI", "MapNoWayfinding", "Aún no has aprendido ninguna técnica de navegación. Las ruinas completas enseñan una."))
			.Font(Style.FontSmall())
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(Style.ColorSheetInk()))
		];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
		[
			SNew(SCheckBox)
			.Style(&Style.CheckBoxStyle())
			.IsChecked_Lambda([WeakSelf]()
			{
				const TSharedPtr<SExploredMapInHands> Pinned = WeakSelf.Pin();
				return Pinned.IsValid() && Pinned->bShowWayfinding ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([WeakSelf](ECheckBoxState State)
			{
				if (const TSharedPtr<SExploredMapInHands> Pinned = WeakSelf.Pin())
				{
					Pinned->bShowWayfinding = State == ECheckBoxState::Checked;
				}
			})
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("ExploredUI", "MapShowWayfinding", "Mostrar las anotaciones de rumbo"))
				.Font(Style.FontSmall())
				.ColorAndOpacity(FSlateColor(Style.ColorSheetInk()))
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SScrollBox)
			.ScrollBarStyle(&Style.ScrollBarStyle())
			+ SScrollBox::Slot() [ List ]
		];
}

TSharedRef<SWidget> SExploredMapInHands::BuildRecipesPage()
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	const UCartographyComponent* Component = Cartography.Get();
	const TArray<FMapRecipeNote> Recipes = Component ? Component->GetModel().GetState().Recipes : TArray<FMapRecipeNote>();
	for (const FMapRecipeNote& Recipe : Recipes)
	{
		List->AddSlot().AutoHeight().Padding(FMargin(0.0f, 3.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
			[
				SNew(SBox)
				.WidthOverride(40.0f)
				.HeightOverride(40.0f)
				[
					Recipe.bDoodle ? StaticCastSharedRef<SWidget>(SNew(SExploredDoodle).RecipeId(Recipe.RecipeId).Size(40.0f)) : SNullWidget::NullWidget
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(RecipeLabel(Recipe.RecipeId))
				.Font(Style.FontSmall())
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(Style.ColorSheetInk()))
			]
		];
	}
	if (Recipes.Num() == 0)
	{
		List->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ExploredUI", "MapNoRecipes", "Aún no has anotado recetas al margen."))
			.Font(Style.FontSmall())
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(Style.ColorSheetInk()))
		];
	}
	return SNew(SScrollBox)
		.ScrollBarStyle(&Style.ScrollBarStyle())
		+ SScrollBox::Slot() [ List ];
}

void SExploredMapInHands::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (RaiseAlpha < 1.0f)
	{
		RaiseAlpha = FMath::Min(1.0f, RaiseAlpha + InDeltaTime / ExploredMapInHandsDetail::RaiseSeconds);
	}
}

TOptional<FSlateRenderTransform> SExploredMapInHands::GetPaperTransform() const
{
	using namespace ExploredMapInHandsDetail;
	if (bReduceMotion)
	{
		return TOptional<FSlateRenderTransform>();
	}
	// Sube con frenada (ease-out cúbico) y queda algo ladeada, como una hoja sostenida.
	const float T = 1.0f - FMath::Pow(1.0f - FMath::Clamp(RaiseAlpha, 0.0f, 1.0f), 3.0f);
	const float Offset = (1.0f - T) * RaiseTravel;
	return FSlateRenderTransform(FQuat2f(FMath::DegreesToRadians(TiltDegrees)), FVector2f(0.0f, Offset));
}

TSharedPtr<SWidget> SExploredMapInHands::GetInitialFocus() const
{
	return Sheet;
}

FReply SExploredMapInHands::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FName Key = InKeyEvent.GetKey().GetFName();
	if (ExploredMapView::IsMapFocusSwitchKey(Key))
	{
		const bool bOnSheet = Sheet.IsValid() && Sheet->HasKeyboardFocus();
		const TSharedPtr<SWidget> Target = bOnSheet ? NotebookFirstFocus : TSharedPtr<SWidget>(Sheet);
		if (Target.IsValid())
		{
			return FReply::Handled().SetUserFocus(Target.ToSharedRef(), EFocusCause::Navigation);
		}
	}
	if (ExploredMapView::ShouldCloseMapOnKey(Key, IsTypingText()))
	{
		return HandleClose();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

bool SExploredMapInHands::IsTypingText() const
{
	return MarkTextBox.IsValid() && (MarkTextBox->HasKeyboardFocus() || MarkTextBox->HasFocusedDescendants());
}

bool SExploredMapInHands::CanCommit() const
{
	return Cartography.IsValid() && ExploredMapView::CanCommitMark(SelectedStamp, PendingText);
}

void SExploredMapInHands::HandleViewRequested(const ExploredMapView::FMapView& NewView)
{
	View = ExploredMapView::ClampView(NewView);
}

void SExploredMapInHands::HandleSheetClicked(const FVector2D& MapPosition, int32 MarkIndex)
{
	// Pulsar una marca la selecciona; pulsar papel en blanco pone la marca elegida ahí.
	if (MarkIndex != INDEX_NONE)
	{
		SelectedMark = MarkIndex;
		StatusText = FText::GetEmpty();
		return;
	}
	UCartographyComponent* Component = Cartography.Get();
	if (!Component)
	{
		return;
	}
	if (!ExploredMapView::CanCommitMark(SelectedStamp, PendingText))
	{
		StatusText = NSLOCTEXT("ExploredUI", "MapNeedStampOrText", "Elige un sello o escribe una nota.");
		return;
	}
	if (Component->AddMarkOnSheet(SelectedStamp, MapPosition, PendingText))
	{
		SelectedMark = Component->GetModel().GetState().Marks.Num() - 1;
		StatusText = NSLOCTEXT("ExploredUI", "MapMarkPlaced", "Marca dibujada.");
		PendingText.Reset();
		if (MarkTextBox.IsValid())
		{
			MarkTextBox->SetText(FText::GetEmpty());
		}
	}
	else
	{
		StatusText = NSLOCTEXT("ExploredUI", "MapMarksFull", "No caben más marcas en la hoja.");
	}
}

FReply SExploredMapInHands::HandleSelectStamp(FName StampId)
{
	SelectedStamp = StampId;
	RefreshStampButtons();
	StatusText = NSLOCTEXT("ExploredUI", "MapHintPlace", "Pulsa en la hoja para poner la marca.");
	return FReply::Handled();
}

FReply SExploredMapInHands::HandleMarkHere()
{
	UCartographyComponent* Component = Cartography.Get();
	if (!Component || !ExploredMapView::CanCommitMark(SelectedStamp, PendingText))
	{
		return FReply::Handled();
	}
	if (Component->AddMarkHere(SelectedStamp, PendingText))
	{
		SelectedMark = Component->GetModel().GetState().Marks.Num() - 1;
		StatusText = NSLOCTEXT("ExploredUI", "MapMarkPlacedHere", "Marca dibujada donde crees estar.");
		PendingText.Reset();
		if (MarkTextBox.IsValid())
		{
			MarkTextBox->SetText(FText::GetEmpty());
		}
	}
	else
	{
		StatusText = NSLOCTEXT("ExploredUI", "MapMarksFull", "No caben más marcas en la hoja.");
	}
	return FReply::Handled();
}

FReply SExploredMapInHands::HandleSelectTab(int32 Tab)
{
	ActiveTab = FMath::Clamp(Tab, 0, 2);
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	for (int32 I = 0; I < TabButtons.Num(); ++I)
	{
		if (TabButtons[I].IsValid())
		{
			TabButtons[I]->SetButtonStyle(&Style.TabButtonStyle(I == ActiveTab));
		}
	}
	return FReply::Handled();
}

FReply SExploredMapInHands::HandleOpenCollection()
{
	OnOpenCollection.ExecuteIfBound();
	return FReply::Handled();
}

FReply SExploredMapInHands::HandleClose()
{
	OnClose.ExecuteIfBound();
	return FReply::Handled();
}

void SExploredMapInHands::HandleTextChanged(const FText& NewText)
{
	const FString Limited = ExploredMapView::LimitMarkTextWhileTyping(NewText.ToString());
	PendingText = Limited;
	// Solo se reescribe si hubo que cortar: el segundo aviso ya llega recortado y no vuelve a entrar.
	if (Limited != NewText.ToString() && MarkTextBox.IsValid())
	{
		MarkTextBox->SetText(FText::FromString(Limited));
	}
}

void SExploredMapInHands::RefreshStampButtons()
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	for (int32 I = 0; I < StampButtons.Num() && I < StampButtonIds.Num(); ++I)
	{
		if (StampButtons[I].IsValid())
		{
			StampButtons[I]->SetButtonStyle(&Style.TabButtonStyle(StampButtonIds[I] == SelectedStamp));
		}
	}
}

FText SExploredMapInHands::GetSelectedMarkText() const
{
	const UCartographyComponent* Component = Cartography.Get();
	if (!Component || SelectedMark == INDEX_NONE)
	{
		return FText::GetEmpty();
	}
	const TArray<FMapMark>& Marks = Component->GetModel().GetState().Marks;
	if (!Marks.IsValidIndex(SelectedMark))
	{
		return FText::GetEmpty();
	}
	const FMapMark& Mark = Marks[SelectedMark];
	return FText::Format(NSLOCTEXT("ExploredUI", "MapSelectedMark", "{0}: «{1}», {2}"),
		StampLabel(Mark.StampId), FText::FromString(Mark.Text), ExploredMapInHandsDetail::MarkSourceName(Mark.Source));
}

FText SExploredMapInHands::GetWetnessText() const
{
	const UCartographyComponent* Component = Cartography.Get();
	const float Wetness = Component ? Component->GetWetness() : 0.0f;
	if (Wetness >= FCartographyModel::InkRunThreshold)
	{
		return NSLOCTEXT("ExploredUI", "MapInkRunning", "El papel está empapado: la tinta se corre. Guárdalo en seco.");
	}
	if (Wetness > 0.05f)
	{
		return NSLOCTEXT("ExploredUI", "MapDamp", "El papel está húmedo.");
	}
	return FText::GetEmpty();
}

FText SExploredMapInHands::GetRemainingText() const
{
	return FText::Format(NSLOCTEXT("ExploredUI", "MapCharsLeft", "{0} caracteres libres"), FText::AsNumber(ExploredMapView::RemainingMarkChars(PendingText)));
}
