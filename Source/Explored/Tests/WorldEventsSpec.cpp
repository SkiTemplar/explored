#include "Misc/AutomationTest.h"

#include "Events/WorldEventsModel.h"
#include "Ocean/OceanCurrents.h"
#include "Sky/MoonModel.h"
#include "Weather/WeatherModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace WorldEventsSpecDetail
{
	constexpr uint32 EventSeed = 20260926;
	constexpr uint32 WeatherSeed = 99;
	/** Veinte años de juego: suficiente para ver los eventos raros varias veces. */
	constexpr float Horizon = 20.0f * FWeatherModel::DaysPerYear;

	TArray<FWorldEvent> OfType(const TArray<FWorldEvent>& Events, EWorldEventType Type)
	{
		TArray<FWorldEvent> Result;
		for (const FWorldEvent& Event : Events)
		{
			if (Event.Type == Type)
			{
				Result.Add(Event);
			}
		}
		return Result;
	}

	float HourOf(float TotalDays)
	{
		return FMath::Frac(TotalDays) * 24.0f;
	}

	/** ¿Algún instante de [Start, End), muestreado cada 0.005 días, cae en uno de esos estados? */
	bool AnyStateIn(const FWeatherModel& Weather, const FWorldEvent& Event, std::initializer_list<EWeatherState> States)
	{
		for (float T = Event.Start; T < Event.End; T += 0.005f)
		{
			const EWeatherState State = Weather.StateAt(T);
			for (const EWeatherState Candidate : States)
			{
				if (State == Candidate)
				{
					return true;
				}
			}
		}
		return false;
	}
}

