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

	/** Minutos en [LoDays, HiDays] días, sorteados al minuto. */
	int64 DaysBetween(uint32 Hash, int32 LoDays, int32 HiDays)
	{
		const int64 Lo = static_cast<int64>(LoDays) * FCoconutPalmModel::MinutesPerDay;
		const int64 Span = static_cast<int64>(HiDays - LoDays) * FCoconutPalmModel::MinutesPerDay;
		return Lo + (Span > 0 ? static_cast<int64>(Hash % static_cast<uint64>(Span + 1)) : 0);
	}

	/** CycleOf sin volver a sanear: P ya viene saneado (va en el bucle caliente de Advance). */
	void RawCycle(const FCoconutPalmState& State, const FCoconutPalmProfile& P, int32 SlotIndex, int64& OutSet, int64& OutMature, int64& OutFall)
	{
		const FCoconutSlot& Slot = State.Slots[SlotIndex];
		OutSet = Slot.CycleStartMinute + DaysBetween(SlotHash(State.Seed, SaltRefill, SlotIndex, Slot.Generation), P.RefillMinDays, P.RefillMaxDays);
		OutMature = OutSet + static_cast<int64>(P.GreenDays) * FCoconutPalmModel::MinutesPerDay;
		OutFall = OutMature + DaysBetween(SlotHash(State.Seed, SaltHang, SlotIndex, Slot.Generation), P.HangMinDays, P.HangMaxDays);
	}

	bool InClockRange(int64 Minute)
	{
		return Minute >= -FCoconutPalmModel::MaxSupportedMinute && Minute <= FCoconutPalmModel::MaxSupportedMinute;
	}

	/**
	 * Inicio de ciclo admitido: una copa creada con bStocked en el primer minuto
	 * admitido lo retrasa hasta un ciclo entero (≤ 3 × MaxDays), así que el margen
	 * por abajo es mayor. Ponerse al día sigue acotado a unos 40 000 días.
	 */
	bool InCycleRange(int64 Minute)
	{
		return Minute >= -3 * FCoconutPalmModel::MaxSupportedMinute && Minute <= FCoconutPalmModel::MaxSupportedMinute;
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

	/**
	 * Minutos de madurar y caer de la generación actual de cada hueco. Vive solo
	 * durante una llamada (no se guarda): evita rehacer los hashes del ciclo en
	 * cada hora de temporal. Se refresca al vaciar un hueco.
	 */
	struct FCycleCache
	{
		TArray<int64> Mature;
		TArray<int64> Fall;

		FCycleCache(const FCoconutPalmState& State, const FCoconutPalmProfile& P)
		{
			Mature.SetNum(State.Slots.Num());
			Fall.SetNum(State.Slots.Num());
			for (int32 i = 0; i < State.Slots.Num(); ++i)
			{
				Refresh(State, P, i);
			}
		}

		void Refresh(const FCoconutPalmState& State, const FCoconutPalmProfile& P, int32 i)
		{
			int64 Set = 0;
			RawCycle(State, P, i, Set, Mature[i], Fall[i]);
		}
	};

	/** Tirada de sacudida (jugador o racha) en el minuto T de la última actualización. */
	int32 ShakeAt(FCoconutPalmState& State, const FCoconutPalmProfile& P, float Strength, uint32 Salt, uint32 Roll,
		const FVector2D* Shaker, int32& Counter, TArray<FCoconutDrop>& OutDrops, FCycleCache& Cache)
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
			const int64 Mature = Cache.Mature[i];
			const int64 Fall = Cache.Fall[i];
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
			Cache.Refresh(State, P, i);
			FCoconutDrop Drop;
			Drop.ItemId = FCoconutPalmModel::MatureItem;
			Drop.Position = Landed.Position;
			Drop.Id = Landed.Id;
			Drop.bHitsShaker = Shaker != nullptr && FVector2D::Distance(Landed.Position, *Shaker) < HeadCm;
			OutDrops.Add(Drop);
			++Counter;
			++Fallen;
		}
		if (Fallen > 0)
		{
			State.Ground.Sort(SortGround);
		}
		return Fallen;
	}

	/** Ciclo natural hasta NowMinute (≥ LastUpdateMinute), sin rachas. P ya saneado. */
	int32 AdvanceNatural(FCoconutPalmState& State, const FCoconutPalmProfile& P, int64 NowMinute, FCycleCache& Cache)
	{
		const int64 Life = static_cast<int64>(P.GroundLifeDays) * FCoconutPalmModel::MinutesPerDay;
		int32 Fallen = 0;
		if (!State.bFelled)
		{
			for (int32 i = 0; i < State.Slots.Num(); ++i)
			{
				for (;;)
				{
					const int64 Fall = Cache.Fall[i];
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
					Cache.Refresh(State, P, i);
				}
			}
		}
		const int32 Before = State.Ground.Num();
		State.Ground.RemoveAll([NowMinute, Life](const FFallenCoconut& C) { return C.LandedMinute + Life <= NowMinute; });
		State.Counters.Rotted += Before - State.Ground.Num();
		// RemoveAll conserva el orden: solo hay que ordenar si ha caído algo nuevo.
		if (Fallen > 0)
		{
			State.Ground.Sort(SortGround);
		}
		State.LastUpdateMinute = NowMinute;
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
	P.MaxFellMature = FMath::Clamp(P.MaxFellMature, 1, MaxSlots);
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
	RawCycle(State, Sanitize(InProfile), SlotIndex, OutSetMinute, OutMatureMinute, OutFallMinute);
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

int32 FCoconutPalmModel::Advance(FCoconutPalmState& State, const FCoconutPalmProfile& InProfile, int64 NowMinute, const TArray<FCoconutGust>& Gusts)
{
	if (NowMinute < State.LastUpdateMinute || !InClockRange(NowMinute) || !InClockRange(State.LastUpdateMinute))
	{
		return 0;
	}
	// Un ciclo fuera de rango haría que ponerse al día recorriera millones de ciclos (o desbordara).
	for (const FCoconutSlot& Slot : State.Slots)
	{
		if (!InCycleRange(Slot.CycleStartMinute))
		{
			return 0;
		}
	}
	const FCoconutPalmProfile P = Sanitize(InProfile);
	int32 Fallen = 0;
	TArray<FCoconutDrop> GustDrops;
	FCycleCache Cache(State, P);
	for (const FCoconutGust& Gust : Gusts)
	{
		// Lista ordenada: una hora ya aplicada o desordenada se salta, igual avance como avance.
		if (Gust.Hour <= State.LastGustHour)
		{
			continue;
		}
		const int64 T = Gust.Hour * 60;
		if (T > NowMinute)
		{
			break;
		}
		State.LastGustHour = Gust.Hour;
		const float Strength = GustStrength(Gust.Wind);
		if (T < State.LastUpdateMinute || Strength <= 0.0f)
		{
			continue; // hacia atrás no se aplica nada; sin fuerza no hay nada que hacer
		}
		Fallen += AdvanceNatural(State, P, T, Cache);
		Fallen += ShakeAt(State, P, Strength, SaltGust, static_cast<uint32>(Gust.Hour), nullptr, State.Counters.GustFalls, GustDrops, Cache);
	}
	return Fallen + AdvanceNatural(State, P, NowMinute, Cache);
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
	int64 NowMinute, const TArray<FCoconutGust>& Gusts, TArray<FCoconutDrop>& OutDrops)
{
	const FCoconutPalmProfile P = Sanitize(InProfile);
	// Un reloj que va hacia atrás no deshace nada: se sacude en el último minuto conocido.
	Advance(State, P, FMath::Max(NowMinute, State.LastUpdateMinute), Gusts);
	++State.ShakeSerial;
	FCycleCache Cache(State, P);
	return ShakeAt(State, P, Clamp01OrZero(Strength), SaltShake, State.ShakeSerial, &ShakerPosition, State.Counters.Shaken, OutDrops, Cache);
}

FName FCoconutPalmModel::PickFromCrown(FCoconutPalmState& State, const FCoconutPalmProfile& InProfile, bool bWantGreen, int64 NowMinute, const TArray<FCoconutGust>& Gusts)
{
	const FCoconutPalmProfile P = Sanitize(InProfile);
	Advance(State, P, FMath::Max(NowMinute, State.LastUpdateMinute), Gusts);
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

int32 FCoconutPalmModel::FellMatureKept(const FCoconutPalmProfile& InProfile, int32 Mature, int32 Survivors)
{
	const FCoconutPalmProfile P = Sanitize(InProfile);
	if (Mature <= 1)
	{
		return 0; // «nunca los 100 %»: con uno solo, se abre
	}
	return FMath::Clamp(Survivors, 1, FMath::Min(P.MaxFellMature, Mature - 1));
}

int32 FCoconutPalmModel::Fell(FCoconutPalmState& State, const FCoconutPalmProfile& InProfile, const FVector2D& FallDirection, int64 NowMinute,
	const TArray<FCoconutGust>& Gusts, TArray<FCoconutDrop>& OutDrops)
{
	const FCoconutPalmProfile P = Sanitize(InProfile);
	Advance(State, P, FMath::Max(NowMinute, State.LastUpdateMinute), Gusts);
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

	// Primero la tirada de cada maduro; después se acota a [1, min(MaxFellMature, M − 1)] en orden de hueco.
	TArray<int32> MatureSlots;
	TArray<uint8> Intact; // uint8 y no bool: TArray<bool> no es contiguo en el shim
	int32 Taken = 0;
	for (int32 i = 0; i < State.Slots.Num(); ++i)
	{
		const ECoconutStage Stage = StageOf(State, P, i);
		if (Stage == ECoconutStage::Empty)
		{
			continue;
		}
		++Taken;
		if (Stage == ECoconutStage::Mature)
		{
			MatureSlots.Add(i);
			Intact.Add(ExploredHash::ToUnitFloat(SlotHash(State.Seed, SaltCrack, i, State.Slots[i].Generation)) >= P.CrackChanceOnFell ? 1 : 0);
		}
	}
	int32 Survivors = 0;
	for (const uint8 b : Intact)
	{
		Survivors += b;
	}
	const int32 Kept = FellMatureKept(P, MatureSlots.Num(), Survivors);
	// Sobran enteros: se abren los últimos. Faltan: se salvan los primeros abiertos.
	for (int32 k = Intact.Num() - 1; k >= 0 && Survivors > Kept; --k)
	{
		if (Intact[k]) { Intact[k] = 0; --Survivors; }
	}
	for (int32 k = 0; k < Intact.Num() && Survivors < Kept; ++k)
	{
		if (!Intact[k]) { Intact[k] = 1; ++Survivors; }
	}
	for (int32 k = 0; k < MatureSlots.Num(); ++k)
	{
		const int32 i = MatureSlots[k];
		const int32 Gen = State.Slots[i].Generation;
		FCoconutDrop Drop;
		Drop.ItemId = Intact[k] ? MatureItem : ShellItem;
		// Esparcidos por la copa caída (la misma fórmula del anillo, pero con el centro en la copa).
		const double Angle = static_cast<double>(ExploredHash::ToUnitFloat(SlotHash(State.Seed, SaltAngle ^ SaltCrack, i, Gen))) * 2.0 * PI;
		const double Radius = FMath::Sqrt(static_cast<double>(ExploredHash::ToUnitFloat(SlotHash(State.Seed, SaltRadius ^ SaltCrack, i, Gen)))) * CrownCm;
		Drop.Position = Crown + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
		OutDrops.Add(Drop);
	}
	// Los verdes se pierden con el golpe: el agua del verde solo se consigue trepando (biblia 02 §13.1).
	for (int32 i = 0; i < State.Slots.Num(); ++i)
	{
		if (StageOf(State, P, i) != ECoconutStage::Empty)
		{
			Vacate(State.Slots[i], T);
			++State.Counters.Felled;
		}
	}
	State.bFelled = true;
	return Taken;
}

bool FCoconutPalmModel::IsValidSaved(const FCoconutPalmState& State, const FCoconutPalmProfile& InProfile)
{
	const FCoconutPalmProfile P = Sanitize(InProfile);
	const int64 Now = State.LastUpdateMinute;
	if (!InClockRange(Now) || State.Slots.Num() != P.Slots
		|| State.LastGustHour < -1 || State.LastGustHour > FMath::Max<int64>(-1, Now / 60)
		|| !FMath::IsFinite(State.TrunkPosition.X) || !FMath::IsFinite(State.TrunkPosition.Y))
	{
		return false;
	}
	for (const FCoconutSlot& Slot : State.Slots)
	{
		// El Id guarda 24 bits de generación: más allá, dos cocos del mismo hueco compartirían Id.
		if (Slot.Generation < 0 || Slot.Generation > 0xFFFFFF || !InCycleRange(Slot.CycleStartMinute) || Slot.CycleStartMinute > Now)
		{
			return false;
		}
	}
	TSet<uint32> Ids;
	for (int32 i = 0; i < State.Ground.Num(); ++i)
	{
		const FFallenCoconut& C = State.Ground[i];
		const int32 SlotIndex = static_cast<int32>(C.Id >> 24) - 1;
		const bool bDuplicate = Ids.Contains(C.Id);
		Ids.Add(C.Id);
		if (bDuplicate || !State.Slots.IsValidIndex(SlotIndex) || static_cast<int32>(C.Id & 0xFFFFFFu) >= State.Slots[SlotIndex].Generation
			|| !InClockRange(C.LandedMinute) || C.LandedMinute > Now
			|| !FMath::IsFinite(C.Position.X) || !FMath::IsFinite(C.Position.Y)
			|| (i > 0 && !SortGround(State.Ground[i - 1], C)))
		{
			return false;
		}
	}
	return true;
}
