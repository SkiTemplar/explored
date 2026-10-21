#include "UI/Widgets/SExploredNotebook.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "UI/ExploredUIPalette.h"
#include "UI/ExploredUIStyle.h"
#include "UI/ScreensLogic.h"

#define LOCTEXT_NAMESPACE "ExploredNotebook"

namespace ExploredNotebookDetail
{
	constexpr float PageWidth = 640.0f;
	constexpr float PageHeight = 760.0f;

	TSharedRef<SWidget> Icon(EExploredIcon Icon, float Side, const FLinearColor& Color)
	{
		return SNew(SBox).WidthOverride(Side).HeightOverride(Side)
			[
				SNew(SImage).Image(FExploredUIStyle::Get().IconBrush(Icon)).ColorAndOpacity(FSlateColor(Color))
			];
	}

	TSharedRef<SWidget> Label(const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color, bool bWrap = true)
	{
		return SNew(STextBlock).Text(Text).Font(Font).ColorAndOpacity(FSlateColor(Color)).AutoWrapText(bWrap);
	}

	/** Una hoja del cuaderno abierto: papel con la línea roja del margen. */
	TSharedRef<SWidget> Sheet(TSharedRef<SWidget> Content, bool bLeft)
	{
		const FExploredUIStyle& Style = FExploredUIStyle::Get();
		return SNew(SBox).WidthOverride(PageWidth).HeightOverride(PageHeight)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(Style.BrushRound(0)).ColorAndOpacity(FSlateColor(ExploredPalette::Linear(ExploredPalette::Papel)))]
				+ SOverlay::Slot().HAlign(HAlign_Left).Padding(FMargin(bLeft ? 70.0f : 56.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SBox).WidthOverride(2.0f)
					[
						SNew(SImage).Image(Style.BrushWhite()).ColorAndOpacity(FSlateColor(ExploredPalette::Linear(ExploredPalette::RojoFaro, 0.6f)))
					]
				]
				+ SOverlay::Slot().Padding(FMargin(bLeft ? 92.0f : 78.0f, 44.0f, 48.0f, 40.0f))[Content]
			];
	}
}

void SExploredNotebook::Construct(const FArguments& InArgs)
{
	using namespace ExploredNotebookDetail;
	PagesMask = InArgs._PagesMask;
	PageTexts = InArgs._PageTexts;
	NoteTexts = InArgs._NoteTexts;
	Objective = InArgs._Objective;
	OnClose = InArgs._OnClose;
	Shown = ExploredScreens::BuildNotebookView(PagesMask, ExploredScreens::NotebookPages).Shown;

	ChildSlot
	[
		SNew(SOverlay)
		// Velo de tinta sobre el juego: el cuaderno se lee como una pantalla aparte.
		+ SOverlay::Slot()[SNew(SImage).Image(FExploredUIStyle::Get().BrushWhite()).ColorAndOpacity(FSlateColor(ExploredPalette::Linear(ExploredPalette::Tinta, 0.55f)))]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FExploredUIStyle::Get().BrushRound(1))
			.BorderBackgroundColor(FSlateColor(ExploredPalette::Linear(ExploredPalette::MaderaOscura)))
			.Padding(FMargin(14.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Sheet(SAssignNew(IndexBox, SBox), true)]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(6.0f)
					[
						SNew(SImage).Image(FExploredUIStyle::Get().BrushWhite()).ColorAndOpacity(FSlateColor(ExploredPalette::Linear(ExploredPalette::Tinta, 0.35f)))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()[Sheet(SAssignNew(PageBox, SBox), false)]
			]
		]
	];
	ShowPage(Shown);
}

void SExploredNotebook::ShowPage(int32 Page)
{
	Shown = ExploredScreens::BuildNotebookView(PagesMask, Page).Shown;
	IndexBox->SetContent(BuildIndex());
	PageBox->SetContent(BuildPage());
}

