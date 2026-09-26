#include "UI/MapViewLogic.h"

#include "Save/SaveValue.h"
#include "UI/SettingsLogic.h"

namespace ExploredMapViewDetail
{
	/** Generador congruencial pequeño y determinista (enteros sin signo: el desbordamiento está definido). */
	struct FDoodleRandom
	{
		uint32 State;

		explicit FDoodleRandom(uint32 Seed) : State(Seed ^ 0x9E3779B9u) {}

		double Next01()
		{
			State = State * 1664525u + 1013904223u;
			return static_cast<double>((State >> 8) & 0xFFFFFFu) / 16777216.0;
		}

		double Range(double Min, double Max)
		{
			return Min + (Max - Min) * Next01();
		}
	};

	void AddDash(TArray<ExploredMapView::FPolyline>& Out, const FVector2D& A, const FVector2D& B)
	{
		ExploredMapView::FPolyline& Dash = Out.AddDefaulted_GetRef();
		Dash.Add(A);
		Dash.Add(B);
	}

	/** Anillo discontinuo: Segments tramos alrededor de Center, dibujando uno sí y otro no. */
	void AddDashedRing(TArray<ExploredMapView::FPolyline>& Out, const FVector2D& Center, double Radius, int32 Segments)
	{
		for (int32 I = 0; I < Segments; I += 2)
		{
			const double A0 = 2.0 * UE_DOUBLE_PI * static_cast<double>(I) / static_cast<double>(Segments);
			const double A1 = 2.0 * UE_DOUBLE_PI * static_cast<double>(I + 1) / static_cast<double>(Segments);
			AddDash(Out, Center + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Radius, Center + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Radius);
		}
	}

	bool IsFiniteVector(const FVector2D& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y);
	}
}

namespace ExploredMapView
{
	FMapView ClampView(const FMapView& View)
	{
		FMapView Out;
		Out.Size = FMath::IsFinite(View.Size) && View.Size > 0.0 ? FMath::Clamp(View.Size, MinViewSize, MaxViewSize) : MaxViewSize;
		const FVector2D Center = ExploredMapViewDetail::IsFiniteVector(View.Center) ? View.Center : FVector2D(0.5, 0.5);
		const double Half = Out.Size * 0.5;
		Out.Center = FVector2D(FMath::Clamp(Center.X, Half, 1.0 - Half), FMath::Clamp(Center.Y, Half, 1.0 - Half));
		return Out;
	}

	FMapView ZoomAt(const FMapView& View, double Factor, const FVector2D& AnchorMap)
	{
		const FMapView Current = ClampView(View);
		if (!FMath::IsFinite(Factor) || Factor <= 0.0 || !ExploredMapViewDetail::IsFiniteVector(AnchorMap))
		{
			return Current;
		}
		const double NewSize = FMath::Clamp(Current.Size / Factor, MinViewSize, MaxViewSize);
		// Posición relativa del ancla dentro de la zona visible: se conserva al cambiar el lado.
		const FVector2D Origin = Current.Center - FVector2D(Current.Size * 0.5);
		const FVector2D Relative = (AnchorMap - Origin) / Current.Size;
		FMapView Out;
		Out.Size = NewSize;
		Out.Center = AnchorMap - Relative * NewSize + FVector2D(NewSize * 0.5);
		return ClampView(Out);
	}

	FMapView PanByPixels(const FMapView& View, const FVector2D& DeltaPixels, double SidePixels)
	{
		const FMapView Current = ClampView(View);
		if (!FMath::IsFinite(SidePixels) || SidePixels <= 0.0 || !ExploredMapViewDetail::IsFiniteVector(DeltaPixels))
		{
			return Current;
		}
		FMapView Out = Current;
		Out.Center -= DeltaPixels * (Current.Size / SidePixels);
		return ClampView(Out);
	}

	FMapView PanByFraction(const FMapView& View, const FVector2D& Direction, double Fraction)
	{
		const FMapView Current = ClampView(View);
		if (!FMath::IsFinite(Fraction) || !ExploredMapViewDetail::IsFiniteVector(Direction))
		{
			return Current;
		}
		FMapView Out = Current;
		Out.Center += Direction.ClampAxes(-1.0, 1.0) * (Current.Size * Fraction);
		return ClampView(Out);
	}

