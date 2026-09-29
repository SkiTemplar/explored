#include "WorldGen/WildfireModel.h"

#include "Core/ExploredRandom.h"

// Espacio con nombre y no anónimo: en unity build otro .cpp del módulo puede tener un FloorDiv o un Roll propio.
namespace WildfireModelPrivate
{
	constexpr int32 SaveVersion = 1;
	/** Radio máximo de Douse: un cubo no apaga medio monte. */
	constexpr int32 MaxDouseRadiusCells = 8;
	/** Cota de celda (la misma que CellAt): ±1e9 deja margen para sumar vecinas sin desbordar int32. */
	constexpr int64 MaxAbsCell = 1000000000;
	/** Cota de minuto de juego en una partida cargada (~1,9 millones de años): sumar rebrotes no desborda. */
	constexpr int64 MaxAbsMinute = 1000000000000ll;

	/** Minutos transcurridos desde From hasta Now (Now >= From), sin desbordar con valores extremos. */
	uint64 MinutesSince(int64 Now, int64 From)
	{
		return (uint64)Now - (uint64)From;
	}

	const FIntPoint NeighborOffsets[8] = {
		FIntPoint(1, 0), FIntPoint(1, 1), FIntPoint(0, 1), FIntPoint(-1, 1),
		FIntPoint(-1, 0), FIntPoint(-1, -1), FIntPoint(0, -1), FIntPoint(1, -1),
	};

	bool CellLess(const FIntPoint& A, const FIntPoint& B)
	{
		return A.Y != B.Y ? A.Y < B.Y : A.X < B.X;
	}

	int32 FloorDiv(int32 A, int32 B)
	{
		const int32 Q = A / B;
		return (A % B != 0 && ((A < 0) != (B < 0))) ? Q - 1 : Q;
	}

	int32 BurnStepsFor(EFireFuel Fuel)
	{
		switch (Fuel)
		{
		case EFireFuel::Grass: return FWildfireModel::GrassBurnSteps;
		case EFireFuel::Shrub: return FWildfireModel::ShrubBurnSteps;
		default:               return 0;
		}
	}

	int64 RegrowMinutesFor(EFireFuel Fuel)
	{
		return Fuel == EFireFuel::Shrub ? FWildfireModel::ShrubRegrowMinutes : FWildfireModel::GrassRegrowMinutes;
	}

	/** Tirada 0–999 para (paso, celda destino, vecina de origen): no depende del orden de visita. */
	int32 Roll(uint32 Seed, int64 StepIndex, FIntPoint Target, int32 FromNeighbor)
	{
		const uint32 StepSeed = ExploredHash::Hash2D(Seed, (int32)(StepIndex & 0xFFFFFFFF), (int32)(StepIndex >> 32));
		return (int32)(ExploredHash::Hash3D(StepSeed, Target.X, Target.Y, FromNeighbor) % 1000u);
	}

	/** Primera posición de un array ordenado por CellLess donde cabe Cell. */
	int32 LowerBound(const TArray<FIntPoint>& Sorted, const FIntPoint& Cell)
	{
		int32 Lo = 0, Hi = Sorted.Num();
		while (Lo < Hi)
		{
			const int32 Mid = Lo + (Hi - Lo) / 2;
			if (CellLess(Sorted[Mid], Cell)) { Lo = Mid + 1; } else { Hi = Mid; }
		}
		return Lo;
	}
}

namespace WildfirePriv = WildfireModelPrivate;

const TCHAR* LexToString(EFireFuel Fuel)
{
	switch (Fuel)
	{
	case EFireFuel::None:  return TEXT("None");
	case EFireFuel::Grass: return TEXT("Grass");
	case EFireFuel::Shrub: return TEXT("Shrub");
	default:               return TEXT("?");
	}
}

const TCHAR* LexToString(EFireCellState State)
{
	switch (State)
	{
	case EFireCellState::Unburnt: return TEXT("Unburnt");
	case EFireCellState::Burning: return TEXT("Burning");
	case EFireCellState::Burnt:   return TEXT("Burnt");
	default:                      return TEXT("?");
	}
}

