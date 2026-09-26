#include "UI/Widgets/SExploredMapSheet.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/WidgetStyle.h"
#include "UObject/UObjectGlobals.h"

#include "Cartography/CartographyComponent.h"
#include "Cartography/CartographyModel.h"
#include "UI/ExploredUIStyle.h"

namespace ExploredMapSheetDetail
{
	/** Ruta de la textura de papel que importa Tools/Unreal/import_textures.py. */
	const TCHAR* const PaperTexturePath = TEXT("/Game/Generated/Textures/T_MapPaper_BC.T_MapPaper_BC");

	/** Distancia (píxeles locales) a partir de la cual una pulsación pasa a ser un arrastre. */
	constexpr double DragThreshold = 4.0;

	FLinearColor WithAlpha(const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, FMath::Clamp(Alpha, 0.0f, 1.0f));
	}

	FVector2f ToLocal2f(const ExploredMapView::FSheetTransform& Transform, const FVector2D& Map)
	{
		const FVector2D Local = Transform.MapToLocal(Map);
		return FVector2f(static_cast<float>(Local.X), static_cast<float>(Local.Y));
	}

	void DrawPolyline(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, TArray<FVector2f> Points,
		const FLinearColor& Color, float Thickness)
	{
		if (Points.Num() >= 2)
		{
			FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Color, true, Thickness);
		}
	}

	/** Tinta de cada técnica de wayfinding: tonos fríos que no se confunden con la costa sepia. */
	FLinearColor TechniqueColor(EWayfindingTechnique Technique)
	{
		switch (Technique)
		{
		case EWayfindingTechnique::StarPath: return FLinearColor(0.18f, 0.24f, 0.52f, 0.85f);
		case EWayfindingTechnique::SwellReading: return FLinearColor(0.10f, 0.38f, 0.46f, 0.8f);
		case EWayfindingTechnique::BirdsAtDusk: return FLinearColor(0.42f, 0.22f, 0.30f, 0.8f);
		case EWayfindingTechnique::FixedClouds: return FLinearColor(0.38f, 0.40f, 0.48f, 0.8f);
		case EWayfindingTechnique::WaterColour: return FLinearColor(0.08f, 0.45f, 0.38f, 0.8f);
		default: return FLinearColor(0.2f, 0.2f, 0.2f, 0.8f);
		}
	}

	FVector2D LocalPosition(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
	{
		// AbsoluteToLocal deshace también la rotación de la hoja en las manos.
		const FVector2f Local = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		return FVector2D(Local.X, Local.Y);
	}
}

void SExploredMapSheet::Construct(const FArguments& InArgs)
{
	Cartography = InArgs._Cartography;
	ViewCenter = InArgs._ViewCenter;
	ViewSize = InArgs._ViewSize;
	ShowWayfinding = InArgs._ShowWayfinding;
	SelectedMark = InArgs._SelectedMark;
	bInteractive = InArgs._bInteractive;
	OnViewRequested = InArgs._OnViewRequested;
	OnSheetClicked = InArgs._OnSheetClicked;
	SetClipping(EWidgetClipping::ClipToBounds);

	FlatBrush = FSlateColorBrush(FLinearColor::White);
	StainBrush = FSlateRoundedBoxBrush(FLinearColor::White, 4.0f);
	// Óvalo: radio de media altura, así una caja cuadrada es un círculo.
	StainBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	PaperBrush = FlatBrush;
	if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, ExploredMapSheetDetail::PaperTexturePath, nullptr, LOAD_NoWarn))
	{
		PaperTexture.Reset(Texture);
		PaperBrush = FSlateBrush();
		PaperBrush.SetResourceObject(Texture);
		PaperBrush.ImageSize = FVector2D(256.0, 256.0);
		PaperBrush.DrawAs = ESlateBrushDrawType::Image;
		PaperBrush.Tiling = ESlateBrushTileType::Both;
	}
}

void SExploredMapSheet::SetAnnotations(const TArray<FWayfindingAnnotation>& Annotations)
{
	AnnotationLines.Reset();
	for (const FWayfindingAnnotation& Annotation : Annotations)
	{
		for (ExploredMapView::FPolyline& Line : ExploredMapView::AnnotationStrokes(Annotation))
		{
			AnnotationLines.Emplace(Annotation.Technique, MoveTemp(Line));
		}
	}
}

ExploredMapView::FMapView SExploredMapSheet::CurrentView() const
{
	ExploredMapView::FMapView View;
	View.Center = ViewCenter.Get();
	View.Size = static_cast<double>(ViewSize.Get());
	return ExploredMapView::ClampView(View);
}