TSharedRef<SWidget> SExploredNotebook::BuildIndex() const
{
	using namespace ExploredNotebookDetail;
	using namespace ExploredPalette;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const ExploredScreens::FNotebookView View = ExploredScreens::BuildNotebookView(PagesMask, Shown);
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Label(LOCTEXT("Title", "Cuaderno de guardia"), Style.SerifBold(36.0f), Style.ColorInk(), false)]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 2.0f, 0.0f, 18.0f))
		[
			Label(FText::Format(LOCTEXT("Count", "Faro de Isla de Sal · páginas {0}/{1}"), FText::AsNumber(View.ReadCount),
				FText::AsNumber(ExploredScreens::NotebookPages)), Style.SerifItalic(24.0f), Style.ColorInkDim(), false)
		];
	const TSharedRef<SExploredNotebook> Self = ConstCastSharedRef<SExploredNotebook>(SharedThis(this));
	const TWeakPtr<SExploredNotebook> WeakSelf = Self;
	for (const ExploredScreens::FNotebookPageRow& Row : View.Rows)
	{
		const int32 Page = Row.Page;
		const FText Text = Row.bRead ? FText::Format(LOCTEXT("PageRead", "Página {0}"), FText::AsNumber(Page))
			: FText::Format(LOCTEXT("PageMissing", "Página {0} · sin encontrar"), FText::AsNumber(Page));
		const FLinearColor Color = Row.bRead ? Style.ColorInk() : Style.ColorDisabled();
		Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 1.0f))
		[
			SNew(SButton)
			.ButtonStyle(&Style.ButtonStyle())
			.IsFocusable(false)
			.IsEnabled(Row.bRead)
			.ContentPadding(FMargin(8.0f, 4.0f))
			.OnClicked_Lambda([WeakSelf, Page]()
			{
				if (const TSharedPtr<SExploredNotebook> Pinned = WeakSelf.Pin())
				{
					Pinned->ShowPage(Page);
				}
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
				[
					Icon(Row.bRead ? EExploredIcon::Pagina : EExploredIcon::Candado, 28.0f, Row.bSelected ? Style.ColorAccent() : Color)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					Label(Text, Row.bSelected ? Style.SerifBold(26.0f) : Style.Serif(26.0f), Row.bSelected ? Style.ColorAccent() : Color, false)
				]
			]
		];
	}
	Box->AddSlot().FillHeight(1.0f)[SNew(SBox)];
	Box->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(0.0f, 2.0f, 12.0f, 0.0f))
		[
			Icon(EExploredIcon::Faro, 30.0f, Linear(RojoFaro))
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Label(LOCTEXT("Objective", "Objetivo"), Style.SerifBold(24.0f), Style.ColorInkDim(), false)]
			+ SVerticalBox::Slot().AutoHeight()[Label(Objective, Style.Serif(25.0f), Style.ColorInk())]
		]
	];
	return Box;
}

TSharedRef<SWidget> SExploredNotebook::BuildPage() const
{
	using namespace ExploredNotebookDetail;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	if (Shown <= 0)
	{
		return SNew(SBox).VAlign(VAlign_Center)
			[
				Label(LOCTEXT("NoPages", "Aún no has encontrado ninguna página del cuaderno de Roig. Busca en el faro."),
					Style.SerifItalic(26.0f), Style.ColorInkDim())
			];
	}
	const int32 Index = Shown - 1;
	const FText Body = PageTexts.IsValidIndex(Index) ? PageTexts[Index] : FText::GetEmpty();
	const FText Note = NoteTexts.IsValidIndex(Index) ? NoteTexts[Index] : FText::GetEmpty();
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 16.0f))
		[
			Label(FText::Format(LOCTEXT("PageHeading", "Página {0}"), FText::AsNumber(Shown)), Style.SerifBold(30.0f), Style.ColorInkDim(), false)
		]
		+ SVerticalBox::Slot().AutoHeight()[Label(Body, Style.Serif(26.0f), Style.ColorInk())]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 26.0f, 0.0f, 0.0f))
		[
			Label(Note, Style.SerifItalic(26.0f), Style.ColorNoteInk())
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)[SNew(SBox)]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
		[
			Label(LOCTEXT("Help", "A / D: pasar página · J: cerrar"), Style.SerifItalic(22.0f), Style.ColorInkDim(), false)
		];
}

FReply SExploredNotebook::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Right || Key == EKeys::D)
	{
		const int32 Direction = (Key == EKeys::Left || Key == EKeys::A) ? -1 : 1;
		ShowPage(ExploredScreens::StepNotebookPage(PagesMask, Shown, Direction));
		return FReply::Handled();
	}
	if (Key == EKeys::J || Key == EKeys::Escape || Key == EKeys::Tab || Key == EKeys::Gamepad_FaceButton_Right)
	{
		OnClose.ExecuteIfBound();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