FWildfireModel::FWildfireModel(uint32 InSeed, FFuelQuery InFuel)
	: Seed(InSeed)
	, Fuel(MoveTemp(InFuel))
{
}

FIntPoint FWildfireModel::CellAt(const FVector2D& WorldCm)
{
	// Fuera de rango o no finito: celda 0 (la capa de UE nunca pide eso; el modelo no se rompe).
	const double MaxCoord = 1.0e9;
	const double X = FMath::IsFinite(WorldCm.X) ? FMath::Clamp(WorldCm.X / CellSizeCm, -MaxCoord, MaxCoord) : 0.0;
	const double Y = FMath::IsFinite(WorldCm.Y) ? FMath::Clamp(WorldCm.Y / CellSizeCm, -MaxCoord, MaxCoord) : 0.0;
	return FIntPoint((int32)FMath::FloorToDouble(X), (int32)FMath::FloorToDouble(Y));
}

FVector2D FWildfireModel::CellCenter(FIntPoint Cell)
{
	return FVector2D(((double)Cell.X + 0.5) * CellSizeCm, ((double)Cell.Y + 0.5) * CellSizeCm);
}

FIntPoint FWildfireModel::ChunkOf(FIntPoint Cell)
{
	return FIntPoint(WildfirePriv::FloorDiv(Cell.X, ChunkCells), WildfirePriv::FloorDiv(Cell.Y, ChunkCells));
}

bool FWildfireModel::RainQuenches(EWeatherState Weather)
{
	return Weather == EWeatherState::Shower || Weather == EWeatherState::Thunderstorm || Weather == EWeatherState::Cyclone;
}

int32 FWildfireModel::BaseChancePermille(ESeason Season, EWeatherState Weather)
{
	if (RainQuenches(Weather))
	{
		return 0;
	}
	if (Weather == EWeatherState::MorningFog)
	{
		return FogChancePermille;
	}
	return Season == ESeason::Dry ? DryChancePermille : WetSeasonChancePermille;
}

int32 FWildfireModel::SpreadChancePermille(const FWildfireConditions& Conditions, FIntPoint Offset)
{
	const int32 Base = BaseChancePermille(Conditions.Season, Conditions.Weather);
	if (Base <= 0)
	{
		return 0;
	}
	int32 Bonus = 0;
	const FVector2D Wind = Conditions.WindDirection;
	const bool bWindValid = FMath::IsFinite(Wind.X) && FMath::IsFinite(Wind.Y) && FMath::IsFinite(Conditions.Wind);
	const double WindLen = bWindValid ? Wind.Size() : 0.0;
	if (bWindValid && Conditions.Wind >= CalmWind && WindLen > UE_KINDA_SMALL_NUMBER && (Offset.X != 0 || Offset.Y != 0))
	{
		const FVector2D Dir((double)Offset.X, (double)Offset.Y);
		const double Cos = (Dir.X * Wind.X + Dir.Y * Wind.Y) / (Dir.Size() * WindLen);
		Bonus = (int32)FMath::RoundToDouble((double)WindBonusPermille * FMath::Clamp(Cos, -1.0, 1.0));
	}
	return FMath::Clamp(Base + Bonus, 0, 1000);
}

EFireCellState FWildfireModel::StateAt(FIntPoint Cell, int64 NowMinute) const
{
	const FWildfireCell* Found = Cells.Find(Cell);
	if (!Found)
	{
		return EFireCellState::Unburnt;
	}
	// Comparar antes de restar: BurntMinute + rebrote desborda con minutos extremos.
	if (Found->State == EFireCellState::Burnt && NowMinute >= Found->BurntMinute
		&& WildfirePriv::MinutesSince(NowMinute, Found->BurntMinute) >= (uint64)WildfirePriv::RegrowMinutesFor(Found->Fuel))
	{
		return EFireCellState::Unburnt;
	}
	return Found->State;
}

EFireFuel FWildfireModel::FuelAt(FIntPoint Cell, int64 NowMinute) const
{
	switch (StateAt(Cell, NowMinute))
	{
	case EFireCellState::Burnt:
		return EFireFuel::None;
	case EFireCellState::Burning:
		return Cells.FindChecked(Cell).Fuel;
	default:
	{
		const EFireFuel Base = Fuel ? Fuel(Cell) : EFireFuel::None;
		return (uint8)Base < (uint8)EFireFuel::Count ? Base : EFireFuel::None;
	}
	}
}

