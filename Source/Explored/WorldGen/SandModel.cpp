#include "WorldGen/SandModel.h"

namespace SandModelDetail
{
	int32 FloorDiv(int32 A, int32 B)
	{
		const int32 Q = A / B;
		return (A % B != 0 && ((A < 0) != (B < 0))) ? Q - 1 : Q;
	}

	bool ColumnLess(const FIntPoint& A, const FIntPoint& B)
	{
		return A.Y != B.Y ? A.Y < B.Y : A.X < B.X;
	}

	bool ReadInt32(const FSaveValue& V, int32& Out)
	{
		int64 Value = 0;
		if (!V.TryGetInt(Value) || Value < MIN_int32 || Value > MAX_int32)
		{
			return false;
		}
		Out = static_cast<int32>(Value);
		return true;
	}

	const FIntPoint Neighbours[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	int64 MetersToMm(double Meters)
	{
		return FMath::FloorToInt64(Meters * 1000.0 + 0.5);
	}

	/** Estado de una columna leído antes de aplicar ningún flujo del paso. */
	struct FColumnState
	{
		int64 Height = 0;
		int64 BaseMm = 0;
		int32 Delta = 0;
		bool bHeld = false;
		bool bWet = false;
	};
}

double FSandEnvironment::DistanceToFoci(const FVector2D& P) const
{
	double Best = FVector2D::Distance(P, Focus);
	for (const FVector2D& Extra : ExtraFoci)
	{
		Best = FMath::Min(Best, FVector2D::Distance(P, Extra));
	}
	return Best;
}

int32 FSandModel::ReposeDropMm(float AngleDeg, float CellSize)
{
	const double Drop = FMath::Tan(FMath::DegreesToRadians(static_cast<double>(AngleDeg))) * CellSize * 1000.0;
	return static_cast<int32>(FMath::FloorToInt64(Drop));
}

int32 FSandModel::RefillMilli(int64 BaseMm, int64 HighMm, int64 LowMm, bool bSpring)
{
	if (BaseMm > HighMm)
	{
		return 0;
	}
	LowMm = FMath::Min(LowMm, HighMm);
	// Lineal: RefillAtHighTideMilli en la pleamar y RefillAtLowTideMilli en la bajamar y por debajo.
	int64 Rate = RefillAtLowTideMilli;
	if (BaseMm > LowMm)
	{
		Rate = RefillAtHighTideMilli + (HighMm - BaseMm) * (RefillAtLowTideMilli - RefillAtHighTideMilli) / (HighMm - LowMm);
	}
	if (bSpring)
	{
		Rate += SpringRefillBonusMilli;
	}
	return static_cast<int32>(FMath::Min<int64>(Rate, 1000));
}

double FSandModel::MassToCubicMeters(int64 Mass) const
{
	return static_cast<double>(Mass) * 0.001 * Settings.CellSize * Settings.CellSize;
}

int64 FSandModel::CubicMetersToMass(double CubicMeters) const
{
	return FMath::FloorToInt64(CubicMeters / (0.001 * Settings.CellSize * Settings.CellSize) + 0.5);
}

FSandModel::FSandModel(const FSandSettings& InSettings)
	: Settings(InSettings)
{
	check(Settings.CellSize > 0.0f && Settings.CellsPerChunk > 0);
}

FIntPoint FSandModel::ColumnOf(double X, double Y) const
{
	return FIntPoint(
		static_cast<int32>(FMath::FloorToInt64(X / Settings.CellSize + 0.5)),
		static_cast<int32>(FMath::FloorToInt64(Y / Settings.CellSize + 0.5)));
}

FVector2D FSandModel::ColumnPosition(const FIntPoint& Column) const
{
	return FVector2D(static_cast<double>(Column.X) * Settings.CellSize, static_cast<double>(Column.Y) * Settings.CellSize);
}

FIntPoint FSandModel::ChunkOfColumn(const FIntPoint& Column) const
{
	const int32 N = Settings.CellsPerChunk;
	return FIntPoint(SandModelDetail::FloorDiv(Column.X, N), SandModelDetail::FloorDiv(Column.Y, N));
}

int32 FSandModel::LocalIndex(const FIntPoint& Column, const FIntPoint& Chunk) const
{
	const int32 N = Settings.CellsPerChunk;
	return (Column.X - Chunk.X * N) + N * (Column.Y - Chunk.Y * N);
}

FSandModel::FChunk& FSandModel::TouchChunk(const FIntPoint& Chunk, FBaseHeight Base)
{
	const int32 N = Settings.CellsPerChunk;
	FChunk& C = Chunks.FindOrAdd(Chunk);
	if (C.Delta.Num() != N * N)
	{
		C.Delta.SetNumZeroed(N * N);
		C.Anchor.SetNumZeroed(N * N);
		C.Hold.SetNumZeroed(N * N);
		C.NonZero = 0;
	}
	if (!C.bBaseValid)
	{
		C.BaseMm.SetNumZeroed(N * N);
		for (int32 Y = 0; Y < N; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				const FVector2D P = ColumnPosition(FIntPoint(Chunk.X * N + X, Chunk.Y * N + Y));
				C.BaseMm[X + N * Y] = static_cast<int32>(SandModelDetail::MetersToMm(Base(P.X, P.Y)));
			}
		}
		C.bBaseValid = true;
	}
	return C;
}

