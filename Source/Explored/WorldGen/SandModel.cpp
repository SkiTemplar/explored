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
		int32 WaveMilli = 0;
		bool bAnchored = false;
		bool bNearAnchor = false;
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

int32 FSandModel::WaveWeightMilli(int64 HeightMm, int64 SeaMm)
{
	const int64 Swash = static_cast<int64>(SwashHeightMeters * 1000.0f);
	const int64 Depth = static_cast<int64>(SwashDepthMeters * 1000.0f);
	const int64 Above = HeightMm - SeaMm;
	if (Above >= Swash || Above < -Depth)
	{
		return 0;
	}
	if (Above <= 0)
	{
		return 1000;
	}
	// Lineal: 1000 en la línea del agua, 0 donde ya no llega la ola.
	return static_cast<int32>((Swash - Above) * 1000 / Swash);
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

int64 FSandModel::HeightMm(const FIntPoint& Column, FBaseHeight Base)
{
	int32 Local = 0;
	const FChunk& C = TouchColumn(Column, Base, Local);
	return static_cast<int64>(C.BaseMm[Local]) + C.Delta[Local];
}

bool FSandModel::IsAnchored(const FIntPoint& Column) const
{
	int32 Local = 0;
	const FChunk* C = FindChunk(Column, Local);
	return C && C->Anchor[Local] > 0;
}

bool FSandModel::NearAnchor(const FIntPoint& Column) const
{
	if (AnchoredColumns == 0)
	{
		return false;
	}
	for (int32 DY = -1; DY <= 1; ++DY)
	{
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			if (IsAnchored(Column + FIntPoint(DX, DY)))
			{
				return true;
			}
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
	// Columnas cuyo punto de rejilla cae dentro de la caja (bordes incluidos).
	const double Cell = Settings.CellSize;
	const FIntPoint Lo(
		static_cast<int32>(-FMath::FloorToInt64(-Min.X / Cell)),
		static_cast<int32>(-FMath::FloorToInt64(-Min.Y / Cell)));
	const FIntPoint Hi(
		static_cast<int32>(FMath::FloorToInt64(Max.X / Cell)),
		static_cast<int32>(FMath::FloorToInt64(Max.Y / Cell)));
	for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
	{
		for (int32 X = Lo.X; X <= Hi.X; ++X)
		{
			const FIntPoint Column(X, Y);
			int32 Local = 0;
			FChunk& C = TouchColumn(Column, Base, Local);
			if (bAnchor)
			{
				if (C.Anchor[Local] < 255)
				{
					AnchoredColumns += C.Anchor[Local] == 0 ? 1 : 0;
					++C.Anchor[Local];
				}
			}
			else if (C.Anchor[Local] > 0)
			{
				--C.Anchor[Local];
				if (C.Anchor[Local] == 0)
				{
					--AnchoredColumns;
					// Sin la estructura, la arena de alrededor puede volver a moverse.
					for (int32 DY = -1; DY <= 1; ++DY)
					{
						for (int32 DX = -1; DX <= 1; ++DX)
						{
							MarkDirtyAround(Column + FIntPoint(DX, DY));
						}
					}
				}
			}
			++Result.ColumnsChanged;
		}
	}
	return Result;
}

void FSandModel::WakeForTide(const FSandEnvironment& Env, FBaseHeight Base)
{
	const int64 SeaMm = SandModelDetail::MetersToMm(Env.SeaLevel);
	if (bHasWoken && Env.bRaining == bLastWakeRaining && FMath::Abs(SeaMm - LastWakeSeaMm) < TideWakeStepMm)
	{
		return;
	}
	bHasWoken = true;
	LastWakeSeaMm = SeaMm;
	bLastWakeRaining = Env.bRaining;

	// La marea o la lluvia cambian qué arena está húmeda y dónde llegan las olas: se
	// revisan las columnas editadas cerca del foco (los chunks sin deltas se saltan).
	const int32 N = Settings.CellsPerChunk;
	const double Reach = Env.ActiveRadius + Settings.ChunkSizeMeters() * 1.5;
	TArray<FIntPoint> Keys;
	Chunks.GetKeys(Keys);
	Keys.Sort(&SandModelDetail::ColumnLess);
	for (const FIntPoint& Key : Keys)
	{
		const FChunk& C = Chunks.FindChecked(Key);
		if (C.NonZero == 0)
		{
			continue;
		}
		const FVector2D Center = ColumnPosition(FIntPoint(Key.X * N + N / 2, Key.Y * N + N / 2));
		if (Env.DistanceToFoci(Center) > Reach)
		{
			continue;
		}
		for (int32 I = 0; I < N * N; ++I)
		{
			if (C.Delta[I] != 0)
			{
				Dirty.FindOrAdd(FIntPoint(Key.X * N + I % N, Key.Y * N + I / N));
			}
		}
	}
	(void)Base;
}

FSandResult FSandModel::Advance(int32 DeltaMs, const FSandEnvironment& Env, FBaseHeight Base)
{
	FSandResult Total;
	if (DeltaMs > 0)
	{
		AccumulatedMs += DeltaMs;
	}
	TArray<FIntPoint> DirtyChunks;
	while (AccumulatedMs >= TickMs && Total.Ticks < MaxTicksPerAdvance)
	{
		AccumulatedMs -= TickMs;
		FSandResult Step = Tick(Env, Base);
		DirtyChunks.Append(Step.DirtyChunks);
		Total.ColumnsChanged += Step.ColumnsChanged;
		Total.ActiveColumns += Step.ActiveColumns;
		Total.DormantColumns = Step.DormantColumns;
		++Total.Ticks;
	}
	// Un paso larguísimo no se come la partida: lo que no cabe espera a la siguiente llamada.
	AccumulatedMs = FMath::Min(AccumulatedMs, TickMs * MaxTicksPerAdvance);
	FinishDirtyChunks(DirtyChunks);
	Total.DirtyChunks = MoveTemp(DirtyChunks);
	return Total;
}

FSandResult FSandModel::Tick(const FSandEnvironment& Env, FBaseHeight Base)
{
	using namespace SandModelDetail;
	FSandResult Result;
	Result.Ticks = 1;
	WakeForTide(Env, Base);

	const int64 SeaMm = MetersToMm(Env.SeaLevel);
	const int64 WetTopMm = SeaMm + static_cast<int64>(WetBandMeters * 1000.0f);
	const int32 DryDrop = ReposeDropMm(DryReposeDeg, Settings.CellSize);
	const int32 WetDrop = ReposeDropMm(WetReposeDeg, Settings.CellSize);
	const int32 AnchoredDrop = ReposeDropMm(AnchoredReposeDeg, Settings.CellSize);

	// 1. Columnas sucias en orden (Y, X): activas cerca del foco, dormidas lejos.
	TArray<FIntPoint> Pending;
	Dirty.GetKeys(Pending);
	Pending.Sort(&ColumnLess);
	struct FCandidate
	{
		FIntPoint Column;
		double Distance;
	};
	TArray<FCandidate> Candidates;
	for (const FIntPoint& Column : Pending)
	{
		const double Distance = Env.DistanceToFoci(ColumnPosition(Column));
		if (Distance <= Env.ActiveRadius)
		{
			Candidates.Add({ Column, Distance });
		}
	}
	if (Candidates.Num() > MaxActiveColumnsPerTick)
	{
		// Tope de coste: primero lo más cercano a un jugador; el resto espera al paso siguiente.
		Candidates.Sort([](const FCandidate& A, const FCandidate& B)
		{
			return A.Distance != B.Distance ? A.Distance < B.Distance : ColumnLess(A.Column, B.Column);
		});
		Candidates.SetNum(MaxActiveColumnsPerTick);
		Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return ColumnLess(A.Column, B.Column); });
	}
	TMap<FIntPoint, uint8> Active;
	TArray<FIntPoint> ActiveList;
	for (const FCandidate& C : Candidates)
	{
		Active.Add(C.Column, 1);
		ActiveList.Add(C.Column);
	}
	Result.ActiveColumns = ActiveList.Num();
	Result.DormantColumns = Pending.Num() - ActiveList.Num();

	// 2. Instantánea de las columnas implicadas (activas y sus vecinas).
	TMap<FIntPoint, FColumnState> States;
	auto StateOf = [&](const FIntPoint& Column) -> const FColumnState&
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
		S.bAnchored = C.Anchor[Local] > 0;
		S.bNearAnchor = NearAnchor(Column);
		S.bWet = Env.bRaining || S.Height <= WetTopMm;
		S.WaveMilli = WaveWeightMilli(S.Height, SeaMm);
		return States.Add(Column, S);
	};

	// 3. Flujos por pareja (positivo: de A a B), cada pareja una sola vez.
	struct FFlow
	{
		FIntPoint A;
		FIntPoint B;
		int64 Amount;
	};
	TArray<FFlow> Flows;
	for (const FIntPoint& A : ActiveList)
	{
		for (const FIntPoint& Offset : Neighbours)
		{
			const FIntPoint B = A + Offset;
			if (Active.Contains(B) && ColumnLess(B, A))
			{
				continue;
			}
			const FColumnState SA = StateOf(A);
			const FColumnState& SB = StateOf(B);
			if (SA.bAnchored || SB.bAnchored)
			{
				continue;
			}
			const bool bHardened = SA.bNearAnchor || SB.bNearAnchor;
			int64 Flow = 0;

			// Avalancha: del lado alto al bajo, solo el exceso sobre el reposo y sobre la base.
			const bool bAHigh = SA.Height >= SB.Height;
			const FColumnState& High = bAHigh ? SA : SB;
			const FColumnState& Low = bAHigh ? SB : SA;
			const int64 Drop = High.Height - Low.Height;
			const int64 Repose = bHardened ? AnchoredDrop : (High.bWet ? WetDrop : DryDrop);
			const int64 Threshold = FMath::Max<int64>(Repose, High.BaseMm - Low.BaseMm);
			const int64 Excess = Drop - Threshold;
			if (Excess > 0)
			{
				const int64 Slide = FMath::Max<int64>(1, Excess / AvalancheDivisor);
				Flow += bAHigh ? Slide : -Slide;
			}

			// Olas: difunden el delta (no la altura), así que devuelven la playa a su perfil.
			int64 WaveMilli = FMath::Min(SA.WaveMilli, SB.WaveMilli);
			if (bHardened)
			{
				WaveMilli = WaveMilli * AnchoredWaveMilli / 1000;
			}
			if (WaveMilli > 0)
			{
				const int64 Rate = static_cast<int64>(WaveRateMilliAtShore) * WaveMilli;
				Flow += (static_cast<int64>(SA.Delta) - SB.Delta) * Rate / 1000000;
			}

			if (Flow != 0)
			{
				Flows.Add({ A, B, Flow });
			}
		}
	}

	// 4. Nadie da más arena de la que tiene sobre la roca (reparto proporcional truncado).
	// Con los números actuales no llega a pasar (cada salida es como mucho 1/5 del exceso
	// y las olas un 8 % por vecina), pero protege ante cambios de números o datos cargados.
	TMap<FIntPoint, int64> Outflow;
	for (const FFlow& F : Flows)
	{
		Outflow.FindOrAdd(F.Amount > 0 ? F.A : F.B) += FMath::Abs(F.Amount);
	}
	TMap<FIntPoint, int64> Change;
	for (FFlow& F : Flows)
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

	// 5. Aplicar. Las activas que no se han movido ni tienen vecinas movidas se duermen.
	// Se rehace el conjunto en vez de quitar una a una: quitar de un TMap grande no es gratis
	// (en el host medía 26 ms por paso con 1300 columnas activas; así, 1,6 ms).
	TMap<FIntPoint, uint8> NextDirty;
	for (const FIntPoint& Column : Pending)
	{
		if (!Active.Contains(Column))
		{
			NextDirty.Add(Column, 1);
		}
	}
	Dirty = MoveTemp(NextDirty);
	TArray<FIntPoint> Changed;
	for (const auto& Pair : Change)
	{
		if (Pair.Value != 0)
		{
			Changed.Add(Pair.Key);
		}
	}
	Changed.Sort(&ColumnLess);
	TArray<FIntPoint> DirtyChunks;
	for (const FIntPoint& Column : Changed)
	{
		AddDelta(Column, static_cast<int32>(Change.FindChecked(Column)), Base);
		MarkDirtyAround(Column);
		ChunksReadingColumn(Column, DirtyChunks);
	}
	Result.ColumnsChanged = Changed.Num();
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
	AnchoredColumns = 0;
	AccumulatedMs = 0;
	LastWakeSeaMm = 0;
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
	return true;
}

bool FSandModel::operator==(const FSandModel& Other) const
{
	if (Settings.CellSize != Other.Settings.CellSize || Settings.CellsPerChunk != Other.Settings.CellsPerChunk)
	{
		return false;
	}
	const TArray<FIntPoint> Mine = EditedChunks();
	if (Mine != Other.EditedChunks() || Dirty.Num() != Other.Dirty.Num())
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
