// Actualización incremental de FGrassFieldModel: niveles de densidad por celda (FGrassThinning), celdas
// que entran y salen del alcance y reciclaje de las ranuras de instancia. La generación de cada celda
// está en GrassFieldModel.cpp.
#include "WorldGen/GrassFieldModel.h"

namespace GrassStreamingDetail
{
	/** Movimiento mínimo de la cámara (m²) para recalcular niveles sin trabajo pendiente. */
	constexpr double RecomputeDistSq = 1.0;

	bool ByRank(const FGrassInstance& A, const FGrassInstance& B)
	{
		return A.Rank < B.Rank;
	}
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

int32 FGrassFieldModel::GetCellLevel(const FIntPoint& Cell) const
{
	const FCellState* State = Cells.Find(Cell);
	return State ? State->Level : INDEX_NONE;
}

const TArray<FGrassInstance>* FGrassFieldModel::FindCell(const FIntPoint& Cell) const
{
	const FCellState* State = Cells.Find(Cell);
	return State ? &State->Instances : nullptr;
}

int64 FGrassFieldModel::CountTrianglesInView(const FVector2D& Camera, const FVector2D& Forward, float HalfAngleRad, const int32 (&TrisPerKind)[GrassKindCount]) const
{
	const FVector2D Dir = Forward.GetSafeNormal();
	const double MinCos = FMath::Cos(static_cast<double>(FMath::Clamp(HalfAngleRad, 0.0f, 3.14159265f)));
	int64 Tris = 0;
	for (const TPair<FIntPoint, FCellState>& Pair : Cells)
	{
		for (const FGrassInstance& Inst : Pair.Value.Instances)
		{
			const FVector2D To = FVector2D(Inst.Location.X, Inst.Location.Y) - Camera;
			const double Len = To.Size();
			// Lo que está a menos de 2 m cuenta siempre (cerca del borde del encuadre por la altura del ojo).
			if (Len < 2.0 || FVector2D::DotProduct(To, Dir) >= MinCos * Len)
			{
				Tris += TrisPerKind[static_cast<int32>(Inst.Kind)];
			}
		}
	}
	return Tris;
}

int32 FGrassFieldModel::AcquireSlot(int32 Kind)
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

void FGrassFieldModel::ReleaseTail(FCellState& State, int32 Keep)
{
	for (int32 I = State.Instances.Num() - 1; I >= Keep; --I)
	{
		const int32 Kind = static_cast<int32>(State.Instances[I].Kind);
		const int32 Slot = State.Slots[I];
		SlotVisible[Kind][Slot] = 0;
		FreeSlots[Kind].Add(Slot);
		ReleasedNow[Kind].Add(Slot);
	}
	LiveInstances -= State.Instances.Num() - Keep;
	State.Instances.SetNum(Keep);
	State.Slots.SetNum(Keep);
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
	ShrinkOrRemoveCells(CameraMeters, Out);
	GrowCells(CameraMeters, Out);
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

void FGrassFieldModel::ShrinkOrRemoveCells(const FVector2D& Camera, FGrassUpdate& Update)
{
	// Bajar de nivel solo quita parches con rango ≥ f_k ≥ f(distancia de la celda): el material ya los
	// tiene encogidos a 0. La histéresis evita que una cámara que oscila suba y baje la misma celda.
	const FGrassThinning& Thin = Config.Thinning;
	const double Extra = Thin.MarginM + Thin.HysteresisM;
	TArray<FIntPoint> Gone;
	for (TPair<FIntPoint, FCellState>& Pair : Cells)
	{
		const int32 Keep = Thin.LevelFor(DistanceToCell(Camera, Pair.Key), Extra);
		FCellState& State = Pair.Value;
		if (Keep == INDEX_NONE)
		{
			Gone.Add(Pair.Key);
			continue;
		}
		if (Keep <= State.Level)
		{
			continue;
		}
		const float Limit = Thin.LevelFraction(Keep);
		int32 Count = State.Instances.Num();
		while (Count > 0 && State.Instances[Count - 1].Rank >= Limit)
		{
			--Count;
		}
		ReleaseTail(State, Count);
		State.Level = Keep;
		++Update.CellsRemoved;
	}
	for (const FIntPoint& Cell : Gone)
	{
		ReleaseTail(Cells.FindChecked(Cell), 0);
		Cells.Remove(Cell);
		++Update.CellsRemoved;
	}
}

void FGrassFieldModel::GrowCells(const FVector2D& Camera, FGrassUpdate& Update)
{
	const FGrassThinning& Thin = Config.Thinning;
	const FIntPoint Center = CellOf(Camera);
	const int32 Reach = FMath::CeilToInt32(Thin.ReachM() / Config.CellSizeM) + 1;
	TArray<FCellJob> Jobs;
	for (int32 Dx = -Reach; Dx <= Reach; ++Dx)
	{
		for (int32 Dy = -Reach; Dy <= Reach; ++Dy)
		{
			const FIntPoint Cell(Center.X + Dx, Center.Y + Dy);
			const double Dist = DistanceToCell(Camera, Cell);
			const int32 Need = Thin.LevelFor(Dist, Thin.MarginM);
			if (Need == INDEX_NONE)
			{
				continue;
			}
			const FCellState* State = Cells.Find(Cell);
			if (!State || Need < State->Level)
			{
				Jobs.Add({Cell, Dist, Need});
			}
		}
	}
	// Las más cercanas primero; el desempate por coordenada deja el orden determinista.
	Jobs.Sort([](const FCellJob& A, const FCellJob& B)
	{
		if (A.Dist != B.Dist)
		{
			return A.Dist < B.Dist;
		}
		return A.Cell.X != B.Cell.X ? A.Cell.X < B.Cell.X : A.Cell.Y < B.Cell.Y;
	});
	const int32 PerCell = Config.Subdivisions() * Config.Subdivisions();
	int32 Done = 0;
	for (; Done < Jobs.Num(); ++Done)
	{
		const FCellState* State = Cells.Find(Jobs[Done].Cell);
		const float From = State ? Thin.LevelFraction(State->Level) : 0.0f;
		const int32 Expected = FMath::CeilToInt32(PerCell * (Thin.LevelFraction(Jobs[Done].Level) - From));
		if (Update.Work > 0 && Update.Work + Expected > Config.WorkPerFrame)
		{
			break;
		}
		RunJob(Jobs[Done], Update);
	}
	Pending = Jobs.Num() - Done;
}

void FGrassFieldModel::RunJob(const FCellJob& Job, FGrassUpdate& Update)
{
	const FGrassThinning& Thin = Config.Thinning;
	FCellState* Existing = Cells.Find(Job.Cell);
	FCellState& State = Existing ? *Existing : Cells.Add(Job.Cell);
	const float From = Existing ? Thin.LevelFraction(State.Level) : 0.0f;
	const float To = Thin.LevelFraction(Job.Level);
	TArray<FGrassInstance> Added;
	Update.Work += GenerateCell(Config, *World, Job.Cell, From, To, Added);
	// Todos los nuevos tienen rango ≥ From > los que ya estaban: el orden por rango se conserva.
	Added.StableSort(GrassStreamingDetail::ByRank);
	State.Instances.Reserve(State.Instances.Num() + Added.Num());
	State.Slots.Reserve(State.Slots.Num() + Added.Num());
	for (const FGrassInstance& Inst : Added)
	{
		const int32 Kind = static_cast<int32>(Inst.Kind);
		const int32 Slot = AcquireSlot(Kind);
		State.Instances.Add(Inst);
		State.Slots.Add(Slot);
		Update.Writes[Kind].Add({Slot, Inst});
	}
	LiveInstances += Added.Num();
	State.Level = Job.Level;
	++Update.CellsBuilt;
}
