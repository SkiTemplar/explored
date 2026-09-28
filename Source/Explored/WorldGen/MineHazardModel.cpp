#include "WorldGen/MineHazardModel.h"

namespace MineHazardDetail
{
	/** Pesos de chaflán 3D en décimas de celda: cara, arista, vértice. */
	constexpr int32 FaceWeight = 10;
	constexpr int32 EdgeWeight = 14;
	constexpr int32 CornerWeight = 17;

	const FIntVector Faces[6] = {
		FIntVector(-1, 0, 0), FIntVector(1, 0, 0), FIntVector(0, -1, 0),
		FIntVector(0, 1, 0), FIntVector(0, 0, -1), FIntVector(0, 0, 1),
	};

	bool ReadInt64(const FSaveValue& Value, int64& Out)
	{
		return Value.IsInt() && Value.TryGetInt(Out);
	}

	/** Metros enteros en milímetros; acota ficheros manipulados a ±1 000 km. */
	constexpr int64 MaxWorldMm = 1000000000;
}

void FMineHazardModel::AdvanceAir(FMineAirState& State, bool bStale, float Seconds)
{
	if (!FMath::IsFinite(State.Air))
	{
		State.Air = 1.0f;
	}
	if (!FMath::IsFinite(State.StaleSeconds) || State.StaleSeconds < 0.0f)
	{
		State.StaleSeconds = 0.0f;
	}
	if (!FMath::IsFinite(Seconds) || Seconds <= 0.0f)
	{
		State.Air = FMath::Clamp(State.Air, 0.0f, 1.0f);
		return;
	}
	if (bStale)
	{
		const float Before = FMath::Max(0.0f, State.StaleSeconds - StaleAirGraceSeconds);
		State.StaleSeconds += Seconds;
		const float After = FMath::Max(0.0f, State.StaleSeconds - StaleAirGraceSeconds);
		State.Air -= (After - Before) / 60.0f * AirLossPerMinute;
	}
	else
	{
		State.StaleSeconds = 0.0f;
		State.Air += Seconds / 60.0f * AirRecoveryPerMinute;
	}
	State.Air = FMath::Clamp(State.Air, 0.0f, 1.0f);
}

FMineAirSignals FMineHazardModel::AirSignals(const FMineAirState& State)
{
	FMineAirSignals Out;
	const float Air = FMath::IsFinite(State.Air) ? FMath::Clamp(State.Air, 0.0f, 1.0f) : 1.0f;
	Out.bLaboredBreathing = FMath::IsFinite(State.StaleSeconds) && State.StaleSeconds > StaleAirGraceSeconds;
	Out.Dizziness = Air < DizzyBelowAir ? (DizzyBelowAir - Air) / DizzyBelowAir : 0.0f;
	return Out;
}

double FMineHazardModel::MonsoonFloodFraction(bool bFlooding, double HoursSinceFloodEnded)
{
	if (bFlooding)
	{
		return MonsoonFloodShare;
	}
	if (!FMath::IsFinite(HoursSinceFloodEnded) || HoursSinceFloodEnded >= MonsoonDrainHours)
	{
		return 0.0;
	}
	const double Hours = FMath::Max(0.0, HoursSinceFloodEnded);
	return MonsoonFloodShare * (1.0 - Hours / MonsoonDrainHours);
}

