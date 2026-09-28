#include "WorldGen/CoconutPalmModel.h"

#include "Core/ExploredRandom.h"

const FName FCoconutPalmModel::MatureItem(TEXT("coco_maduro"));
const FName FCoconutPalmModel::GreenItem(TEXT("coco_verde"));
const FName FCoconutPalmModel::ShellItem(TEXT("cascara_coco"));

namespace
{
	// Sales de cada tirada: la misma (hueco, generación) no comparte hash entre usos distintos.
	constexpr uint32 SaltRefill = 0x52454649u;
	constexpr uint32 SaltHang = 0x48414E47u;
	constexpr uint32 SaltPhase = 0x50484153u;
	constexpr uint32 SaltAngle = 0x414E474Cu;
	constexpr uint32 SaltRadius = 0x52414449u;
	constexpr uint32 SaltShake = 0x5348414Bu;
	constexpr uint32 SaltGust = 0x47555354u;
	constexpr uint32 SaltCrack = 0x43524B21u;
	constexpr int32 MaxDays = 3650;
	constexpr int32 MaxSlots = 64;

	uint32 SlotHash(uint32 Seed, uint32 Salt, int32 Slot, int32 Generation, uint32 Extra = 0)
	{
		return ExploredHash::Hash3D(ExploredHash::Hash32(Seed ^ Salt), Slot, Generation, static_cast<int32>(Extra));
	}

	/** Minutos en [MinDays, MaxDays] días, sorteados al minuto. */
	int64 DaysBetween(uint32 Hash, int32 MinDays, int32 MaxDays)
	{
		const int64 Lo = static_cast<int64>(MinDays) * FCoconutPalmModel::MinutesPerDay;
		const int64 Span = static_cast<int64>(MaxDays - MinDays) * FCoconutPalmModel::MinutesPerDay;
		return Lo + (Span > 0 ? static_cast<int64>(Hash % static_cast<uint64>(Span + 1)) : 0);
	}

	float Clamp01OrZero(float V)
	{
		return FMath::IsFinite(V) ? FMath::Clamp(V, 0.0f, 1.0f) : 0.0f;
	}

	float NonNegativeOrZero(float V)
	{
		return FMath::IsFinite(V) ? FMath::Max(0.0f, V) : 0.0f;
	}

	bool SortGround(const FFallenCoconut& A, const FFallenCoconut& B)
	{
		return A.LandedMinute != B.LandedMinute ? A.LandedMinute < B.LandedMinute : A.Id < B.Id;
	}

	/** El coco del hueco sale de la copa en el minuto T: el hueco empieza un ciclo nuevo. */
	void Vacate(FCoconutSlot& Slot, int64 T)
	{
		++Slot.Generation;
		Slot.CycleStartMinute = T;
	}

	/** Deja el coco del hueco en el suelo en el minuto T (antes de vaciar el hueco). */
	FFallenCoconut LandOnGround(FCoconutPalmState& State, const FCoconutPalmProfile& P, int32 SlotIndex, int64 T)
	{
		FFallenCoconut Coconut;
		Coconut.Id = FCoconutPalmModel::MakeId(SlotIndex, State.Slots[SlotIndex].Generation);
		Coconut.Position = FCoconutPalmModel::FallPosition(State, P, SlotIndex, State.Slots[SlotIndex].Generation);
		Coconut.LandedMinute = T;
		State.Ground.Add(Coconut);
		Vacate(State.Slots[SlotIndex], T);
		return Coconut;
	}

	/** Tirada de sacudida (jugador o racha) en el minuto T de la última actualización. */
	int32 ShakeAt(FCoconutPalmState& State, const FCoconutPalmProfile& P, float Strength, uint32 Salt, uint32 Roll,
		const FVector2D* Shaker, int32& Counter, TArray<FCoconutDrop>& OutDrops)
	{
		if (State.bFelled || Strength <= 0.0f)
		{
			return 0;
		}
		const int64 T = State.LastUpdateMinute;
		const double HeadCm = FCoconutPalmModel::HeadHitRadiusMeters * 100.0;
		int32 Fallen = 0;
		for (int32 i = 0; i < State.Slots.Num(); ++i)
		{
			int64 Set = 0, Mature = 0, Fall = 0;
			FCoconutPalmModel::CycleOf(State, P, i, Set, Mature, Fall);
			if (T < Mature || T >= Fall)
			{
				continue;
			}
			const float Looseness = static_cast<float>(static_cast<double>(T - Mature) / static_cast<double>(Fall - Mature));
			const float Chance = Strength * (FCoconutPalmModel::BaseLooseness + (1.0f - FCoconutPalmModel::BaseLooseness) * Looseness);
			const float Dice = ExploredHash::ToUnitFloat(SlotHash(State.Seed, Salt, i, State.Slots[i].Generation, Roll));
			if (Dice >= Chance)
			{
				continue;
			}
			const FFallenCoconut Landed = LandOnGround(State, P, i, T);
			FCoconutDrop Drop;
			Drop.ItemId = FCoconutPalmModel::MatureItem;
			Drop.Position = Landed.Position;
			Drop.Id = Landed.Id;
			Drop.bHitsShaker = Shaker != nullptr && FVector2D::Distance(Landed.Position, *Shaker) < HeadCm;
			OutDrops.Add(Drop);
			++Counter;
			++Fallen;
		}
		State.Ground.Sort(SortGround);
		return Fallen;
	}
}

