#include "WorldGen/TerrainEditModel.h"

namespace TerrainEditDetail
{
	/** Paso de SplitMix64: semilla → forma del golpe, sin depender de ningún generador global. */
	uint64 SplitMix(uint64& State)
	{
		State += 0x9E3779B97F4A7C15ull;
		uint64 Z = State;
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		return Z ^ (Z >> 31);
	}

	float Unit(uint64& State)
	{
		return static_cast<float>(SplitMix(State) >> 40) / static_cast<float>(1ull << 24);
	}

	/** Tres lóbulos senoidales sobre la dirección: deforman el radio del pincel en [-1, 1]. */
	struct FLobes
	{
		FVector Axis[3];
		float Phase[3];

		explicit FLobes(uint32 Seed)
		{
			uint64 State = static_cast<uint64>(Seed) * 0x2545F4914F6CDD1Dull + 0x51ED27A1ull;
			for (int32 I = 0; I < 3; ++I)
			{
				const float Z = 2.0f * Unit(State) - 1.0f;
				const float Phi = 2.0f * PI * Unit(State);
				const float R = FMath::Sqrt(FMath::Max(0.0f, 1.0f - Z * Z));
				Axis[I] = FVector(R * FMath::Cos(Phi), R * FMath::Sin(Phi), Z);
				Phase[I] = 2.0f * PI * Unit(State);
			}
		}

		float Eval(const FVector& Dir) const
		{
			static constexpr float Frequency[3] = { 3.0f, 5.0f, 7.0f };
			float Sum = 0.0f;
			for (int32 I = 0; I < 3; ++I)
			{
				Sum += FMath::Sin(Frequency[I] * static_cast<float>(Dir | Axis[I]) + Phase[I]);
			}
			return Sum / 3.0f;
		}
	};

	int32 FloorDiv(int32 A, int32 B)
	{
		const int32 Q = A / B;
		return (A % B != 0 && ((A < 0) != (B < 0))) ? Q - 1 : Q;
	}