FSandModel::FChunk& FSandModel::TouchColumn(const FIntPoint& Column, FBaseHeight Base, int32& OutLocal)
{
	const FIntPoint Chunk = ChunkOfColumn(Column);
	OutLocal = LocalIndex(Column, Chunk);
	return TouchChunk(Chunk, Base);
}

const FSandModel::FChunk* FSandModel::FindChunk(const FIntPoint& Column, int32& OutLocal) const
{
	const FIntPoint Chunk = ChunkOfColumn(Column);
	OutLocal = LocalIndex(Column, Chunk);
	const FChunk* C = Chunks.Find(Chunk);
	return (C && C->Delta.Num() > 0) ? C : nullptr;
}

int32 FSandModel::DeltaMm(const FIntPoint& Column) const
{
	int32 Local = 0;
	const FChunk* C = FindChunk(Column, Local);
	return C ? C->Delta[Local] : 0;
}

double FSandModel::Height(const FIntPoint& Column, FBaseHeight Base) const
{
	const FVector2D P = ColumnPosition(Column);
	// Misma cuantización que la caché: la altura que ve el jugador es la que simula el modelo.
	return static_cast<double>(SandModelDetail::MetersToMm(Base(P.X, P.Y)) + DeltaMm(Column)) * 0.001;
}

bool FSandModel::IsAnchored(const FIntPoint& Column) const
{
	int32 Local = 0;
	const FChunk* C = FindChunk(Column, Local);
	return C && C->Anchor[Local] > 0;
}

bool FSandModel::IsHeld(const FIntPoint& Column) const
{
	int32 Local = 0;
	const FChunk* C = FindChunk(Column, Local);
	return C && C->Hold[Local] > 0;
}

double FSandModel::DistanceToChunk(const FVector2D& P, const FIntPoint& Chunk) const
{
	// El chunk cubre las columnas [C·N, C·N + N − 1]; sus puntos de rejilla forman la caja.
	const int32 N = Settings.CellsPerChunk;
	const FVector2D Lo = ColumnPosition(FIntPoint(Chunk.X * N, Chunk.Y * N));
	const FVector2D Hi = ColumnPosition(FIntPoint(Chunk.X * N + N - 1, Chunk.Y * N + N - 1));
	const double DX = FMath::Max(0.0, FMath::Max(Lo.X - P.X, P.X - Hi.X));
	const double DY = FMath::Max(0.0, FMath::Max(Lo.Y - P.Y, P.Y - Hi.Y));
	return FMath::Sqrt(DX * DX + DY * DY);
}

bool FSandModel::ChunkIsActive(const FIntPoint& Chunk, const FSandEnvironment& Env) const
{
	if (DistanceToChunk(Env.Focus, Chunk) <= Env.ActiveRadius)
	{
		return true;
	}
	for (const FVector2D& Extra : Env.ExtraFoci)
	{
		if (DistanceToChunk(Extra, Chunk) <= Env.ActiveRadius)
		{
			return true;
		}
	}
	return false;
}

void FSandModel::AddDelta(const FIntPoint& Column, int32 Amount, FBaseHeight Base)
{
	if (Amount == 0)
	{
		return;
	}
	int32 Local = 0;
	FChunk& C = TouchColumn(Column, Base, Local);
	const bool bWasZero = C.Delta[Local] == 0;
	C.Delta[Local] += Amount;
	const bool bIsZero = C.Delta[Local] == 0;
	C.NonZero += (bWasZero ? 1 : 0) - (bIsZero ? 1 : 0);
}

void FSandModel::MarkDirtyAround(const FIntPoint& Column)
{
	Dirty.FindOrAdd(Column);
	for (const FIntPoint& Offset : SandModelDetail::Neighbours)
	{
		Dirty.FindOrAdd(Column + Offset);
	}
}

void FSandModel::ChunksReadingColumn(const FIntPoint& Column, TArray<FIntPoint>& Out) const
{
	// El chunk C malla los vértices [C·N, C·N + N] y lee uno más a cada lado para las normales.
	const int32 N = Settings.CellsPerChunk;
	auto Range = [N](int32 V, int32& Lo, int32& Hi)
	{
		Lo = -SandModelDetail::FloorDiv(-(V - N - 1), N);
		Hi = SandModelDetail::FloorDiv(V + 1, N);
	};
	int32 X0 = 0, X1 = 0, Y0 = 0, Y1 = 0;
	Range(Column.X, X0, X1);
	Range(Column.Y, Y0, Y1);
	for (int32 Y = Y0; Y <= Y1; ++Y)
	{
		for (int32 X = X0; X <= X1; ++X)
		{
			Out.Add(FIntPoint(X, Y));
		}
	}
}

