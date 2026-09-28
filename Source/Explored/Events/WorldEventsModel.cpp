#include "Events/WorldEventsModel.h"

#include "Core/ExploredRandom.h"
#include "Ocean/OceanCurrents.h"
#include "Sky/MoonModel.h"

namespace WorldEventsDetail
{
	constexpr float HoursToDays = 1.0f / 24.0f;

	/** Sal de los hashes por uso, para que añadir un tipo nuevo no cambie los demás. */
	constexpr int32 CandidateSalt = 0x5EED;
	constexpr int32 EventSeedSalt = 0xE5EED;
	constexpr int32 WhaleBlockSalt = 0xBA1E;

	/** Duración máxima de cualquier evento (días): acota qué días de anclaje hay que mirar. */
	constexpr float MaxEventDays = 1.0f;

	/** Umbral de FOceanTide::SpringNeapFactorAt que marca el instante de marea viva. */
	constexpr float SpringTidePeakFactor = 0.99f;

	/**
	 * Bajamar diurna tras la marea viva de medianoche: FOceanTide::Level es
	 * sin(4·π·T), con mínimos en T = 3/8 + n/2, es decir, a las 09:00 y a las 21:00.
	 */
	constexpr float MorningLowTideDays = 0.375f;
	constexpr float ExtremeTideHalfWindowDays = 2.0f / 24.0f;

	FExploredRandom MakeRng(uint32 Seed, int32 Anchor, EWorldEventType Type, int32 Salt)
	{
		return FExploredRandom(ExploredHash::Hash3D(Seed, Anchor, static_cast<int32>(Type), Salt));
	}

	uint32 EventSeed(uint32 Seed, int32 Anchor, EWorldEventType Type)
	{
		return ExploredHash::Hash3D(Seed, Anchor, static_cast<int32>(Type), EventSeedSalt);
	}

	FVector2D RandomBearing(FExploredRandom& Rng)
	{
		const float Angle = Rng.NextFloat() * UE_TWO_PI;
		return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
	}

	/**
	 * Instantes donde evaluar el clima para conocer todo [Start, End): el
	 * inicio y cada frontera de bloque dentro del intervalo. StateAt es
	 * constante a trozos entre fronteras, así que la comprobación es exacta
	 * (no depende de un paso de muestreo).
	 */
	void CollectWeatherProbes(const FWeatherModel& Weather, float Start, float End, TArray<float>& OutProbes)
	{
		OutProbes.Reset();
		OutProbes.Add(Start);
		const int32 FirstDay = FMath::Max(0, FMath::FloorToInt32(Start) - 1);
		const int32 LastDay = FMath::FloorToInt32(End);
		for (int32 Day = FirstDay; Day <= LastDay; ++Day)
		{
			for (const FWeatherSpan& Span : Weather.SpansForDay(Day))
			{
				for (const float Boundary : {Span.Start, Span.End})
				{
					if (Boundary > Start && Boundary < End)
					{
						OutProbes.Add(Boundary);
					}
				}
			}
		}
	}

	bool IsOneOf(EWeatherState State, std::initializer_list<EWeatherState> States)
	{
		for (const EWeatherState Candidate : States)
		{
			if (State == Candidate)
			{
				return true;
			}
		}
		return false;
	}

	/** Oscuridad en [0, 1] por la hora local: 1 de noche, 0 de día, rampas al anochecer y al amanecer. */
	float Darkness(float TotalDays)
	{
		const float Hours = FMath::Frac(TotalDays) * 24.0f;
		const float Dusk = FMath::SmoothStep(17.5f, 19.0f, Hours);
		const float Dawn = 1.0f - FMath::SmoothStep(5.0f, 6.5f, Hours);
		return FMath::Max(Dusk, Dawn);
	}

	void SortByStart(TArray<FWorldEvent>& Events)
	{
		Events.StableSort([](const FWorldEvent& A, const FWorldEvent& B) { return A.Start < B.Start; });
	}
}

const TCHAR* LexToString(EWorldEventType Type)
{
	switch (Type)
	{
	case EWorldEventType::TurtleNesting: return TEXT("TurtleNesting");
	case EWorldEventType::TurtleHatchlings: return TEXT("TurtleHatchlings");
	case EWorldEventType::Bioluminescence: return TEXT("Bioluminescence");
	case EWorldEventType::MeteorShower: return TEXT("MeteorShower");
	case EWorldEventType::WhalePassage: return TEXT("WhalePassage");
	case EWorldEventType::ShipOnHorizon: return TEXT("ShipOnHorizon");
	case EWorldEventType::MinorEruption: return TEXT("MinorEruption");
	case EWorldEventType::ExtremeSpringTide: return TEXT("ExtremeSpringTide");
	default: return TEXT("Unknown");
	}
}