FCoconutPalmProfile FCoconutPalmModel::DefaultProfile()
{
	return FCoconutPalmProfile();
}

FCoconutPalmProfile FCoconutPalmModel::Sanitize(const FCoconutPalmProfile& In)
{
	FCoconutPalmProfile P = In;
	P.Slots = FMath::Clamp(P.Slots, 0, MaxSlots);
	// Todo ciclo dura al menos dos días (verde y colgando): Advance nunca da vueltas de un minuto.
	P.RefillMinDays = FMath::Clamp(P.RefillMinDays, 0, MaxDays);
	P.RefillMaxDays = FMath::Clamp(P.RefillMaxDays, P.RefillMinDays, MaxDays);
	P.GreenDays = FMath::Clamp(P.GreenDays, 1, MaxDays);
	P.HangMinDays = FMath::Clamp(P.HangMinDays, 1, MaxDays);
	P.HangMaxDays = FMath::Clamp(P.HangMaxDays, P.HangMinDays, MaxDays);
	P.GroundLifeDays = FMath::Clamp(P.GroundLifeDays, 1, MaxDays);
	P.TrunkHeightMeters = NonNegativeOrZero(P.TrunkHeightMeters);
	P.CrownRadiusMeters = NonNegativeOrZero(P.CrownRadiusMeters);
	P.CrackChanceOnFell = Clamp01OrZero(P.CrackChanceOnFell);
	return P;
}

uint32 FCoconutPalmModel::MakeId(int32 SlotIndex, int32 Generation)
{
	// Id 0 queda para «no está en el suelo»: el hueco va desplazado en uno.
	return (static_cast<uint32>(SlotIndex + 1) << 24) | (static_cast<uint32>(Generation) & 0xFFFFFFu);
}

void FCoconutPalmModel::CycleOf(const FCoconutPalmState& State, const FCoconutPalmProfile& InProfile, int32 SlotIndex,
	int64& OutSetMinute, int64& OutMatureMinute, int64& OutFallMinute)
{
	const FCoconutPalmProfile P = Sanitize(InProfile);
	const FCoconutSlot& Slot = State.Slots[SlotIndex];
	OutSetMinute = Slot.CycleStartMinute + DaysBetween(SlotHash(State.Seed, SaltRefill, SlotIndex, Slot.Generation), P.RefillMinDays, P.RefillMaxDays);
	OutMatureMinute = OutSetMinute + static_cast<int64>(P.GreenDays) * MinutesPerDay;
	OutFallMinute = OutMatureMinute + DaysBetween(SlotHash(State.Seed, SaltHang, SlotIndex, Slot.Generation), P.HangMinDays, P.HangMaxDays);
}

FVector2D FCoconutPalmModel::FallPosition(const FCoconutPalmState& State, const FCoconutPalmProfile& InProfile, int32 SlotIndex, int32 Generation)
{
	const FCoconutPalmProfile P = Sanitize(InProfile);
	const double MinCm = MinFallRadiusMeters * 100.0;
	const double MaxCm = FMath::Max(MinCm, static_cast<double>(P.CrownRadiusMeters) * FallRadiusCrownFraction * 100.0);
	const double Angle = static_cast<double>(ExploredHash::ToUnitFloat(SlotHash(State.Seed, SaltAngle, SlotIndex, Generation))) * 2.0 * PI;
	const double U = static_cast<double>(ExploredHash::ToUnitFloat(SlotHash(State.Seed, SaltRadius, SlotIndex, Generation)));
	// Uniforme en el área del anillo, no en el radio: si no, se amontonan junto al tronco.
	const double Radius = FMath::Sqrt(MinCm * MinCm + U * (MaxCm * MaxCm - MinCm * MinCm));
	return State.TrunkPosition + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
}