void FSandModel::FinishDirtyChunks(TArray<FIntPoint>& List)
{
	List.Sort(&SandModelDetail::ColumnLess);
	TArray<FIntPoint> Unique;
	for (const FIntPoint& C : List)
	{
		if (Unique.Num() == 0 || Unique.Last() != C)
		{
			Unique.Add(C);
		}
	}
	List = MoveTemp(Unique);
}

FSandResult FSandModel::Brush(const FSandBrush& In, FBaseHeight Base, bool bDig)
{
	FSandResult Result;
	if (!(In.Radius > 0.0f) || !(In.Depth > 0.0f) || (!bDig && In.MassBudget <= 0) || In.MassBudget < 0
		|| !FMath::IsFinite(In.Center.X) || !FMath::IsFinite(In.Center.Y) || !FMath::IsFinite(In.Radius) || !FMath::IsFinite(In.Depth))
	{
		return Result;
	}
	const FIntPoint Lo = ColumnOf(In.Center.X - In.Radius, In.Center.Y - In.Radius);
	const FIntPoint Hi = ColumnOf(In.Center.X + In.Radius, In.Center.Y + In.Radius);

	struct FWant
	{
		FIntPoint Column;
		int64 Amount;
	};
	TArray<FWant> Wants;
	int64 Total = 0;
	for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
	{
		for (int32 X = Lo.X; X <= Hi.X; ++X)
		{
			const FIntPoint Column(X, Y);
			const double R = FVector2D::Distance(ColumnPosition(Column), In.Center);
			if (R >= In.Radius || IsAnchored(Column))
			{
				continue;
			}
			// Cono: toda la profundidad en el centro y nada en el borde.
			int64 Amount = SandModelDetail::MetersToMm(In.Depth * (1.0 - R / In.Radius));
			const int32 D = DeltaMm(Column);
			Amount = bDig ? FMath::Min<int64>(Amount, static_cast<int64>(D) + MaxDigDepthMm)
			              : FMath::Min<int64>(Amount, static_cast<int64>(MaxPileHeightMm) - D);
			if (Amount > 0)
			{
				Wants.Add({ Column, Amount });
				Total += Amount;
			}
		}
	}
	if (Total == 0)
	{
		return Result;
	}
	const bool bScale = In.MassBudget > 0 && Total > In.MassBudget;

	if (bScale)
	{
		// Reparto proporcional truncado y el resto de milímetro en milímetro, en orden:
		// la pasada mueve exactamente el presupuesto, sin pasarse en ninguna columna.
		int64 Given = 0;
		TArray<int64> Caps;
		for (FWant& W : Wants)
		{
			Caps.Add(W.Amount);
			W.Amount = W.Amount * In.MassBudget / Total;
			Given += W.Amount;
		}
		for (int32 I = 0; I < Wants.Num() && Given < In.MassBudget; ++I)
		{
			if (Wants[I].Amount < Caps[I])
			{
				++Wants[I].Amount;
				++Given;
			}
		}
	}

	TArray<FIntPoint> DirtyChunks;
	for (const FWant& W : Wants)
	{
		const int64 Amount = W.Amount;
		if (Amount <= 0)
		{
			continue;
		}
		AddDelta(W.Column, static_cast<int32>(bDig ? -Amount : Amount), Base);
		MarkDirtyAround(W.Column);
		ChunksReadingColumn(W.Column, DirtyChunks);
		Result.Mass += Amount;
		++Result.ColumnsChanged;
	}
	FinishDirtyChunks(DirtyChunks);
	Result.DirtyChunks = MoveTemp(DirtyChunks);
	return Result;
}

FSandResult FSandModel::Dig(const FSandBrush& In, FBaseHeight Base)
{
	return Brush(In, Base, true);
}

FSandResult FSandModel::Pile(const FSandBrush& In, FBaseHeight Base)
{
	return Brush(In, Base, false);
}

