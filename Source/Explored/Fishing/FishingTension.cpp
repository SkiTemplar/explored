#include "Fishing/FishingTension.h"

namespace FishingTensionDetail
{
	/** Carga de rotura de cada sedal (kgf) con una caña media. */
	constexpr float LineKgf[] = { 7.0f, 16.0f, 35.0f };

	/** Carga a la que se abre cada anzuelo (kgf), en el orden de EFishHook. */
	constexpr float HookKgf[] = { 5.0f, 6.0f, 7.5f, 8.0f, 25.0f, 45.0f };

	/** Holgura que perdona cada anzuelo (s): la lengüeta de alambre agarra mejor. */
	constexpr float HookGrace[] = { 0.9f, 1.0f, 1.1f, 1.2f, 1.6f, 2.2f };

	static_assert(static_cast<int32>(UE_ARRAY_COUNT(LineKgf)) == static_cast<int32>(EFishingLine::Count), "Un valor por sedal");
	static_assert(static_cast<int32>(UE_ARRAY_COUNT(HookKgf)) == static_cast<int32>(EFishHook::Count), "Un valor por anzuelo");
	static_assert(static_cast<int32>(UE_ARRAY_COUNT(HookGrace)) == static_cast<int32>(EFishHook::Count), "Un valor por anzuelo");

	/** Tirón en descanso y agotado, como fracción de la fuerza del pez. */
	constexpr float RestPull = 0.25f;
	constexpr float TiredPull = 0.08f;
	/** Velocidad a la que el pez se acerca nadando suave (m/s, negativa = hacia el pescador). */
	constexpr float RestDrift = -0.25f;
	constexpr float TiredDrift = -0.1f;

	int32 Index(uint8 V, int32 Count)
	{
		return FMath::Clamp(static_cast<int32>(V), 0, Count - 1);
	}
}

float FFishingTackle::LineBreakKgf() const
{
	using namespace FishingTensionDetail;
	const float Rod = FMath::Clamp(RodQuality01, 0.0f, 1.0f);
	return LineKgf[Index(static_cast<uint8>(Line), static_cast<int32>(UE_ARRAY_COUNT(LineKgf)))] * (0.9f + 0.2f * Rod);
}

float FFishingTackle::HookHoldKgf() const
{
	using namespace FishingTensionDetail;
	return HookKgf[Index(static_cast<uint8>(Hook), static_cast<int32>(UE_ARRAY_COUNT(HookKgf)))];
}

float FFishingTackle::BreakKgf() const
{
	return FMath::Min(LineBreakKgf(), HookHoldKgf());
}

float FFishingTackle::SlackGraceSeconds() const
{
	using namespace FishingTensionDetail;
	return HookGrace[Index(static_cast<uint8>(Hook), static_cast<int32>(UE_ARRAY_COUNT(HookGrace)))];
}

float FFishingTackle::ShockRate() const
{
	return 7.0f - 3.0f * FMath::Clamp(RodQuality01, 0.0f, 1.0f);
}

float FFishingTackle::ReelSpeedMps() const
{
	return 1.2f * (0.8f + 0.4f * FMath::Clamp(RodQuality01, 0.0f, 1.0f));
}

void FFishFightParams::ApplyTackle(const FFishingTackle& Tackle, float RockAbrasionPerSecond)
{
	AbrasionPerSecond = Tackle.ResistsAbrasion() ? 0.0f : FMath::Max(0.0f, RockAbrasionPerSecond);
	BreakKgf = Tackle.BreakKgf();
	SlackGraceSeconds = Tackle.SlackGraceSeconds();
	ShockRate = Tackle.ShockRate();
	ReelSpeedMps = Tackle.ReelSpeedMps();
}

FFishFight::FFishFight(const FFishFightParams& InParams, uint32 Seed)
	: Params(InParams)
	, Rng(Seed, 0xF15Bu)
{
	Params.BreakKgf = FMath::Max(0.1f, Params.BreakKgf);
	Params.StrengthKgf = FMath::Max(0.0f, Params.StrengthKgf);
	State.DistanceM = Params.StartDistanceM;
	State.Stamina = FMath::Max(0.0f, Params.StaminaSeconds);
	// Recién clavado el pez arranca casi siempre: el primer descanso es corto.
	StartRest();
	State.PhaseTimeLeft *= 0.3f;
	State.PullKgf = Params.StrengthKgf * FishingTensionDetail::RestPull;
	State.Tension01 = FMath::Clamp(State.PullKgf / Params.BreakKgf, 0.1f, 0.5f);
	if (State.Stamina <= 0.0f)
	{
		State.Phase = EFishFightPhase::Tired;
	}
}

void FFishFight::StartRest()
{
	State.Phase = EFishFightPhase::Resting;
	const float Calm = 1.3f - 0.6f * FMath::Clamp(Params.Aggression, 0.0f, 1.0f);
	State.PhaseTimeLeft = Rng.RangeFloat(1.5f, 3.5f) * Calm;
}

void FFishFight::StartBurst()
{
	State.Phase = EFishFightPhase::Burst;
	State.PhaseTimeLeft = Rng.RangeFloat(0.8f, 2.2f);
	State.BurstMultiplier = Rng.RangeFloat(0.85f, 1.25f);
}

