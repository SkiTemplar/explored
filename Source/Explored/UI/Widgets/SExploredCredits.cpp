#include "UI/Widgets/SExploredCredits.h"

#include "Input/Events.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "InputCoreTypes.h"
#include "UI/ExploredUIPalette.h"
#include "UI/ExploredUIStyle.h"
#include "UI/SettingsLogic.h"
#include "UI/Widgets/ExploredUIWidgets.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ExploredCredits"

// Espacio de nombres con nombre (no anónimo) para no chocar en el Unity build.
namespace ExploredCreditsDetail
{
	/** Los créditos completos pasan en 60 s (GDD v3 §9.2). */
	constexpr float RollSeconds = 60.0f;
	/** Margen vertical (px) del rollo: las líneas se cortan a esta distancia del borde, bajo el degradado. */
	constexpr float RollMarginY = 120.0f;
	/** Alto (px) del degradado de la noche a transparente arriba y abajo del rollo. */
	constexpr float FadeHeight = 160.0f;

	/** Degradado vertical de la noche (opaca en el borde del rollo) a transparente hacia dentro. */
	class SNightFade : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SNightFade) {}
			SLATE_ARGUMENT(bool, OpaqueAtTop)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs) { bOpaqueAtTop = InArgs._OpaqueAtTop; }

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1.0f, FadeHeight); }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			const FVector2f Size(AllottedGeometry.GetLocalSize());
			const FLinearColor Night = ExploredPalette::Linear(ExploredPalette::NocheCenit);
			const FLinearColor Clear(Night.R, Night.G, Night.B, 0.0f);
			TArray<FSlateGradientStop> Stops;
			Stops.Add(FSlateGradientStop(FVector2f(0.0f, 0.0f), bOpaqueAtTop ? Night : Clear));
			Stops.Add(FSlateGradientStop(Size, bOpaqueAtTop ? Clear : Night));
			FSlateDrawElement::MakeGradient(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), MoveTemp(Stops), Orient_Horizontal);
			return LayerId;
		}

	private:
		bool bOpaqueAtTop = true;
	};

	enum class ELine : uint8 { Title, Heading, Body, Small };

	TSharedRef<SWidget> Line(const FText& Text, ELine Kind)
	{
		const FExploredUIStyle& Style = FExploredUIStyle::Get();
		const FSlateFontInfo Font = Kind == ELine::Title ? Style.FontTitle() : Kind == ELine::Heading ? Style.SerifBold(32.0f)
			: Kind == ELine::Body ? Style.Serif(28.0f) : Style.SerifItalic(23.0f);
		const FLinearColor Color = Kind == ELine::Heading ? ExploredPalette::Linear(ExploredPalette::Laton)
			: Kind == ELine::Small ? ExploredPalette::Linear(ExploredPalette::ArenaOscura) : Style.ColorOnScene();
		return SNew(STextBlock)
			.Text(Text)
			.Font(Font)
			.ColorAndOpacity(FSlateColor(Color))
			.Justification(ETextJustify::Center)
			.AutoWrapText(true);
	}

	/** Sin las sombras de los bordes del SScrollBox: el rollo entra y sale limpio sobre la noche. */
	const FScrollBoxStyle& NoShadowStyle()
	{
		static const FScrollBoxStyle Style = FScrollBoxStyle()
			.SetTopShadowBrush(FSlateNoResource())
			.SetBottomShadowBrush(FSlateNoResource())
			.SetLeftShadowBrush(FSlateNoResource())
			.SetRightShadowBrush(FSlateNoResource());
		return Style;
	}

	/** Pistas de música CC BY (docs/creditos-musica.md, generado por Tools/Audio): título y licencia. */
	const TCHAR* const MusicTracks[] = {
		TEXT("Morning"), TEXT("Clear Air"), TEXT("Evening"), TEXT("Plaint"), TEXT("Gymnopedie No. 3"),
		TEXT("Enchanted Journey"), TEXT("Evening Fall (Harp)"), TEXT("Lamentation"), TEXT("Canon in D Major"), TEXT("Danse Morialta"),
	};
}

