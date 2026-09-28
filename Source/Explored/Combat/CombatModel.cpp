#include "Combat/CombatModel.h"

#include "Core/ExploredRandom.h"
#include "Survival/BodyModel.h"

namespace
{
	/** Reloj máximo (2^52 ms, unos 140 000 años): sumarle cualquier duración no desborda int64. */
	constexpr int64 MaxClockMs = int64(1) << 52;
	/** Salud mínima que deja un golpe cuando el modo no deja morir (Explorador), igual que una caída. */
	constexpr float NonLethalFloor = 10.0f;
	/** Sal de la tirada del arco, para que no coincida con otras tiradas de la misma semilla. */
	constexpr int32 BowRollSalt = 0x41524331; // «ARC1»

	const FName NameFilo(TEXT("Filo"));
	const FName NamePunta(TEXT("Punta"));
	const FName NameContundente(TEXT("Contundente"));
	const FName NameSpear(TEXT("lanza"));

	/** Fichas de biblia 05 §5, en el orden de ECombatCreature. Espejo de combat.json «creatures». */
	const FCombatCreatureStats CreatureTable[] = {
		// Id, vida, tipo, propiedad, daño, corte, aturdimiento ms, huida por vida, huida por golpes, vuelco %
		{ TEXT("cerdo_salvaje"), 35, ECombatDamageKind::Blunt, 4, 16, 0.0f, 1000, 0.3f, 0, 0 },
		{ TEXT("cabra_salvaje"), 20, ECombatDamageKind::Blunt, 1, 4, 0.0f, 0, 1.0f, 0, 0 },
		{ TEXT("cangrejo_cocotero_salvaje"), 15, ECombatDamageKind::Blunt, 2, 8, 0.0f, 0, 0.0f, 1, 0 },
		{ TEXT("tiburon_arrecife"), 55, ECombatDamageKind::Cut, 4, 12, 0.6f, 0, 0.0f, 0, 15 },
	};
	static_assert(UE_ARRAY_COUNT(CreatureTable) == static_cast<int32>(ECombatCreature::Count),
		"una ficha por ECombatCreature");

	const FCombatCreatureStats EmptyCreature;

	int64 ClampClock(int64 Ms)
	{
		return FMath::Clamp<int64>(Ms, 0, MaxClockMs);
	}

	/** El reloj del estado solo avanza: un tiempo anterior (paquete desordenado) cuenta como el último visto. */
	int64 Tick(FCombatTimingState& S, int64 NowMs)
	{
		S.NowMs = FMath::Max(ClampClock(S.NowMs), ClampClock(NowMs));
		return S.NowMs;
	}

	void EnterPhase(FCombatTimingState& S, ECombatPhase Phase, int64 StartMs, int32 DurationMs)
	{
		S.Phase = Phase;
		S.PhaseStartMs = StartMs;
		S.PhaseEndMs = StartMs + DurationMs;
	}

	/** Cierra las fases vencidas hasta T. Un golpe cargado que llega queda pendiente para Advance. */
	void CloseExpired(FCombatTimingState& S, int64 T)
	{
		for (;;)
		{
			if (S.Phase == ECombatPhase::Idle || T < S.PhaseEndMs)
			{
				return;
			}
			const int64 End = S.PhaseEndMs;
			switch (S.Phase)
			{
			case ECombatPhase::QuickSwing:
				S.LastQuickEndMs = End;
				S.bHasQuick = true;
				if (S.ChainCount >= FCombatModel::QuickChainMax)
				{
					EnterPhase(S, ECombatPhase::ChainPause, End, FCombatModel::ChainPauseMs);
				}
				else
				{
					EnterPhase(S, ECombatPhase::Idle, End, 0);
				}
				break;
			case ECombatPhase::ChainPause:
				// La pausa rompe la racha: el siguiente golpe rápido vuelve a ser el primero.
				S.ChainCount = 0;
				S.bHasQuick = false;
				EnterPhase(S, ECombatPhase::Idle, End, 0);
				break;
			case ECombatPhase::Charging:
				// Solo el cargado que llega corta la racha: cargar y cancelar no la reinicia.
				S.ChainCount = 0;
				S.bHasQuick = false;
				S.bChargedImpactPending = true;
				S.ChargedImpactMs = End;
				EnterPhase(S, ECombatPhase::Recovery, End, FCombatModel::ChargeRecoveryMs);
				break;
			case ECombatPhase::Recovery:
			default:
				EnterPhase(S, ECombatPhase::Idle, End, 0);
				break;
			}
		}
	}

