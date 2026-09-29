#include "WorldGen/FelledDriftModel.h"

namespace
{
	FDriftFloatSpec MakeSpec(const TCHAR* ItemId, bool bFloats, float DraftCm, const TCHAR* HandoverItemId)
	{
		FDriftFloatSpec Spec;
		Spec.ItemId = FName(ItemId);
		Spec.bFloats = bFloats;
		Spec.DraftCm = DraftCm;
		Spec.HandoverItemId = HandoverItemId ? FName(HandoverItemId) : NAME_None;
		return Spec;
	}
}

const TCHAR* LexToString(EFelledPieceState State)
{
	switch (State)
	{
	case EFelledPieceState::Resting:    return TEXT("Resting");
	case EFelledPieceState::Floating:   return TEXT("Floating");
	case EFelledPieceState::Collected:  return TEXT("Collected");
	case EFelledPieceState::HandedOver: return TEXT("HandedOver");
	case EFelledPieceState::Decayed:    return TEXT("Decayed");
	}
	return TEXT("Unknown");
}

TArray<FDriftFloatSpec> FFelledDriftModel::DefaultFloatSpecs()
{
	// Calado ≈ fracción sumergida (densidad / 1,025) × grosor. Los troncos son lo
	// único que llega a ser madera flotante; los cocos cruzan el mar enteros.
	return {
		MakeSpec(TEXT("tronco_pequeno"), true, 20.0f, TEXT("madera_flotante")),
		MakeSpec(TEXT("madera_blanda"), true, 8.0f, TEXT("madera_flotante")),
		MakeSpec(TEXT("madera_flotante"), true, 8.0f, TEXT("madera_flotante")),
		MakeSpec(TEXT("madera_dura"), false, 0.0f, nullptr),   // más densa que el agua de mar
		MakeSpec(TEXT("resina"), false, 0.0f, nullptr),        // 1,05–1,10 g/cm³
		MakeSpec(TEXT("corteza"), true, 1.0f, nullptr),
		MakeSpec(TEXT("rama_seca"), true, 4.0f, nullptr),
		MakeSpec(TEXT("rama_verde"), true, 4.0f, nullptr),
		MakeSpec(TEXT("palo_recto"), true, 3.0f, nullptr),
		MakeSpec(TEXT("vara_flexible"), true, 2.0f, nullptr),
		MakeSpec(TEXT("liana"), true, 1.0f, nullptr),
		MakeSpec(TEXT("hoja_platano"), true, 1.0f, nullptr),
		MakeSpec(TEXT("algodon_silvestre"), true, 1.0f, nullptr),
		MakeSpec(TEXT("hoja_palma"), true, 1.0f, nullptr),
		MakeSpec(TEXT("fibra_coco"), true, 1.0f, nullptr),
		MakeSpec(TEXT("coco_maduro"), true, 8.0f, TEXT("coco_maduro")),
		MakeSpec(TEXT("coco_verde"), true, 10.0f, TEXT("coco_verde")),
		MakeSpec(TEXT("cascara_coco"), true, 3.0f, nullptr),
	};
}

FDriftFloatSpec FFelledDriftModel::FindFloatSpec(const TArray<FDriftFloatSpec>& InSpecs, FName ItemId)
{
	for (const FDriftFloatSpec& Spec : InSpecs)
	{
		if (Spec.ItemId == ItemId)
		{
			FDriftFloatSpec Out = Spec;
			if (!FMath::IsFinite(Out.DraftCm) || Out.DraftCm < 0.0f)
			{
				Out.DraftCm = 0.0f;
			}
			return Out;
		}
	}
	FDriftFloatSpec Sinks;
	Sinks.ItemId = ItemId;
	return Sinks;
}

bool FFelledDriftModel::NeedsTracking(const FDriftFloatSpec& Spec, float DepthAtHighTideCm)
{
	return Spec.bFloats && FMath::IsFinite(DepthAtHighTideCm) && DepthAtHighTideCm > Spec.DraftCm;
}

FFelledDriftModel::FFelledDriftModel(TArray<FDriftFloatSpec> InSpecs)
	: Specs(MoveTemp(InSpecs))
{
}

float FFelledDriftModel::SafeDepth(FWaterDepth WaterDepth, const FVector2D& At)
{
	const float Depth = WaterDepth(At);
	// Un dato no finito cuenta como seco: la pieza se queda donde está.
	return FMath::IsFinite(Depth) ? Depth : -1.0f;
}

int32 FFelledDriftModel::AddPiece(const FFellingDrop& Drop, FWaterDepth WaterDepth)
{
	if (!FMath::IsFinite(Drop.Position.X) || !FMath::IsFinite(Drop.Position.Y))
	{
		return INDEX_NONE;
	}
	// Lo ya resuelto (recogido, entregado o deshecho) deja su hueco: si no, una sesión
	// larga talando junto al mar llenaría el modelo. El más bajo, para que el índice
	// dependa solo de la historia y no del orden de un contenedor.
	int32 Slot = INDEX_NONE;
	for (int32 i = 0; i < Pieces.Num(); ++i)
	{
		if (!Pieces[i].IsActive())
		{
			Slot = i;
			break;
		}
	}
	if (Slot == INDEX_NONE && Pieces.Num() >= MaxPieces)
	{
		return INDEX_NONE;
	}
	FFelledPiece Piece;
	Piece.Drop = Drop;
	Piece.Spec = FindFloatSpec(Specs, Drop.ItemId);
	if (Piece.Spec.bFloats && SafeDepth(WaterDepth, Drop.Position) > Piece.Spec.DraftCm)
	{
		Piece.State = EFelledPieceState::Floating;
	}
	if (Slot == INDEX_NONE)
	{
		return Pieces.Add(Piece);
	}
	Pieces[Slot] = Piece;
	return Slot;
}