ExploredMapView::FSheetTransform SExploredMapSheet::MakeTransform(const FGeometry& Geometry) const
{
	const FVector2f Size = FVector2f(Geometry.GetLocalSize());
	return ExploredMapView::FSheetTransform::Make(CurrentView(), FVector2D(Size.X, Size.Y));
}

void SExploredMapSheet::RequestView(const ExploredMapView::FMapView& NewView) const
{
	OnViewRequested.ExecuteIfBound(ExploredMapView::ClampView(NewView));
}

void SExploredMapSheet::ClickAt(const FGeometry& Geometry, const FVector2D& Local) const
{
	const ExploredMapView::FSheetTransform Transform = MakeTransform(Geometry);
	if (!Transform.IsInsideSheet(Local))
	{
		return;
	}
	int32 Hit = INDEX_NONE;
	if (const UCartographyComponent* Component = Cartography.Get())
	{
		Hit = ExploredMapView::HitTestMarks(Component->GetModel().GetState().Marks, Transform, Local);
	}
	OnSheetClicked.ExecuteIfBound(Transform.LocalToMap(Local), Hit);
}

FVector2D SExploredMapSheet::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(720.0, 720.0);
}

FReply SExploredMapSheet::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bInteractive || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	bPressed = true;
	bDragging = false;
	DragDistance = 0.0;
	// El foco pasa a la hoja para que las flechas y la cruceta funcionen después del clic.
	return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SExploredMapSheet::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bInteractive || !bPressed || !HasMouseCapture())
	{
		return FReply::Unhandled();
	}
	// Delta en píxeles locales: la posición anterior se reconstruye con el delta de pantalla
	// para que la rotación y la escala del marco no desvíen el arrastre.
	const FVector2D Now = ExploredMapSheetDetail::LocalPosition(MyGeometry, MouseEvent);
	const FVector2f PreviousScreen = MouseEvent.GetLastScreenSpacePosition();
	const FVector2f PreviousLocalF = MyGeometry.AbsoluteToLocal(PreviousScreen);
	const FVector2D Delta = Now - FVector2D(PreviousLocalF.X, PreviousLocalF.Y);
	DragDistance += Delta.Size();
	if (!bDragging && DragDistance < ExploredMapSheetDetail::DragThreshold)
	{
		return FReply::Handled();
	}
	bDragging = true;
	RequestView(ExploredMapView::PanByPixels(CurrentView(), Delta, MakeTransform(MyGeometry).Side));
	return FReply::Handled();
}

FReply SExploredMapSheet::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bInteractive || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !bPressed)
	{
		return FReply::Unhandled();
	}
	const bool bWasDragging = bDragging;
	bPressed = false;
	bDragging = false;
	if (!bWasDragging)
	{
		ClickAt(MyGeometry, ExploredMapSheetDetail::LocalPosition(MyGeometry, MouseEvent));
	}
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SExploredMapSheet::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bInteractive)
	{
		return FReply::Unhandled();
	}
	const double Factor = MouseEvent.GetWheelDelta() > 0.0f ? ExploredMapView::ZoomStep : 1.0 / ExploredMapView::ZoomStep;
	const FVector2D Anchor = MakeTransform(MyGeometry).LocalToMap(ExploredMapSheetDetail::LocalPosition(MyGeometry, MouseEvent));
	RequestView(ExploredMapView::ZoomAt(CurrentView(), Factor, Anchor));
	return FReply::Handled();
}

FReply SExploredMapSheet::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bInteractive)
	{
		return FReply::Unhandled();
	}
	const FName Key = InKeyEvent.GetKey().GetFName();

	const FVector2D Direction = ExploredMapView::PanDirectionForKey(Key);
	if (!Direction.IsZero())
	{
		RequestView(ExploredMapView::PanByFraction(CurrentView(), Direction));
		return FReply::Handled();
	}

	const double Zoom = ExploredMapView::ZoomFactorForKey(Key);
	if (Zoom != 1.0)
	{
		const ExploredMapView::FMapView View = CurrentView();
		RequestView(ExploredMapView::ZoomAt(View, Zoom, View.Center));
		return FReply::Handled();
	}

	if (ExploredMapView::IsSheetAcceptKey(Key))
	{
		const FVector2f Size = FVector2f(MyGeometry.GetLocalSize());
		ClickAt(MyGeometry, FVector2D(Size.X, Size.Y) * 0.5);
		return FReply::Handled();
	}

	// Volver, M, View y el resto suben al mapa en las manos.
	return FReply::Unhandled();
}