float FWorldEvent::ProgressAt(float TotalDays) const
{
	const float Length = End - Start;
	return Length > 0.0f ? FMath::Clamp((TotalDays - Start) / Length, 0.0f, 1.0f) : 0.0f;
}

FWorldEventsModel::FWorldEventsModel(uint32 InSeed, uint32 InWeatherSeed)
	: Seed(InSeed)
	, Weather(InWeatherSeed)
{
}

uint64 FWorldEventsModel::MakeEventId(EWorldEventType Type, int32 AnchorDay)
{
	return (static_cast<uint64>(Type) << 56) | static_cast<uint64>(static_cast<uint32>(AnchorDay));
}

bool FWorldEventsModel::IsWindowFreeOf(float Start, float End, std::initializer_list<EWeatherState> Forbidden) const
{
	TArray<float> Probes;
	WorldEventsDetail::CollectWeatherProbes(Weather, Start, End, Probes);
	for (const float T : Probes)
	{
		if (WorldEventsDetail::IsOneOf(Weather.StateAt(T), Forbidden))
		{
			return false;
		}
	}
	return true;
}

bool FWorldEventsModel::IsWindowOnly(float Start, float End, std::initializer_list<EWeatherState> Allowed) const
{
	TArray<float> Probes;
	WorldEventsDetail::CollectWeatherProbes(Weather, Start, End, Probes);
	for (const float T : Probes)
	{
		if (!WorldEventsDetail::IsOneOf(Weather.StateAt(T), Allowed))
		{
			return false;
		}
	}
	return true;
}

bool FWorldEventsModel::HasTurtleNesting(int32 Day) const
{
	if (Day < 0)
	{
		return false;
	}
	// La noche de desove es la que contiene la luna llena exacta (medianoche del día Day + 1).
	const int32 NightMidnight = Day + 1;
	const float FullMoon = FMoonModel::FullMoonOfCycle(FMoonModel::CycleIndex(static_cast<float>(NightMidnight)));
	if (FMath::FloorToInt32(FullMoon) != NightMidnight)
	{
		return false;
	}
	// Con temporal las tortugas no suben a la playa.
	const float Start = Day + FWorldEventsModel::NightStartHour * WorldEventsDetail::HoursToDays;
	const float End = NightMidnight + FWorldEventsModel::NightEndHour * WorldEventsDetail::HoursToDays;
	return IsWindowFreeOf(Start, End, {EWeatherState::Cyclone, EWeatherState::Gale});
}

bool FWorldEventsModel::WhaleForBlock(int32 Block, FWorldEvent& OutEvent) const
{
	using namespace WorldEventsDetail;
	if (Block < 0)
	{
		return false;
	}
	// Un paso por bloque de WhaleCadenceDays días, en un día al azar del bloque: la media
	// entre pasos es WhaleCadenceDays. Todo se sortea antes de mirar el clima.
	FExploredRandom Rng = MakeRng(Seed, Block, EWorldEventType::WhalePassage, WhaleBlockSalt);
	const int32 PlannedDay = Block * WhaleCadenceDays + Rng.RangeInt(2, WhaleCadenceDays - 3);
	const float StartHour = Rng.RangeFloat(9.0f, 12.0f);
	const float DurationHours = Rng.RangeFloat(3.0f, 5.0f);
	const float Intensity = Rng.RangeFloat(0.4f, 1.0f);
	const FVector2D Direction = RandomBearing(Rng);

	// Con temporal o niebla no se ven: el grupo pasa algún día más tarde, sin salir del bloque.
	const int32 LastDayOfBlock = (Block + 1) * WhaleCadenceDays - 1;
	for (int32 Day = PlannedDay; Day <= LastDayOfBlock; ++Day)
	{
		const float Start = Day + StartHour * HoursToDays;
		const float End = Start + DurationHours * HoursToDays;
		if (IsWindowFreeOf(Start, End, {EWeatherState::Cyclone, EWeatherState::Gale, EWeatherState::Thunderstorm, EWeatherState::MorningFog}))
		{
			OutEvent.Type = EWorldEventType::WhalePassage;
			OutEvent.Start = Start;
			OutEvent.End = End;
			OutEvent.Id = MakeEventId(EWorldEventType::WhalePassage, Day);
			OutEvent.Island = EIslandArchetype::Count;
			OutEvent.Intensity = Intensity;
			OutEvent.Direction = Direction;
			OutEvent.Seed = EventSeed(Seed, Day, EWorldEventType::WhalePassage);
			return true;
		}
	}
	return false;
}