FCoconutPalmState FCoconutPalmModel::Initialize(uint32 Seed, const FVector2D& TrunkPosition, const FCoconutPalmProfile& InProfile, int64 NowMinute, bool bStocked)
{
	const FCoconutPalmProfile P = Sanitize(InProfile);
	FCoconutPalmState State;
	State.Seed = Seed;
	State.TrunkPosition = TrunkPosition;
	State.LastUpdateMinute = NowMinute;
	State.Slots.SetNum(P.Slots);
	for (int32 i = 0; i < P.Slots; ++i)
	{
		State.Slots[i].CycleStartMinute = NowMinute;
		if (bStocked)
		{
			// Retrasa el inicio del ciclo una fracción de su duración, sin llegar a la caída.
			int64 Set = 0, Mature = 0, Fall = 0;
			CycleOf(State, P, i, Set, Mature, Fall);
			const uint64 Length = static_cast<uint64>(Fall - NowMinute);
			State.Slots[i].CycleStartMinute = NowMinute - static_cast<int64>(SlotHash(Seed, SaltPhase, i, 0) % Length);
		}
	}
	return State;
}

int32 FCoconutPalmModel::Advance(FCoconutPalmState& State, const FCoconutPalmProfile& InProfile, int64 NowMinute)
{
	if (NowMinute < State.LastUpdateMinute)
	{
		return 0;
	}
	const FCoconutPalmProfile P = Sanitize(InProfile);
	const int64 Life = static_cast<int64>(P.GroundLifeDays) * MinutesPerDay;
	int32 Fallen = 0;
	if (!State.bFelled)
	{
		for (int32 i = 0; i < State.Slots.Num(); ++i)
		{
			for (;;)
			{
				int64 Set = 0, Mature = 0, Fall = 0;
				CycleOf(State, P, i, Set, Mature, Fall);
				if (Fall > NowMinute)
				{
					break;
				}
				++State.Counters.NaturalFalls;
				++Fallen;
				if (Fall + Life <= NowMinute)
				{
					// Cayó y se pudrió mientras nadie miraba: no hace falta ni ponerlo en la lista.
					++State.Counters.Rotted;
					Vacate(State.Slots[i], Fall);
				}
				else
				{
					LandOnGround(State, P, i, Fall);
				}
			}
		}
	}
	const int32 Before = State.Ground.Num();
	State.Ground.RemoveAll([NowMinute, Life](const FFallenCoconut& C) { return C.LandedMinute + Life <= NowMinute; });
	State.Counters.Rotted += Before - State.Ground.Num();
	State.Ground.Sort(SortGround);
	State.LastUpdateMinute = NowMinute;
	return Fallen;
}

ECoconutStage FCoconutPalmModel::StageOf(const FCoconutPalmState& State, const FCoconutPalmProfile& Profile, int32 SlotIndex)
{
	if (State.bFelled || !State.Slots.IsValidIndex(SlotIndex))
	{
		return ECoconutStage::Empty;
	}
	int64 Set = 0, Mature = 0, Fall = 0;
	CycleOf(State, Profile, SlotIndex, Set, Mature, Fall);
	const int64 T = State.LastUpdateMinute;
	if (T < Set || T >= Fall)
	{
		return ECoconutStage::Empty;
	}
	return T < Mature ? ECoconutStage::Green : ECoconutStage::Mature;
}

int32 FCoconutPalmModel::CountOnTree(const FCoconutPalmState& State, const FCoconutPalmProfile& Profile, ECoconutStage Stage)
{
	int32 Count = 0;
	for (int32 i = 0; i < State.Slots.Num(); ++i)
	{
		Count += StageOf(State, Profile, i) == Stage ? 1 : 0;
	}
	return Count;
}

float FCoconutPalmModel::HandShakeStrength(const FCoconutPalmProfile& InProfile)
{
	const FCoconutPalmProfile P = Sanitize(InProfile);
	if (P.TrunkHeightMeters <= HandShakeReferenceMeters)
	{
		return 1.0f;
	}
	return FMath::Max(MinHandShakeStrength, HandShakeReferenceMeters / P.TrunkHeightMeters);
}

float FCoconutPalmModel::GustStrength(float Wind)
{
	const float W = Clamp01OrZero(Wind);
	if (W <= GustWindThreshold)
	{
		return 0.0f;
	}
	return MaxGustStrength * (W - GustWindThreshold) / (1.0f - GustWindThreshold);
}

int32 FCoconutPalmModel::Shake(FCoconutPalmState& State, const FCoconutPalmProfile& InProfile, float Strength, const FVector2D& ShakerPosition,
	int64 NowMinute, TArray<FCoconutDrop>& OutDrops)
{
	const FCoconutPalmProfile P = Sanitize(InProfile);
	// Un reloj que va hacia atrás no deshace nada: se sacude en el último minuto conocido.
	Advance(State, P, FMath::Max(NowMinute, State.LastUpdateMinute));
	++State.ShakeSerial;
	return ShakeAt(State, P, Clamp01OrZero(Strength), SaltShake, State.ShakeSerial, &ShakerPosition, State.Counters.Shaken, OutDrops);
}