FMineHazardModel::FMineHazardModel(const FMineGridSettings& InSettings)
	: Settings(InSettings)
{
	if (!FMath::IsFinite(Settings.CellSize) || Settings.CellSize < 0.05f)
	{
		Settings.CellSize = 0.5f;
	}
	if (!FMath::IsFinite(Settings.Origin.X) || !FMath::IsFinite(Settings.Origin.Y) || !FMath::IsFinite(Settings.Origin.Z))
	{
		Settings.Origin = FVector::ZeroVector;
	}
	Settings.Size.X = FMath::Clamp(Settings.Size.X, 1, 1024);
	Settings.Size.Y = FMath::Clamp(Settings.Size.Y, 1, 1024);
	Settings.Size.Z = FMath::Clamp(Settings.Size.Z, 1, 1024);
	int64 Total = static_cast<int64>(Settings.Size.X) * Settings.Size.Y * Settings.Size.Z;
	while (Total > MaxCells)
	{
		// Caja demasiado grande: se recorta en horizontal, que es donde sobra.
		Settings.Size.X = FMath::Max(1, Settings.Size.X / 2);
		Settings.Size.Y = FMath::Max(1, Settings.Size.Y / 2);
		Total = static_cast<int64>(Settings.Size.X) * Settings.Size.Y * Settings.Size.Z;
	}
	NumCells = static_cast<int32>(Total);
	Cells.Init(static_cast<uint8>(EMineCell::Solid), NumCells);
}

bool FMineHazardModel::IsInside(const FIntVector& Cell) const
{
	return Cell.X >= 0 && Cell.Y >= 0 && Cell.Z >= 0
		&& Cell.X < Settings.Size.X && Cell.Y < Settings.Size.Y && Cell.Z < Settings.Size.Z;
}

int32 FMineHazardModel::Index(const FIntVector& Cell) const
{
	return Cell.X + Settings.Size.X * (Cell.Y + Settings.Size.Y * Cell.Z);
}

FIntVector FMineHazardModel::CellAt(int32 I) const
{
	const int32 X = I % Settings.Size.X;
	const int32 Y = (I / Settings.Size.X) % Settings.Size.Y;
	const int32 Z = I / (Settings.Size.X * Settings.Size.Y);
	return FIntVector(X, Y, Z);
}

FIntVector FMineHazardModel::CellOf(const FVector& P) const
{
	if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y) || !FMath::IsFinite(P.Z))
	{
		return FIntVector(-1, -1, -1);
	}
	const FVector L = (P - Settings.Origin) / static_cast<double>(Settings.CellSize);
	auto Axis = [](double V)
	{
		return static_cast<int32>(FMath::Clamp(FMath::FloorToDouble(V), -1.0e6, 1.0e6));
	};
	return FIntVector(Axis(L.X), Axis(L.Y), Axis(L.Z));
}

FVector FMineHazardModel::CellCenter(const FIntVector& Cell) const
{
	const double H = Settings.CellSize;
	return Settings.Origin + FVector((Cell.X + 0.5) * H, (Cell.Y + 0.5) * H, (Cell.Z + 0.5) * H);
}

EMineCell FMineHazardModel::GetCell(const FIntVector& Cell) const
{
	return IsInside(Cell) ? static_cast<EMineCell>(Cells[Index(Cell)]) : EMineCell::Solid;
}

void FMineHazardModel::SetCell(const FIntVector& Cell, EMineCell State)
{
	if (!IsInside(Cell) || static_cast<uint8>(State) > static_cast<uint8>(EMineCell::Sea))
	{
		return;
	}
	uint8& Slot = Cells[Index(Cell)];
	if (Slot != static_cast<uint8>(State))
	{
		Slot = static_cast<uint8>(State);
		MarkDirty();
	}
}

void FMineHazardModel::FillFromDensity(TFunctionRef<float(const FVector&)> Density, TFunctionRef<bool(const FVector&)> IsSea)
{
	for (int32 I = 0; I < NumCells; ++I)
	{
		const FVector P = CellCenter(CellAt(I));
		const float D = Density(P);
		EMineCell State = EMineCell::Solid;
		if (FMath::IsFinite(D) && D > 0.0f)
		{
			State = IsSea(P) ? EMineCell::Sea : EMineCell::Open;
		}
		Cells[I] = static_cast<uint8>(State);
	}
	MarkDirty();
}