void FWorldEventsModel::GenerateDay(int32 Day, TArray<FWorldEvent>& Out) const
{
	using namespace WorldEventsDetail;
	if (Day < 0)
	{
		return;
	}
	const float D = static_cast<float>(Day);

	auto MakeEvent = [this, Day](EWorldEventType Type, float Start, float End) {
		FWorldEvent Event;
		Event.Type = Type;
		Event.Start = Start;
		Event.End = End;
		Event.Id = MakeEventId(Type, Day);
		Event.Seed = EventSeed(Seed, Day, Type);
		return Event;
	};
	auto CandidateRoll = [this, Day](EWorldEventType Type) {
		return ExploredHash::ToUnitFloat(ExploredHash::Hash3D(Seed, Day, static_cast<int32>(Type), CandidateSalt));
	};

	const float NightStart = D + NightStartHour * HoursToDays;
	const float NightEnd = D + 1.0f + NightEndHour * HoursToDays;

	// Desove de tortugas: la noche de luna llena, en Arenas Blancas.
	if (HasTurtleNesting(Day))
	{
		FWorldEvent Event = MakeEvent(EWorldEventType::TurtleNesting, NightStart, NightEnd);
		Event.Island = EIslandArchetype::WhiteSands;
		Event.Intensity = MakeRng(Seed, Day, EWorldEventType::TurtleNesting, CandidateSalt).RangeFloat(0.5f, 1.0f);
		Out.Add(Event);
	}

	// Crías hacia el mar al amanecer que sigue a la noche de desove.
	if (HasTurtleNesting(Day - 1))
	{
		FWorldEvent Event = MakeEvent(EWorldEventType::TurtleHatchlings, D + DawnStartHour * HoursToDays, D + DawnEndHour * HoursToDays);
		Event.Island = EIslandArchetype::WhiteSands;
		Out.Add(Event);
	}

	// Bioluminiscencia máxima: la noche que contiene la luna nueva exacta.
	if (FMath::FloorToInt32(FMoonModel::NewMoonOfCycle(FMoonModel::CycleIndex(D + 1.0f))) == Day + 1)
	{
		FWorldEvent Event = MakeEvent(EWorldEventType::Bioluminescence, NightStart, NightEnd);
		Event.Intensity = FMoonModel::Bioluminescence(FMoonModel::Phase(D + 1.0f));
		Out.Add(Event);
	}

	// Lluvia de estrellas: rara y solo con la noche entera despejada.
	if (CandidateRoll(EWorldEventType::MeteorShower) < MeteorChancePerNight)
	{
		const float Start = D + MeteorStartHour * HoursToDays;
		const float End = D + 1.0f + MeteorEndHour * HoursToDays;
		if (IsWindowOnly(Start, End, {EWeatherState::Clear, EWeatherState::HeatWave}))
		{
			FWorldEvent Event = MakeEvent(EWorldEventType::MeteorShower, Start, End);
			Event.Intensity = MakeRng(Seed, Day, EWorldEventType::MeteorShower, CandidateSalt).RangeFloat(0.5f, 1.0f);
			Out.Add(Event);
		}
	}

	// Paso de ballenas: uno por bloque de WhaleCadenceDays días.
	{
		FWorldEvent Whale;
		if (WhaleForBlock(Day / WhaleCadenceDays, Whale) && FMath::FloorToInt32(Whale.Start) == Day)
		{
			Out.Add(Whale);
		}
	}

	// Barco en el horizonte: a cualquier hora (de noche se ven sus luces), nunca con ciclón ni galerna.
	if (CandidateRoll(EWorldEventType::ShipOnHorizon) < ShipChancePerDay)
	{
		FExploredRandom Rng = MakeRng(Seed, Day, EWorldEventType::ShipOnHorizon, CandidateSalt);
		const float Start = D + Rng.NextFloat();
		const float End = Start + Rng.RangeFloat(0.1f, 0.15f);
		const float Intensity = Rng.RangeFloat(0.4f, 1.0f);
		const FVector2D Direction = RandomBearing(Rng);
		if (IsWindowFreeOf(Start, End, {EWeatherState::Cyclone, EWeatherState::Gale}))
		{
			FWorldEvent Event = MakeEvent(EWorldEventType::ShipOnHorizon, Start, End);
			Event.Intensity = Intensity;
			Event.Direction = Direction;
			Out.Add(Event);
		}
	}

	// Erupción menor en la Isla del Humo: al azar, sin depender del clima (nunca el primer par de días).
	if (Day >= 2 && CandidateRoll(EWorldEventType::MinorEruption) < EruptionChancePerDay)
	{
		FExploredRandom Rng = MakeRng(Seed, Day, EWorldEventType::MinorEruption, CandidateSalt);
		const float Start = D + Rng.NextFloat();
		const float End = Start + Rng.RangeFloat(0.15f, 0.3f);
		FWorldEvent Event = MakeEvent(EWorldEventType::MinorEruption, Start, End);
		Event.Island = EIslandArchetype::Smoke;
		Event.Intensity = Rng.RangeFloat(0.3f, 1.0f);
		Out.Add(Event);
	}

	// Marea viva extrema: solo el día cuya medianoche es el pico de marea viva según
	// FOceanTide (luna nueva o llena), en la bajamar de la mañana. Más probable en la
	// estación seca (biblia §6.1, «mareas vivas»).
	if (Day > 0 && FOceanTide::SpringNeapFactorAt(D) >= SpringTidePeakFactor)
	{
		const float Chance = FWeatherModel::SeasonForDay(D) == ESeason::Dry ? ExtremeTideChanceDry : ExtremeTideChanceOther;
		if (CandidateRoll(EWorldEventType::ExtremeSpringTide) < Chance)
		{
			const float LowTide = D + MorningLowTideDays;
			const float Start = LowTide - ExtremeTideHalfWindowDays;
			const float End = LowTide + ExtremeTideHalfWindowDays;
			if (IsWindowFreeOf(Start, End, {EWeatherState::Cyclone, EWeatherState::Gale}))
			{
				FWorldEvent Event = MakeEvent(EWorldEventType::ExtremeSpringTide, Start, End);
				Event.Intensity = MakeRng(Seed, Day, EWorldEventType::ExtremeSpringTide, CandidateSalt).RangeFloat(0.5f, 1.0f);
				Out.Add(Event);
			}
		}
	}

	SortByStart(Out);
}