FSandResult FSandModel::SetAnchor(const FVector2D& Min, const FVector2D& Max, bool bAnchor, FBaseHeight Base)
{
	FSandResult Result;
	if (!FMath::IsFinite(Min.X) || !FMath::IsFinite(Min.Y) || !FMath::IsFinite(Max.X) || !FMath::IsFinite(Max.Y)
		|| Min.X > Max.X || Min.Y > Max.Y)
	{
		return Result;
	}
	// Huella: columnas cuyo punto de rejilla cae dentro de la caja (bordes incluidos).
	// Sujeción: columnas a ≤ AnchorHoldMeters de la caja (distancia euclídea, esquinas redondas).
	const double Cell = Settings.CellSize;
	const double Reach = AnchorHoldMeters;
	const FIntPoint Lo(
		static_cast<int32>(-FMath::FloorToInt64(-(Min.X - Reach) / Cell)),
		static_cast<int32>(-FMath::FloorToInt64(-(Min.Y - Reach) / Cell)));
	const FIntPoint Hi(
		static_cast<int32>(FMath::FloorToInt64((Max.X + Reach) / Cell)),
		static_cast<int32>(FMath::FloorToInt64((Max.Y + Reach) / Cell)));
	for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
	{
		for (int32 X = Lo.X; X <= Hi.X; ++X)
		{
			const FIntPoint Column(X, Y);
			const FVector2D P = ColumnPosition(Column);
			const double DX = FMath::Max(0.0, FMath::Max(Min.X - P.X, P.X - Max.X));
			const double DY = FMath::Max(0.0, FMath::Max(Min.Y - P.Y, P.Y - Max.Y));
			const double Distance = FMath::Sqrt(DX * DX + DY * DY);
			// Margen de redondeo: una columna a 1 m justo (4 celdas) queda dentro.
			if (Distance > Reach + 1e-6)
			{
				continue;
			}
			const bool bFootprint = Distance == 0.0;
			int32 Local = 0;
			FChunk& C = TouchColumn(Column, Base, Local);
			if (bAnchor)
			{
				if (C.Hold[Local] == 255 || (bFootprint && C.Anchor[Local] == 255))
				{
					continue;
				}
				++C.Hold[Local];
				if (bFootprint)
				{
					++C.Anchor[Local];
					++Result.ColumnsChanged;
				}
			}
			else if (C.Hold[Local] > 0 && (!bFootprint || C.Anchor[Local] > 0))
			{
				--C.Hold[Local];
				if (bFootprint)
				{
					--C.Anchor[Local];
					++Result.ColumnsChanged;
				}
				if (C.Hold[Local] == 0)
				{
					// Sin la estructura, esta arena puede volver a moverse.
					MarkDirtyAround(Column);
				}
			}
		}
	}
	return Result;
}

void FSandModel::WakeForTide(const FSandEnvironment& Env)
{
	const int64 HighMm = SandModelDetail::MetersToMm(Env.HighTide);
	if (bHasWoken && Env.bRaining == bLastWakeRaining && FMath::Abs(HighMm - LastWakeHighMm) < TideWakeStepMm)
	{
		return;
	}
	bHasWoken = true;
	LastWakeHighMm = HighMm;
	bLastWakeRaining = Env.bRaining;

	// La pleamar del día o la lluvia cambian qué arena está húmeda: se revisan las columnas
	// editadas de los chunks activos (los chunks sin deltas se saltan).
	const int32 N = Settings.CellsPerChunk;
	for (const FIntPoint& Key : EditedChunks())
	{
		if (!ChunkIsActive(Key, Env))
		{
			continue;
		}
		const FChunk& C = Chunks.FindChecked(Key);
		for (int32 I = 0; I < N * N; ++I)
		{
			if (C.Delta[I] != 0)
			{
				Dirty.FindOrAdd(FIntPoint(Key.X * N + I % N, Key.Y * N + I / N));
			}
		}
	}
}

FSandResult FSandModel::Advance(int32 DeltaMs, const FSandEnvironment& Env, FBaseHeight Base)
{
	FSandResult Total;
	if (DeltaMs > 0)
	{
		// Suma sin desbordar: más de MaxTicksPerAdvance revisiones se descartan igualmente.
		AccumulatedMs = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(AccumulatedMs) + DeltaMs, static_cast<int64>(TickMs) * (MaxTicksPerAdvance + 1)));
	}
	TArray<FIntPoint> DirtyChunks;
	while (AccumulatedMs >= TickMs && Total.Ticks < MaxTicksPerAdvance)
	{
		AccumulatedMs -= TickMs;
		FSandResult Step = Tick(Env, Base);
		DirtyChunks.Append(Step.DirtyChunks);
		Total.ColumnsChanged += Step.ColumnsChanged;
		Total.ActiveColumns += Step.ActiveColumns;
		Total.DeferredColumns = Step.DeferredColumns;
		Total.DormantColumns = Step.DormantColumns;
		Total.ActiveChunks = FMath::Max(Total.ActiveChunks, Step.ActiveChunks);
		++Total.Ticks;
	}
	// Una playa no recuerda diez minutos de avalancha (08 §2.6): el tiempo que no cabe
	// en MaxTicksPerAdvance revisiones se descarta; solo se guarda la fracción de segundo.
	AccumulatedMs = FMath::Min(AccumulatedMs, TickMs - 1);
	FinishDirtyChunks(DirtyChunks);
	Total.DirtyChunks = MoveTemp(DirtyChunks);
	return Total;
}

