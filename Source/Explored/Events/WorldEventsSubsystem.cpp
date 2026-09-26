#include "Events/WorldEventsSubsystem.h"

#include "Engine/World.h"
#include "Stats/Stats.h"
#include "Subsystems/SubsystemCollection.h"

#include "Sky/TimeOfDaySubsystem.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/ArchipelagoLayout.h"

namespace WorldEventsSubsystemDetail
{
	/** Semilla del calendario de eventos, derivada de la del mundo oficial. */
	constexpr uint32 EventsSeed = FArchipelagoLayout::OfficialSeed ^ 0xE7E47u;
	/** Cada cuánto se sondea el modelo (s reales): con días de 40 min son unos 18 s de juego. */
	constexpr float PollInterval = 0.5f;
}

bool UWorldEventsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UWorldEventsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UTimeOfDaySubsystem>();
	Collection.InitializeDependency<UExploredWeatherSubsystem>();
	// El modelo tiene su propio FWeatherModel con la misma semilla que el clima del mundo:
	// ve el mismo cielo planificado (no los estados forzados de depuración).
	Model = MakeUnique<FWorldEventsModel>(WorldEventsSubsystemDetail::EventsSeed, UExploredWeatherSubsystem::GetWorldWeatherSeed());
}

void UWorldEventsSubsystem::Deinitialize()
{
	OnEventStarted.Clear();
	OnEventEnded.Clear();
	Active.Reset();
	Model.Reset();
	Super::Deinitialize();
}

float UWorldEventsSubsystem::GetTotalDays() const
{
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	return Time ? Time->GetTotalDays() : 0.0f;
}

void UWorldEventsSubsystem::Tick(float DeltaTime)
{
	if (!Model)
	{
		return;
	}
	PollTimer -= DeltaTime;
	if (PollTimer > 0.0f && bHasPolled)
	{
		return;
	}
	PollTimer = WorldEventsSubsystemDetail::PollInterval;
	Poll(GetTotalDays());
}

void UWorldEventsSubsystem::Poll(float TotalDays)
{
	TArray<FWorldEvent> NewActive = Model->ActiveAt(TotalDays);

	TArray<FWorldEvent> Ended;
	TArray<FWorldEvent> Started;
	TArray<FWorldEvent> Skipped;
	for (const FWorldEvent& Event : Active)
	{
		if (!NewActive.Contains(Event))
		{
			Ended.Add(Event);
		}
	}
	for (const FWorldEvent& Event : NewActive)
	{
		if (!Active.Contains(Event))
		{
			Started.Add(Event);
		}
	}
	// Eventos que empezaron y acabaron entre dos sondeos (dormir, SetTime hacia delante):
	// se anuncian igualmente para que nadie se pierda un resultado.
	if (bHasPolled && TotalDays > LastPolledDays)
	{
		for (const FWorldEvent& Event : Model->EventsInWindow(LastPolledDays, TotalDays))
		{
			if (Event.Start > LastPolledDays && Event.End <= TotalDays)
			{
				Skipped.Add(Event);
			}
		}
	}

	// Se actualiza el estado antes de avisar: los oyentes pueden consultar el subsistema.
	Active = MoveTemp(NewActive);
	LastPolledDays = TotalDays;
	bHasPolled = true;

	for (const FWorldEvent& Event : Ended)
	{
		OnEventEnded.Broadcast(Event);
	}
	for (const FWorldEvent& Event : Skipped)
	{
		OnEventStarted.Broadcast(Event);
		OnEventEnded.Broadcast(Event);
	}
	for (const FWorldEvent& Event : Started)
	{
		OnEventStarted.Broadcast(Event);
	}
}

bool UWorldEventsSubsystem::IsEventActive(EWorldEventType Type, FWorldEvent* OutEvent) const
{
	for (const FWorldEvent& Event : Active)
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

bool UWorldEventsSubsystem::GetNextOccurrence(EWorldEventType Type, FWorldEvent& OutEvent) const
{
	return Model && Model->NextOccurrence(Type, GetTotalDays(), OutEvent);
}

TArray<FWorldEvent> UWorldEventsSubsystem::GetEventsInWindow(float FromDays, float ToDays) const
{
	return Model ? Model->EventsInWindow(FromDays, ToDays) : TArray<FWorldEvent>();
}

float UWorldEventsSubsystem::GetMoonPhase() const
{
	return FMoonModel::Phase(GetTotalDays());
}

EMoonPhase UWorldEventsSubsystem::GetNamedMoonPhase() const
{
	return FMoonModel::NamedPhase(GetMoonPhase());
}

float UWorldEventsSubsystem::GetMoonIllumination() const
{
	return FMoonModel::Illumination(GetMoonPhase());
}

float UWorldEventsSubsystem::GetBioluminescence() const
{
	return FWorldEventsModel::BioluminescenceAt(GetTotalDays());
}

float UWorldEventsSubsystem::GetExtremeTideDrawdown() const
{
	return Model ? Model->ExtremeTideDrawdown(GetTotalDays()) : 0.0f;
}

FEruptionSample UWorldEventsSubsystem::GetEruption() const
{
	FWorldEvent Eruption;
	return IsEventActive(EWorldEventType::MinorEruption, &Eruption)
		? FWorldEventsModel::SampleEruption(Eruption, GetTotalDays())
		: FEruptionSample();
}

bool UWorldEventsSubsystem::TryDropShipPackage(bool bSignalFireLit, FWorldEvent& OutShip)
{
	if (!Model || Model->DecideShipPackage(GetTotalDays(), bSignalFireLit, State, OutShip) != EShipSignalOutcome::DropPackage)
	{
		return false;
	}
	State.Consume(OutShip.Id);
	return true;
}

bool UWorldEventsSubsystem::TryDepositObsidian(const FWorldEvent& Eruption)
{
	if (!FWorldEventsModel::ShouldDepositObsidian(Eruption, GetTotalDays(), State))
	{
		return false;
	}
	State.Consume(Eruption.Id);
	return true;
}

void UWorldEventsSubsystem::LoadSaveState(const FWorldEventsState& InState)
{
	State = InState;
	// Tras cargar, el siguiente sondeo parte de cero y anuncia lo que ya esté activo.
	Active.Reset();
	bHasPolled = false;
	PollTimer = 0.0f;
}

TStatId UWorldEventsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWorldEventsSubsystem, STATGROUP_Tickables);
}