TArray<FWorldEvent> FWorldEventsModel::EventsStartingOnDay(int32 Day) const
{
	TArray<FWorldEvent> Events;
	GenerateDay(Day, Events);
	return Events;
}

TArray<FWorldEvent> FWorldEventsModel::ActiveAt(float TotalDays) const
{
	return EventsInWindow(TotalDays, TotalDays);
}

bool FWorldEventsModel::IsActive(EWorldEventType Type, float TotalDays, FWorldEvent* OutEvent) const
{
	for (const FWorldEvent& Event : ActiveAt(TotalDays))
	{
		if (Event.Type == Type)
		{
			if (OutEvent)
			{
				*OutEvent = Event;
			}
			return true;
		}
	}
	return false;
}

void FWorldEventsModel::ForEachInWindow(float FromDays, float ToDays, TFunctionRef<bool(const FWorldEvent&)> Visitor) const
{
	// Relojes no finitos o fuera de partida (estado corrupto): sin acotar, un reloj de 1e9
	// generaba 1e9 días y FloorToInt32 o ++Day desbordaban.
	if (!FMath::IsFinite(FromDays) || !FMath::IsFinite(ToDays))
	{
		return;
	}
	const float MaxDays = static_cast<float>(FWeatherModel::MaxSupportedDays);
	FromDays = FMath::Clamp(FromDays, -1.0f, MaxDays);
	ToDays = FMath::Clamp(ToDays, -1.0f, MaxDays);
	// Un evento anclado al día D empieza en [D, D + 1) y dura menos de MaxEventDays: basta
	// con mirar desde el día anterior a FromDays. Los días salen en orden y cada día ya viene
	// ordenado, así que la visita queda ordenada por inicio.
	const int32 FirstDay = FMath::Max(0, FMath::FloorToInt32(FromDays - WorldEventsDetail::MaxEventDays));
	const int32 LastDay = FMath::FloorToInt32(ToDays);
	TArray<FWorldEvent> DayEvents;
	for (int32 Day = FirstDay; Day <= LastDay; ++Day)
	{
		DayEvents.Reset();
		GenerateDay(Day, DayEvents);
		for (const FWorldEvent& Event : DayEvents)
		{
			// Un instante (FromDays == ToDays) cuenta como la ventana [T, T]: activo si Start <= T < End.
			const bool bOverlaps = FromDays < ToDays
				? (Event.Start < ToDays && Event.End > FromDays)
				: Event.IsActiveAt(FromDays);
			if (bOverlaps && !Visitor(Event))
			{
				return;
			}
		}
	}
}