FSandResult FSandModel::Tick(const FSandEnvironment& Env, FBaseHeight Base)
{
	using namespace SandModelDetail;
	FSandResult Result;
	Result.Ticks = 1;
	WakeForTide(Env);

	const int64 HighMm = MetersToMm(Env.HighTide);
	const int32 DryDrop = ReposeDropMm(DryReposeDeg, Settings.CellSize);
	const int32 WetDrop = ReposeDropMm(WetReposeDeg, Settings.CellSize);

	// 1. Columnas sucias en orden (Y, X): las de chunks activos se revisan, las demás se congelan.
	TArray<FIntPoint> Pending;
	Dirty.GetKeys(Pending);
	Pending.Sort(&ColumnLess);
	TMap<FIntPoint, bool> ChunkActive;
	auto IsActiveChunk = [&](const FIntPoint& Column)
	{
		const FIntPoint Chunk = ChunkOfColumn(Column);
		if (const bool* Found = ChunkActive.Find(Chunk))
		{
			return *Found;
		}
		const bool bActive = ChunkIsActive(Chunk, Env);
		ChunkActive.Add(Chunk, bActive);
		Result.ActiveChunks += bActive ? 1 : 0;
		return bActive;
	};
	TMap<FIntPoint, uint8> NextDirty;
	TArray<FIntPoint> Sweep;
	for (const FIntPoint& Column : Pending)
	{
		if (IsActiveChunk(Column))
		{
			Sweep.Add(Column);
		}
		else
		{
			NextDirty.Add(Column, 1);
		}
	}
	Result.ActiveColumns = Sweep.Num();
	Result.DormantColumns = NextDirty.Num();

	// Columnas que ya han cambiado en esta revisión y cuántas lleva cada chunk (tope de red).
	TMap<FIntPoint, uint8> ChangedThisTick;
	TMap<FIntPoint, int32> ChangedPerChunk;
	TMap<FIntPoint, uint8> Deferred;
	/** Delta de cada columna tocada antes de la revisión, para contar solo el cambio neto. */
	TMap<FIntPoint, int32> Moved;
	TArray<FIntPoint> LastChanged;

	for (int32 Pass = 0; Pass < SweepsPerTick && Sweep.Num() > 0; ++Pass)
	{
		TMap<FIntPoint, uint8> InSweep;
		for (const FIntPoint& Column : Sweep)
		{
			InSweep.Add(Column, 1);
		}

		// 2. Instantánea de las columnas implicadas (las de la pasada y sus vecinas).
		TMap<FIntPoint, FColumnState> States;
		auto StateOf = [&](const FIntPoint& Column) -> FColumnState
		{
			if (const FColumnState* Found = States.Find(Column))
			{
				return *Found;
			}
			FColumnState S;
			int32 Local = 0;
			const FChunk& C = TouchColumn(Column, Base, Local);
			S.BaseMm = C.BaseMm[Local];
			S.Delta = C.Delta[Local];
			S.Height = S.BaseMm + S.Delta;
			S.bHeld = C.Hold[Local] > 0;
			S.bWet = Env.bRaining || S.Height <= HighMm;
			States.Add(Column, S);
			return S;
		};

		// 3. Flujos por pareja (positivo: de A a B), cada pareja una sola vez.
		struct FFlow
		{
			FIntPoint A;
			FIntPoint B;
			int64 Amount;
		};
		TArray<FFlow> Flows;
		for (const FIntPoint& A : Sweep)
		{
			for (const FIntPoint& Offset : Neighbours)
			{
				const FIntPoint B = A + Offset;
				if (InSweep.Contains(B) && ColumnLess(B, A))
				{
					continue;
				}
				const FColumnState SA = StateOf(A);
				const FColumnState SB = StateOf(B);
				// Del lado alto al bajo, solo el exceso sobre el reposo y sobre la base.
				const bool bAHigh = SA.Height >= SB.Height;
				const FColumnState& High = bAHigh ? SA : SB;
				const FColumnState& Low = bAHigh ? SB : SA;
				// La arena sujeta por una estructura no desliza; la de fuera sí puede caer contra ella.
				if (High.bHeld)
				{
					continue;
				}
				const int64 Drop = High.Height - Low.Height;
				const int64 Repose = High.bWet ? WetDrop : DryDrop;
				const int64 Threshold = FMath::Max<int64>(Repose, High.BaseMm - Low.BaseMm);
				const int64 Excess = Drop - Threshold;
				if (Excess > 0)
				{
					const int64 Slide = FMath::Max<int64>(1, Excess / AvalancheDivisor);
					Flows.Add({ A, B, bAHigh ? Slide : -Slide });
				}
			}
		}

		// 4. Tope de 64 columnas cambiadas por chunk y revisión. Primero las pendientes más
		// fuertes; lo que no cabe espera a la revisión siguiente.
		Flows.Sort([](const FFlow& X, const FFlow& Y)
		{
			const int64 AX = FMath::Abs(X.Amount);
			const int64 AY = FMath::Abs(Y.Amount);
			if (AX != AY)
			{
				return AX > AY;
			}
			return X.A != Y.A ? ColumnLess(X.A, Y.A) : ColumnLess(X.B, Y.B);
		});
		auto Cost = [&](const FIntPoint& Column, TMap<FIntPoint, int32>& Extra)
		{
			if (!ChangedThisTick.Contains(Column))
			{
				Extra.FindOrAdd(ChunkOfColumn(Column)) += 1;
			}
		};
		TArray<FFlow> Accepted;
		for (const FFlow& F : Flows)
		{
			TMap<FIntPoint, int32> Extra;
			Cost(F.A, Extra);
			Cost(F.B, Extra);
			bool bFits = true;
			for (const auto& Pair : Extra)
			{
				const int32* Used = ChangedPerChunk.Find(Pair.Key);
				if ((Used ? *Used : 0) + Pair.Value > MaxChangedColumnsPerChunk)
				{
					bFits = false;
				}
			}
			if (!bFits)
			{
				Deferred.FindOrAdd(F.A);
				Deferred.FindOrAdd(F.B);
				continue;
			}
			for (const auto& Pair : Extra)
			{
				ChangedPerChunk.FindOrAdd(Pair.Key) += Pair.Value;
			}
			ChangedThisTick.FindOrAdd(F.A);
			ChangedThisTick.FindOrAdd(F.B);
			Accepted.Add(F);
		}

		// 5. Nadie da más arena de la que tiene sobre la roca (reparto proporcional truncado).
		TMap<FIntPoint, int64> Outflow;
		for (const FFlow& F : Accepted)
		{
			Outflow.FindOrAdd(F.Amount > 0 ? F.A : F.B) += FMath::Abs(F.Amount);
		}
		TMap<FIntPoint, int64> Change;
		for (FFlow& F : Accepted)
		{
			const FIntPoint& Source = F.Amount > 0 ? F.A : F.B;
			const int64 Available = FMath::Max<int64>(0, static_cast<int64>(StateOf(Source).Delta) + MaxDigDepthMm);
			const int64 Out = Outflow.FindChecked(Source);
			if (Out > Available)
			{
				F.Amount = F.Amount * Available / Out;
			}
			if (F.Amount != 0)
			{
				Change.FindOrAdd(F.A) -= F.Amount;
				Change.FindOrAdd(F.B) += F.Amount;
			}
		}

		// 6. Aplicar y preparar la pasada siguiente: lo que ha cambiado y sus vecinas activas.
		TArray<FIntPoint> Changed;
		for (const auto& Pair : Change)
		{
			if (Pair.Value != 0)
			{
				Changed.Add(Pair.Key);
			}
		}
		Changed.Sort(&ColumnLess);
		TMap<FIntPoint, uint8> NextSweep;
		for (const FIntPoint& Column : Changed)
		{
			if (!Moved.Contains(Column))
			{
				Moved.Add(Column, DeltaMm(Column));
			}
			AddDelta(Column, static_cast<int32>(Change.FindChecked(Column)), Base);
			if (IsActiveChunk(Column))
			{
				NextSweep.FindOrAdd(Column);
			}
			else
			{
				NextDirty.FindOrAdd(Column);
			}
			for (const FIntPoint& Offset : Neighbours)
			{
				const FIntPoint Next = Column + Offset;
				if (IsActiveChunk(Next))
				{
					NextSweep.FindOrAdd(Next);
				}
				else
				{
					NextDirty.FindOrAdd(Next);
				}
			}
		}
		LastChanged = MoveTemp(Changed);
		Sweep.Reset();
		NextSweep.GetKeys(Sweep);
		Sweep.Sort(&ColumnLess);
	}

	// 7. Lo que sigue moviéndose al acabar las pasadas, y lo aplazado por el tope, se
	// revisa en la revisión siguiente. Lo demás está asentado y deja de estar sucio.
	if (LastChanged.Num() > 0)
	{
		for (const FIntPoint& Column : Sweep)
		{
			NextDirty.FindOrAdd(Column);
		}
	}
	for (const auto& Pair : Deferred)
	{
		NextDirty.FindOrAdd(Pair.Key);
	}
	Result.DeferredColumns = Deferred.Num();
	Dirty = MoveTemp(NextDirty);

	// Una columna puede volver a su valor dentro de la revisión (va y viene entre pasadas):
	// solo cuenta, y solo sale por la red, el cambio neto.
	TArray<FIntPoint> DirtyChunks;
	for (const auto& Pair : Moved)
	{
		if (DeltaMm(Pair.Key) != Pair.Value)
		{
			ChunksReadingColumn(Pair.Key, DirtyChunks);
			++Result.ColumnsChanged;
		}
	}
	FinishDirtyChunks(DirtyChunks);
	Result.DirtyChunks = MoveTemp(DirtyChunks);
	return Result;
}