	bool ChunkLess(const FIntVector& A, const FIntVector& B)
	{
		if (A.Z != B.Z) { return A.Z < B.Z; }
		if (A.Y != B.Y) { return A.Y < B.Y; }
		return A.X < B.X;
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

	float SnapTo(float Value, float Step)
	{
		return FMath::RoundToFloat(Value / Step) * Step;
	}
}

// ---------------------------------------------------------------------------
// Datos de diseño
// ---------------------------------------------------------------------------

const FTerrainMaterialInfo& FTerrainEditModel::MaterialInfo(ETerrainMaterial Material)
{
	static const FTerrainMaterialInfo Table[] = {
		{ 0.75f, 1 }, // Arena
		{ 1.0f, 1 },  // Tierra
		{ 2.0f, 2 },  // Caliza
		{ 3.0f, 3 },  // Basalto
		{ 4.0f, 4 },  // Obsidiana
	};
	const int32 Index = FMath::Clamp(static_cast<int32>(Material), 0, static_cast<int32>(ETerrainMaterial::Count) - 1);
	return Table[Index];
}

float FTerrainEditModel::ToolFactor(ETerrainMaterial Material, int32 ToolTier)
{
	const FTerrainMaterialInfo& Info = MaterialInfo(Material);
	if (ToolTier < Info.MinToolTier)
	{
		return 0.0f;
	}
	return FMath::Pow(TierBonus, static_cast<float>(ToolTier - Info.MinToolTier)) / Info.Hardness;
}

float FTerrainEditModel::DesignHitsPerCubicMeter(ETerrainMaterial Material, int32 ToolTier)
{
	const float Factor = ToolFactor(Material, ToolTier);
	return Factor > 0.0f ? FMath::Max(6.0f / Factor, MinHitsPerCubicMeter) : 0.0f;
}

float FTerrainEditModel::Occupancy(float Density, float CellSize)
{
	return FMath::Clamp(0.5f - Density / CellSize, 0.0f, 1.0f);
}

bool FTerrainEditModel::SnapStairs(const FStairCarve& In, FStairCarve& Out)
{
	const FVector2D Flat(In.Direction.X, In.Direction.Y);
	if (Flat.Size() < 1.0e-3)
	{
		return false;
	}
	// Ocho rumbos, como el kit de construcción: la escalera encaja con muros y suelos.
	const double Angle = FMath::Atan2(Flat.Y, Flat.X);
	const int32 Octant = ((FMath::RoundToInt32(Angle / (PI * 0.25)) % 8) + 8) % 8;
	static const double D = 0.70710678118654752;
	static const FVector Directions[8] = {
		FVector(1, 0, 0), FVector(D, D, 0), FVector(0, 1, 0), FVector(-D, D, 0),
		FVector(-1, 0, 0), FVector(-D, -D, 0), FVector(0, -1, 0), FVector(D, -D, 0),
	};

	Out = In;
	Out.Direction = Directions[Octant];
	Out.Start = FVector(
		TerrainEditDetail::SnapTo(In.Start.X, StairGrid),
		TerrainEditDetail::SnapTo(In.Start.Y, StairGrid),
		TerrainEditDetail::SnapTo(In.Start.Z, StairGrid));
	const float HalfGrid = StairGrid * 0.5f;
	const float Rise = FMath::Clamp(TerrainEditDetail::SnapTo(FMath::Abs(In.StepRise), HalfGrid), HalfGrid, 3.0f * HalfGrid);
	Out.StepRise = In.StepRise < 0.0f ? -Rise : Rise;
	Out.StepRun = FMath::Clamp(TerrainEditDetail::SnapTo(In.StepRun, HalfGrid), StairGrid, 3.0f * StairGrid);
	Out.NumSteps = FMath::Clamp(In.NumSteps, 1, 64);
	Out.Width = FMath::Clamp(In.Width, 0.6f, 3.0f);
	Out.Headroom = FMath::Clamp(In.Headroom, 1.8f, 3.0f);
	return true;
}

// ---------------------------------------------------------------------------
// Rejilla
// ---------------------------------------------------------------------------

FTerrainEditModel::FTerrainEditModel(const FTerrainEditSettings& InSettings)
	: Settings(InSettings)
{
	check(Settings.CellSize > 0.0f && Settings.CellsPerChunk >= 2);
}

FVector FTerrainEditModel::SamplePosition(const FIntVector& Global) const
{
	return FVector(Global.X, Global.Y, Global.Z) * static_cast<double>(Settings.CellSize);
}

FIntVector FTerrainEditModel::ChunkOfSample(const FIntVector& Global) const
{
	const int32 N = Settings.CellsPerChunk;
	return FIntVector(TerrainEditDetail::FloorDiv(Global.X, N), TerrainEditDetail::FloorDiv(Global.Y, N),
		TerrainEditDetail::FloorDiv(Global.Z, N));
}

void FTerrainEditModel::ChunksReadingSample(const FIntVector& Global, TArray<FIntVector>& Out) const
{
	const int32 N = Settings.CellsPerChunk;
	const FIntVector Owner = ChunkOfSample(Global);
	// Un chunk C lee [C·N − 1, C·N + N]: la primera muestra de un chunk también la lee el
	// anterior y la última, el siguiente.
	int32 Candidates[3][2];
	int32 Count[3];
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const int32 G = Global[Axis];
		const int32 C = Owner[Axis];
		const int32 Local = G - C * N;
		Candidates[Axis][0] = C;
		Count[Axis] = 1;
		if (Local == 0)
		{
			Candidates[Axis][Count[Axis]++] = C - 1;
		}
		else if (Local == N - 1)
		{
			Candidates[Axis][Count[Axis]++] = C + 1;
		}
	}
	for (int32 Z = 0; Z < Count[2]; ++Z)
	{
		for (int32 Y = 0; Y < Count[1]; ++Y)
		{
			for (int32 X = 0; X < Count[0]; ++X)
			{
				Out.AddUnique(FIntVector(Candidates[0][X], Candidates[1][Y], Candidates[2][Z]));
			}
		}
	}
}

int32 FTerrainEditModel::GetDeltaMm(const FIntVector& Global) const
{
	const int32 N = Settings.CellsPerChunk;
	const FIntVector Chunk = ChunkOfSample(Global);
	const TMap<int32, int32>* Deltas = Chunks.Find(Chunk);
	if (!Deltas)
	{
		return 0;
	}
	const FIntVector L = Global - Chunk * N;
	const int32* Value = Deltas->Find(L.X + N * (L.Y + N * L.Z));
	return Value ? *Value : 0;
}

void FTerrainEditModel::SetDeltaMm(const FIntVector& Global, int32 DeltaMm)
{
	const int32 N = Settings.CellsPerChunk;
	const FIntVector Chunk = ChunkOfSample(Global);
	const FIntVector L = Global - Chunk * N;
	const int32 Local = L.X + N * (L.Y + N * L.Z);
	if (DeltaMm != 0)
	{
		Chunks.FindOrAdd(Chunk).Add(Local, DeltaMm);
		return;
	}
	if (TMap<int32, int32>* Deltas = Chunks.Find(Chunk))
	{
		Deltas->Remove(Local);
		if (Deltas->Num() == 0)
		{
			Chunks.Remove(Chunk);
		}
	}
}

float FTerrainEditModel::SampleDelta(const FIntVector& Global) const
{
	return static_cast<float>(GetDeltaMm(Global)) * 0.001f;
}

float FTerrainEditModel::SampleDensity(const FIntVector& Global, FBaseDensity Base) const
{
	return Base(SamplePosition(Global)) + static_cast<float>(GetDeltaMm(Global)) * 0.001f;
}