	FCombatActionResult Rejected(ECombatReject Reason)
	{
		FCombatActionResult R;
		R.Reject = Reason;
		return R;
	}

	FName PropertyName(ECombatDamageKind Kind)
	{
		switch (Kind)
		{
		case ECombatDamageKind::Cut: return NameFilo;
		case ECombatDamageKind::Pierce: return NamePunta;
		case ECombatDamageKind::Blunt: return NameContundente;
		default: return NAME_None;
		}
	}

	int32 DamagePerPoint(ECombatDamageKind Kind)
	{
		switch (Kind)
		{
		case ECombatDamageKind::Cut:
		case ECombatDamageKind::Pierce:
			return FCombatModel::CutDamagePerPoint;
		case ECombatDamageKind::Blunt:
			return FCombatModel::BluntDamagePerPoint;
		default:
			return 0;
		}
	}

	void PutU16(TArray<uint8>& Out, uint16 V)
	{
		Out.Add(static_cast<uint8>(V & 0xFF));
		Out.Add(static_cast<uint8>(V >> 8));
	}

	uint16 GetU16(const TArray<uint8>& In, int32 At)
	{
		return static_cast<uint16>(In[At] | (In[At + 1] << 8));
	}
}

const TCHAR* LexToString(ECombatDamageKind Kind)
{
	switch (Kind)
	{
	case ECombatDamageKind::None: return TEXT("None");
	case ECombatDamageKind::Cut: return TEXT("Cut");
	case ECombatDamageKind::Pierce: return TEXT("Pierce");
	case ECombatDamageKind::Blunt: return TEXT("Blunt");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(ECombatSwing Swing)
{
	switch (Swing)
	{
	case ECombatSwing::Plain: return TEXT("Plain");
	case ECombatSwing::Quick: return TEXT("Quick");
	case ECombatSwing::Charged: return TEXT("Charged");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(ECombatPhase Phase)
{
	switch (Phase)
	{
	case ECombatPhase::Idle: return TEXT("Idle");
	case ECombatPhase::QuickSwing: return TEXT("QuickSwing");
	case ECombatPhase::ChainPause: return TEXT("ChainPause");
	case ECombatPhase::Charging: return TEXT("Charging");
	case ECombatPhase::Recovery: return TEXT("Recovery");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(ECombatReject Reject)
{
	switch (Reject)
	{
	case ECombatReject::None: return TEXT("None");
	case ECombatReject::Busy: return TEXT("Busy");
	case ECombatReject::ChainPause: return TEXT("ChainPause");
	case ECombatReject::DodgeCooldown: return TEXT("DodgeCooldown");
	case ECombatReject::NotCharging: return TEXT("NotCharging");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(ECombatCreature Creature)
{
	switch (Creature)
	{
	case ECombatCreature::WildBoar: return TEXT("WildBoar");
	case ECombatCreature::WildGoat: return TEXT("WildGoat");
	case ECombatCreature::CoconutCrab: return TEXT("CoconutCrab");
	case ECombatCreature::ReefShark: return TEXT("ReefShark");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(ECombatNetAction Action)
{
	switch (Action)
	{
	case ECombatNetAction::QuickStrike: return TEXT("QuickStrike");
	case ECombatNetAction::ChargeStart: return TEXT("ChargeStart");
	case ECombatNetAction::ChargeCancel: return TEXT("ChargeCancel");
	case ECombatNetAction::Dodge: return TEXT("Dodge");
	case ECombatNetAction::BowShot: return TEXT("BowShot");
	default: return TEXT("Unknown");
	}
}

// --- Daño -------------------------------------------------------------------------------

int32 FCombatModel::PropertyTenths(const TMap<FName, float>& Properties, FName Name)
{
	const float* Value = Properties.Find(Name);
	if (!Value || !FMath::IsFinite(*Value) || *Value <= 0.0f)
	{
		return 0;
	}
	return FMath::RoundToInt(FMath::Min(*Value, static_cast<float>(MaxProperty)) * 10.0f);
}

FCombatStrike FCombatModel::StrikeOfKind(const TMap<FName, float>& Properties, ECombatDamageKind Kind)
{
	FCombatStrike Strike;
	const FName Name = PropertyName(Kind);
	if (Name.IsNone())
	{
		return Strike;
	}
	const int32 Tenths = PropertyTenths(Properties, Name);
	if (Tenths > 0)
	{
		Strike.Kind = Kind;
		Strike.PropertyTenths = Tenths;
	}
	return Strike;
}

FCombatStrike FCombatModel::BestStrike(const TMap<FName, float>& Properties)
{
	FCombatStrike Best;
	int32 BestDamage = 0;
	// Orden de desempate: Filo, Punta, Contundente (a igual daño, mejor lo que abre herida).
	for (const ECombatDamageKind Kind : { ECombatDamageKind::Cut, ECombatDamageKind::Pierce, ECombatDamageKind::Blunt })
	{
		const FCombatStrike Strike = StrikeOfKind(Properties, Kind);
		const int32 Damage = Strike.PropertyTenths * DamagePerPoint(Kind);
		if (Damage > BestDamage)
		{
			Best = Strike;
			BestDamage = Damage;
		}
	}
	return Best;
}

int32 FCombatModel::SwingMultiplierPct(ECombatSwing Swing)
{
	switch (Swing)
	{
	case ECombatSwing::Quick: return QuickMultiplierPct;
	case ECombatSwing::Charged: return ChargedMultiplierPct;
	default: return 100;
	}
}

FCombatHit FCombatModel::HitFor(const FCombatStrike& Strike, ECombatSwing Swing)
{
	FCombatHit Hit;
	const int32 PerPoint = DamagePerPoint(Strike.Kind);
	if (PerPoint <= 0 || Strike.PropertyTenths <= 0)
	{
		return Hit;
	}
	const int32 Tenths = FMath::Min(Strike.PropertyTenths, MaxProperty * 10);
	Hit.Kind = Strike.Kind;
	Hit.Swing = Swing < ECombatSwing::Count ? Swing : ECombatSwing::Plain;
	// Décimas × puntos × porcentaje / 1000: entero y con redondeo hacia abajo, como la tabla
	// de la biblia 05 §3.1 (Filo 3 rápido = 6,3 → 6; Punta 5 rápido = 10,5 → 10).
	Hit.HealthDamage = Tenths * PerPoint * SwingMultiplierPct(Hit.Swing) / 1000;
	if (Strike.Kind == ECombatDamageKind::Blunt)
	{
		Hit.StunMs = Tenths >= BluntStunMinProperty * 10 ? BluntStunMs : 0;
	}
	else
	{
		// La profundidad sale de la propiedad, no de la variante: un golpe cargado duele más, no corta más hondo.
		Hit.CutDepth = static_cast<float>(Tenths) / static_cast<float>(CutDepthDivisor * 10);
	}
	return Hit;
}

FCombatHit FCombatModel::HitWithItem(const TMap<FName, float>& Properties, ECombatSwing Swing)
{
	return HitFor(BestStrike(Properties), Swing);
}

float FCombatModel::MeleeReachM(FName DefinitionId)
{
	return DefinitionId == NameSpear ? ReachSpearM : ReachShortM;
}

// --- Tiempos ----------------------------------------------------------------------------

FCombatActionResult FCombatModel::TryQuick(FCombatTimingState& S, int64 NowMs)
{
	const int64 T = Tick(S, NowMs);
	CloseExpired(S, T);
	if (S.Phase == ECombatPhase::ChainPause)
	{
		return Rejected(ECombatReject::ChainPause);
	}
	if (S.Phase != ECombatPhase::Idle)
	{
		return Rejected(ECombatReject::Busy);
	}
	const bool bChains = S.bHasQuick && T - S.LastQuickEndMs < ChainPauseMs && S.ChainCount < QuickChainMax;
	S.ChainCount = bChains ? S.ChainCount + 1 : 1;
	EnterPhase(S, ECombatPhase::QuickSwing, T, QuickExecuteMs);

	FCombatActionResult R;
	R.bAccepted = true;
	R.Swing = ECombatSwing::Quick;
	R.ComboIndex = S.ChainCount;
	R.ImpactMs = T;
	return R;
}

FCombatActionResult FCombatModel::StartCharge(FCombatTimingState& S, int64 NowMs)
{
	const int64 T = Tick(S, NowMs);
	CloseExpired(S, T);
	if (S.Phase == ECombatPhase::ChainPause)
	{
		return Rejected(ECombatReject::ChainPause);
	}
	if (S.Phase != ECombatPhase::Idle)
	{
		return Rejected(ECombatReject::Busy);
	}
	// El cargado corta la racha de rápidos cuando llega (CloseExpired), no al empezar:
	// si no, cargar y cancelar daría rápidos sin la pausa obligatoria.
	EnterPhase(S, ECombatPhase::Charging, T, ChargeTelegraphMs);

	FCombatActionResult R;
	R.bAccepted = true;
	R.Swing = ECombatSwing::Charged;
	R.ImpactMs = S.PhaseEndMs;
	return R;
}

bool FCombatModel::CancelCharge(FCombatTimingState& S, int64 NowMs)
{
	const int64 T = Tick(S, NowMs);
	CloseExpired(S, T);
	if (S.Phase != ECombatPhase::Charging)
	{
		return false;
	}
	EnterPhase(S, ECombatPhase::Idle, T, 0);
	return true;
}

FCombatActionResult FCombatModel::TryDodge(FCombatTimingState& S, int64 NowMs)
{
	const int64 T = Tick(S, NowMs);
	CloseExpired(S, T);
	if (S.bHasDodged && T - S.DodgeStartMs < DodgeCooldownMs)
	{
		return Rejected(ECombatReject::DodgeCooldown);
	}
	if (S.Phase == ECombatPhase::QuickSwing || S.Phase == ECombatPhase::Recovery)
	{
		return Rejected(ECombatReject::Busy);
	}
	if (S.Phase == ECombatPhase::Charging)
	{
		EnterPhase(S, ECombatPhase::Idle, T, 0);
	}
	S.DodgeStartMs = T;
	S.bHasDodged = true;

	FCombatActionResult R;
	R.bAccepted = true;
	R.ImpactMs = T;
	return R;
}

bool FCombatModel::Advance(FCombatTimingState& S, int64 NowMs, FCombatActionResult& OutImpact)
{
	CloseExpired(S, Tick(S, NowMs));
	if (!S.bChargedImpactPending)
	{
		return false;
	}
	S.bChargedImpactPending = false;
	OutImpact = FCombatActionResult();
	OutImpact.bAccepted = true;
	OutImpact.Swing = ECombatSwing::Charged;
	OutImpact.ImpactMs = S.ChargedImpactMs;
	return true;
}

bool FCombatModel::IsInvulnerable(const FCombatTimingState& S, int64 NowMs)
{
	return S.bHasDodged && NowMs >= S.DodgeStartMs && NowMs - S.DodgeStartMs < DodgeInvulnerableMs;
}

bool FCombatModel::IsTelegraphing(const FCombatTimingState& S, int64 NowMs)
{
	return S.Phase == ECombatPhase::Charging && NowMs >= S.PhaseStartMs && NowMs < S.PhaseEndMs;
}

int32 FCombatModel::DodgeCooldownRemainingMs(const FCombatTimingState& S, int64 NowMs)
{
	if (!S.bHasDodged)
	{
		return 0;
	}
	const int64 Remaining = S.DodgeStartMs + DodgeCooldownMs - ClampClock(NowMs);
	return static_cast<int32>(FMath::Clamp<int64>(Remaining, 0, DodgeCooldownMs));
}

// --- Recibir ----------------------------------------------------------------------------

FCombatReceiveResult FCombatModel::ReceiveHit(FSurvivalState& Body, const FCombatTimingState& Defender, int64 NowMs,
	const FCombatHit& Hit, const FSurvivalModeSettings& Mode, TArray<ESurvivalEvent>& OutEvents)
{
	FCombatReceiveResult R;
	if (IsInvulnerable(Defender, ClampClock(NowMs)))
	{
		R.bDodged = true;
		return R;
	}
	if (Body.IsDead())
	{
		return R;
	}
	float Damage = static_cast<float>(FMath::Max(0, Hit.HealthDamage));
	if (!Mode.NeedsCanKill())
	{
		Damage = FMath::Min(Damage, FMath::Max(0.0f, Body.Health - NonLethalFloor));
	}
	const float Before = Body.Health;
	Body.Health = FMath::Clamp(Body.Health - Damage, 0.0f, 100.0f);
	R.HealthLost = Before - Body.Health;
	if (FMath::IsFinite(Hit.CutDepth) && Hit.CutDepth > 0.0f)
	{
		FBodyModel::AddCut(Body, Hit.CutDepth);
		R.bCutOpened = true;
	}
	if (R.HealthLost > 0.0f || R.bCutOpened)
	{
		FBodyModel::ApplyMoraleEvent(Body, EMoraleEvent::Injured);
	}
	if (Body.IsDead())
	{
		R.bDied = true;
		OutEvents.AddUnique(ESurvivalEvent::Died);
	}
	return R;
}

// --- Arco -------------------------------------------------------------------------------

int32 FCombatModel::BowAccuracyPct(float DistanceM)
{
	if (!FMath::IsFinite(DistanceM))
	{
		return 0;
	}
	const float D = FMath::Max(DistanceM, 0.0f);
	if (D < BowFullBandM)
	{
		return BowFullAccuracyPct;
	}
	if (D < BowMidBandM)
	{
		return BowMidAccuracyPct;
	}
	if (D < BowFarBandM)
	{
		return BowFarAccuracyPct;
	}
	return 0;
}

bool FCombatModel::BowShotHits(float DistanceM, uint32 Seed, uint32 ShotIndex)
{
	const int32 Accuracy = BowAccuracyPct(DistanceM);
	if (Accuracy <= 0)
	{
		return false;
	}
	if (Accuracy >= 100)
	{
		return true;
	}
	const uint32 Roll = ExploredHash::Hash3D(Seed, static_cast<int32>(ShotIndex), BowRollSalt, 0) % 100u;
	return Roll < static_cast<uint32>(Accuracy);
}

FCombatHit FCombatModel::ArrowHit(const TMap<FName, float>& ArrowProperties)
{
	return HitFor(StrikeOfKind(ArrowProperties, ECombatDamageKind::Pierce), ECombatSwing::Plain);
}

// --- Animales ---------------------------------------------------------------------------

const FCombatCreatureStats& FCombatModel::CreatureStats(ECombatCreature Creature)
{
	return Creature < ECombatCreature::Count ? CreatureTable[static_cast<int32>(Creature)] : EmptyCreature;
}

FCombatHit FCombatModel::CreatureAttack(ECombatCreature Creature)
{
	const FCombatCreatureStats& Stats = CreatureStats(Creature);
	FCombatHit Hit;
	if (Stats.AttackKind == ECombatDamageKind::None)
	{
		return Hit;
	}
	Hit.Kind = Stats.AttackKind;
	Hit.HealthDamage = Stats.AttackDamage;
	Hit.CutDepth = Stats.AttackCutDepth;
	Hit.StunMs = Stats.AttackStunMs;
	return Hit;
}

FCombatCreatureState FCombatModel::NewCreature(ECombatCreature Creature)
{
	FCombatCreatureState State;
	State.Health = CreatureStats(Creature).HealthPoints;
	State.bDead = State.Health <= 0;
	return State;
}

FCombatCreatureHitResult FCombatModel::HitCreature(FCombatCreatureState& State, ECombatCreature Creature, const FCombatHit& Hit)
{
	FCombatCreatureHitResult R;
	if (State.bDead)
	{
		return R;
	}
	const FCombatCreatureStats& Stats = CreatureStats(Creature);
	R.DamageDealt = FMath::Clamp(Hit.HealthDamage, 0, FMath::Max(0, State.Health));
	State.Health -= R.DamageDealt;
	++State.HitsTaken;
	if (State.Health <= 0)
	{
		State.Health = 0;
		State.bDead = true;
		State.bFleeing = false;
		R.bKilled = true;
		return R;
	}
	if (!State.bFleeing)
	{
		const bool bByHits = Stats.FleeAfterHits > 0 && State.HitsTaken >= Stats.FleeAfterHits;
		const bool bByHealth = Stats.FleeHealthFraction > 0.0f
			&& static_cast<float>(State.Health) <= Stats.FleeHealthFraction * static_cast<float>(Stats.HealthPoints);
		if (bByHits || bByHealth)
		{
			State.bFleeing = true;
			R.bStartedFleeing = true;
		}
	}
	return R;
}

// --- Red: validación --------------------------------------------------------------------

bool FCombatModel::ServerAcceptsReach(float DistanceM, float WeaponReachM)
{
	if (!FMath::IsFinite(DistanceM) || !FMath::IsFinite(WeaponReachM) || DistanceM < 0.0f || WeaponReachM <= 0.0f)
	{
		return false;
	}
	return DistanceM <= WeaponReachM + ServerReachMarginM;
}

bool FCombatModel::ServerAcceptsClientTime(uint16 ServerTimeMs, uint16 ClientTimeMs)
{
	// Edad del golpe módulo 65 536: un reloj del cliente por delante del servidor da una
	// edad enorme y se rechaza, igual que uno de hace más de 250 ms.
	const uint16 Age = static_cast<uint16>(ServerTimeMs - ClientTimeMs);
	return Age <= ServerGraceMs;
}

int32 FCombatModel::MinQuickCadenceMs()
{
	return QuickExecuteMs * (100 - CadenceTolerancePct) / 100;
}

// --- Red: codificación ------------------------------------------------------------------

void FCombatModel::EncodeAction(const FCombatActionMsg& Msg, TArray<uint8>& Out)
{
	Out.Reset(ActionMsgBytes);
	Out.Add(static_cast<uint8>(Msg.Action));
	PutU16(Out, Msg.ClientTimeMs);
	PutU16(Out, Msg.AimYaw);
	PutU16(Out, static_cast<uint16>(Msg.AimPitch));
}

bool FCombatModel::DecodeAction(const TArray<uint8>& In, FCombatActionMsg& OutMsg)
{
	if (In.Num() != ActionMsgBytes || In[0] >= static_cast<uint8>(ECombatNetAction::Count))
	{
		return false;
	}
	OutMsg.Action = static_cast<ECombatNetAction>(In[0]);
	OutMsg.ClientTimeMs = GetU16(In, 1);
	OutMsg.AimYaw = GetU16(In, 3);
	OutMsg.AimPitch = static_cast<int16>(GetU16(In, 5));
	return true;
}

FCombatImpactMsg FCombatModel::MakeImpact(uint16 AttackerId, uint16 VictimId, const FCombatHit& Hit, bool bDodged,
	bool bKilled, bool bFleeing)
{
	FCombatImpactMsg Msg;
	Msg.AttackerId = AttackerId;
	Msg.VictimId = VictimId;
	Msg.Kind = Hit.Kind < ECombatDamageKind::Count ? Hit.Kind : ECombatDamageKind::None;
	Msg.Swing = Hit.Swing < ECombatSwing::Count ? Hit.Swing : ECombatSwing::Plain;
	Msg.bDodged = bDodged;
	Msg.bKilled = bKilled && !bDodged;
	Msg.bFleeing = bFleeing;
	if (bDodged)
	{
		return Msg;
	}
	Msg.HealthDamage = static_cast<uint8>(FMath::Clamp(Hit.HealthDamage, 0, 255));
	const float Depth = FMath::IsFinite(Hit.CutDepth) ? FMath::Clamp(Hit.CutDepth, 0.0f, 1.0f) : 0.0f;
	Msg.CutDepth255 = static_cast<uint8>(FMath::RoundToInt(Depth * 255.0f));
	Msg.StunDeciseconds = static_cast<uint8>(FMath::Clamp<int64>((static_cast<int64>(FMath::Max(0, Hit.StunMs)) + 50) / 100, 0, 255));
	return Msg;
}

void FCombatModel::EncodeImpact(const FCombatImpactMsg& Msg, TArray<uint8>& Out)
{
	Out.Reset(ImpactMsgBytes);
	PutU16(Out, Msg.AttackerId);
	PutU16(Out, Msg.VictimId);
	const uint8 Flags = static_cast<uint8>((static_cast<uint8>(Msg.Kind) & 0x3)
		| ((static_cast<uint8>(Msg.Swing) & 0x3) << 2)
		| (Msg.bDodged ? 0x10 : 0)
		| (Msg.bKilled ? 0x20 : 0)
		| (Msg.bFleeing ? 0x40 : 0));
	Out.Add(Flags);
	Out.Add(Msg.HealthDamage);
	Out.Add(Msg.CutDepth255);
	Out.Add(Msg.StunDeciseconds);
}

bool FCombatModel::DecodeImpact(const TArray<uint8>& In, FCombatImpactMsg& OutMsg)
{
	if (In.Num() != ImpactMsgBytes)
	{
		return false;
	}
	const uint8 Flags = In[4];
	const uint8 Swing = (Flags >> 2) & 0x3;
	if ((Flags & 0x80) != 0 || Swing >= static_cast<uint8>(ECombatSwing::Count))
	{
		return false;
	}
	FCombatImpactMsg Msg;
	Msg.AttackerId = GetU16(In, 0);
	Msg.VictimId = GetU16(In, 2);
	Msg.Kind = static_cast<ECombatDamageKind>(Flags & 0x3);
	Msg.Swing = static_cast<ECombatSwing>(Swing);
	Msg.bDodged = (Flags & 0x10) != 0;
	Msg.bKilled = (Flags & 0x20) != 0;
	Msg.bFleeing = (Flags & 0x40) != 0;
	Msg.HealthDamage = In[5];
	Msg.CutDepth255 = In[6];
	Msg.StunDeciseconds = In[7];
	// Un golpe esquivado no hace nada: si trae daño, el paquete está manipulado.
	if (Msg.bDodged && (Msg.bKilled || Msg.HealthDamage || Msg.CutDepth255 || Msg.StunDeciseconds))
	{
		return false;
	}
	OutMsg = Msg;
	return true;
}

FCombatNetState FCombatModel::MakeNetState(const FCombatTimingState& State, int64 NowMs)
{
	FCombatTimingState Copy = State;
	const int64 T = Tick(Copy, NowMs);
	CloseExpired(Copy, T);
	FCombatNetState Net;
	Net.Phase = Copy.Phase;
	Net.ChainCount = static_cast<uint8>(FMath::Clamp(Copy.ChainCount, 0, QuickChainMax));
	if (Copy.Phase != ECombatPhase::Idle)
	{
		Net.ElapsedDeciseconds = static_cast<uint8>(FMath::Clamp<int64>((T - Copy.PhaseStartMs) / 100, 0, 255));
	}
	return Net;
}

void FCombatModel::EncodeNetState(const FCombatNetState& Msg, TArray<uint8>& Out)
{
	Out.Reset(NetStateBytes);
	Out.Add(static_cast<uint8>((static_cast<uint8>(Msg.Phase) & 0x7) | ((Msg.ChainCount & 0x3) << 3)));
	Out.Add(Msg.ElapsedDeciseconds);
}

bool FCombatModel::DecodeNetState(const TArray<uint8>& In, FCombatNetState& OutMsg)
{
	if (In.Num() != NetStateBytes)
	{
		return false;
	}
	const uint8 Phase = In[0] & 0x7;
	if ((In[0] & 0xE0) != 0 || Phase >= static_cast<uint8>(ECombatPhase::Count))
	{
		return false;
	}
	OutMsg.Phase = static_cast<ECombatPhase>(Phase);
	OutMsg.ChainCount = (In[0] >> 3) & 0x3;
	OutMsg.ElapsedDeciseconds = In[1];
	return true;
}