	FSheetTransform FSheetTransform::Make(const FMapView& View, const FVector2D& LocalSize)
	{
		const FMapView Clamped = ClampView(View);
		FSheetTransform Out;
		const double Width = FMath::IsFinite(LocalSize.X) ? FMath::Max(LocalSize.X, 0.0) : 0.0;
		const double Height = FMath::IsFinite(LocalSize.Y) ? FMath::Max(LocalSize.Y, 0.0) : 0.0;
		Out.Side = FMath::Min(Width, Height);
		Out.Scale = Out.Side / Clamped.Size;
		Out.Origin = Clamped.Center - FVector2D(Clamped.Size * 0.5);
		Out.Offset = FVector2D((Width - Out.Side) * 0.5, (Height - Out.Side) * 0.5);
		return Out;
	}

	FVector2D FSheetTransform::MapToLocal(const FVector2D& Map) const
	{
		return (Map - Origin) * Scale + Offset;
	}

	FVector2D FSheetTransform::LocalToMap(const FVector2D& Local) const
	{
		if (Scale <= 0.0)
		{
			return Origin;
		}
		return (Local - Offset) / Scale + Origin;
	}

	bool FSheetTransform::IsInsideSheet(const FVector2D& Local) const
	{
		return Side > 0.0 && Local.X >= Offset.X && Local.Y >= Offset.Y && Local.X <= Offset.X + Side && Local.Y <= Offset.Y + Side;
	}

	int32 HitTestMarks(const TArray<FMapMark>& Marks, const FSheetTransform& Transform, const FVector2D& LocalPoint, double RadiusPixels)
	{
		int32 Best = INDEX_NONE;
		double BestDistSq = RadiusPixels * RadiusPixels;
		for (int32 I = 0; I < Marks.Num(); ++I)
		{
			const double DistSq = FVector2D::DistSquared(Transform.MapToLocal(Marks[I].Position), LocalPoint);
			if (DistSq <= BestDistSq)
			{
				Best = I;
				BestDistSq = DistSq;
			}
		}
		return Best;
	}

	FVector2D PanDirectionForKey(FName KeyName)
	{
		if (KeyName == FName(TEXT("Up")) || KeyName == FName(TEXT("Gamepad_DPad_Up")))
		{
			return FVector2D(0.0, -1.0);
		}
		if (KeyName == FName(TEXT("Down")) || KeyName == FName(TEXT("Gamepad_DPad_Down")))
		{
			return FVector2D(0.0, 1.0);
		}
		if (KeyName == FName(TEXT("Left")) || KeyName == FName(TEXT("Gamepad_DPad_Left")))
		{
			return FVector2D(-1.0, 0.0);
		}
		if (KeyName == FName(TEXT("Right")) || KeyName == FName(TEXT("Gamepad_DPad_Right")))
		{
			return FVector2D(1.0, 0.0);
		}
		return FVector2D::ZeroVector;
	}

	double ZoomFactorForKey(FName KeyName)
	{
		static const TArray<FName> ZoomIn = {
			FName(TEXT("PageUp")), FName(TEXT("Add")), FName(TEXT("Equals")),
			FName(TEXT("Gamepad_RightShoulder")), FName(TEXT("Gamepad_RightTrigger"))
		};
		static const TArray<FName> ZoomOut = {
			FName(TEXT("PageDown")), FName(TEXT("Subtract")), FName(TEXT("Hyphen")),
			FName(TEXT("Gamepad_LeftShoulder")), FName(TEXT("Gamepad_LeftTrigger"))
		};
		if (ZoomIn.Contains(KeyName))
		{
			return ZoomStep;
		}
		if (ZoomOut.Contains(KeyName))
		{
			return 1.0 / ZoomStep;
		}
		return 1.0;
	}

	bool IsSheetAcceptKey(FName KeyName)
	{
		return KeyName == FName(TEXT("Enter")) || KeyName == FName(TEXT("SpaceBar")) || KeyName == FName(TEXT("Gamepad_FaceButton_Bottom"));
	}

	double StickPanAmount(float AxisValue)
	{
		if (!FMath::IsFinite(AxisValue) || FMath::Abs(AxisValue) <= StickDeadZone)
		{
			return 0.0;
		}
		// Reescala lo que queda fuera de la zona muerta a 0…1 para que no haya salto al salir de ella.
		const double Sign = AxisValue > 0.0f ? 1.0 : -1.0;
		const double Beyond = (FMath::Min(FMath::Abs(static_cast<double>(AxisValue)), 1.0) - StickDeadZone) / (1.0 - StickDeadZone);
		return Sign * Beyond * StickPanFraction;
	}

	bool IsMapFocusSwitchKey(FName KeyName)
	{
		return KeyName == FName(TEXT("Gamepad_FaceButton_Top"));
	}