bool FWildfireModel::IsWet(FIntPoint Cell, int64 NowMinute) const
{
	const FWildfireCell* Found = Cells.Find(Cell);
	return Found && NowMinute < Found->WetUntilMinute;
}

bool FWildfireModel::Ignite(FIntPoint Cell, int64 NowMinute)
{
	// Fuera de la cota de CellAt: las vecinas (Cell.X + 1) desbordarían int32.
	if (FMath::Abs((int64)Cell.X) > WildfirePriv::MaxAbsCell || FMath::Abs((int64)Cell.Y) > WildfirePriv::MaxAbsCell)
	{
		return false;
	}
	if (StateAt(Cell, NowMinute) != EFireCellState::Unburnt || IsWet(Cell, NowMinute))
	{
		return false;
	}
	const EFireFuel CellFuel = FuelAt(Cell, NowMinute);
	if (CellFuel == EFireFuel::None)
	{
		return false;
	}
	FWildfireCell& Stored = Cells.FindOrAdd(Cell);
	Stored = FWildfireCell();
	Stored.State = EFireCellState::Burning;
	Stored.Fuel = CellFuel;
	Stored.BurnStepsLeft = WildfirePriv::BurnStepsFor(CellFuel);
	const int32 At = WildfirePriv::LowerBound(Burning, Cell);
	Burning.Insert(Cell, At);
	return true;
}

void FWildfireModel::Extinguish(FIntPoint Cell, int64 NowMinute)
{
	FWildfireCell* Found = Cells.Find(Cell);
	if (!Found || Found->State != EFireCellState::Burning)
	{
		return;
	}
	Found->State = EFireCellState::Burnt;
	Found->BurnStepsLeft = 0;
	Found->BurntMinute = NowMinute;
	Found->bAshTaken = false;
}

void FWildfireModel::Douse(FIntPoint Center, int32 RadiusCells, int64 NowMinute, int64 WetMinutes)
{
	if (FMath::Abs((int64)Center.X) > WildfirePriv::MaxAbsCell || FMath::Abs((int64)Center.Y) > WildfirePriv::MaxAbsCell)
	{
		return;
	}
	const int32 R = FMath::Clamp(RadiusCells, 0, WildfirePriv::MaxDouseRadiusCells);
	// Saturar en vez de sumar: NowMinute + WetMinutes desborda con valores extremos.
	const int64 Wet = FMath::Max<int64>(0, WetMinutes);
	// Con un reloj dentro del rango de Load, acotar a ese rango: un INT64_MAX guardado («mojada
	// para siempre») haría que Load rechazase toda la capa de incendios.
	const int64 Saturated = NowMinute > INT64_MAX - Wet ? INT64_MAX : NowMinute + Wet;
	const int64 WetUntil = NowMinute <= WildfirePriv::MaxAbsMinute ? FMath::Min(Saturated, WildfirePriv::MaxAbsMinute) : Saturated;
	bool bRemovedBurning = false;
	for (int32 DY = -R; DY <= R; ++DY)
	{
		for (int32 DX = -R; DX <= R; ++DX)
		{
			if (DX * DX + DY * DY > R * R)
			{
				continue;
			}
			const FIntPoint Cell(Center.X + DX, Center.Y + DY);
			const EFireCellState State = StateAt(Cell, NowMinute);
			if (State == EFireCellState::Burning)
			{
				Extinguish(Cell, NowMinute);
				bRemovedBurning = true;
			}
			else if (State == EFireCellState::Unburnt && WetUntil > NowMinute && FuelAt(Cell, NowMinute) != EFireFuel::None)
			{
				FWildfireCell& Stored = Cells.FindOrAdd(Cell);
				if (Stored.State == EFireCellState::Burnt)
				{
					Stored = FWildfireCell(); // ya había rebrotado
				}
				Stored.WetUntilMinute = FMath::Max(Stored.WetUntilMinute, WetUntil);
			}
		}
	}
	if (bRemovedBurning)
	{
		RebuildBurning();
	}
}