EFishFightOutcome FFishFight::Tick(float DeltaSeconds, float ReelInput)
{
	float Remaining = FMath::Max(0.0f, DeltaSeconds);
	const float Input = FMath::Clamp(ReelInput, -1.0f, 1.0f);
	while (Remaining > 0.0f && !IsFinished())
	{
		const float Dt = FMath::Min(Remaining, MaxStepSeconds);
		Step(Dt, Input);
		Remaining -= Dt;
	}
	return State.Outcome;
}

void FFishFight::Step(float Dt, float ReelInput)
{
	using namespace FishingTensionDetail;
	State.ElapsedSeconds += Dt;

	// 1. Qué hace el pez.
	State.PhaseTimeLeft -= Dt;
	if (State.Phase != EFishFightPhase::Tired && State.Stamina <= 0.0f)
	{
		State.Phase = EFishFightPhase::Tired;
	}
	else if (State.Phase != EFishFightPhase::Tired && State.PhaseTimeLeft <= 0.0f)
	{
		if (State.Phase == EFishFightPhase::Burst)
		{
			StartRest();
		}
		else
		{
			StartBurst();
		}
	}

	float FishSpeed = RestDrift;
	switch (State.Phase)
	{
	case EFishFightPhase::Burst:
		State.PullKgf = Params.StrengthKgf * State.BurstMultiplier;
		FishSpeed = FMath::Min(4.0f, 1.2f + 0.12f * Params.StrengthKgf);
		break;
	case EFishFightPhase::Tired:
		State.PullKgf = Params.StrengthKgf * TiredPull;
		FishSpeed = TiredDrift;
		break;
	case EFishFightPhase::Resting:
	default:
		State.PullKgf = Params.StrengthKgf * RestPull;
		FishSpeed = RestDrift;
		break;
	}
	const float Pull01 = State.PullKgf / Params.BreakKgf;
	const bool bRunning = FishSpeed > 0.0f;

	// 2. Qué hace el jugador: tensión objetivo y sedal que entra o sale.
	float Target = Pull01;
	float LineRate = 0.0f;
	if (ReelInput > 0.05f)
	{
		// Recoger: al tirón del pez se suma el del carrete. En una arrancada
		// el pez aún gana algo de sedal aunque se recoja.
		Target = Pull01 + ReelEffort01 * ReelInput;
		LineRate = (bRunning ? FishSpeed * 0.4f : FishSpeed) - Params.ReelSpeedMps * ReelInput;
	}
	else if (ReelInput < -0.05f)
	{
		// Soltar sedal: el carrete gira libre; solo queda el roce.
		Target = 0.1f * Pull01;
		LineRate = FMath::Max(FishSpeed, 0.2f) * -ReelInput;
	}
	else
	{
		// Aguantar: en una arrancada la caña se dobla y cede un poco; si el
		// pez se acerca solo, el sedal se afloja.
		Target = bRunning ? Pull01 : Pull01 * 0.5f;
		LineRate = bRunning ? FishSpeed * 0.15f : FishSpeed;
	}

	State.Tension01 += (Target - State.Tension01) * FMath::Min(1.0f, Params.ShockRate * Dt);
	State.Tension01 = FMath::Max(0.0f, State.Tension01);
	State.DistanceM = FMath::Max(0.0f, State.DistanceM + LineRate * Dt);

	// 3. El pez se cansa: mucho en las arrancadas, algo más con el sedal tenso.
	if (State.Phase == EFishFightPhase::Burst)
	{
		State.Stamina -= Dt * (0.5f + 1.5f * State.Tension01);
		if (Params.AbrasionPerSecond > 0.0f && State.Tension01 > SlackTension01)
		{
			// El sedal tenso roza la roca: pierde resistencia y la misma carga pesa más.
			const float Wear = FMath::Min(0.5f, Params.AbrasionPerSecond * Dt);
			Params.BreakKgf = FMath::Max(0.1f, Params.BreakKgf * (1.0f - Wear));
			State.Tension01 /= (1.0f - Wear);
		}
	}
	else if (State.Phase == EFishFightPhase::Resting)
	{
		State.Stamina -= Dt * 0.8f * State.Tension01;
		if (State.Tension01 < 0.15f)
		{
			State.Stamina = FMath::Min(Params.StaminaSeconds, State.Stamina + 0.1f * Dt);
		}
	}

	// 4. Holgura con memoria: un instante flojo se perdona, varios seguidos no.
	if (State.Tension01 < SlackTension01)
	{
		State.SlackSeconds += Dt;
	}
	else
	{
		State.SlackSeconds = FMath::Max(0.0f, State.SlackSeconds - 2.0f * Dt);
	}

	// 5. Resultado.
	if (State.Tension01 >= 1.0f || State.DistanceM >= Params.SpoolLengthM)
	{
		State.Outcome = EFishFightOutcome::LineSnapped;
	}
	else if (State.SlackSeconds > Params.SlackGraceSeconds || State.ElapsedSeconds >= Params.MaxSeconds)
	{
		State.Outcome = EFishFightOutcome::Escaped;
	}
	else if (State.DistanceM <= LandDistanceM)
	{
		State.Outcome = EFishFightOutcome::Caught;
	}
}