FSandResult FSandModel::ApplyHalfTide(const FSandTide& Tide, FBaseHeight Base)
{
	using namespace SandModelDetail;
	FSandResult Result;
	if (!FMath::IsFinite(Tide.HighTide) || !FMath::IsFinite(Tide.LowTide))
	{
		return Result;
	}
	const int64 HighMm = MetersToMm(Tide.HighTide);
	const int64 LowMm = MetersToMm(Tide.LowTide);
	const int32 N = Settings.CellsPerChunk;
	TArray<FIntPoint> DirtyChunks;
	// Todas las columnas editadas, cerca o lejos de los jugadores: la marea sube en toda la isla.
	for (const FIntPoint& Key : EditedChunks())
	{
		FChunk& C = TouchChunk(Key, Base);
		for (int32 I = 0; I < N * N; ++I)
		{
			const int32 D = C.Delta[I];
			if (D == 0 || C.Hold[I] > 0)
			{
				continue;
			}
			const int32 Rate = RefillMilli(C.BaseMm[I], HighMm, LowMm, Tide.bSpring);
			if (Rate == 0)
			{
				continue;
			}
			// Hacia la altura original: se redondea hacia arriba para que un delta pequeño
			// también se cierre, y la última onda remata lo que queda por debajo de RefillSnapMm.
			const int64 Size = FMath::Abs(static_cast<int64>(D));
			int64 Amount = (Size * Rate + 999) / 1000;
			if (Size - Amount <= RefillSnapMm)
			{
				Amount = Size;
			}
			const int32 Signed = static_cast<int32>(D > 0 ? -Amount : Amount);
			const FIntPoint Column(Key.X * N + I % N, Key.Y * N + I / N);
			AddDelta(Column, Signed, Base);
			SeaBank -= Signed;
			Result.SeaMass += Signed;
			MarkDirtyAround(Column);
			ChunksReadingColumn(Column, DirtyChunks);
			++Result.ColumnsChanged;
		}
	}
	FinishDirtyChunks(DirtyChunks);
	Result.DirtyChunks = MoveTemp(DirtyChunks);
	return Result;
}