float FTerrainEditModel::Density(const FVector& P, FBaseDensity Base) const
{
	const double H = Settings.CellSize;
	const FVector Q = P / H;
	const FIntVector G0(FMath::FloorToInt32(Q.X), FMath::FloorToInt32(Q.Y), FMath::FloorToInt32(Q.Z));
	const FVector F = Q - FVector(G0.X, G0.Y, G0.Z);
	float Delta = 0.0f;
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FIntVector O(Corner & 1, (Corner >> 1) & 1, (Corner >> 2) & 1);
		const int32 Mm = GetDeltaMm(G0 + O);
		if (Mm != 0)
		{
			const double W = (O.X ? F.X : 1.0 - F.X) * (O.Y ? F.Y : 1.0 - F.Y) * (O.Z ? F.Z : 1.0 - F.Z);
			Delta += static_cast<float>(W * Mm * 0.001);
		}
	}
	return Base(P) + Delta;
}

void FTerrainEditModel::ForEachSampleInBox(const FBox& Box,
	TFunctionRef<void(const FIntVector&, const FVector&)> Visit) const
{
	const double H = Settings.CellSize;
	const FIntVector Min(FMath::CeilToInt32(Box.Min.X / H), FMath::CeilToInt32(Box.Min.Y / H), FMath::CeilToInt32(Box.Min.Z / H));
	const FIntVector Max(FMath::FloorToInt32(Box.Max.X / H), FMath::FloorToInt32(Box.Max.Y / H), FMath::FloorToInt32(Box.Max.Z / H));
	for (int32 Z = Min.Z; Z <= Max.Z; ++Z)
	{
		for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
		{
			for (int32 X = Min.X; X <= Max.X; ++X)
			{
				const FIntVector G(X, Y, Z);
				Visit(G, SamplePosition(G));
			}
		}
	}
}

double FTerrainEditModel::SolidVolume(const FBox& Box, FBaseDensity Base) const
{
	double Sum = 0.0;
	ForEachSampleInBox(Box, [&](const FIntVector& G, const FVector&)
	{
		Sum += Occupancy(SampleDensity(G, Base), Settings.CellSize);
	});
	return Sum * FMath::Cube(static_cast<double>(Settings.CellSize));
}

