#include "UI/Widgets/SExploredMapSheet.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
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

	/** Transformación de la zona visible del mapa a la geometría local (cuadrada y centrada). */
	struct FSheetTransform
	{
		FVector2D Origin = FVector2D::ZeroVector;
		double Scale = 1.0;
		FVector2f Offset = FVector2f::ZeroVector;

		FVector2f ToLocal(const FVector2D& Map) const
		{
			const FVector2D Local = (Map - Origin) * Scale;
			return FVector2f(static_cast<float>(Local.X), static_cast<float>(Local.Y)) + Offset;
		}
	};

	FLinearColor WithAlpha(const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, FMath::Clamp(Alpha, 0.0f, 1.0f));
	}

	void DrawPolyline(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, TArray<FVector2f> Points,
		const FLinearColor& Color, float Thickness)
	{
		if (Points.Num() >= 2)
		{
			FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Color, true, Thickness);
		}
	}
}

void SExploredMapSheet::Construct(const FArguments& InArgs)
{
	Cartography = InArgs._Cartography;
	ViewCenter = InArgs._ViewCenter;
	ViewSize = InArgs._ViewSize;
	SetClipping(EWidgetClipping::ClipToBounds);

	FlatBrush = FSlateColorBrush(FLinearColor::White);
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

FVector2D SExploredMapSheet::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(720.0, 720.0);
}

int32 SExploredMapSheet::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace ExploredMapSheetDetail;

	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	// En la hoja se invierte la paleta del frontend: el crema de la tinta es el papel y
	// el sepia oscuro del panel es la tinta.
	const FLinearColor Paper = WithAlpha(Style.ColorInk(), 1.0f);
	const FLinearColor Ink = WithAlpha(Style.ColorPaper(), 1.0f);
	const FLinearColor Accent = Style.ColorAccentDim();
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &PaperBrush,
		ESlateDrawEffect::None, Paper * Tint);

	const UCartographyComponent* Component = Cartography.Get();
	if (!Component)
	{
		return LayerId;
	}
	const FCartographyState& State = Component->GetModel().GetState();

	// Papel húmedo: más oscuro cuanto más mojado.
	if (State.Wetness > 0.01f)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), &FlatBrush,
			ESlateDrawEffect::None, WithAlpha(Ink, 0.25f * State.Wetness) * Tint);
	}

	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	const float Side = FMath::Min(Size.X, Size.Y);
	const float VisibleSize = FMath::Max(ViewSize.Get(), 0.01f);
	FSheetTransform Transform;
	Transform.Origin = ViewCenter.Get() - FVector2D(VisibleSize * 0.5);
	Transform.Scale = Side / VisibleSize;
	Transform.Offset = (Size - FVector2f(Side)) * 0.5f;

	const int32 InkLayer = LayerId + 2;

	// Bocetos de mirador: solo lo que aún no se ha recorrido, tenue.
	for (const FMapSketch& Sketch : State.Sketches)
	{
		TArray<FVector2f> Run;
		const int32 Count = Sketch.Points.Num();
		for (int32 I = 0; I <= Count; ++I)
		{
			const int32 Index = I % FMath::Max(Count, 1);
			if (Count > 0 && Sketch.Confirmed[Index] == 0)
			{
				Run.Add(Transform.ToLocal(Sketch.Points[Index]));
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
			Local.Add(Transform.ToLocal(P));
		}
		if (Stroke.Blur > 0.0f)
		{
			DrawPolyline(OutDrawElements, InkLayer, AllottedGeometry, Local, WithAlpha(Ink, 0.2f * Stroke.Blur * Stroke.Ink) * Tint, 2.0f + 4.0f * Stroke.Blur);
		}
		DrawPolyline(OutDrawElements, InkLayer + 1, AllottedGeometry, MoveTemp(Local), WithAlpha(Ink, Stroke.Ink) * Tint, 1.6f);
	}

	// Marcas: rombo a mano, círculo de catalejo, cruz de sextante; y el texto al lado.
	const FSlateFontInfo Font = Style.FontSmall();
	for (const FMapMark& Mark : State.Marks)
	{
		const FVector2f C = Transform.ToLocal(Mark.Position);
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
		const FString Label = Mark.Text.IsEmpty() ? Mark.StampId.ToString() : Mark.Text;
		if (!Mark.StampId.IsNone() || !Mark.Text.IsEmpty())
		{
			FSlateDrawElement::MakeText(OutDrawElements, InkLayer + 2,
				AllottedGeometry.ToPaintGeometry(FVector2f(220.0f, 20.0f), FSlateLayoutTransform(C + FVector2f(R + 3.0f, -8.0f))),
				Label, Font, ESlateDrawEffect::None, WithAlpha(Ink, Mark.Ink) * Tint);
		}
	}

	// Marco de la hoja.
	DrawPolyline(OutDrawElements, InkLayer + 3, AllottedGeometry,
		{FVector2f(1.0f, 1.0f), FVector2f(Size.X - 1.0f, 1.0f), FVector2f(Size.X - 1.0f, Size.Y - 1.0f), FVector2f(1.0f, Size.Y - 1.0f), FVector2f(1.0f, 1.0f)},
		WithAlpha(Ink, 0.6f) * Tint, 1.0f);

	return InkLayer + 3;
}