bool FMineHazardModel::IsSupport(const FIntVector& Cell) const
{
	if (!IsInside(Cell))
	{
		// Fuera de la caja es roca, salvo por encima: la capa superior ve el cielo.
		return Cell.Z < Settings.Size.Z;
	}
	const EMineCell State = GetCell(Cell);
	return State == EMineCell::Solid || State == EMineCell::Wall;
}

bool FMineHazardModel::IsRoof(const FIntVector& Cell) const
{
	const FIntVector Above = Cell + FIntVector(0, 0, 1);
	return IsOpen(Cell) && IsInside(Above) && GetCell(Above) == EMineCell::Solid;
}

bool FMineHazardModel::BeamSpan(const FIntVector& Cell, int32& OutBottomZ, int32& OutTopZ) const
{
	if (!IsOpen(Cell))
	{
		return false;
	}
	int32 Bottom = Cell.Z;
	while (IsOpen(FIntVector(Cell.X, Cell.Y, Bottom - 1)))
	{
		--Bottom;
	}
	int32 Top = Cell.Z;
	while (IsOpen(FIntVector(Cell.X, Cell.Y, Top + 1)))
	{
		++Top;
	}
	// Suelo firme (roca o pared, no agua de mar) y techo de roca dentro de la caja.
	const FIntVector Below(Cell.X, Cell.Y, Bottom - 1);
	const EMineCell Floor = GetCell(Below);
	if (Floor == EMineCell::Sea || !IsRoof(FIntVector(Cell.X, Cell.Y, Top)))
	{
		return false;
	}
	const double Height = static_cast<double>(Top - Bottom + 1) * Settings.CellSize;
	if (Height > MaxBeamHeightMeters + 1.0e-4)
	{
		return false;
	}
	OutBottomZ = Bottom;
	OutTopZ = Top;
	return true;
}

bool FMineHazardModel::CanPlaceBeam(const FVector& P) const
{
	const FIntVector Cell = CellOf(P);
	int32 Bottom = 0;
	int32 Top = 0;
	if (!BeamSpan(Cell, Bottom, Top))
	{
		return false;
	}
	for (const FIntVector& Beam : Beams)
	{
		if (Beam.X == Cell.X && Beam.Y == Cell.Y && Beam.Z >= Bottom && Beam.Z <= Top)
		{
			return false;
		}
	}
	return true;
}

bool FMineHazardModel::PlaceBeam(const FVector& P)
{
	if (!CanPlaceBeam(P))
	{
		return false;
	}
	Beams.Add(CellOf(P));
	MarkDirty();
	return true;
}

bool FMineHazardModel::RemoveBeam(const FVector& P)
{
	const FIntVector Cell = CellOf(P);
	for (int32 I = 0; I < Beams.Num(); ++I)
	{
		int32 Bottom = 0;
		int32 Top = 0;
		const bool bSpan = BeamSpan(Beams[I], Bottom, Top);
		const bool bSame = Beams[I] == Cell
			|| (bSpan && Beams[I].X == Cell.X && Beams[I].Y == Cell.Y && Cell.Z >= Bottom && Cell.Z <= Top);
		if (bSame)
		{
			Beams.RemoveAt(I);
			MarkDirty();
			return true;
		}
	}
	return false;
}

void FMineHazardModel::EnsureComputed() const
{
	if (!bDirty)
	{
		return;
	}
	ComputeGalleries();
	ComputeSupport();
	ComputeAir();
	bDirty = false;
}