void FTerrainEditModel::BuildChunkGrid(const FIntVector& Chunk, FBaseDensity Base, FDensityGrid& Out) const
{
	const int32 N = Settings.CellsPerChunk;
	const FIntVector First = Chunk * N - FIntVector(1);
	Out.Init(FIntVector(N + 2), SamplePosition(First), Settings.CellSize);
	for (int32 Z = 0; Z < Out.Dims.Z; ++Z)
	{
		for (int32 Y = 0; Y < Out.Dims.Y; ++Y)
		{
			for (int32 X = 0; X < Out.Dims.X; ++X)
			{
				Out.Set(X, Y, Z, SampleDensity(First + FIntVector(X, Y, Z), Base));
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Propuestas, presupuesto de volumen y escritura
// ---------------------------------------------------------------------------

double FTerrainEditModel::ProposalVolume(const TArray<FProposal>& Proposals, float S) const
{
	double Sum = 0.0;
	for (const FProposal& P : Proposals)
	{
		const float From = Band(P.Old);
		Sum += Occupancy(From + S * (P.New - From), Settings.CellSize) - Occupancy(P.Old, Settings.CellSize);
	}
	return Sum * FMath::Cube(static_cast<double>(Settings.CellSize));
}

float FTerrainEditModel::ScaleToVolume(const TArray<FProposal>& Proposals, double Limit) const
{
	if (FMath::Abs(ProposalVolume(Proposals, 1.0f)) <= Limit)
	{
		return 1.0f;
	}
	// Todas las propuestas de un lote van en el mismo sentido: |volumen| crece con S.
	float Lo = 0.0f;
	float Hi = 1.0f;
	for (int32 Iteration = 0; Iteration < 24; ++Iteration)
	{
		const float Mid = 0.5f * (Lo + Hi);
		if (FMath::Abs(ProposalVolume(Proposals, Mid)) <= Limit)
		{
			Lo = Mid;
		}
		else
		{
			Hi = Mid;
		}
	}
	return Lo;
}

void FTerrainEditModel::Commit(const TArray<FProposal>& Proposals, float Scale, FTerrainEditResult& Result)
{
	const double CellVolume = FMath::Cube(static_cast<double>(Settings.CellSize));
	for (const FProposal& P : Proposals)
	{
		const float From = Band(P.Old);
		const float Value = From + Scale * (P.New - From);
		if (FMath::Abs(Band(Value) - Band(P.Old)) < 1.0e-6f)
		{
			continue;
		}
		const int32 OldMm = GetDeltaMm(P.Global);
		const int32 NewMm = FMath::Clamp(FMath::RoundToInt32((Value - P.Base) * 1000.0f), -MaxDeltaMm, MaxDeltaMm);
		if (NewMm == OldMm)
		{
			continue;
		}
		SetDeltaMm(P.Global, NewMm);
		const float Realized = P.Base + static_cast<float>(NewMm) * 0.001f;
		const double Volume = (Occupancy(Realized, Settings.CellSize) - Occupancy(P.Old, Settings.CellSize)) * CellVolume;
		if (Volume < 0.0)
		{
			Result.VolumeRemoved -= Volume;
		}
		else
		{
			Result.VolumeAdded += Volume;
		}
		++Result.SamplesChanged;
		ChunksReadingSample(P.Global, Result.DirtyChunks);
	}
}

// ---------------------------------------------------------------------------
// Caminos
// ---------------------------------------------------------------------------

FIntPoint FTerrainEditModel::ColumnOf(double X, double Y) const
{
	return FIntPoint(FMath::FloorToInt32(X / Settings.CellSize), FMath::FloorToInt32(Y / Settings.CellSize));
}

int32 FTerrainEditModel::Compaction(double X, double Y) const
{
	const int32* Value = PathColumns.Find(ColumnOf(X, Y));
	return Value ? *Value : 0;
}

float FTerrainEditModel::HardnessAt(ETerrainMaterial Material, const FVector& P) const
{
	return MaterialInfo(Material).Hardness * (1.0f + CompactedHardnessBonus * static_cast<float>(Compaction(P.X, P.Y)) / 100.0f);
}

void FTerrainEditModel::AddCompaction(const FVector& Center, float Radius, int32 Amount)
{
	const double H = Settings.CellSize;
	const FIntPoint Min = ColumnOf(Center.X - Radius, Center.Y - Radius);
	const FIntPoint Max = ColumnOf(Center.X + Radius, Center.Y + Radius);
	for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
	{
		for (int32 X = Min.X; X <= Max.X; ++X)
		{
			const FVector2D ColumnCenter((X + 0.5) * H, (Y + 0.5) * H);
			if (FVector2D::Distance(ColumnCenter, FVector2D(Center.X, Center.Y)) <= Radius)
			{
				int32& Value = PathColumns.FindOrAdd(FIntPoint(X, Y));
				Value = FMath::Min(100, Value + Amount);
			}
		}
	}
}

void FTerrainEditModel::ClearCompaction(const TArray<FProposal>& Proposals)
{
	// Picar o echar tierra encima deshace el camino de esa columna.
	for (const FProposal& P : Proposals)
	{
		PathColumns.Remove(FIntPoint(P.Global.X, P.Global.Y));
	}
}

// ---------------------------------------------------------------------------
// Herramientas
// ---------------------------------------------------------------------------

namespace TerrainEditDetail
{
	void Finish(FTerrainEditResult& Result)
	{
		Result.DirtyChunks.Sort(&ChunkLess);
	}
}

FTerrainEditResult FTerrainEditModel::Pickaxe(const FPickaxeHit& Hit, FBaseDensity Base)
{
	FTerrainEditResult Result;
	const FTerrainMaterialInfo& Info = MaterialInfo(Hit.Material);
	if (Hit.ToolTier < Info.MinToolTier)
	{
		Result.bRejected = true;
		return Result;
	}
	const float Strength = PickaxeStrength * FMath::Pow(TierBonus, static_cast<float>(Hit.ToolTier - Info.MinToolTier))
		/ HardnessAt(Hit.Material, Hit.ImpactPoint);
	const FVector Dir = Hit.Direction.SizeSquared() > 1.0e-8 ? Hit.Direction.GetSafeNormal() : FVector(0.0, 0.0, -1.0);
	const FVector Center = Hit.ImpactPoint + Dir * PickaxeBite;
	const float Nominal = FMath::IsFinite(Hit.Radius) ? FMath::Clamp(Hit.Radius, MinPickaxeRadius, MaxPickaxeRadius) : PickaxeRadius;
	const float MaxRadius = Nominal * (1.0f + PickaxeIrregularity);
	const TerrainEditDetail::FLobes Lobes(Hit.Seed);

	TArray<FProposal> Proposals;
	ForEachSampleInBox(FBox(Center - FVector(MaxRadius), Center + FVector(MaxRadius)),
		[&](const FIntVector& G, const FVector& P)
		{
			const FVector Offset = P - Center;
			const float Dist = static_cast<float>(Offset.Size());
			const FVector Unit = Dist > 1.0e-6f ? Offset / Dist : FVector(0.0, 0.0, 1.0);
			const float Radius = Nominal * (1.0f + PickaxeIrregularity * Lobes.Eval(Unit));
			if (Dist >= Radius)
			{
				return;
			}
			const float BaseValue = Base(P);
			const float Old = BaseValue + static_cast<float>(GetDeltaMm(G)) * 0.001f;
			if (Old >= BandMax())
			{
				return;
			}
			const float T = Dist / Radius;
			const float Falloff = FMath::Square(1.0f - T * T);
			const float New = FMath::Min(Band(Old) + Strength * Falloff, BandMax());
			if (New > Band(Old))
			{
				Proposals.Add({ G, BaseValue, Old, New });
			}
		});

	Commit(Proposals, 1.0f, Result);
	ClearCompaction(Proposals);
	TerrainEditDetail::Finish(Result);
	return Result;
}

FTerrainEditResult FTerrainEditModel::Shovel(const FShovelStroke& Stroke, FBaseDensity Base)
{
	FTerrainEditResult Result;
	const float Factor = ToolFactor(Stroke.Material, Stroke.ToolTier);
	if (Factor <= 0.0f)
	{
		Result.bRejected = true;
		return Result;
	}
	const FVector N = Stroke.PlaneNormal.SizeSquared() > 1.0e-8 ? Stroke.PlaneNormal.GetSafeNormal() : FVector(0.0, 0.0, 1.0);
	const float Outer = Stroke.Radius + FMath::Max(0.0f, Stroke.EdgeWidth);
	const float Reach = FMath::Max(Settings.CellSize, Stroke.VerticalReach);
	const float MaxChange = ShovelMaxChange * Factor;
	const float Extent = Outer + Reach;

	TArray<FProposal> Cut;
	TArray<FProposal> Fill;
	ForEachSampleInBox(FBox(Stroke.Center - FVector(Extent), Stroke.Center + FVector(Extent)),
		[&](const FIntVector& G, const FVector& P)
		{
			const FVector Offset = P - Stroke.Center;
			const float Height = static_cast<float>(Offset | N);
			if (FMath::Abs(Height) > Reach)
			{
				return;
			}
			const float Lateral = static_cast<float>((Offset - N * Height).Size());
			if (Lateral >= Outer)
			{
				return;
			}
			float Weight = 1.0f;
			if (Lateral > Stroke.Radius)
			{
				const float S = (Lateral - Stroke.Radius) / (Outer - Stroke.Radius);
				Weight = 1.0f - S * S * (3.0f - 2.0f * S);
			}
			const float BaseValue = Base(P);
			const float Old = BaseValue + static_cast<float>(GetDeltaMm(G)) * 0.001f;
			const float From = Band(Old);
			const float Change = FMath::Clamp(Weight * (Band(Height) - From), -MaxChange, MaxChange);
			if (FMath::Abs(Change) < 1.0e-5f)
			{
				return;
			}
			(Change > 0.0f ? Cut : Fill).Add({ G, BaseValue, Old, From + Change });
		});

	// Lo que se rellena sale de lo que se lleva más lo que corta esta misma pasada.
	const double CutVolume = -ProposalVolume(Cut, 1.0f);
	const double Allowed = FMath::Max(0.0, Stroke.SoilBudget) + CutVolume;
	Commit(Cut, 1.0f, Result);
	Commit(Fill, ScaleToVolume(Fill, Allowed), Result);

	if (Stroke.bMarkPath)
	{
		AddCompaction(Stroke.Center, Stroke.Radius, CompactionPerStroke);
		// El camino cambia la capa de superficie: remallar los chunks de toda la huella compactada
		// aunque no cambie la forma (la huella puede cruzar el borde de un chunk).
		const FIntPoint Min = ColumnOf(Stroke.Center.X - Stroke.Radius, Stroke.Center.Y - Stroke.Radius);
		const FIntPoint Max = ColumnOf(Stroke.Center.X + Stroke.Radius, Stroke.Center.Y + Stroke.Radius);
		const int32 Z = FMath::RoundToInt32(Stroke.Center.Z / Settings.CellSize);
		for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
		{
			for (int32 X = Min.X; X <= Max.X; ++X)
			{
				ChunksReadingSample(FIntVector(X, Y, Z), Result.DirtyChunks);
			}
		}
	}
	TerrainEditDetail::Finish(Result);
	return Result;
}

FTerrainEditResult FTerrainEditModel::CompactStrip(const FVector& A, const FVector& B, float HalfWidth)
{
	FTerrainEditResult Result;
	if (!FMath::IsFinite(HalfWidth) || HalfWidth <= 0.0f || !FMath::IsFinite(A.X) || !FMath::IsFinite(A.Y)
		|| !FMath::IsFinite(A.Z) || !FMath::IsFinite(B.X) || !FMath::IsFinite(B.Y) || !FMath::IsFinite(B.Z))
	{
		return Result;
	}
	const double H = Settings.CellSize;
	const FVector2D PA(A.X, A.Y);
	const FVector2D PB(B.X, B.Y);
	const FVector2D AB = PB - PA;
	const double Len2 = AB.SizeSquared();
	const FIntPoint Min = ColumnOf(FMath::Min(A.X, B.X) - HalfWidth, FMath::Min(A.Y, B.Y) - HalfWidth);
	const FIntPoint Max = ColumnOf(FMath::Max(A.X, B.X) + HalfWidth, FMath::Max(A.Y, B.Y) + HalfWidth);
	for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
	{
		for (int32 X = Min.X; X <= Max.X; ++X)
		{
			const FVector2D C((X + 0.5) * H, (Y + 0.5) * H);
			const double T = Len2 > 1.0e-12 ? FMath::Clamp(FVector2D::DotProduct(C - PA, AB) / Len2, 0.0, 1.0) : 0.0;
			if (FVector2D::Distance(C, PA + AB * T) > HalfWidth + 1.0e-6)
			{
				continue;
			}
			PathColumns.FindOrAdd(FIntPoint(X, Y)) = 100;
			const int32 Z = FMath::RoundToInt32((A.Z + (B.Z - A.Z) * T) / H);
			ChunksReadingSample(FIntVector(X, Y, Z), Result.DirtyChunks);
		}
	}
	TerrainEditDetail::Finish(Result);
	return Result;
}

FTerrainEditResult FTerrainEditModel::PlaceSoil(const FSoilPlacement& Placement, FBaseDensity Base)
{
	FTerrainEditResult Result;
	if (Placement.SoilBudget <= 0.0 || Placement.Radius <= 0.0f)
	{
		return Result;
	}
	const float Reach = Placement.Radius + Settings.CellSize;
	TArray<FProposal> Proposals;
	ForEachSampleInBox(FBox(Placement.Center - FVector(Reach), Placement.Center + FVector(Reach)),
		[&](const FIntVector& G, const FVector& P)
		{
			const float Dist = static_cast<float>((P - Placement.Center).Size());
			if (Dist >= Reach)
			{
				return;
			}
			const float BaseValue = Base(P);
			const float Old = BaseValue + static_cast<float>(GetDeltaMm(G)) * 0.001f;
			const float Target = Band(Dist - Placement.Radius);
			if (Target < Band(Old))
			{
				Proposals.Add({ G, BaseValue, Old, Target });
			}
		});
	Commit(Proposals, ScaleToVolume(Proposals, Placement.SoilBudget), Result);
	ClearCompaction(Proposals);
	TerrainEditDetail::Finish(Result);
	return Result;
}

FTerrainEditResult FTerrainEditModel::CarveStairs(const FStairCarve& Stairs, FBaseDensity Base)
{
	FTerrainEditResult Result;
	const FVector Flat(Stairs.Direction.X, Stairs.Direction.Y, 0.0);
	if (ToolFactor(Stairs.Material, Stairs.ToolTier) <= 0.0f || Flat.SizeSquared() < 1.0e-6
		|| Stairs.NumSteps < 1 || Stairs.StepRun <= 0.0f || Stairs.Width <= 0.0f || Stairs.Headroom <= 0.0f)
	{
		Result.bRejected = true;
		return Result;
	}

	// Una escalera que baja es la misma que sube vista desde el final.
	const int32 Steps = Stairs.NumSteps;
	const float Run = Stairs.StepRun;
	const float Length = Steps * Run;
	FVector Dir = Flat.GetSafeNormal();
	FVector Start = Stairs.Start;
	float Rise = Stairs.StepRise;
	if (Rise < 0.0f)
	{
		Start = Start + Dir * Length + FVector(0.0, 0.0, Rise * (Steps - 1));
		Dir = -Dir;
		Rise = -Rise;
	}
	const FVector Side(-Dir.Y, Dir.X, 0.0);
	const float Z0 = static_cast<float>(Start.Z);
	const float HalfWidth = Stairs.Width * 0.5f;
	const float H = Settings.CellSize;

	FBox Box(ForceInit);
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		const float U = (Corner & 1) ? Length + H : -H;
		const float V = (Corner & 2) ? HalfWidth + H : -HalfWidth - H;
		const FVector P = Start + Dir * U + Side * V;
		Box += FVector(P.X, P.Y, Z0 - H);
		Box += FVector(P.X, P.Y, Z0 + Rise * (Steps - 1) + Stairs.Headroom + H);
	}

	TArray<FProposal> Proposals;
	ForEachSampleInBox(Box, [&](const FIntVector& G, const FVector& P)
	{
		const FVector Rel = P - Start;
		const float U = static_cast<float>(Rel | Dir);
		const float V = static_cast<float>(Rel | Side);
		const float W = static_cast<float>(P.Z);
		// Aire sobre el perfil escalonado: intersección de «antes de la contrahuella J o
		// por encima de la huella J». Da contrahuellas verticales y huellas planas.
		// La huella 0 no tiene contrahuella delante: es el suelo del arranque.
		float Profile = W - Z0;
		for (int32 J = 1; J < Steps; ++J)
		{
			Profile = FMath::Min(Profile, FMath::Max(J * Run - U, W - (Z0 + J * Rise)));
		}
		const int32 Step = FMath::Clamp(FMath::FloorToInt32(U / Run), 0, Steps - 1);
		const float Ceiling = Z0 + Step * Rise + Stairs.Headroom - W;
		const float Target = FMath::Min(FMath::Min(Profile, Ceiling), FMath::Min(FMath::Min(U, Length - U), HalfWidth - FMath::Abs(V)));
		if (Target <= -H)
		{
			return;
		}
		const float BaseValue = Base(P);
		const float Old = BaseValue + static_cast<float>(GetDeltaMm(G)) * 0.001f;
		const float New = Band(Target);
		if (New > Band(Old))
		{
			Proposals.Add({ G, BaseValue, Old, New });
		}
	});

	const float Scale = Stairs.MaxVolume > 0.0 ? ScaleToVolume(Proposals, Stairs.MaxVolume) : 1.0f;
	Commit(Proposals, Scale, Result);
	ClearCompaction(Proposals);
	TerrainEditDetail::Finish(Result);
	return Result;
}

// ---------------------------------------------------------------------------
// Estado, guardado y comparación
// ---------------------------------------------------------------------------

TArray<FIntVector> FTerrainEditModel::EditedChunks() const
{
	TArray<FIntVector> Out;
	for (const auto& Pair : Chunks)
	{
		Out.Add(Pair.Key);
	}
	Out.Sort(&TerrainEditDetail::ChunkLess);
	return Out;
}

int32 FTerrainEditModel::NumEditedSamples() const
{
	int32 Count = 0;
	for (const auto& Pair : Chunks)
	{
		Count += Pair.Value.Num();
	}
	return Count;
}

void FTerrainEditModel::Reset()
{
	Chunks.Reset();
	PathColumns.Reset();
}

FSaveValue FTerrainEditModel::ToValue() const
{
	FSaveValue Root = FSaveValue::MakeObject();
	Root.Set(TEXT("v"), FSaveValue::MakeInt(1));
	Root.Set(TEXT("cell"), FSaveValue::MakeFloat(Settings.CellSize));
	Root.Set(TEXT("n"), FSaveValue::MakeInt(Settings.CellsPerChunk));

	FSaveValue ChunkList = FSaveValue::MakeArray();
	for (const FIntVector& Chunk : EditedChunks())
	{
		const TMap<int32, int32>& Deltas = Chunks.FindChecked(Chunk);
		TArray<int32> Locals;
		for (const auto& Pair : Deltas)
		{
			Locals.Add(Pair.Key);
		}
		Locals.Sort();

		// Tramos de índices consecutivos: [Inicio, Cuenta, d…].
		FSaveValue Runs = FSaveValue::MakeArray();
		int32 I = 0;
		while (I < Locals.Num())
		{
			int32 End = I + 1;
			while (End < Locals.Num() && Locals[End] == Locals[End - 1] + 1)
			{
				++End;
			}
			Runs.Add(FSaveValue::MakeInt(Locals[I]));
			Runs.Add(FSaveValue::MakeInt(End - I));
			for (int32 K = I; K < End; ++K)
			{
				Runs.Add(FSaveValue::MakeInt(Deltas.FindChecked(Locals[K])));
			}
			I = End;
		}

		FSaveValue Entry = FSaveValue::MakeArray();
		Entry.Add(FSaveValue::MakeInt(Chunk.X));
		Entry.Add(FSaveValue::MakeInt(Chunk.Y));
		Entry.Add(FSaveValue::MakeInt(Chunk.Z));
		Entry.Add(MoveTemp(Runs));
		ChunkList.Add(MoveTemp(Entry));
	}
	Root.Set(TEXT("chunks"), MoveTemp(ChunkList));

	TArray<FIntPoint> Columns;
	for (const auto& Pair : PathColumns)
	{
		Columns.Add(Pair.Key);
	}
	Columns.Sort(&TerrainEditDetail::ColumnLess);
	FSaveValue Paths = FSaveValue::MakeArray();
	for (const FIntPoint& Column : Columns)
	{
		Paths.Add(FSaveValue::MakeInt(Column.X));
		Paths.Add(FSaveValue::MakeInt(Column.Y));
		Paths.Add(FSaveValue::MakeInt(PathColumns.FindChecked(Column)));
	}
	Root.Set(TEXT("paths"), MoveTemp(Paths));
	return Root;
}

bool FTerrainEditModel::FromValue(const FSaveValue& Value)
{
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
		|| static_cast<float>(CellSize) != Settings.CellSize || !Cells || !TerrainEditDetail::ReadInt32(*Cells, N)
		|| N != Settings.CellsPerChunk)
	{
		// Otra rejilla: las muestras no significarían lo mismo.
		return Fail();
	}
	const int64 LocalCount = static_cast<int64>(N) * N * N;

	if (const FSaveValue* ChunkList = Value.Find(TEXT("chunks")))
	{
		if (!ChunkList->IsArray())
		{
			return Fail();
		}
		for (int32 C = 0; C < ChunkList->Num(); ++C)
		{
			const FSaveValue& Entry = ChunkList->At(C);
			FIntVector Chunk;
			if (!Entry.IsArray() || Entry.Num() != 4 || !TerrainEditDetail::ReadInt32(Entry.At(0), Chunk.X)
				|| !TerrainEditDetail::ReadInt32(Entry.At(1), Chunk.Y) || !TerrainEditDetail::ReadInt32(Entry.At(2), Chunk.Z)
				|| !Entry.At(3).IsArray() || Chunks.Contains(Chunk))
			{
				return Fail();
			}
			// Chunk·N (y ±1 al leer los vecinos) tiene que caber en int32.
			const int32 MaxChunk = MAX_int32 / N - 2;
			if (FMath::Abs(static_cast<int64>(Chunk.X)) > MaxChunk || FMath::Abs(static_cast<int64>(Chunk.Y)) > MaxChunk
				|| FMath::Abs(static_cast<int64>(Chunk.Z)) > MaxChunk)
			{
				return Fail();
			}
			const FSaveValue& Runs = Entry.At(3);
			TMap<int32, int32> Deltas;
			int64 PreviousEnd = 0;
			int32 Pos = 0;
			while (Pos < Runs.Num())
			{
				int32 First = 0;
				int32 Count = 0;
				if (Pos + 1 >= Runs.Num() || !TerrainEditDetail::ReadInt32(Runs.At(Pos), First)
					|| !TerrainEditDetail::ReadInt32(Runs.At(Pos + 1), Count) || First < PreviousEnd || Count < 1
					|| static_cast<int64>(First) + Count > LocalCount || Pos + 2 + Count > Runs.Num())
				{
					return Fail();
				}
				for (int32 K = 0; K < Count; ++K)
				{
					int32 Mm = 0;
					if (!TerrainEditDetail::ReadInt32(Runs.At(Pos + 2 + K), Mm) || Mm < -MaxDeltaMm || Mm > MaxDeltaMm)
					{
						return Fail();
					}
					if (Mm != 0)
					{
						Deltas.Add(First + K, Mm);
					}
				}
				PreviousEnd = static_cast<int64>(First) + Count;
				Pos += 2 + Count;
			}
			if (Deltas.Num() > 0)
			{
				Chunks.Add(Chunk, MoveTemp(Deltas));
			}
		}
	}

	if (const FSaveValue* Paths = Value.Find(TEXT("paths")))
	{
		if (!Paths->IsArray() || Paths->Num() % 3 != 0)
		{
			return Fail();
		}
		for (int32 I = 0; I < Paths->Num(); I += 3)
		{
			FIntPoint Column;
			int32 Amount = 0;
			if (!TerrainEditDetail::ReadInt32(Paths->At(I), Column.X) || !TerrainEditDetail::ReadInt32(Paths->At(I + 1), Column.Y)
				|| !TerrainEditDetail::ReadInt32(Paths->At(I + 2), Amount) || Amount < 1 || Amount > 100
				|| PathColumns.Contains(Column))
			{
				return Fail();
			}
			PathColumns.Add(Column, Amount);
		}
	}
	return true;
}

bool FTerrainEditModel::operator==(const FTerrainEditModel& Other) const
{
	if (Settings.CellSize != Other.Settings.CellSize || Settings.CellsPerChunk != Other.Settings.CellsPerChunk
		|| Chunks.Num() != Other.Chunks.Num() || PathColumns.Num() != Other.PathColumns.Num())
	{
		return false;
	}
	for (const auto& Pair : Chunks)
	{
		const TMap<int32, int32>* Match = Other.Chunks.Find(Pair.Key);
		if (!Match || Match->Num() != Pair.Value.Num())
		{
			return false;
		}
		for (const auto& Delta : Pair.Value)
		{
			const int32* OtherDelta = Match->Find(Delta.Key);
			if (!OtherDelta || *OtherDelta != Delta.Value)
			{
				return false;
			}
		}
	}
	for (const auto& Pair : PathColumns)
	{
		const int32* Match = Other.PathColumns.Find(Pair.Key);
		if (!Match || *Match != Pair.Value)
		{
			return false;
		}
	}
	return true;
}