int64 FSandModel::TotalMass() const
{
	int64 Sum = 0;
	for (const auto& Pair : Chunks)
	{
		for (const int32 D : Pair.Value.Delta)
		{
			Sum += D;
		}
	}
	return Sum;
}

TArray<FIntPoint> FSandModel::EditedChunks() const
{
	TArray<FIntPoint> Out;
	for (const auto& Pair : Chunks)
	{
		if (Pair.Value.NonZero > 0)
		{
			Out.Add(Pair.Key);
		}
	}
	Out.Sort(&SandModelDetail::ColumnLess);
	return Out;
}

bool FSandModel::IsEmpty() const
{
	return Dirty.Num() == 0 && EditedChunks().Num() == 0;
}

void FSandModel::Reset()
{
	Chunks.Reset();
	Dirty.Reset();
	SeaBank = 0;
	AccumulatedMs = 0;
	LastWakeHighMm = 0;
	bLastWakeRaining = false;
	bHasWoken = false;
}

FSaveValue FSandModel::ToValue() const
{
	FSaveValue Root = FSaveValue::MakeObject();
	Root.Set(TEXT("v"), FSaveValue::MakeInt(1));
	Root.Set(TEXT("cell"), FSaveValue::MakeFloat(Settings.CellSize));
	Root.Set(TEXT("n"), FSaveValue::MakeInt(Settings.CellsPerChunk));

	FSaveValue ChunkList = FSaveValue::MakeArray();
	for (const FIntPoint& Key : EditedChunks())
	{
		const FChunk& C = Chunks.FindChecked(Key);
		// Tramos de columnas con delta: [Inicio, Cuenta, d…].
		FSaveValue Runs = FSaveValue::MakeArray();
		int32 I = 0;
		while (I < C.Delta.Num())
		{
			if (C.Delta[I] == 0)
			{
				++I;
				continue;
			}
			int32 End = I + 1;
			while (End < C.Delta.Num() && C.Delta[End] != 0)
			{
				++End;
			}
			Runs.Add(FSaveValue::MakeInt(I));
			Runs.Add(FSaveValue::MakeInt(End - I));
			for (int32 K = I; K < End; ++K)
			{
				Runs.Add(FSaveValue::MakeInt(C.Delta[K]));
			}
			I = End;
		}
		FSaveValue Entry = FSaveValue::MakeArray();
		Entry.Add(FSaveValue::MakeInt(Key.X));
		Entry.Add(FSaveValue::MakeInt(Key.Y));
		Entry.Add(MoveTemp(Runs));
		ChunkList.Add(MoveTemp(Entry));
	}
	Root.Set(TEXT("chunks"), MoveTemp(ChunkList));

	TArray<FIntPoint> Pending;
	Dirty.GetKeys(Pending);
	Pending.Sort(&SandModelDetail::ColumnLess);
	FSaveValue DirtyList = FSaveValue::MakeArray();
	for (const FIntPoint& Column : Pending)
	{
		DirtyList.Add(FSaveValue::MakeInt(Column.X));
		DirtyList.Add(FSaveValue::MakeInt(Column.Y));
	}
	Root.Set(TEXT("dirty"), MoveTemp(DirtyList));
	Root.Set(TEXT("sea"), FSaveValue::MakeInt(SeaBank));
	return Root;
}

