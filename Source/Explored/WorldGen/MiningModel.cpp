#include "WorldGen/MiningModel.h"

#include "Core/ExploredRandom.h"

namespace MiningDetail
{
	constexpr int32 NumStrata = static_cast<int32>(EMineStratum::Count);
	constexpr int32 NumTools = static_cast<int32>(EMineTool::Count);

	bool ReadInt32(const FSaveValue& Value, int32& Out)
	{
		int64 V = 0;
		if (!Value.IsInt() || !Value.TryGetInt(V) || V < MIN_int32 || V > MAX_int32)
		{
			return false;
		}
		Out = static_cast<int32>(V);
		return true;
	}

	bool KeyLess(EMineStratum SA, const FIntVector& A, EMineStratum SB, const FIntVector& B)
	{
		if (SA != SB) { return SA < SB; }
		if (A.Z != B.Z) { return A.Z < B.Z; }
		if (A.Y != B.Y) { return A.Y < B.Y; }
		return A.X < B.X;
	}
}

const FMineStratumInfo& FMiningModel::StratumInfo(EMineStratum Stratum)
{
	// Biblia 02 §2.3 y mining.json/strata. Dureza y nivel mínimo del estrato; el material
	// es el de mining.json (lo que cava el pincel), el anfitrión es lo que queda al agotarse.
	static const FMineStratumInfo Table[] = {
		{ TEXT("tierra"), TEXT("tierra_suelta"), ETerrainMaterial::Tierra, 1, 1, 0, 0, EMineStratum::Tierra },
		{ TEXT("arena"), TEXT("arena"), ETerrainMaterial::Arena, 1, 1, 0, 0, EMineStratum::Arena },
		{ TEXT("arcilla"), TEXT("arcilla_roja"), ETerrainMaterial::Tierra, 1, 1, 0, 0, EMineStratum::Arcilla },
		{ TEXT("azufre"), TEXT("azufre"), ETerrainMaterial::Tierra, 1, 0, 0, 0, EMineStratum::Azufre },
		{ TEXT("caliza"), TEXT("caliza"), ETerrainMaterial::Caliza, 2, 2, 0, 0, EMineStratum::Caliza },
		{ TEXT("veta_cobre"), TEXT("mineral_cobre"), ETerrainMaterial::Caliza, 2, 2, 10, 20, EMineStratum::Caliza },
		{ TEXT("basalto"), TEXT("basalto"), ETerrainMaterial::Basalto, 3, 3, 0, 0, EMineStratum::Basalto },
		{ TEXT("hierro_meteorito"), TEXT("hierro_meteorito"), ETerrainMaterial::Basalto, 3, 3, 4, 0, EMineStratum::Basalto },
		{ TEXT("obsidiana"), TEXT("obsidiana"), ETerrainMaterial::Obsidiana, 4, 4, 0, 0, EMineStratum::Obsidiana },
		// [Decisión] las cavernas de cristal están en el basalto profundo: agotada, queda basalto.
		{ TEXT("cristal"), TEXT("cristal_cuarzo"), ETerrainMaterial::Obsidiana, 4, 4, 12, 0, EMineStratum::Basalto },
	};
	static_assert(UE_ARRAY_COUNT(Table) == MiningDetail::NumStrata, "Una fila por estrato");
	const int32 Index = FMath::Clamp(static_cast<int32>(Stratum), 0, MiningDetail::NumStrata - 1);
	return Table[Index];
}

const FMineToolInfo& FMiningModel::ToolInfo(EMineTool Tool)
{
	// Biblia 02 §2.2. [Decisión] mano y pala no están en la tabla de picos: la mano es un
	// hueco pequeño y lento (azufre), la pala usa el ritmo de FTerrainEditModel y su
	// durabilidad la fija la plantilla `pala` (aquí 0 = no es de este catálogo).
	static const FMineToolInfo Table[] = {
		{ TEXT("mano"), 0, 0.30f, 1.5f, 0, false },
		{ TEXT("pala_tosca"), 1, 0.50f, FTerrainEditModel::SecondsPerShovelStroke, 0, false },
		{ TEXT("pico_piedra"), 2, 0.40f, 1.3f, 55, false },
		{ TEXT("pico_tallado"), 3, 0.42f, 1.2f, 75, false },
		{ TEXT("pico_obsidiana"), 4, 0.50f, 1.0f, 30, true },
		{ TEXT("pico_rescatado"), 4, 0.55f, 1.1f, 95, false },
	};
	static_assert(UE_ARRAY_COUNT(Table) == MiningDetail::NumTools, "Una fila por herramienta");
	const int32 Index = FMath::Clamp(static_cast<int32>(Tool), 0, MiningDetail::NumTools - 1);
	return Table[Index];
}