bool FWildfireModel::IsChunkActive(FIntPoint Chunk, const TArray<FVector2D>& ObserversCm) const
{
	// Distancia del observador al rectángulo del chunk (no a su centro): un chunk cuenta si lo toca el radio.
	const double Size = (double)ChunkCells * CellSizeCm;
	const double MinX = (double)Chunk.X * Size, MinY = (double)Chunk.Y * Size;
	for (const FVector2D& P : ObserversCm)
	{
		if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y))
		{
			continue;
		}
		const double DX = FMath::Max3(MinX - P.X, 0.0, P.X - (MinX + Size));
		const double DY = FMath::Max3(MinY - P.Y, 0.0, P.Y - (MinY + Size));
		if (DX * DX + DY * DY <= ActiveRadiusCm * ActiveRadiusCm)
		{
			return true;
		}
	}
	return false;
}

void FWildfireModel::AddUnique(TArray<FIntPoint>& Array, FIntPoint Value)
{
	if (!Array.Contains(Value))
	{
		Array.Add(Value);
	}
}

void FWildfireModel::RebuildBurning()
{
	Burning.Reset();
	for (const TPair<FIntPoint, FWildfireCell>& Pair : Cells)
	{
		if (Pair.Value.State == EFireCellState::Burning)
		{
			Burning.Add(Pair.Key);
		}
	}
	Burning.Sort(WildfirePriv::CellLess);
}

void FWildfireModel::Step(int64 StepIndex, int64 NowMinute, const FWildfireConditions& Conditions, const TArray<FVector2D>& ObserversCm, FWildfireStepResult& Out)
{
	if (Burning.Num() == 0)
	{
		return;
	}
	if (RainQuenches(Conditions.Weather))
	{
		// La lluvia cae en toda la isla: apaga también lo que está congelado lejos.
		for (const FIntPoint& Cell : Burning)
		{
			Extinguish(Cell, NowMinute);
			Out.BurnedOut.Add(Cell);
			AddUnique(Out.DirtyChunks, ChunkOf(Cell));
		}
		Burning.Reset();
		Out.bRainQuenched = true;
		return;
	}

	int32 ChancePerOffset[8];
	for (int32 i = 0; i < 8; ++i)
	{
		ChancePerOffset[i] = SpreadChancePermille(Conditions, WildfirePriv::NeighborOffsets[i]);
	}

	// 1) Contagio contra el estado del principio del paso (doble búfer).
	TArray<FIntPoint> Candidates;
	TArray<FIntPoint> Active;
	TMap<FIntPoint, bool> ChunkActiveCache;
	for (const FIntPoint& Cell : Burning)
	{
		const FIntPoint Chunk = ChunkOf(Cell);
		bool* Cached = ChunkActiveCache.Find(Chunk);
		const bool bActive = Cached ? *Cached : ChunkActiveCache.Add(Chunk, IsChunkActive(Chunk, ObserversCm));
		if (!bActive)
		{
			continue;
		}
		Active.Add(Cell);
		++Out.CellsVisited;
		for (int32 i = 0; i < 8; ++i)
		{
			if (ChancePerOffset[i] <= 0)
			{
				continue;
			}
			const FIntPoint Target(Cell.X + WildfirePriv::NeighborOffsets[i].X, Cell.Y + WildfirePriv::NeighborOffsets[i].Y);
			if (StateAt(Target, NowMinute) != EFireCellState::Unburnt || IsWet(Target, NowMinute) || FuelAt(Target, NowMinute) == EFireFuel::None)
			{
				continue;
			}
			// La vecina de origen vista desde la celda destino es la opuesta: (i + 4) % 8.
			if (WildfirePriv::Roll(Seed, StepIndex, Target, (i + 4) % 8) < ChancePerOffset[i])
			{
				Candidates.Add(Target);
			}
		}
	}

	// 2) Se consume el combustible de lo que arde en chunks activos.
	bool bAnyBurnedOut = false;
	for (const FIntPoint& Cell : Active)
	{
		FWildfireCell& Stored = Cells.FindChecked(Cell);
		if (--Stored.BurnStepsLeft <= 0)
		{
			Extinguish(Cell, NowMinute);
			Out.BurnedOut.Add(Cell);
			AddUnique(Out.DirtyChunks, ChunkOf(Cell));
			bAnyBurnedOut = true;
		}
	}
	if (bAnyBurnedOut)
	{
		Burning.RemoveAll([this](const FIntPoint& Cell) { return Cells.FindChecked(Cell).State != EFireCellState::Burning; });
	}

	// 3) Prenden las candidatas, en orden fijo y con tope por chunk.
	Candidates.Sort(WildfirePriv::CellLess);
	TMap<FIntPoint, int32> IgnitionsPerChunk;
	FIntPoint Previous(INT32_MIN, INT32_MIN);
	for (const FIntPoint& Target : Candidates)
	{
		if (Target == Previous)
		{
			continue; // varias vecinas han acertado la misma celda
		}
		Previous = Target;
		const FIntPoint Chunk = ChunkOf(Target);
		int32& Count = IgnitionsPerChunk.FindOrAdd(Chunk);
		if (Count >= MaxIgnitionsPerChunkStep)
		{
			++Out.IgnitionsDeferred;
			continue;
		}
		if (Ignite(Target, NowMinute))
		{
			++Count;
			Out.Ignited.Add(Target);
			AddUnique(Out.DirtyChunks, Chunk);
		}
	}
}