void FMineHazardModel::ComputeGalleries() const
{
	// El agua de las galerías viejas pasa a las nuevas: al unir dos, gana la más alta; al
	// partir una, las dos mitades se quedan con su nivel (recortado a su suelo y su techo).
	const TArray<int32> OldGallery = MoveTemp(Gallery);
	const TArray<FGallery> OldGalleries = MoveTemp(Galleries);
	Gallery.Init(INDEX_NONE, NumCells);
	Galleries.Reset();

	const double H = Settings.CellSize;
	TArray<int32> Stack;
	for (int32 Start = 0; Start < NumCells; ++Start)
	{
		if (Cells[Start] != static_cast<uint8>(EMineCell::Open) || Gallery[Start] != INDEX_NONE)
		{
			continue;
		}
		const int32 Id = Galleries.Num();
		FGallery G;
		G.FirstCell = Start;
		G.Floor = TNumericLimits<double>::Max();
		G.Top = TNumericLimits<double>::Lowest();
		double Carried = TNumericLimits<double>::Lowest();
		Gallery[Start] = Id;
		Stack.Reset();
		Stack.Add(Start);
		while (Stack.Num() > 0)
		{
			const int32 I = Stack.Pop();
			const FIntVector C = CellAt(I);
			const double Bottom = Settings.Origin.Z + C.Z * H;
			G.Floor = FMath::Min(G.Floor, Bottom);
			G.Top = FMath::Max(G.Top, Bottom + H);
			if (OldGallery.IsValidIndex(I) && OldGallery[I] != INDEX_NONE && OldGalleries.IsValidIndex(OldGallery[I]))
			{
				const FGallery& Old = OldGalleries[OldGallery[I]];
				if (Old.Level > Old.Floor)
				{
					Carried = FMath::Max(Carried, Old.Level);
				}
			}
			for (const FIntVector& F : MineHazardDetail::Faces)
			{
				const FIntVector N = C + F;
				const EMineCell State = GetCell(N);
				if (State == EMineCell::Sea)
				{
					G.bTouchesSea = true;
				}
				else if (State == EMineCell::Open)
				{
					const int32 NI = Index(N);
					if (Gallery[NI] == INDEX_NONE)
					{
						Gallery[NI] = Id;
						Stack.Add(NI);
					}
				}
			}
		}
		G.Level = FMath::Clamp(Carried, G.Floor, G.Top);
		Galleries.Add(G);
	}

	for (const TPair<FVector, double>& Water : PendingWater)
	{
		const FIntVector C = CellOf(Water.Key);
		if (!IsInside(C) || Gallery[Index(C)] == INDEX_NONE)
		{
			continue;
		}
		FGallery& G = Galleries[Gallery[Index(C)]];
		G.Level = FMath::Clamp(FMath::Max(G.Level, Water.Value), G.Floor, G.Top);
	}
	PendingWater.Reset();
}

void FMineHazardModel::ComputeSupport() const
{
	Unsupported.Init(0, NumCells);
	const double H = Settings.CellSize;
	const double Half = MaxSpanMeters * 0.5;
	// Más allá de esta ventana ninguna celda queda a 1,5 m del centro.
	const int32 R = FMath::CeilToInt32(Half / H) + 1;

	// Vigas válidas con su tramo.
	struct FBeamSpan { FIntVector Cell; int32 Bottom; int32 Top; };
	TArray<FBeamSpan> Spans;
	for (const FIntVector& Beam : Beams)
	{
		int32 Bottom = 0;
		int32 Top = 0;
		if (BeamSpan(Beam, Bottom, Top))
		{
			Spans.Add({ Beam, Bottom, Top });
		}
	}

	for (int32 I = 0; I < NumCells; ++I)
	{
		const FIntVector C = CellAt(I);
		if (!IsRoof(C))
		{
			continue;
		}
		bool bSupported = false;
		for (int32 DY = -R; DY <= R && !bSupported; ++DY)
		{
			for (int32 DX = -R; DX <= R && !bSupported; ++DX)
			{
				if ((DX == 0 && DY == 0) || !IsSupport(C + FIntVector(DX, DY, 0)))
				{
					continue;
				}
				// Distancia del centro de la celda al punto más cercano de la celda de apoyo.
				const double AX = FMath::Max(0.0, FMath::Abs(DX) - 0.5) * H;
				const double AY = FMath::Max(0.0, FMath::Abs(DY) - 0.5) * H;
				bSupported = AX * AX + AY * AY <= Half * Half + 1.0e-6;
			}
		}
		for (int32 B = 0; B < Spans.Num() && !bSupported; ++B)
		{
			const FBeamSpan& S = Spans[B];
			if (C.Z < S.Bottom || C.Z > S.Top)
			{
				continue;
			}
			const double DX = static_cast<double>(C.X - S.Cell.X) * H;
			const double DY = static_cast<double>(C.Y - S.Cell.Y) * H;
			bSupported = DX * DX + DY * DY <= Half * Half + 1.0e-6;
		}
		Unsupported[I] = bSupported ? 0 : 1;
	}
}