	bool ParseMapStampLabels(const FString& JsonText, TArray<FMapStampLabel>& OutStamps, FString& OutError)
	{
		OutStamps.Reset();
		FSaveValue Root;
		if (!FSaveText::Parse(JsonText, Root, OutError))
		{
			return false;
		}
		const FSaveValue* Marks = Root.IsObject() ? Root.Find(TEXT("map_marks")) : nullptr;
		if (!Marks || !Marks->IsArray())
		{
			OutError = TEXT("story_es.json sin lista map_marks");
			return false;
		}
		for (int32 I = 0; I < Marks->Num(); ++I)
		{
			const FSaveValue& Entry = Marks->At(I);
			const FSaveValue* Id = Entry.Find(TEXT("id"));
			if (!Id || !Id->IsString())
			{
				continue;
			}
			const FName StampId(*Id->AsString());
			if (!FCartographyModel::IsKnownStamp(StampId) || OutStamps.ContainsByPredicate([&StampId](const FMapStampLabel& L) { return L.Id == StampId; }))
			{
				continue;
			}
			FMapStampLabel& Label = OutStamps.AddDefaulted_GetRef();
			Label.Id = StampId;
			const FSaveValue* Es = Entry.Find(TEXT("label"));
			const FSaveValue* En = Entry.Find(TEXT("labelEn"));
			Label.LabelEs = Es ? Es->AsString() : FString();
			Label.LabelEn = En ? En->AsString() : FString();
			if (Label.LabelEs.IsEmpty())
			{
				Label.LabelEs = Id->AsString();
			}
		}
		OutError.Reset();
		return true;
	}

	TArray<FMapStampLabel> FallbackStampLabels()
	{
		TArray<FMapStampLabel> Out;
		for (const FName& Id : FCartographyModel::KnownStamps())
		{
			FMapStampLabel& Label = Out.AddDefaulted_GetRef();
			Label.Id = Id;
			Label.LabelEs = Id.ToString();
		}
		return Out;
	}

	FString LimitMarkTextWhileTyping(const FString& Text)
	{
		FString Out;
		for (int32 I = 0; I < Text.Len() && Out.Len() < FCartographyModel::MaxMarkTextLength; ++I)
		{
			const TCHAR C = Text[I];
			const bool bBreak = C == TEXT('\n') || C == TEXT('\r') || C == TEXT('\t');
			Out.AppendChar(bBreak ? TEXT(' ') : C);
		}
		return Out;
	}

	int32 RemainingMarkChars(const FString& Text)
	{
		return FMath::Max(0, FCartographyModel::MaxMarkTextLength - Text.Len());
	}

	bool CanCommitMark(FName StampId, const FString& Text)
	{
		return StampId.IsNone() ? !FCartographyModel::CapMarkText(Text).IsEmpty() : FCartographyModel::IsKnownStamp(StampId);
	}

	bool ShouldCloseMapOnKey(FName KeyName, bool bTypingText)
	{
		if (ExploredSettingsLogic::IsMenuBackKey(KeyName) || ExploredSettingsLogic::IsPauseToggleKey(KeyName))
		{
			return true;
		}
		return ExploredSettingsLogic::IsMapToggleKey(KeyName) && !bTypingText;
	}

	TArray<FWetStain> WetStains(uint32 Seed, float Wetness, int32 InkRuns)
	{
		TArray<FWetStain> Out;
		const float Wet = FMath::IsFinite(Wetness) ? FMath::Clamp(Wetness, 0.0f, 1.0f) : 0.0f;
		const int32 WetCount = FMath::Clamp(FMath::RoundToInt(Wet * static_cast<float>(MaxWetStains)), 0, MaxWetStains);
		const int32 DriedCount = FMath::Clamp(InkRuns, 0, MaxDriedStains);

		// Dos series separadas: al secarse el papel no se mueven los cercos de las pasadas.
		ExploredMapViewDetail::FDoodleRandom WetRandom(Seed);
		for (int32 I = 0; I < WetCount; ++I)
		{
			FWetStain& Stain = Out.AddDefaulted_GetRef();
			Stain.Center = FVector2D(WetRandom.Range(0.08, 0.92), WetRandom.Range(0.08, 0.92));
			Stain.Radius = WetRandom.Range(0.03, 0.09);
			Stain.Alpha = 0.08f + 0.22f * Wet;
		}
		ExploredMapViewDetail::FDoodleRandom DriedRandom(Seed ^ 0x85EBCA6Bu);
		for (int32 I = 0; I < DriedCount; ++I)
		{
			FWetStain& Stain = Out.AddDefaulted_GetRef();
			Stain.Center = FVector2D(DriedRandom.Range(0.1, 0.9), DriedRandom.Range(0.1, 0.9));
			Stain.Radius = DriedRandom.Range(0.04, 0.09);
			Stain.Alpha = 0.12f;
			Stain.bDried = true;
		}
		return Out;
	}