bool FMiningModel::StratumFromId(const FString& Id, EMineStratum& Out)
{
	for (int32 I = 0; I < MiningDetail::NumStrata; ++I)
	{
		if (Id.Equals(StratumInfo(static_cast<EMineStratum>(I)).Id, ESearchCase::CaseSensitive))
		{
			Out = static_cast<EMineStratum>(I);
			return true;
		}
	}
	return false;
}

bool FMiningModel::ToolFromId(const FString& Id, EMineTool& Out)
{
	for (int32 I = 0; I < MiningDetail::NumTools; ++I)
	{
		if (Id.Equals(ToolInfo(static_cast<EMineTool>(I)).Id, ESearchCase::CaseSensitive))
		{
			Out = static_cast<EMineTool>(I);
			return true;
		}
	}
	return false;
}

bool FMiningModel::CanMine(EMineStratum Stratum, EMineTool Tool)
{
	return ToolInfo(Tool).Tier >= StratumInfo(Stratum).MinToolTier;
}

uint32 FMiningModel::HitSeed(uint32 WorldSeed, uint32 PlayerId, uint32 HitCounter)
{
	return ExploredHash::Hash3D(WorldSeed, static_cast<int32>(PlayerId), static_cast<int32>(HitCounter), 0x4D494E45);
}

bool FMiningModel::RollChip(uint32 Seed)
{
	// Sal propia: la tirada no se correlaciona con la forma del hueco, que usa la misma semilla.
	return ExploredHash::ToUnitFloat(ExploredHash::Hash32(Seed ^ 0xC41B5EEDu)) < FragileChance;
}

int32 FMiningModel::RemainingOn(const FMineStratumInfo& Info, const FVeinState* State, int32 Day)
{
	if (Info.VeinUnits <= 0)
	{
		return 0;
	}
	if (!State)
	{
		return Info.VeinUnits;
	}
	if (State->Remaining > 0)
	{
		return State->Remaining;
	}
	// Resta en 64 bits: un día manipulado no desborda. Un día anterior al agotamiento no reaparece.
	const int64 Elapsed = static_cast<int64>(Day) - static_cast<int64>(State->ExhaustedDay);
	return Info.RespawnDays > 0 && Elapsed >= Info.RespawnDays ? Info.VeinUnits : 0;
}

int32 FMiningModel::VeinRemaining(EMineStratum Stratum, const FIntVector& Vein, int32 Day) const
{
	const FMineStratumInfo& Info = StratumInfo(Stratum);
	return RemainingOn(Info, Veins.Find(FVeinKey{ Stratum, Vein }), Day);
}

FMineHitResult FMiningModel::Hit(const FMineHitRequest& Request, FTerrainEditModel& Terrain, FTerrainEditModel::FBaseDensity Base)
{
	FMineHitResult Result;
	const FMineToolInfo& Tool = ToolInfo(Request.Tool);
	Result.Seconds = Tool.SecondsPerHit;

	// Una veta agotada se pica como la roca que la rodea.
	const FMineStratumInfo& Asked = StratumInfo(Request.Stratum);
	const FVeinKey Key{ Request.Stratum, Request.Vein };
	const bool bVein = Asked.VeinUnits > 0;
	const int32 Remaining = bVein ? RemainingOn(Asked, Veins.Find(Key), Request.Day) : 0;
	const EMineStratum Effective = bVein && Remaining == 0 ? Asked.Host : Request.Stratum;
	const FMineStratumInfo& Info = StratumInfo(Effective);
	Result.Stratum = Effective;

	if (Tool.Tier < Info.MinToolTier)
	{
		Result.Cue = EMineHitCue::Rebound;
		return Result;
	}

	// El azufre se arranca a mano: el pincel de tierra necesita al menos nivel 1.
	FPickaxeHit Pick;
	Pick.ImpactPoint = Request.ImpactPoint;
	Pick.Direction = Request.Direction;
	Pick.Material = Info.Material;
	Pick.ToolTier = FMath::Max(Tool.Tier, FTerrainEditModel::MaterialInfo(Info.Material).MinToolTier);
	Pick.Seed = Request.Seed;
	Pick.Radius = Tool.Radius;
	Result.Edit = Terrain.Pickaxe(Pick, Base);
	if (Result.Edit.bRejected)
	{
		Result.Cue = EMineHitCue::Rebound;
		return Result;
	}
	if (Result.Edit.VolumeRemoved < MinEffectiveVolume)
	{
		Result.Cue = EMineHitCue::Miss;
		return Result;
	}

	Result.Cue = EMineHitCue::Hit;
	Result.LootItem = Info.Item;
	Result.LootUnits = 1;
	Result.DurabilityLoss = Request.Tool == EMineTool::Mano ? 0 : DurabilityPerHit;
	if (Tool.bFragile && Info.Hardness >= FragileMinHardness && RollChip(Request.Seed))
	{
		Result.Cue = EMineHitCue::Chipped;
		Result.DurabilityLoss += FragileDurabilityLoss;
	}

	if (bVein && Remaining > 0)
	{
		FVeinState& State = Veins.FindOrAdd(Key);
		State.Remaining = Remaining - 1;
		if (State.Remaining == 0)
		{
			State.ExhaustedDay = Request.Day;
			Result.bVeinExhausted = true;
		}
	}
	return Result;
}