FFelledDriftReport FFelledDriftModel::Advance(float DeltaSeconds, FWaterDepth WaterDepth, FWaterCurrent Current)
{
	FFelledDriftReport Report;
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return Report;
	}
	PendingS += (double)DeltaSeconds;
	// El margen absorbe el error de sumar fotogramas de 1/60 s; no adelanta un paso de verdad.
	const double Steps = FMath::FloorToDouble((PendingS + 1.0e-6) / (double)FixedStepS);
	PendingS = FMath::Max(0.0, PendingS - Steps * (double)FixedStepS);
	int32 NumSteps = Steps > (double)MAX_int32 ? MAX_int32 : (int32)Steps;
	if (NumSteps > MaxStepsPerAdvance)
	{
		Report.StepsDropped = NumSteps - MaxStepsPerAdvance;
		NumSteps = MaxStepsPerAdvance;
	}
	for (int32 s = 0; s < NumSteps; ++s)
	{
		for (int32 i = 0; i < Pieces.Num(); ++i)
		{
			Step(i, WaterDepth, Current, Report);
		}
	}
	Report.StepsRun = NumSteps;
	return Report;
}

void FFelledDriftModel::Step(int32 Index, FWaterDepth WaterDepth, FWaterCurrent Current, FFelledDriftReport& Report)
{
	FFelledPiece& Piece = Pieces[Index];
	// Lo que se hunde no se vuelve a mirar: no gasta consultas de profundidad.
	if (!Piece.IsActive() || !Piece.Spec.bFloats)
	{
		return;
	}
	const float Draft = Piece.Spec.DraftCm;

	if (Piece.State == EFelledPieceState::Resting)
	{
		if (SafeDepth(WaterDepth, Piece.Drop.Position) > Draft)
		{
			Piece.State = EFelledPieceState::Floating;
			Report.Refloated.Add(Index);
		}
		return;
	}

	// Flotando. Si la marea ha bajado bajo ella, vara donde está.
	if (SafeDepth(WaterDepth, Piece.Drop.Position) <= Draft)
	{
		Piece.State = EFelledPieceState::Resting;
		Report.Beached.Add(Index);
		return;
	}

	FVector2D Velocity = Current(Piece.Drop.Position);
	if (!FMath::IsFinite(Velocity.X) || !FMath::IsFinite(Velocity.Y))
	{
		Velocity = FVector2D::ZeroVector;
	}
	const double Speed = Velocity.Size();
	if (Speed > MaxCurrentCmS)
	{
		Velocity *= MaxCurrentCmS / Speed;
	}
	const FVector2D Move = Velocity * (double)FixedStepS;
	const int32 Segments = FMath::Max(1, FMath::CeilToInt(Move.Size() / MaxSegmentCm));
	const FVector2D SegmentMove = Move / (double)Segments;
	for (int32 k = 0; k < Segments; ++k)
	{
		const FVector2D Next = Piece.Drop.Position + SegmentMove;
		if (SafeDepth(WaterDepth, Next) <= Draft)
		{
			// Vara en el punto de contacto: el primero del tramo con menos agua que su
			// calado, afinado por bisección. Si varase en el último punto con agua, el
			// paso siguiente volvería a flotar allí mismo y la pieza oscilaría.
			FVector2D Afloat = Piece.Drop.Position;
			FVector2D Aground = Next;
			for (int32 b = 0; b < ContactBisections; ++b)
			{
				const FVector2D Mid = (Afloat + Aground) * 0.5;
				(SafeDepth(WaterDepth, Mid) <= Draft ? Aground : Afloat) = Mid;
			}
			Piece.Drop.Position = Aground;
			Piece.State = EFelledPieceState::Resting;
			Report.Beached.Add(Index);
			break;
		}
		Piece.Drop.Position = Next;
	}

	++Piece.FloatingSteps;
	if (Piece.State == EFelledPieceState::Floating && Piece.FloatingSteps >= HandoverSteps())
	{
		Finish(Index, Report);
	}
}

void FFelledDriftModel::Finish(int32 Index, FFelledDriftReport& Report)
{
	FFelledPiece& Piece = Pieces[Index];
	if (Piece.Spec.HandoverItemId.IsNone())
	{
		Piece.State = EFelledPieceState::Decayed;
		Report.Decayed.Add(Index);
	}
	else
	{
		Piece.State = EFelledPieceState::HandedOver;
		Report.HandedOver.Add(Index);
	}
}

bool FFelledDriftModel::Collect(int32 Index)
{
	if (!Pieces.IsValidIndex(Index) || !Pieces[Index].IsActive())
	{
		return false;
	}
	Pieces[Index].State = EFelledPieceState::Collected;
	return true;
}

bool FFelledDriftModel::HandOver(int32 Index)
{
	if (!Pieces.IsValidIndex(Index) || Pieces[Index].State != EFelledPieceState::Floating)
	{
		return false;
	}
	FFelledDriftReport Ignored;
	Finish(Index, Ignored);
	return true;
}

int32 FFelledDriftModel::NumActive() const
{
	int32 N = 0;
	for (const FFelledPiece& Piece : Pieces)
	{
		N += Piece.IsActive() ? 1 : 0;
	}
	return N;
}

int32 FFelledDriftModel::NumFloating() const
{
	int32 N = 0;
	for (const FFelledPiece& Piece : Pieces)
	{
		N += Piece.State == EFelledPieceState::Floating ? 1 : 0;
	}
	return N;
}