void FMineHazardModel::ComputeAir() const
{
	using namespace MineHazardDetail;
	AirDistance.Init(INDEX_NONE, NumCells);

	// Salidas: celdas abiertas que ven el cielo en vertical (la columna hasta arriba, abierta).
	TArray<TArray<int32>> Buckets;
	Buckets.SetNum(CornerWeight + 1);
	int32 Pending = 0;
	for (int32 Y = 0; Y < Settings.Size.Y; ++Y)
	{
		for (int32 X = 0; X < Settings.Size.X; ++X)
		{
			for (int32 Z = Settings.Size.Z - 1; Z >= 0; --Z)
			{
				const FIntVector C(X, Y, Z);
				if (!IsOpen(C))
				{
					break;
				}
				AirDistance[Index(C)] = 0;
				Buckets[0].Add(Index(C));
				++Pending;
			}
		}
	}
	// Las fuentes en orden de índice: el recorrido no depende del orden de las columnas.
	Buckets[0].Sort();

	// Dijkstra con cubos (Dial): pesos enteros de 10 a 17.
	int32 Current = 0;
	int32 Empty = 0;
	while (Pending > 0 && Empty <= CornerWeight)
	{
		TArray<int32>& Bucket = Buckets[Current % (CornerWeight + 1)];
		if (Bucket.Num() == 0)
		{
			++Current;
			++Empty;
			continue;
		}
		Empty = 0;
		TArray<int32> Now = MoveTemp(Bucket);
		Bucket.Reset();
		for (const int32 I : Now)
		{
			--Pending;
			if (AirDistance[I] != Current)
			{
				continue;
			}
			const FIntVector C = CellAt(I);
			for (int32 DZ = -1; DZ <= 1; ++DZ)
			{
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					for (int32 DX = -1; DX <= 1; ++DX)
					{
						const int32 Axes = (DX != 0) + (DY != 0) + (DZ != 0);
						if (Axes == 0)
						{
							continue;
						}
						// En diagonal solo si todo el cubo entre las dos celdas es aire: el aire
						// no se cuela por una arista ni por una esquina de roca o de una pared.
						bool bClear = true;
						for (int32 M = 1; M < 8 && bClear; ++M)
						{
							const FIntVector Step((M & 1) ? DX : 0, (M & 2) ? DY : 0, (M & 4) ? DZ : 0);
							if (Step != FIntVector::ZeroValue && !IsOpen(C + Step))
							{
								bClear = false;
							}
						}
						if (!bClear)
						{
							continue;
						}
						const int32 NI = Index(C + FIntVector(DX, DY, DZ));
						const int32 Weight = Axes == 1 ? FaceWeight : (Axes == 2 ? EdgeWeight : CornerWeight);
						const int32 Candidate = Current + Weight;
						if (AirDistance[NI] == INDEX_NONE || Candidate < AirDistance[NI])
						{
							AirDistance[NI] = Candidate;
							Buckets[Candidate % (CornerWeight + 1)].Add(NI);
							++Pending;
						}
					}
				}
			}
		}
		++Current;
	}
}