TArray<FWorldEvent> FWorldEventsModel::EventsInWindow(float FromDays, float ToDays) const
{
	TArray<FWorldEvent> Result;
	ForEachInWindow(FromDays, ToDays, [&Result](const FWorldEvent& Event) {
		Result.Add(Event);
		return true;
	});
	return Result;
}

bool FWorldEventsModel::NextOccurrence(EWorldEventType Type, float AfterDays, FWorldEvent& OutEvent, int32 SearchDays) const
{
	// Reloj y búsqueda acotados a MaxSupportedDays: FloorToInt32(3e9) y FirstDay + SearchDays desbordaban.
	if (!FMath::IsFinite(AfterDays))
	{
		return false;
	}
	AfterDays = FMath::Min(AfterDays, static_cast<float>(FWeatherModel::MaxSupportedDays));
	const int32 FirstDay = FMath::Max(0, FMath::FloorToInt32(FMath::Max(AfterDays, -1.0f)));
	const int32 LastDay = FirstDay + FMath::Clamp(SearchDays, 0, FWeatherModel::MaxSupportedDays);
	TArray<FWorldEvent> DayEvents;
	for (int32 Day = FirstDay; Day <= LastDay; ++Day)
	{
		DayEvents.Reset();
		GenerateDay(Day, DayEvents);
		for (const FWorldEvent& Event : DayEvents)
		{
			if (Event.Type == Type && Event.Start > AfterDays)
			{
				OutEvent = Event;
				return true;
			}
		}
	}
	return false;
}

EShipSignalOutcome FWorldEventsModel::DecideShipPackage(float TotalDays, bool bSignalFireLit, const FWorldEventsState& State, FWorldEvent& OutShip) const
{
	if (!IsActive(EWorldEventType::ShipOnHorizon, TotalDays, &OutShip))
	{
		return EShipSignalOutcome::NoShip;
	}
	if (State.IsConsumed(OutShip.Id))
	{
		return EShipSignalOutcome::AlreadyDropped;
	}
	return bSignalFireLit ? EShipSignalOutcome::DropPackage : EShipSignalOutcome::NoSignal;
}

FEruptionSample FWorldEventsModel::SampleEruption(const FWorldEvent& Eruption, float TotalDays)
{
	FEruptionSample Sample;
	if (Eruption.Type != EWorldEventType::MinorEruption || !Eruption.IsActiveAt(TotalDays))
	{
		return Sample;
	}
	const float P = Eruption.ProgressAt(TotalDays);
	// Temblor: aviso creciente, sacudida máxima en el estallido (30 %) y calma hacia el 60 %.
	float Tremor;
	if (P < 0.25f)
	{
		Tremor = 0.5f * FMath::SmoothStep(0.0f, 0.25f, P);
	}
	else if (P < 0.35f)
	{
		Tremor = FMath::Lerp(0.5f, 1.0f, FMath::SmoothStep(0.25f, 0.3f, P));
	}
	else
	{
		Tremor = 1.0f - FMath::SmoothStep(0.35f, 0.6f, P);
	}
	// Ceniza: empieza con el estallido, llena el aire hacia el 45 % y se posa despacio.
	const float Ash = FMath::SmoothStep(0.28f, 0.45f, P) * FMath::Lerp(1.0f, 0.2f, FMath::SmoothStep(0.45f, 1.0f, P));
	Sample.Tremor = Tremor * Eruption.Intensity;
	Sample.Ash = Ash * Eruption.Intensity;
	return Sample;
}

bool FWorldEventsModel::ShouldDepositObsidian(const FWorldEvent& Eruption, float TotalDays, const FWorldEventsState& State)
{
	return Eruption.Type == EWorldEventType::MinorEruption && TotalDays >= Eruption.End && !State.IsConsumed(Eruption.Id);
}

float FWorldEventsModel::ExtremeTideDrawdown(float TotalDays) const
{
	FWorldEvent Tide;
	if (!IsActive(EWorldEventType::ExtremeSpringTide, TotalDays, &Tide))
	{
		return 0.0f;
	}
	// Campana suave centrada en la bajamar: cero en los bordes de la ventana.
	return Tide.Intensity * FMath::Square(FMath::Sin(UE_PI * Tide.ProgressAt(TotalDays)));
}

float FWorldEventsModel::BioluminescenceAt(float TotalDays)
{
	return FMoonModel::Bioluminescence(FMoonModel::Phase(TotalDays)) * WorldEventsDetail::Darkness(TotalDays);
}