int32 FCoconutPalmModel::ApplyGust(FCoconutPalmState& State, const FCoconutPalmProfile& InProfile, float Wind, int64 HourIndex, TArray<FCoconutDrop>& OutDrops)
{
	const int64 T = HourIndex * 60;
	if (HourIndex <= State.LastGustHour || T < State.LastUpdateMinute)
	{
		return 0;
	}
	const FCoconutPalmProfile P = Sanitize(InProfile);
	Advance(State, P, T);
	State.LastGustHour = HourIndex;
	return ShakeAt(State, P, GustStrength(Wind), SaltGust, static_cast<uint32>(HourIndex), nullptr, State.Counters.GustFalls, OutDrops);
}

FName FCoconutPalmModel::PickFromCrown(FCoconutPalmState& State, const FCoconutPalmProfile& InProfile, bool bWantGreen, int64 NowMinute)
{
	const FCoconutPalmProfile P = Sanitize(InProfile);
	Advance(State, P, FMath::Max(NowMinute, State.LastUpdateMinute));
	const ECoconutStage Wanted = bWantGreen ? ECoconutStage::Green : ECoconutStage::Mature;
	for (int32 i = 0; i < State.Slots.Num(); ++i)
	{
		if (StageOf(State, P, i) == Wanted)
		{
			Vacate(State.Slots[i], State.LastUpdateMinute);
			++State.Counters.Climbed;
			return bWantGreen ? GreenItem : MatureItem;
		}
	}
	return NAME_None;
}

bool FCoconutPalmModel::PickFromGround(FCoconutPalmState& State, uint32 Id, FFallenCoconut* OutCoconut)
{
	const int32 Index = State.Ground.IndexOfByPredicate([Id](const FFallenCoconut& C) { return C.Id == Id; });
	if (Index == INDEX_NONE)
	{
		return false;
	}
	if (OutCoconut)
	{
		*OutCoconut = State.Ground[Index];
	}
	State.Ground.RemoveAt(Index);
	++State.Counters.PickedFromGround;
	return true;
}

int32 FCoconutPalmModel::Fell(FCoconutPalmState& State, const FCoconutPalmProfile& InProfile, const FVector2D& FallDirection, int64 NowMinute, TArray<FCoconutDrop>& OutDrops)
{
	const FCoconutPalmProfile P = Sanitize(InProfile);
	Advance(State, P, FMath::Max(NowMinute, State.LastUpdateMinute));
	if (State.bFelled)
	{
		return 0;
	}
	FVector2D Dir = FallDirection;
	const bool bFinite = FMath::IsFinite(Dir.X) && FMath::IsFinite(Dir.Y);
	Dir = bFinite ? Dir.GetSafeNormal() : FVector2D::ZeroVector;
	if (Dir.IsZero())
	{
		Dir = FVector2D(1.0, 0.0);
	}
	// La copa queda donde FFellingModel::ComputeFellDrops pone ramas y hojas: al 85 % del tronco.
	const FVector2D Crown = State.TrunkPosition + Dir * (static_cast<double>(P.TrunkHeightMeters) * 0.85 * 100.0);
	const double CrownCm = static_cast<double>(P.CrownRadiusMeters) * 100.0;
	const int64 T = State.LastUpdateMinute;
	int32 Taken = 0;
	for (int32 i = 0; i < State.Slots.Num(); ++i)
	{
		const ECoconutStage Stage = StageOf(State, P, i);
		if (Stage == ECoconutStage::Empty)
		{
			continue;
		}
		const int32 Gen = State.Slots[i].Generation;
		FCoconutDrop Drop;
		Drop.ItemId = GreenItem;
		if (Stage == ECoconutStage::Mature)
		{
			const float Dice = ExploredHash::ToUnitFloat(SlotHash(State.Seed, SaltCrack, i, Gen));
			Drop.ItemId = Dice < P.CrackChanceOnFell ? ShellItem : MatureItem;
		}
		// Esparcidos por la copa caída (la misma fórmula del anillo, pero con el centro en la copa).
		const double Angle = static_cast<double>(ExploredHash::ToUnitFloat(SlotHash(State.Seed, SaltAngle ^ SaltCrack, i, Gen))) * 2.0 * PI;
		const double Radius = FMath::Sqrt(static_cast<double>(ExploredHash::ToUnitFloat(SlotHash(State.Seed, SaltRadius ^ SaltCrack, i, Gen)))) * CrownCm;
		Drop.Position = Crown + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
		OutDrops.Add(Drop);
		Vacate(State.Slots[i], T);
		++State.Counters.Felled;
		++Taken;
	}
	State.bFelled = true;
	return Taken;
}