BEGIN_DEFINE_SPEC(FWorldEventsSpec, "Explored.WorldEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	TUniquePtr<FWorldEventsModel> Model;
	/** Todos los eventos del horizonte de prueba (se generan una vez por caso). */
	TArray<FWorldEvent> All;
END_DEFINE_SPEC(FWorldEventsSpec)

void FWorldEventsSpec::Define()
{
	using namespace WorldEventsSpecDetail;

	BeforeEach([this]()
	{
		Model = MakeUnique<FWorldEventsModel>(EventSeed, WeatherSeed);
		All = Model->EventsInWindow(0.0f, Horizon);
	});

	AfterEach([this]()
	{
		All.Empty();
		Model = nullptr;
	});

	Describe("Luna", [this]()
	{
		It("hace desovar a las tortugas solo de noche con luna llena, en Arenas Blancas", [this]()
		{
			const TArray<FWorldEvent> Nests = OfType(All, EWorldEventType::TurtleNesting);
			const int32 FullMoons = FMath::FloorToInt32(Horizon / FMoonModel::DaysPerCycle);
			TestTrue(TEXT("Casi todas las lunas llenas"), Nests.Num() >= FullMoons * 3 / 4 && Nests.Num() <= FullMoons);
			for (const FWorldEvent& Nest : Nests)
			{
				const float Mid = 0.5f * (Nest.Start + Nest.End);
				TestTrue(TEXT("Luna llena"), FMoonModel::IsFullMoonWindow(Mid) && FMoonModel::IlluminationAt(Mid) > 0.97f);
				TestTrue(TEXT("Arenas Blancas"), Nest.Island == EIslandArchetype::WhiteSands);
				TestEqual(TEXT("Empieza al anochecer"), HourOf(Nest.Start), FWorldEventsModel::NightStartHour, 0.01f);
				TestEqual(TEXT("Termina antes del alba"), HourOf(Nest.End), FWorldEventsModel::NightEndHour, 0.01f);
			}
		});

		It("lleva las crías al mar en el amanecer siguiente a cada desove", [this]()
		{
			const TArray<FWorldEvent> Nests = OfType(All, EWorldEventType::TurtleNesting);
			const TArray<FWorldEvent> Hatchlings = OfType(All, EWorldEventType::TurtleHatchlings);
			TestEqual(TEXT("Una salida de crías por desove"), Hatchlings.Num(), Nests.Num());
			for (int32 I = 0; I < FMath::Min(Nests.Num(), Hatchlings.Num()); ++I)
			{
				const float Gap = Hatchlings[I].Start - Nests[I].End;
				TestTrue(TEXT("Justo después del desove"), Gap >= 0.0f && Gap < 0.1f);
				TestTrue(TEXT("Al amanecer"), HourOf(Hatchlings[I].Start) >= 5.0f && HourOf(Hatchlings[I].End) <= 8.0f);
				TestTrue(TEXT("En Arenas Blancas"), Hatchlings[I].Island == EIslandArchetype::WhiteSands);
			}
		});

		It("hace brillar el mar en la noche de luna nueva", [this]()
		{
			const TArray<FWorldEvent> Glows = OfType(All, EWorldEventType::Bioluminescence);
			TestTrue(TEXT("Una por ciclo"), Glows.Num() >= FMath::FloorToInt32(Horizon / FMoonModel::DaysPerCycle) - 1);
			for (const FWorldEvent& Glow : Glows)
			{
				const float Mid = 0.5f * (Glow.Start + Glow.End);
				TestTrue(TEXT("Luna nueva"), FMoonModel::IsNewMoonWindow(Mid) && FMoonModel::IlluminationAt(Mid) < 0.03f);
				TestTrue(TEXT("Brillo máximo"), Glow.Intensity > 0.95f);
			}
			TestTrue(TEXT("Máxima a medianoche de luna nueva"), FWorldEventsModel::BioluminescenceAt(24.0f) > 0.95f);
			TestTrue(TEXT("Casi nula con luna llena"), FWorldEventsModel::BioluminescenceAt(30.0f) < 0.05f);
			TestTrue(TEXT("Nula a mediodía"), FWorldEventsModel::BioluminescenceAt(24.5f) < 0.01f);
		});
	});

	Describe("Calendario", [this]()
	{
		It("hace pasar ballenas de día cada unos diez días a lo largo del año", [this]()
		{
			const TArray<FWorldEvent> Whales = OfType(All, EWorldEventType::WhalePassage);
			const float MeanGap = Whales.Num() > 1 ? (Whales.Last().Start - Whales[0].Start) / (Whales.Num() - 1) : 0.0f;
			TestTrue(TEXT("Cadencia media de ~10 días"), MeanGap > 8.5f && MeanGap < 11.5f);
			for (int32 Year = 0; Year < 20; ++Year)
			{
				const float From = Year * static_cast<float>(FWeatherModel::DaysPerYear);
				const int32 InYear = OfType(Model->EventsInWindow(From, From + FWeatherModel::DaysPerYear), EWorldEventType::WhalePassage).Num();
				TestTrue(TEXT("Dos a cuatro pasos al año"), InYear >= 2 && InYear <= 4);
			}
			for (int32 I = 0; I < Whales.Num(); ++I)
			{
				TestTrue(TEXT("De día"), HourOf(Whales[I].Start) >= 9.0f && HourOf(Whales[I].End) <= 17.0f);
				TestFalse(TEXT("Sin temporal ni niebla"), AnyStateIn(Model->GetWeather(), Whales[I],
					{EWeatherState::Cyclone, EWeatherState::Gale, EWeatherState::Thunderstorm, EWeatherState::MorningFog}));
				if (I > 0)
				{
					const float Gap = Whales[I].Start - Whales[I - 1].Start;
					TestTrue(TEXT("Ni seguidas ni con huecos enormes"), Gap > 2.0f && Gap < 18.0f);
				}
			}
		});

		It("reserva la lluvia de estrellas para pocas noches, todas despejadas", [this]()
		{
			const TArray<FWorldEvent> Showers = OfType(All, EWorldEventType::MeteorShower);
			const int32 Nights = FMath::FloorToInt32(Horizon);
			TestTrue(TEXT("Alguna en veinte años"), Showers.Num() >= 2);
			TestTrue(TEXT("Rara: menos de una noche de cada treinta"), Showers.Num() * 30 < Nights);
			for (const FWorldEvent& Shower : Showers)
			{
				for (float T = Shower.Start; T < Shower.End; T += 0.005f)
				{
					const EWeatherState State = Model->GetWeather().StateAt(T);
					if (State != EWeatherState::Clear && State != EWeatherState::HeatWave)
					{
						AddError(FString::Printf(TEXT("Lluvia de estrellas con cielo %s en %.3f"), LexToString(State), T));
						return;
					}
				}
				TestTrue(TEXT("De noche"), HourOf(Shower.Start) >= 19.9f && HourOf(Shower.End) <= 4.1f);
			}
		});

		It("no trae barcos durante un ciclón ni una galerna", [this]()
		{
			const TArray<FWorldEvent> Ships = OfType(All, EWorldEventType::ShipOnHorizon);
			TestTrue(TEXT("Ocasionales"), Ships.Num() >= 10 && Ships.Num() * 8 < FMath::FloorToInt32(Horizon));
			bool bSawCyclone = false;
			for (float T = 0.0f; T < Horizon && !bSawCyclone; T += 0.05f)
			{
				bSawCyclone = Model->GetWeather().StateAt(T) == EWeatherState::Cyclone;
			}
			TestTrue(TEXT("Hay ciclones en el horizonte de prueba"), bSawCyclone);
			for (const FWorldEvent& Ship : Ships)
			{
				TestFalse(TEXT("Sin ciclón ni galerna"), AnyStateIn(Model->GetWeather(), Ship, {EWeatherState::Cyclone, EWeatherState::Gale}));
			}
		});

		It("hace coincidir la marea viva extrema con las mareas vivas, rara vez", [this]()
		{
			const TArray<FWorldEvent> Tides = OfType(All, EWorldEventType::ExtremeSpringTide);
			const int32 SpringTides = FMath::FloorToInt32(Horizon / (FMoonModel::DaysPerCycle * 0.5f));
			TestTrue(TEXT("Alguna en veinte años"), Tides.Num() >= 3);
			TestTrue(TEXT("Rara: menos de una de cada cinco mareas vivas"), Tides.Num() * 5 < SpringTides);
			for (const FWorldEvent& Tide : Tides)
			{
				const float Mid = 0.5f * (Tide.Start + Tide.End);
				TestTrue(TEXT("Marea viva según FOceanTide"), FOceanTide::SpringNeapFactorAt(Mid) > 0.95f);
				TestTrue(TEXT("En plena bajamar"), FOceanTide::Level(Mid) < -0.99f);
				TestTrue(TEXT("De día, para ver las ruinas"), HourOf(Tide.Start) >= 6.0f && HourOf(Tide.End) <= 12.0f);
				TestTrue(TEXT("Retirada extra en la bajamar"), Model->ExtremeTideDrawdown(Mid) > 0.45f);
				TestEqual(TEXT("Sin retirada extra fuera"), Model->ExtremeTideDrawdown(Tide.End + 0.01f), 0.0f);
			}
		});

		It("sitúa las erupciones menores en la Isla del Humo, con temblor antes que ceniza", [this]()
		{
			const TArray<FWorldEvent> Eruptions = OfType(All, EWorldEventType::MinorEruption);
			TestTrue(TEXT("Al azar pero presentes"), Eruptions.Num() >= 5 && Eruptions.Num() * 10 < FMath::FloorToInt32(Horizon));
			for (const FWorldEvent& Eruption : Eruptions)
			{
				TestTrue(TEXT("Isla del Humo"), Eruption.Island == EIslandArchetype::Smoke);
				const FEruptionSample Early = FWorldEventsModel::SampleEruption(Eruption, Eruption.Start + Eruption.Duration() * 0.2f);
				const FEruptionSample Late = FWorldEventsModel::SampleEruption(Eruption, Eruption.Start + Eruption.Duration() * 0.7f);
				TestTrue(TEXT("Primero tiembla"), Early.Tremor > 0.0f && Early.Ash == 0.0f);
				TestTrue(TEXT("Luego cae ceniza"), Late.Ash > Late.Tremor);
			}
		});
	});

	Describe("Consultas", [this]()
	{
		It("es determinista para la misma semilla y cambia con otra", [this]()
		{
			const FWorldEventsModel Same(EventSeed, WeatherSeed);
			const TArray<FWorldEvent> Again = Same.EventsInWindow(0.0f, Horizon);
			TestEqual(TEXT("Mismo número"), Again.Num(), All.Num());
			for (int32 I = 0; I < FMath::Min(Again.Num(), All.Num()); ++I)
			{
				const bool bSame = Again[I].Id == All[I].Id && Again[I].Type == All[I].Type && Again[I].Start == All[I].Start
					&& Again[I].End == All[I].End && Again[I].Intensity == All[I].Intensity && Again[I].Seed == All[I].Seed;
				if (!bSame)
				{
					AddError(FString::Printf(TEXT("Divergencia en el evento %d"), I));
					return;
				}
			}
			const FWorldEventsModel Other(EventSeed + 1, WeatherSeed);
			const TArray<FWorldEvent> Different = Other.EventsInWindow(0.0f, Horizon);
			bool bDiffers = Different.Num() != All.Num();
			for (int32 I = 0; !bDiffers && I < All.Num(); ++I)
			{
				bDiffers = Different[I].Start != All[I].Start;
			}
			TestTrue(TEXT("Otra semilla, otro calendario"), bDiffers);
		});

		It("da ids únicos y eventos ordenados por inicio", [this]()
		{
			TSet<uint64> Ids;
			for (int32 I = 0; I < All.Num(); ++I)
			{
				TestFalse(TEXT("Id único"), Ids.Contains(All[I].Id));
				Ids.Add(All[I].Id);
				TestTrue(TEXT("Intervalo válido y de menos de un día"), All[I].End > All[I].Start && All[I].Duration() < 1.0f);
				if (I > 0)
				{
					TestTrue(TEXT("Ordenados"), All[I - 1].Start <= All[I].Start);
				}
			}
		});

		It("responde a eventos activos igual que la ventana", [this]()
		{
			for (float T = 0.0f; T < 200.0f; T += 0.0625f)
			{
				TArray<FWorldEvent> Expected;
				for (const FWorldEvent& Event : All)
				{
					if (Event.IsActiveAt(T))
					{
						Expected.Add(Event);
					}
				}
				const TArray<FWorldEvent> Active = Model->ActiveAt(T);
				if (Active.Num() != Expected.Num())
				{
					AddError(FString::Printf(TEXT("ActiveAt(%.3f) da %d eventos, se esperaban %d"), T, Active.Num(), Expected.Num()));
					return;
				}
				for (int32 I = 0; I < Active.Num(); ++I)
				{
					TestTrue(TEXT("Mismo evento"), Active[I] == Expected[I]);
				}
			}
		});

		It("encuentra la siguiente aparición de cada tipo sin saltarse ninguna", [this]()
		{
			for (int32 TypeIndex = 0; TypeIndex < static_cast<int32>(EWorldEventType::Count); ++TypeIndex)
			{
				const EWorldEventType Type = static_cast<EWorldEventType>(TypeIndex);
				const TArray<FWorldEvent> OfThisType = OfType(All, Type);
				for (float T = 3.3f; T < 300.0f; T += 17.1f)
				{
					const FWorldEvent* Expected = OfThisType.FindByPredicate([T](const FWorldEvent& E) { return E.Start > T; });
					FWorldEvent Next;
					const bool bFound = Model->NextOccurrence(Type, T, Next, FMath::CeilToInt32(Horizon - T));
					TestEqual(TEXT("Encontrada si existe"), bFound, Expected != nullptr);
					if (bFound && Expected)
					{
						TestTrue(FString::Printf(TEXT("Siguiente %s tras %.1f"), LexToString(Type), T), Next == *Expected);
					}
				}
			}
		});

		It("acota relojes no finitos o fuera de partida y la ventana de búsqueda", [this]()
		{
			constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
			const float Max = static_cast<float>(FWeatherModel::MaxSupportedDays);
			TestEqual(TEXT("Ventana NaN vacía"), Model->EventsInWindow(0.0f, NaN).Num(), 0);
			TestEqual(TEXT("Instante NaN vacío"), Model->ActiveAt(NaN).Num(), 0);
			FWorldEvent Next;
			TestFalse(TEXT("Sin siguiente con NaN"), Model->NextOccurrence(EWorldEventType::ShipOnHorizon, NaN, Next));

			// Sin tope, un reloj de 1e9 días generaba 1e9 días (cuelgue) y ++Day desbordaba.
			for (const FWorldEvent& Event : Model->EventsInWindow(Max - 2.0f, 1.0e9f))
			{
				TestTrue(TEXT("Ventana acotada al último día admitido"), Event.Start <= Max);
			}
			// FirstDay + SearchDays desbordaba int32 y no se buscaba nada.
			TestTrue(TEXT("Búsqueda enorme acotada"), Model->NextOccurrence(EWorldEventType::ShipOnHorizon, 0.0f, Next, MAX_int32));
			if (Model->NextOccurrence(EWorldEventType::ShipOnHorizon, 3.0e9f, Next))
			{
				TestTrue(TEXT("Tras el último día admitido"), Next.Start > Max);
			}
		});

		It("recorre una ventana en orden y se detiene cuando se le pide", [this]()
		{
			TArray<FWorldEvent> Visited;
			Model->ForEachInWindow(10.0f, 100.0f, [&Visited](const FWorldEvent& Event) {
				Visited.Add(Event);
				return Visited.Num() < 3;
			});
			TestEqual(TEXT("Para en el tercero"), Visited.Num(), 3);
			const TArray<FWorldEvent> Window = Model->EventsInWindow(10.0f, 100.0f);
			for (int32 I = 0; I < FMath::Min(3, Window.Num()); ++I)
			{
				TestTrue(TEXT("Mismo orden que la lista"), Visited[I] == Window[I]);
			}
			for (const FWorldEvent& Event : Window)
			{
				TestTrue(TEXT("Solapa la ventana"), Event.Start < 100.0f && Event.End > 10.0f);
			}
		});
	});

	Describe("Resultados de un solo uso", [this]()
	{
		It("suelta el paquete del barco una sola vez y solo con la hoguera de señal", [this]()
		{
			FWorldEvent Ship;
			if (!TestTrue(TEXT("Hay barco"), Model->NextOccurrence(EWorldEventType::ShipOnHorizon, 0.0f, Ship)))
			{
				return;
			}
			const float During = 0.5f * (Ship.Start + Ship.End);
			FWorldEventsState State;
			FWorldEvent Seen;
			TestEqual(TEXT("Antes no hay barco"), Model->DecideShipPackage(Ship.Start - 0.01f, true, State, Seen), EShipSignalOutcome::NoShip);
			TestEqual(TEXT("Sin hoguera no ve nada"), Model->DecideShipPackage(During, false, State, Seen), EShipSignalOutcome::NoSignal);
			TestEqual(TEXT("Con hoguera suelta el paquete"), Model->DecideShipPackage(During, true, State, Seen), EShipSignalOutcome::DropPackage);
			TestTrue(TEXT("Es este barco"), Seen == Ship);
			State.Consume(Seen.Id);
			State.Consume(Seen.Id);
			TestEqual(TEXT("Se guarda una vez"), State.ConsumedOutcomes.Num(), 1);
			TestEqual(TEXT("No repite"), Model->DecideShipPackage(During, true, State, Seen), EShipSignalOutcome::AlreadyDropped);
		});

		It("deposita la obsidiana de cada erupción una vez terminada", [this]()
		{
			FWorldEvent Eruption;
			if (!TestTrue(TEXT("Hay erupción"), Model->NextOccurrence(EWorldEventType::MinorEruption, 0.0f, Eruption)))
			{
				return;
			}
			FWorldEventsState State;
			TestFalse(TEXT("Durante no"), FWorldEventsModel::ShouldDepositObsidian(Eruption, Eruption.Start + 0.01f, State));
			TestTrue(TEXT("Al terminar sí"), FWorldEventsModel::ShouldDepositObsidian(Eruption, Eruption.End, State));
			State.Consume(Eruption.Id);
			TestFalse(TEXT("Ya depositada"), FWorldEventsModel::ShouldDepositObsidian(Eruption, Eruption.End + 3.0f, State));
		});
	});
}

#endif