bool FSandModel::FromValue(const FSaveValue& Value)
{
	using namespace SandModelDetail;
	Reset();
	auto Fail = [this]()
	{
		Reset();
		return false;
	};
	if (!Value.IsObject())
	{
		return Fail();
	}
	const FSaveValue* Version = Value.Find(TEXT("v"));
	const FSaveValue* Cell = Value.Find(TEXT("cell"));
	const FSaveValue* Cells = Value.Find(TEXT("n"));
	double CellSize = 0.0;
	int32 N = 0;
	if (!Version || Version->AsInt(-1) != 1 || !Cell || !Cell->TryGetDouble(CellSize)
		|| static_cast<float>(CellSize) != Settings.CellSize || !Cells || !ReadInt32(*Cells, N)
		|| N != Settings.CellsPerChunk)
	{
		return Fail();
	}
	const int32 LocalCount = N * N;

	if (const FSaveValue* ChunkList = Value.Find(TEXT("chunks")))
	{
		if (!ChunkList->IsArray())
		{
			return Fail();
		}
		for (int32 E = 0; E < ChunkList->Num(); ++E)
		{
			const FSaveValue& Entry = ChunkList->At(E);
			FIntPoint Key;
			if (!Entry.IsArray() || Entry.Num() != 3 || !ReadInt32(Entry.At(0), Key.X) || !ReadInt32(Entry.At(1), Key.Y)
				|| !Entry.At(2).IsArray() || Chunks.Contains(Key))
			{
				return Fail();
			}
			FChunk C;
			C.Delta.SetNumZeroed(LocalCount);
			C.Anchor.SetNumZeroed(LocalCount);
			C.Hold.SetNumZeroed(LocalCount);
			const FSaveValue& Runs = Entry.At(2);
			int64 PreviousEnd = 0;
			int32 Pos = 0;
			while (Pos < Runs.Num())
			{
				int32 First = 0;
				int32 Count = 0;
				if (Pos + 1 >= Runs.Num() || !ReadInt32(Runs.At(Pos), First) || !ReadInt32(Runs.At(Pos + 1), Count)
					|| First < PreviousEnd || Count < 1 || static_cast<int64>(First) + Count > LocalCount
					|| Pos + 2 + Count > Runs.Num())
				{
					return Fail();
				}
				for (int32 K = 0; K < Count; ++K)
				{
					int32 Mm = 0;
					// Fuera de [−capa, +montón] no lo ha podido producir el juego.
					if (!ReadInt32(Runs.At(Pos + 2 + K), Mm) || Mm < -MaxDigDepthMm || Mm > MaxPileHeightMm || Mm == 0)
					{
						return Fail();
					}
					C.Delta[First + K] = Mm;
					++C.NonZero;
				}
				PreviousEnd = static_cast<int64>(First) + Count;
				Pos += 2 + Count;
			}
			if (C.NonZero == 0)
			{
				return Fail();
			}
			Chunks.Add(Key, MoveTemp(C));
		}
	}

	if (const FSaveValue* DirtyList = Value.Find(TEXT("dirty")))
	{
		if (!DirtyList->IsArray() || DirtyList->Num() % 2 != 0)
		{
			return Fail();
		}
		for (int32 I = 0; I < DirtyList->Num(); I += 2)
		{
			FIntPoint Column;
			if (!ReadInt32(DirtyList->At(I), Column.X) || !ReadInt32(DirtyList->At(I + 1), Column.Y))
			{
				return Fail();
			}
			Dirty.FindOrAdd(Column);
		}
	}

	if (const FSaveValue* Sea = Value.Find(TEXT("sea")))
	{
		int64 Bank = 0;
		if (!Sea->TryGetInt(Bank))
		{
			return Fail();
		}
		SeaBank = Bank;
	}
	return true;
}

bool FSandModel::operator==(const FSandModel& Other) const
{
	if (Settings.CellSize != Other.Settings.CellSize || Settings.CellsPerChunk != Other.Settings.CellsPerChunk)
	{
		return false;
	}
	const TArray<FIntPoint> Mine = EditedChunks();
	if (Mine != Other.EditedChunks() || Dirty.Num() != Other.Dirty.Num() || SeaBank != Other.SeaBank)
	{
		return false;
	}
	for (const FIntPoint& Key : Mine)
	{
		if (Chunks.FindChecked(Key).Delta != Other.Chunks.FindChecked(Key).Delta)
		{
			return false;
		}
	}
	for (const auto& Pair : Dirty)
	{
		if (!Other.Dirty.Contains(Pair.Key))
		{
			return false;
		}
	}
	return true;
}