FWildfireStepResult FWildfireModel::Advance(int64 NowSecond, int64 NowMinute, const FWildfireConditions& Conditions, const TArray<FVector2D>& ObserversCm)
{
	FWildfireStepResult Out;
	if (NowSecond <= LastSecond)
	{
		return Out;
	}
	// En uint64: con un LastSecond cargado muy negativo, la resta en int64 desborda y el bucle no acaba.
	const uint64 Pending = (uint64)NowSecond - (uint64)LastSecond;
	const int64 Steps = (int64)FMath::Min<uint64>(Pending, (uint64)MaxCatchUpSteps);
	Out.StepsDropped = (int32)FMath::Min<uint64>(Pending - (uint64)Steps, (uint64)MAX_int32);
	for (int64 S = NowSecond - Steps + 1; S <= NowSecond; ++S)
	{
		Step(S, NowMinute, Conditions, ObserversCm, Out);
		++Out.StepsSimulated;
	}
	LastSecond = NowSecond;
	return Out;
}

int32 FWildfireModel::AshAt(FIntPoint Cell, int64 NowMinute) const
{
	const FWildfireCell* Found = Cells.Find(Cell);
	if (!Found || Found->State != EFireCellState::Burnt || Found->bAshTaken)
	{
		return 0;
	}
	return (NowMinute >= Found->BurntMinute && WildfirePriv::MinutesSince(NowMinute, Found->BurntMinute) < (uint64)AshMinutes) ? 1 : 0;
}

int32 FWildfireModel::TakeAsh(FIntPoint Cell, int64 NowMinute)
{
	const int32 Ash = AshAt(Cell, NowMinute);
	if (Ash > 0)
	{
		Cells.FindChecked(Cell).bAshTaken = true;
	}
	return Ash;
}

int32 FWildfireModel::Prune(int64 NowMinute)
{
	TArray<FIntPoint> Remove;
	for (const TPair<FIntPoint, FWildfireCell>& Pair : Cells)
	{
		if (StateAt(Pair.Key, NowMinute) == EFireCellState::Unburnt && NowMinute >= Pair.Value.WetUntilMinute)
		{
			Remove.Add(Pair.Key);
		}
	}
	for (const FIntPoint& Cell : Remove)
	{
		Cells.Remove(Cell);
	}
	return Remove.Num();
}