void SExploredCredits::Construct(const FArguments& InArgs)
{
	using namespace ExploredCreditsDetail;

	OnBack = InArgs._OnBack;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();

	TSharedRef<SWidget> BackButton = ExploredUIWidgets::MakeSceneButton(LOCTEXT("Back", "Volver"),
		FOnClicked::CreateSP(this, &SExploredCredits::HandleBack));
	InitialFocus = BackButton;

	TSharedRef<SVerticalBox> Roll = SNew(SVerticalBox);
	const auto Add = [&Roll](const FText& Text, ELine Kind, float Top)
	{
		Roll->AddSlot().AutoHeight().Padding(FMargin(0.0f, Top, 0.0f, 0.0f))[Line(Text, Kind)];
	};
	// Hueco del alto visible: el rollo empieza vacío y entra desde abajo.
	const auto ViewGap = [this]() { return FOptionalSize(RollViewHeight); };
	Roll->AddSlot().AutoHeight()[SNew(SBox).HeightOverride_Lambda(ViewGap)];
	Add(LOCTEXT("Title", "Baliza Siete"), ELine::Title, 0.0f);
	Add(LOCTEXT("Subtitle", "Isla de Sal"), ELine::Small, 4.0f);
	Add(LOCTEXT("AuthorHeading", "Un juego de"), ELine::Heading, 90.0f);
	Add(LOCTEXT("Author", "Rodrigo Fernández"), ELine::Body, 8.0f);
	Add(LOCTEXT("CodeHeading", "Programación, arte y sonido por código"), ELine::Heading, 70.0f);
	Add(LOCTEXT("Code", "Desarrollado con Claude (Anthropic)"), ELine::Body, 8.0f);
	Add(LOCTEXT("Art", "Mallas low poly generadas con Blender por script; una sola paleta de 41 colores"), ELine::Small, 8.0f);
	Add(LOCTEXT("EngineHeading", "Motor"), ELine::Heading, 70.0f);
	Add(LOCTEXT("Engine", "Unreal Engine 5.6"), ELine::Body, 8.0f);
	Add(LOCTEXT("FontHeading", "Tipografía"), ELine::Heading, 70.0f);
	Add(LOCTEXT("Font", "Lora, de The Lora Project Authors"), ELine::Body, 8.0f);
	Add(LOCTEXT("FontLicense", "SIL Open Font License 1.1"), ELine::Small, 4.0f);
	Add(LOCTEXT("MusicHeading", "Música"), ELine::Heading, 70.0f);
	Add(LOCTEXT("MusicOwn", "Piezas compuestas por código para el juego"), ELine::Body, 8.0f);
	Add(LOCTEXT("MusicExtra", "Música adicional de Kevin MacLeod (incompetech.com)"), ELine::Body, 24.0f);
	for (const TCHAR* Track : MusicTracks)
	{
		Add(FText::Format(LOCTEXT("Track", "«{0}»"), FText::FromString(Track)), ELine::Body, 6.0f);
	}
	Add(LOCTEXT("MusicLicense", "Licencia Creative Commons Reconocimiento 4.0 (CC BY 4.0). Cambios: volumen normalizado y silencios recortados."),
		ELine::Small, 12.0f);
	Add(LOCTEXT("Thanks", "Gracias por encender la baliza."), ELine::Heading, 120.0f);
	// Y otro igual al final: la última línea sale por arriba y el rollo acaba vacío.
	Roll->AddSlot().AutoHeight()[SNew(SBox).HeightOverride_Lambda(ViewGap)];

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SImage).Image(Style.BrushWhite()).ColorAndOpacity(FSlateColor(ExploredPalette::Linear(ExploredPalette::NocheCenit)))]
		+ SOverlay::Slot().HAlign(HAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(1000.0f)
			.Padding(FMargin(0.0f, RollMarginY))
			[
				SAssignNew(Scroll, SScrollBox)
				.Style(&ExploredCreditsDetail::NoShadowStyle())
				.ScrollBarVisibility(EVisibility::Collapsed)
				.ConsumeMouseWheel(EConsumeMouseWheel::Never)
				+ SScrollBox::Slot()[Roll]
			]
		]
		+ SOverlay::Slot().VAlign(VAlign_Top).Padding(FMargin(0.0f, RollMarginY, 0.0f, 0.0f))
		[
			SNew(SNightFade).OpaqueAtTop(true)
		]
		+ SOverlay::Slot().VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 0.0f, RollMarginY))
		[
			SNew(SNightFade).OpaqueAtTop(false)
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 48.0f, 40.0f))
		[
			BackButton
		]
	];
}

void SExploredCredits::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (!Scroll.IsValid())
	{
		return;
	}
	// Velocidad para que todo el rollo pase en RollSeconds (con el mundo en pausa: tiempo de Slate).
	Elapsed += InDeltaTime;
	RollViewHeight = FMath::Max(1.0f, static_cast<float>(AllottedGeometry.GetLocalSize().Y) - 2.0f * ExploredCreditsDetail::RollMarginY);
	const float Total = Scroll->GetScrollOffsetOfEnd();
	Scroll->SetScrollOffset(FMath::Clamp(Elapsed / ExploredCreditsDetail::RollSeconds, 0.0f, 1.0f) * Total);
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

#undef LOCTEXT_NAMESPACE