FMineHazardResult FMineHazardModel::Advance(int32 DeltaMs, const FMineWaterEnvironment& Env)
{
	FMineHazardResult Result;
	if (DeltaMs <= 0)
	{
		return Result;
	}
	const int64 Total = static_cast<int64>(AccumulatedMs) + DeltaMs;
	int64 Ticks = Total / TickMs;
	AccumulatedMs = static_cast<int32>(Total % TickMs);
	if (Ticks > MaxTicksPerAdvance)
	{
		Ticks = MaxTicksPerAdvance;
		AccumulatedMs = 0;
	}
	FMineWaterEnvironment Safe = Env;
	if (!FMath::IsFinite(Safe.SeaLevel))
	{
		Safe.SeaLevel = TNumericLimits<double>::Lowest();
	}
	if (!FMath::IsFinite(Safe.WaterTable))
	{
		Safe.WaterTable = TNumericLimits<double>::Lowest();
	}
	for (int64 T = 0; T < Ticks; ++T)
	{
		Step(Safe, Result);
		++Result.Ticks;
	}
	return Result;
}

void FMineHazardModel::Step(const FMineWaterEnvironment& Env, FMineHazardResult& Result)
{
	const bool bWasDirty = bDirty;
	EnsureComputed();

	// Vigas que se han quedado sin suelo o sin techo (o bajo el escombro).
	if (bWasDirty)
	{
		for (int32 I = Beams.Num() - 1; I >= 0; --I)
		{
			int32 Bottom = 0;
			int32 Top = 0;
			if (!BeamSpan(Beams[I], Bottom, Top))
			{
				Result.Events.Add({ EMineHazardEventKind::BeamLost, CellCenter(Beams[I]), 1 });
				Beams.RemoveAt(I);
				bDirty = true;
			}
		}
		EnsureComputed();
	}

	// --- Derrumbe ---
	TArray<int32> Averted;
	TArray<int32> Keys;
	for (const auto& Pair : Exposure)
	{
		Keys.Add(Pair.Key);
	}
	Keys.Sort();
	for (const int32 I : Keys)
	{
		if (!Unsupported.IsValidIndex(I) || Unsupported[I] == 0)
		{
			// Solo cuenta como salvado si sigue siendo techo: ahora está apoyado (una viga, una
			// pared). Si se ha picado la roca de encima, ya no es el mismo techo.
			if (Cells.IsValidIndex(I) && IsRoof(CellAt(I)))
			{
				Averted.Add(I);
			}
			Exposure.Remove(I);
		}
	}
	if (Averted.Num() > 0)
	{
		FVector Sum = FVector::ZeroVector;
		for (const int32 I : Averted)
		{
			Sum += CellCenter(CellAt(I));
		}
		Result.Events.Add({ EMineHazardEventKind::CollapseAverted, Sum / static_cast<double>(Averted.Num()), Averted.Num() });
	}

	TArray<uint8> Seen;
	Seen.Init(0, NumCells);
	TArray<TArray<int32>> ToCollapse;
	for (int32 Start = 0; Start < NumCells; ++Start)
	{
		if (Unsupported[Start] == 0 || Seen[Start] != 0)
		{
			continue;
		}
		// Hueco: celdas de techo sin apoyo unidas por una cara.
		TArray<int32> Region;
		TArray<int32> Stack;
		Stack.Add(Start);
		Seen[Start] = 1;
		while (Stack.Num() > 0)
		{
			const int32 I = Stack.Pop();
			Region.Add(I);
			const FIntVector C = CellAt(I);
			for (const FIntVector& F : MineHazardDetail::Faces)
			{
				const FIntVector N = C + F;
				if (IsInside(N))
				{
					const int32 NI = Index(N);
					if (Unsupported[NI] != 0 && Seen[NI] == 0)
					{
						Seen[NI] = 1;
						Stack.Add(NI);
					}
				}
			}
		}
		Region.Sort();
		int32 Before = 0;
		int32 After = 0;
		FVector Sum = FVector::ZeroVector;
		for (const int32 I : Region)
		{
			int32& Ms = Exposure.FindOrAdd(I);
			Before = FMath::Max(Before, Ms);
			Ms += TickMs;
			After = FMath::Max(After, Ms);
			Sum += CellCenter(CellAt(I));
		}
		const FVector Center = Sum / static_cast<double>(Region.Num());
		if (Before < CollapseWarningMs && After >= CollapseWarningMs)
		{
			Result.Events.Add({ EMineHazardEventKind::CollapseWarning, Center, Region.Num() });
		}
		if (After >= CollapseDelayMs)
		{
			ToCollapse.Add(MoveTemp(Region));
		}
	}
	for (const TArray<int32>& Region : ToCollapse)
	{
		Collapse(Region, Result);
	}
	if (ToCollapse.Num() > 0)
	{
		EnsureComputed();
	}

	// --- Agua ---
	const double Rate = FloodRiseMetersPerSecond * (Env.bHeavyRain ? HeavyRainFloodFactor : 1.0)
		* (static_cast<double>(TickMs) / 1000.0);
	for (FGallery& G : Galleries)
	{
		double Target = TNumericLimits<double>::Lowest();
		if (G.bTouchesSea)
		{
			Target = FMath::Max(Target, Env.SeaLevel);
		}
		if (G.Floor < Env.WaterTable)
		{
			Target = FMath::Max(Target, Env.WaterTable);
		}
		if (Target == TNumericLimits<double>::Lowest())
		{
			// Sellada: el agua que haya se queda.
			continue;
		}
		Target = FMath::Clamp(Target, G.Floor, G.Top);
		if (G.Level < Target)
		{
			G.Level = FMath::Min(G.Level + Rate, Target);
			Result.bWaterEntering = true;
		}
		else if (G.Level > Target)
		{
			G.Level = FMath::Max(G.Level - Rate, Target);
		}
	}
}