FSaveValue FWildfireModel::Save() const
{
	TArray<FIntPoint> Keys;
	Cells.GetKeys(Keys);
	Keys.Sort(WildfirePriv::CellLess);

	FSaveValue Root = FSaveValue::MakeObject();
	Root.Set(TEXT("version"), FSaveValue::MakeInt(WildfirePriv::SaveVersion));
	Root.Set(TEXT("lastSecond"), FSaveValue::MakeInt(LastSecond));
	FSaveValue& List = Root.Set(TEXT("cells"), FSaveValue::MakeArray());
	for (const FIntPoint& Key : Keys)
	{
		const FWildfireCell& C = Cells.FindChecked(Key);
		FSaveValue Row = FSaveValue::MakeArray();
		Row.Add(FSaveValue::MakeInt(Key.X));
		Row.Add(FSaveValue::MakeInt(Key.Y));
		Row.Add(FSaveValue::MakeInt((int64)C.State));
		Row.Add(FSaveValue::MakeInt((int64)C.Fuel));
		Row.Add(FSaveValue::MakeInt(C.BurnStepsLeft));
		Row.Add(FSaveValue::MakeInt(C.BurntMinute));
		Row.Add(FSaveValue::MakeInt(C.WetUntilMinute));
		Row.Add(FSaveValue::MakeBool(C.bAshTaken));
		List.Add(MoveTemp(Row));
	}
	return Root;
}

bool FWildfireModel::Load(const FSaveValue& Value)
{
	const FSaveValue* Version = Value.Find(TEXT("version"));
	const FSaveValue* Last = Value.Find(TEXT("lastSecond"));
	const FSaveValue* List = Value.Find(TEXT("cells"));
	int64 V = 0, LastValue = 0;
	// Un segundo negativo no sale de ningún reloj de partida (y con INT64_MIN la resta de Advance desbordaba).
	if (!Version || !Version->TryGetInt(V) || V != WildfirePriv::SaveVersion || !Last || !Last->TryGetInt(LastValue) || LastValue < 0
		|| !List || !List->IsArray())
	{
		return false;
	}
	TMap<FIntPoint, FWildfireCell> NewCells;
	for (int32 i = 0; i < List->Num(); ++i)
	{
		const FSaveValue& Row = List->At(i);
		int64 X = 0, Y = 0, State = 0, CellFuel = 0, Steps = 0, BurntMinute = 0, WetUntil = 0;
		bool bAsh = false;
		if (!Row.IsArray() || Row.Num() != 8
			|| !Row.At(0).TryGetInt(X) || !Row.At(1).TryGetInt(Y) || !Row.At(2).TryGetInt(State) || !Row.At(3).TryGetInt(CellFuel)
			|| !Row.At(4).TryGetInt(Steps) || !Row.At(5).TryGetInt(BurntMinute) || !Row.At(6).TryGetInt(WetUntil) || !Row.At(7).TryGetBool(bAsh))
		{
			return false;
		}
		// Rangos, no Abs: Abs(INT64_MIN) desborda.
		const int64 MaxCell = WildfirePriv::MaxAbsCell, MaxMinute = WildfirePriv::MaxAbsMinute;
		if (X < -MaxCell || X > MaxCell || Y < -MaxCell || Y > MaxCell
			|| BurntMinute < -MaxMinute || BurntMinute > MaxMinute || WetUntil < -MaxMinute || WetUntil > MaxMinute
			|| State < 0 || State >= (int64)EFireCellState::Count || CellFuel < 0 || CellFuel >= (int64)EFireFuel::Count
			|| Steps < 0 || Steps > ShrubBurnSteps)
		{
			return false;
		}
		const EFireCellState CellState = (EFireCellState)State;
		// Una celda que arde sin combustible, sin tiempo de quema o con más del que da su combustible no puede existir.
		if (CellState == EFireCellState::Burning
			&& (CellFuel == (int64)EFireFuel::None || Steps == 0 || Steps > WildfirePriv::BurnStepsFor((EFireFuel)CellFuel)))
		{
			return false;
		}
		const FIntPoint Key((int32)X, (int32)Y);
		if (NewCells.Contains(Key))
		{
			return false;
		}
		FWildfireCell& C = NewCells.Add(Key);
		C.State = CellState;
		C.Fuel = (EFireFuel)CellFuel;
		C.BurnStepsLeft = CellState == EFireCellState::Burning ? (int32)Steps : 0;
		C.BurntMinute = BurntMinute;
		C.WetUntilMinute = WetUntil;
		C.bAshTaken = bAsh;
	}
	Cells = MoveTemp(NewCells);
	LastSecond = LastValue;
	RebuildBurning();
	return true;
}