TArray<FMiningModel::FVeinKey> FMiningModel::SortedVeinKeys() const
{
	TArray<FVeinKey> Keys;
	for (const auto& Pair : Veins)
	{
		Keys.Add(Pair.Key);
	}
	Keys.Sort([](const FVeinKey& A, const FVeinKey& B) { return MiningDetail::KeyLess(A.Stratum, A.Cell, B.Stratum, B.Cell); });
	return Keys;
}

FSaveValue FMiningModel::ToValue() const
{
	FSaveValue Root = FSaveValue::MakeObject();
	Root.Set(TEXT("v"), FSaveValue::MakeInt(1));
	FSaveValue List = FSaveValue::MakeArray();
	for (const FVeinKey& Key : SortedVeinKeys())
	{
		const FVeinState& State = Veins.FindChecked(Key);
		FSaveValue Row = FSaveValue::MakeArray();
		Row.Add(FSaveValue::MakeString(StratumInfo(Key.Stratum).Id));
		Row.Add(FSaveValue::MakeInt(Key.Cell.X));
		Row.Add(FSaveValue::MakeInt(Key.Cell.Y));
		Row.Add(FSaveValue::MakeInt(Key.Cell.Z));
		Row.Add(FSaveValue::MakeInt(State.Remaining));
		Row.Add(FSaveValue::MakeInt(State.ExhaustedDay));
		List.Add(MoveTemp(Row));
	}
	Root.Set(TEXT("veins"), MoveTemp(List));
	return Root;
}

bool FMiningModel::FromValue(const FSaveValue& Value)
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
	if (!Version || Version->AsInt(-1) != 1)
	{
		return Fail();
	}
	const FSaveValue* List = Value.Find(TEXT("veins"));
	if (!List)
	{
		return true;
	}
	if (!List->IsArray())
	{
		return Fail();
	}
	for (int32 I = 0; I < List->Num(); ++I)
	{
		const FSaveValue& Row = List->At(I);
		if (!Row.IsArray() || Row.Num() != 6 || !Row.At(0).IsString())
		{
			return Fail();
		}
		FVeinKey Key;
		FVeinState State;
		if (!StratumFromId(Row.At(0).AsString(), Key.Stratum)
			|| !MiningDetail::ReadInt32(Row.At(1), Key.Cell.X) || !MiningDetail::ReadInt32(Row.At(2), Key.Cell.Y)
			|| !MiningDetail::ReadInt32(Row.At(3), Key.Cell.Z) || !MiningDetail::ReadInt32(Row.At(4), State.Remaining)
			|| !MiningDetail::ReadInt32(Row.At(5), State.ExhaustedDay))
		{
			return Fail();
		}
		const FMineStratumInfo& Info = StratumInfo(Key.Stratum);
		// Solo vetas, con unidades dentro de su rango, y cada una una sola vez.
		if (Info.VeinUnits <= 0 || State.Remaining < 0 || State.Remaining >= Info.VeinUnits || Veins.Contains(Key))
		{
			return Fail();
		}
		if (State.Remaining > 0)
		{
			State.ExhaustedDay = 0;
		}
		Veins.Add(Key, State);
	}
	return true;
}

bool FMiningModel::operator==(const FMiningModel& Other) const
{
	if (Veins.Num() != Other.Veins.Num())
	{
		return false;
	}
	for (const auto& Pair : Veins)
	{
		const FVeinState* Theirs = Other.Veins.Find(Pair.Key);
		if (!Theirs || Theirs->Remaining != Pair.Value.Remaining || Theirs->ExhaustedDay != Pair.Value.ExhaustedDay)
		{
			return false;
		}
	}
	return true;
}
