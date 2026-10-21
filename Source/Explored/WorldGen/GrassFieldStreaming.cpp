// Actualización incremental de FGrassFieldModel: celdas que entran y salen del radio y reciclaje de
// las ranuras de instancia. La generación de cada celda está en GrassFieldModel.cpp.
#include "WorldGen/GrassFieldModel.h"

namespace GrassStreamingDetail
{
	/** Movimiento mínimo de la cámara (m²) para recalcular el conjunto de celdas. */
	constexpr double RecomputeDistSq = 1.0;

	struct FCandidate
	{
		FIntPoint Cell;
		double Dist = 0.0;
	};
}

int32 FGrassUpdate::WriteCount() const
{
	int32 N = 0;
	for (const TArray<FGrassWrite>& W : Writes)
	{
		N += W.Num();
	}
	return N;
}

int32 FGrassUpdate::HideCount() const
{
	int32 N = 0;
	for (const TArray<int32>& H : Hides)
	{
		N += H.Num();
	}
	return N;
}

FGrassFieldModel::FGrassFieldModel(const FGrassConfig& InConfig, TSharedPtr<const FGrassWorld> InWorld)
	: Config(InConfig)
	, World(MoveTemp(InWorld))
{
}

FIntPoint FGrassFieldModel::CellOf(const FVector2D& Meters) const
{
	return FIntPoint(FMath::FloorToInt32(Meters.X / Config.CellSizeM), FMath::FloorToInt32(Meters.Y / Config.CellSizeM));
}

double FGrassFieldModel::DistanceToCell(const FVector2D& Meters, const FIntPoint& Cell) const
{
	const double Size = Config.CellSizeM;
	const double Dx = FMath::Max3(Cell.X * Size - Meters.X, 0.0, Meters.X - (Cell.X + 1) * Size);
	const double Dy = FMath::Max3(Cell.Y * Size - Meters.Y, 0.0, Meters.Y - (Cell.Y + 1) * Size);
	return FMath::Sqrt(Dx * Dx + Dy * Dy);
}

int32 FGrassFieldModel::GetLiveCount(EGrassKind Kind) const
{
	int32 N = 0;
	for (const uint8 Visible : SlotVisible[static_cast<int32>(Kind)])
	{
		N += Visible;
	}
	return N;
}

const TArray<FGrassInstance>* FGrassFieldModel::FindCell(const FIntPoint& Cell) const
{
	const FCellState* State = Cells.Find(Cell);
	return State ? &State->Instances : nullptr;
}

int32 FGrassFieldModel::AcquireSlot(int32 Kind, FGrassUpdate& Update)
{
	int32 Slot = INDEX_NONE;
	if (!FreeSlots[Kind].IsEmpty())
	{
		Slot = FreeSlots[Kind].Pop();
	}
	else
	{
		Slot = SlotHighWater[Kind]++;
		SlotVisible[Kind].Add(0);
	}
	SlotVisible[Kind][Slot] = 1;
	return Slot;
}

FGrassUpdate FGrassFieldModel::Update(const FVector2D& CameraMeters)
{
	FGrassUpdate Out;
	if (!World || !FMath::IsFinite(CameraMeters.X) || !FMath::IsFinite(CameraMeters.Y))
	{
		return Out;
	}
	if (bHasCamera && Pending == 0 && FVector2D::DistSquared(CameraMeters, LastCamera) < GrassStreamingDetail::RecomputeDistSq)
	{
		return Out;
	}
	bHasCamera = true;
	LastCamera = CameraMeters;
	for (TArray<int32>& Released : ReleasedNow)
	{
		Released.Reset();
	}
	RemoveFarCells(CameraMeters, Out);
	AddNearCells(CameraMeters, Out);
	for (int32 K = 0; K < GrassKindCount; ++K)
	{
		for (const int32 Slot : ReleasedNow[K])
		{
			if (!SlotVisible[K][Slot])
			{
				Out.Hides[K].Add(Slot);
			}
		}
	}
	return Out;
}

void FGrassFieldModel::RemoveFarCells(const FVector2D& Camera, FGrassUpdate& Update)
{
	// Histéresis de media celda: una cámara que oscila en el borde no crea y borra la misma celda.
	const double Keep = Config.RadiusM + Config.CellSizeM * 0.5;
	TArray<FIntPoint> Far;
	for (const TPair<FIntPoint, FCellState>& Pair : Cells)
	{
		if (DistanceToCell(Camera, Pair.Key) > Keep)
		{
			Far.Add(Pair.Key);
		}
	}
	for (const FIntPoint& Cell : Far)
	{
		const FCellState& State = Cells.FindChecked(Cell);
		for (int32 I = 0; I < State.Instances.Num(); ++I)
		{
			const int32 Kind = static_cast<int32>(State.Instances[I].Kind);
			const int32 Slot = State.Slots[I];
			SlotVisible[Kind][Slot] = 0;
			FreeSlots[Kind].Add(Slot);
			ReleasedNow[Kind].Add(Slot);
		}
		LiveInstances -= State.Instances.Num();
		Cells.Remove(Cell);
		++Update.CellsRemoved;
	}
}

void FGrassFieldModel::AddNearCells(const FVector2D& Camera, FGrassUpdate& Update)
{
	using GrassStreamingDetail::FCandidate;
	const FIntPoint Center = CellOf(Camera);
	const int32 Reach = FMath::CeilToInt32(Config.RadiusM / Config.CellSizeM) + 1;
	TArray<FCandidate> Wanted;
	for (int32 Dx = -Reach; Dx <= Reach; ++Dx)
	{
		for (int32 Dy = -Reach; Dy <= Reach; ++Dy)
		{
			const FIntPoint Cell(Center.X + Dx, Center.Y + Dy);
			const double Dist = DistanceToCell(Camera, Cell);
			if (Dist <= Config.RadiusM && !Cells.Contains(Cell))
			{
				Wanted.Add({Cell, Dist});
			}
		}
	}
	// Las más cercanas primero; el desempate por coordenada deja el orden determinista.
	Wanted.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.Dist != B.Dist)
		{
			return A.Dist < B.Dist;
		}
		return A.Cell.X != B.Cell.X ? A.Cell.X < B.Cell.X : A.Cell.Y < B.Cell.Y;
	});
	const int32 Build = FMath::Min(Wanted.Num(), Config.CellsPerFrame);
	Pending = Wanted.Num() - Build;
	for (int32 C = 0; C < Build; ++C)
	{
		FCellState State;
		GenerateCell(Config, *World, Wanted[C].Cell, State.Instances);
		State.Slots.Reserve(State.Instances.Num());
		for (const FGrassInstance& Inst : State.Instances)
		{
			const int32 Kind = static_cast<int32>(Inst.Kind);
			const int32 Slot = AcquireSlot(Kind, Update);
			State.Slots.Add(Slot);
			Update.Writes[Kind].Add({Slot, Inst});
		}
		LiveInstances += State.Instances.Num();
		Cells.Add(Wanted[C].Cell, MoveTemp(State));
		++Update.CellsBuilt;
	}
}