FReply SExploredMapSheet::OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
	if (!bInteractive)
	{
		return FReply::Unhandled();
	}
	const FKey Key = InAnalogInputEvent.GetKey();
	const double Amount = ExploredMapView::StickPanAmount(InAnalogInputEvent.GetAnalogValue());
	if (Key == EKeys::Gamepad_LeftX && Amount != 0.0)
	{
		RequestView(ExploredMapView::PanByFraction(CurrentView(), FVector2D(1.0, 0.0), Amount));
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_LeftY && Amount != 0.0)
	{
		// Stick hacia arriba = valor positivo = norte (−Y en pantalla).
		RequestView(ExploredMapView::PanByFraction(CurrentView(), FVector2D(0.0, -1.0), Amount));
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

int32 SExploredMapSheet::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace ExploredMapSheetDetail;

	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const FLinearColor Paper = Style.ColorSheet();
	const FLinearColor Ink = Style.ColorSheetInk();
	const FLinearColor Accent = Style.ColorAccentDim();
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &PaperBrush,
		ESlateDrawEffect::None, Paper * Tint);

	const UCartographyComponent* Component = Cartography.Get();
	if (!Component)
	{
		return LayerId;
	}
	const FCartographyModel& Model = Component->GetModel();
	const FCartographyState& State = Model.GetState();

	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	const ExploredMapView::FSheetTransform Transform = MakeTransform(AllottedGeometry);
	const float Scale = static_cast<float>(Transform.Scale);

	// Papel húmedo: más oscuro cuanto más mojado.
	if (State.Wetness > 0.01f)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), &FlatBrush,
			ESlateDrawEffect::None, WithAlpha(Ink, 0.25f * State.Wetness) * Tint);
	}

	// Manchas de agua sobre el papel (siguen a la hoja al desplazar y acercar) y cercos
	// secos de las pasadas de tinta corrida desde la última copia en limpio.
	for (const ExploredMapView::FWetStain& Stain : ExploredMapView::WetStains(Model.GetSeed(), State.Wetness, State.InkRuns))
	{
		const float Diameter = static_cast<float>(Stain.Radius * 2.0) * Scale;
		const FVector2f Center = ToLocal2f(Transform, Stain.Center);
		const FLinearColor StainColor = Stain.bDried ? FLinearColor(0.45f, 0.33f, 0.18f, Stain.Alpha) : WithAlpha(Ink, Stain.Alpha);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2f(Diameter, Diameter), FSlateLayoutTransform(Center - FVector2f(Diameter * 0.5f))),
			&StainBrush, ESlateDrawEffect::None, StainColor * Tint);
	}

	const int32 InkLayer = LayerId + 2;

	// Bocetos de mirador: solo lo que aún no se ha recorrido, tenue.
	for (const FMapSketch& Sketch : State.Sketches)
	{
		TArray<FVector2f> Run;
		const int32 Count = Sketch.Points.Num();
		for (int32 I = 0; I <= Count; ++I)
		{
			const int32 Index = I % FMath::Max(Count, 1);
			if (Count > 0 && Sketch.Confirmed.IsValidIndex(Index) && Sketch.Confirmed[Index] == 0)
			{
				Run.Add(ToLocal2f(Transform, Sketch.Points[Index]));
			}
			else
			{
				DrawPolyline(OutDrawElements, InkLayer, AllottedGeometry, MoveTemp(Run), WithAlpha(Ink, 0.3f * Sketch.Ink) * Tint, 1.0f);
				Run.Reset();
			}
		}
		DrawPolyline(OutDrawElements, InkLayer, AllottedGeometry, MoveTemp(Run), WithAlpha(Ink, 0.3f * Sketch.Ink) * Tint, 1.0f);
	}

	// Trazos de costa: la tinta corrida se ensancha en un halo claro bajo el trazo.
	for (const FMapStroke& Stroke : State.Strokes)
	{
		TArray<FVector2f> Local;
		Local.Reserve(Stroke.Points.Num());
		for (const FVector2D& P : Stroke.Points)
		{
			Local.Add(ToLocal2f(Transform, P));
		}
		if (Stroke.Blur > 0.0f)
		{
			DrawPolyline(OutDrawElements, InkLayer, AllottedGeometry, Local, WithAlpha(Ink, 0.2f * Stroke.Blur * Stroke.Ink) * Tint, 2.0f + 4.0f * Stroke.Blur);
		}
		DrawPolyline(OutDrawElements, InkLayer + 1, AllottedGeometry, MoveTemp(Local), WithAlpha(Ink, Stroke.Ink) * Tint, 1.6f);
	}

	// Anotaciones de wayfinding: lo que enseñan las ruinas, en su propia capa.
	if (ShowWayfinding.Get(true))
	{
		for (const TPair<EWayfindingTechnique, ExploredMapView::FPolyline>& Line : AnnotationLines)
		{
			TArray<FVector2f> Local;
			Local.Reserve(Line.Value.Num());
			for (const FVector2D& P : Line.Value)
			{
				Local.Add(ToLocal2f(Transform, P));
			}
			DrawPolyline(OutDrawElements, InkLayer + 1, AllottedGeometry, MoveTemp(Local), TechniqueColor(Line.Key) * Tint, 1.4f);
		}
	}

	// Marcas: rombo a mano, círculo de catalejo, cruz de sextante; y el texto al lado.
	const FSlateFontInfo Font = Style.FontSmall();
	const int32 Selected = SelectedMark.Get(INDEX_NONE);
	for (int32 MarkIndex = 0; MarkIndex < State.Marks.Num(); ++MarkIndex)
	{
		const FMapMark& Mark = State.Marks[MarkIndex];
		const FVector2f C = ToLocal2f(Transform, Mark.Position);
		const FLinearColor Color = WithAlpha(Accent, Mark.Ink) * Tint;
		constexpr float R = 5.0f;
		switch (Mark.Source)
		{
		case EMapMarkSource::Sextant:
			DrawPolyline(OutDrawElements, InkLayer + 2, AllottedGeometry, {C + FVector2f(-R, -R), C + FVector2f(R, R)}, Color, 1.5f);
			DrawPolyline(OutDrawElements, InkLayer + 2, AllottedGeometry, {C + FVector2f(-R, R), C + FVector2f(R, -R)}, Color, 1.5f);
			break;
		case EMapMarkSource::Spyglass:
		{
			TArray<FVector2f> Ring;
			for (int32 I = 0; I <= 10; ++I)
			{
				const float Angle = UE_TWO_PI * static_cast<float>(I) / 10.0f;
				Ring.Add(C + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * R);
			}
			DrawPolyline(OutDrawElements, InkLayer + 2, AllottedGeometry, MoveTemp(Ring), Color, 1.2f);
			break;
		}
		default:
			DrawPolyline(OutDrawElements, InkLayer + 2, AllottedGeometry,
				{C + FVector2f(0.0f, -R), C + FVector2f(R, 0.0f), C + FVector2f(0.0f, R), C + FVector2f(-R, 0.0f), C + FVector2f(0.0f, -R)}, Color, 1.5f);
			break;
		}
		if (MarkIndex == Selected)
		{
			TArray<FVector2f> Halo;
			for (int32 I = 0; I <= 16; ++I)
			{
				const float Angle = UE_TWO_PI * static_cast<float>(I) / 16.0f;
				Halo.Add(C + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * (R * 2.2f));
			}
			DrawPolyline(OutDrawElements, InkLayer + 2, AllottedGeometry, MoveTemp(Halo), Style.ColorAccent() * Tint, 2.0f);
		}
		// Solo el texto propio: el nombre del sello lo pone la leyenda del mapa en las manos.
		if (!Mark.Text.IsEmpty())
		{
			FSlateDrawElement::MakeText(OutDrawElements, InkLayer + 2,
				AllottedGeometry.ToPaintGeometry(FVector2f(220.0f, 20.0f), FSlateLayoutTransform(C + FVector2f(R + 3.0f, -8.0f))),
				Mark.Text, Font, ESlateDrawEffect::None, WithAlpha(Ink, Mark.Ink) * Tint);
		}
	}

	// Cruz del centro: con teclado o mando, Aceptar pulsa aquí.
	if (bInteractive && HasKeyboardFocus())
	{
		const FVector2f Mid = Size * 0.5f;
		constexpr float Arm = 9.0f;
		const FLinearColor CrossColor = WithAlpha(Style.ColorAccent(), 0.9f) * Tint;
		DrawPolyline(OutDrawElements, InkLayer + 3, AllottedGeometry, {Mid + FVector2f(-Arm, 0.0f), Mid + FVector2f(Arm, 0.0f)}, CrossColor, 1.5f);
		DrawPolyline(OutDrawElements, InkLayer + 3, AllottedGeometry, {Mid + FVector2f(0.0f, -Arm), Mid + FVector2f(0.0f, Arm)}, CrossColor, 1.5f);
	}

	// Marco de la hoja.
	DrawPolyline(OutDrawElements, InkLayer + 3, AllottedGeometry,
		{FVector2f(1.0f, 1.0f), FVector2f(Size.X - 1.0f, 1.0f), FVector2f(Size.X - 1.0f, Size.Y - 1.0f), FVector2f(1.0f, Size.Y - 1.0f), FVector2f(1.0f, 1.0f)},
		WithAlpha(Ink, 0.6f) * Tint, 1.0f);

	return InkLayer + 3;
}