void FMineHazardModel::Collapse(const TArray<int32>& Region, FMineHazardResult& Result)
{
	FVector Sum = FVector::ZeroVector;
	for (const int32 I : Region)
	{
		Sum += CellCenter(CellAt(I));
		Exposure.Remove(I);
	}
	Result.Events.Add({ EMineHazardEventKind::Collapse, Sum / static_cast<double>(Region.Num()), Region.Num() });
	// El techo cae y llena el hueco de escombro hasta el suelo, columna a columna.
	for (const int32 I : Region)
	{
		FIntVector C = CellAt(I);
		while (IsOpen(C))
		{
			Cells[Index(C)] = static_cast<uint8>(EMineCell::Solid);
			Result.CollapsedCells.Add(C);
			C.Z -= 1;
		}
	}
	MarkDirty();
}

bool FMineHazardModel::IsRoofUnsupported(const FIntVector& Cell) const
{
	if (!IsInside(Cell))
	{
		return false;
	}
	EnsureComputed();
	return Unsupported[Index(Cell)] != 0;
}

TArray<FIntVector> FMineHazardModel::UnsupportedRoofCells() const
{
	EnsureComputed();
	TArray<FIntVector> Out;
	for (int32 I = 0; I < NumCells; ++I)
	{
		if (Unsupported[I] != 0)
		{
			Out.Add(CellAt(I));
		}
	}
	return Out;
}

int32 FMineHazardModel::ExposureMs(const FIntVector& Cell) const
{
	if (!IsInside(Cell))
	{
		return 0;
	}
	const int32* Ms = Exposure.Find(Index(Cell));
	return Ms ? *Ms : 0;
}

float FMineHazardModel::AirDistanceMeters(const FIntVector& Cell) const
{
	if (!IsInside(Cell))
	{
		return -1.0f;
	}
	EnsureComputed();
	const int32 D = AirDistance[Index(Cell)];
	return D < 0 ? -1.0f : static_cast<float>(D) / MineHazardDetail::FaceWeight * Settings.CellSize;
}