	TArray<FPolyline> AnnotationStrokes(const FWayfindingAnnotation& Annotation)
	{
		TArray<FPolyline> Out;
		const FVector2D From = FCartographyModel::WorldToMap(Annotation.From);
		const FVector2D To = FCartographyModel::WorldToMap(Annotation.To);
		if (!ExploredMapViewDetail::IsFiniteVector(From) || !ExploredMapViewDetail::IsFiniteVector(To))
		{
			return Out;
		}

		const bool bRing = Annotation.Technique == EWayfindingTechnique::FixedClouds || Annotation.Technique == EWayfindingTechnique::WaterColour;
		const FVector2D Delta = To - From;
		const double Length = Delta.Size();
		if (!FMath::IsFinite(Length))
		{
			return Out;
		}
		if (bRing || Length < UE_DOUBLE_KINDA_SMALL_NUMBER)
		{
			const double Meters = FMath::IsFinite(Annotation.DistanceMeters) ? static_cast<double>(Annotation.DistanceMeters) : 0.0;
			const double Radius = FMath::Max(FCartographyModel::MetersToMap(Meters), MinAnnotationRadius);
			ExploredMapViewDetail::AddDashedRing(Out, To, Radius, 24);
			return Out;
		}

		// Discontinuo de From a To: como mucho 200 trazos (las distancias largas alargan el paso).
		const FVector2D Dir = Delta / Length;
		double Dash = 0.012;
		double Gap = 0.008;
		constexpr int32 MaxDashes = 200;
		if (Length / (Dash + Gap) > static_cast<double>(MaxDashes))
		{
			const double Stretch = Length / ((Dash + Gap) * MaxDashes);
			Dash *= Stretch;
			Gap *= Stretch;
		}
		const int32 Dashes = FMath::Clamp(FMath::CeilToInt(Length / (Dash + Gap)), 1, MaxDashes);
		for (int32 I = 0; I < Dashes; ++I)
		{
			const double T = static_cast<double>(I) * (Dash + Gap);
			ExploredMapViewDetail::AddDash(Out, From + Dir * T, From + Dir * FMath::Min(T + Dash, Length));
		}

		// Punta de flecha en To.
		constexpr double Head = 0.012;
		FPolyline& Arrow = Out.AddDefaulted_GetRef();
		Arrow.Add(To - Dir.GetRotated(25.0) * Head);
		Arrow.Add(To);
		Arrow.Add(To - Dir.GetRotated(-25.0) * Head);

		// El camino de estrellas lleva además una estrella pequeña sobre el destino.
		if (Annotation.Technique == EWayfindingTechnique::StarPath)
		{
			constexpr double Star = 0.008;
			const FVector2D Top = To + FVector2D(0.0, -2.0 * Star);
			ExploredMapViewDetail::AddDash(Out, Top + FVector2D(-Star, 0.0), Top + FVector2D(Star, 0.0));
			ExploredMapViewDetail::AddDash(Out, Top + FVector2D(0.0, -Star), Top + FVector2D(0.0, Star));
		}
		return Out;
	}

	uint32 StableHash(const FString& Text)
	{
		uint32 Hash = 2166136261u;
		for (int32 I = 0; I < Text.Len(); ++I)
		{
			Hash ^= static_cast<uint32>(static_cast<uint16>(Text[I]));
			Hash *= 16777619u;
		}
		return Hash;
	}

	TArray<FPolyline> RecipeDoodle(FName RecipeId)
	{
		TArray<FPolyline> Out;
		const uint32 Seed = StableHash(RecipeId.ToString());
		ExploredMapViewDetail::FDoodleRandom Random(Seed);
		const int32 Strokes = 2 + static_cast<int32>(Seed % 3u);
		for (int32 S = 0; S < Strokes; ++S)
		{
			FPolyline& Line = Out.AddDefaulted_GetRef();
			const int32 Points = 4 + static_cast<int32>(Random.Next01() * 3.0);
			FVector2D P(Random.Range(0.15, 0.85), Random.Range(0.15, 0.85));
			for (int32 I = 0; I < Points; ++I)
			{
				Line.Add(P);
				P = (P + FVector2D(Random.Range(-0.25, 0.25), Random.Range(-0.25, 0.25))).ClampAxes(0.1, 0.9);
			}
		}
		return Out;
	}
}