bool FMineHazardModel::IsStaleAir(const FVector& P) const
{
	const FIntVector Cell = CellOf(P);
	if (!IsInside(Cell) || !IsOpen(Cell))
	{
		return false;
	}
	const float D = AirDistanceMeters(Cell);
	return D < 0.0f || D > StaleAirDistanceMeters + 1.0e-4f;
}

double FMineHazardModel::WaterLevel(const FIntVector& Cell) const
{
	if (!IsInside(Cell))
	{
		return TNumericLimits<double>::Lowest();
	}
	EnsureComputed();
	const int32 G = Gallery[Index(Cell)];
	if (G == INDEX_NONE || Galleries[G].Level <= Galleries[G].Floor)
	{
		return TNumericLimits<double>::Lowest();
	}
	return Galleries[G].Level;
}

double FMineHazardModel::WaterDepthAt(const FVector& P) const
{
	const double Level = WaterLevel(CellOf(P));
	return Level == TNumericLimits<double>::Lowest() ? 0.0 : FMath::Max(0.0, Level - P.Z);
}

int32 FMineHazardModel::NumGalleries() const
{
	EnsureComputed();
	return Galleries.Num();
}

FSaveValue FMineHazardModel::ToValue() const
{
	EnsureComputed();
	FSaveValue Root = FSaveValue::MakeObject();
	Root.Set(TEXT("v"), FSaveValue::MakeInt(1));
	FSaveValue List = FSaveValue::MakeArray();
	for (const FGallery& G : Galleries)
	{
		if (G.Level <= G.Floor)
		{
			continue;
		}
		const FVector P = CellCenter(CellAt(G.FirstCell));
		FSaveValue Row = FSaveValue::MakeArray();
		Row.Add(FSaveValue::MakeInt(static_cast<int64>(FMath::FloorToDouble(P.X * 1000.0 + 0.5))));
		Row.Add(FSaveValue::MakeInt(static_cast<int64>(FMath::FloorToDouble(P.Y * 1000.0 + 0.5))));
		Row.Add(FSaveValue::MakeInt(static_cast<int64>(FMath::FloorToDouble(P.Z * 1000.0 + 0.5))));
		// Hacia arriba al milímetro: un nivel guardado nunca queda por debajo del real.
		Row.Add(FSaveValue::MakeInt(static_cast<int64>(FMath::CeilToDouble(G.Level * 1000.0 - 1.0e-6))));
		List.Add(MoveTemp(Row));
	}
	Root.Set(TEXT("water"), MoveTemp(List));
	return Root;
}

bool FMineHazardModel::FromValue(const FSaveValue& Value)
{
	using namespace MineHazardDetail;
	PendingWater.Reset();
	for (FGallery& G : Galleries)
	{
		G.Level = G.Floor;
	}
	MarkDirty();
	if (!Value.IsObject())
	{
		return false;
	}
	const FSaveValue* Version = Value.Find(TEXT("v"));
	if (!Version || Version->AsInt(-1) != 1)
	{
		return false;
	}
	const FSaveValue* List = Value.Find(TEXT("water"));
	if (!List)
	{
		return true;
	}
	if (!List->IsArray())
	{
		return false;
	}
	TArray<TPair<FVector, double>> Loaded;
	for (int32 I = 0; I < List->Num(); ++I)
	{
		const FSaveValue& Row = List->At(I);
		int64 V[4] = { 0, 0, 0, 0 };
		if (!Row.IsArray() || Row.Num() != 4)
		{
			return false;
		}
		for (int32 K = 0; K < 4; ++K)
		{
			if (!ReadInt64(Row.At(K), V[K]) || V[K] < -MaxWorldMm || V[K] > MaxWorldMm)
			{
				return false;
			}
		}
		Loaded.Add(TPair<FVector, double>(FVector(V[0] * 0.001, V[1] * 0.001, V[2] * 0.001), V[3] * 0.001));
	}
	PendingWater = MoveTemp(Loaded);
	return true;
}
