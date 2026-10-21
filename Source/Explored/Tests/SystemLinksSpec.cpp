#include "Misc/AutomationTest.h"

#include "Core/SystemLinks.h"
#include "Weather/WeatherModel.h"
#include "WorldGen/IslandLayout.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FSystemLinksSpec, "Explored.Links",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FSystemLinksSpec)

void FSystemLinksSpec::Define()
{
	using namespace ExploredLinks;

	Describe("Supervivencia", [this]()
	{
		It("combina el calor de varios fuegos sin pasar de 1", [this]()
		{
			TestEqual(TEXT("Sin fuegos"), CombineFireHeat({}), 0.0f);
			TestEqual(TEXT("Uno"), CombineFireHeat({0.5f}), 0.5f, 1e-6f);
			TestEqual(TEXT("Dos a medias"), CombineFireHeat({0.5f, 0.5f}), 0.75f, 1e-6f);
			TestEqual(TEXT("Acotado"), CombineFireHeat({1.5f, 0.9f}), 1.0f, 1e-6f);
			TestEqual(TEXT("Negativos no enfrían"), CombineFireHeat({-1.0f, 0.2f}), 0.2f, 1e-6f);
		});

		It("mezcla el calor, el techo, la carga y la música con lo que midió el cuerpo", [this]()
		{
			FSurvivalInputs Inputs;
			Inputs.FireHeat = 0.1f;
			Inputs.bSheltered = false;
			FSurvivalLinkInputs Links;
			Links.FireHeat = 0.6f;
			Links.bBuildingShelter = true;
			Links.CarriedWeightRatio = 1.4f;
			Links.bPlayingMusic = true;
			ApplySurvivalLinks(Inputs, Links);
			TestEqual(TEXT("Calor"), Inputs.FireHeat, 0.6f);
			TestTrue(TEXT("A cubierto"), Inputs.bSheltered);
			TestEqual(TEXT("Carga"), Inputs.CarriedWeightRatio, 1.4f);
			TestTrue(TEXT("Música"), Inputs.bPlayingMusic);

			// Un techo medido por la traza no se pierde porque la construcción no lo vea.
			FSurvivalInputs Roofed;
			Roofed.bSheltered = true;
			ApplySurvivalLinks(Roofed, FSurvivalLinkInputs());
			TestTrue(TEXT("Sigue a cubierto"), Roofed.bSheltered);
		});

		It("el calor de un fuego sube la temperatura efectiva del cuerpo", [this]()
		{
			FSurvivalState State;
			State.Wetness = 0.8f;
			FSurvivalInputs Cold;
			Cold.AirTemperature = 20.0f;
			Cold.Wind = 0.8f;
			FSurvivalInputs Warm = Cold;
			FSurvivalLinkInputs Links;
			Links.FireHeat = CombineFireHeat({0.9f});
			ApplySurvivalLinks(Warm, Links);
			TestTrue(TEXT("Más calor junto al fuego"),
				FSurvivalModel::EffectiveTemperature(State, Warm) > FSurvivalModel::EffectiveTemperature(State, Cold));
		});
	});

	Describe("Reaparición", [this]()
	{
		It("decide según el modo y los fuegos encendidos", [this]()
		{
			const FSurvivalModeSettings Survivor = FSurvivalModeSettings::FromMode(ESurvivalMode::Survivor);
			const FSurvivalModeSettings Explorer = FSurvivalModeSettings::FromMode(ESurvivalMode::Explorer);
			const FSurvivalModeSettings Castaway = FSurvivalModeSettings::FromMode(ESurvivalMode::Castaway);
			TestEqual(TEXT("Superviviente con fogata"), DecideRespawn(Survivor, true), ERespawnDecision::AtRespawnPoint);
			TestEqual(TEXT("Superviviente sin fogata"), DecideRespawn(Survivor, false), ERespawnDecision::AtStart);
			TestEqual(TEXT("Explorador con fogata"), DecideRespawn(Explorer, true), ERespawnDecision::AtRespawnPoint);
			TestEqual(TEXT("Náufrago: nunca"), DecideRespawn(Castaway, true), ERespawnDecision::GameOver);
		});

		It("elige el punto más cercano", [this]()
		{
			const TArray<FVector> Points = {FVector(1000.0, 0.0, 0.0), FVector(-200.0, 0.0, 0.0), FVector(0.0, 500.0, 0.0)};
			TestEqual(TEXT("El segundo"), NearestPoint(Points, FVector::ZeroVector), 1);
			TestEqual(TEXT("Sin puntos"), NearestPoint({}, FVector::ZeroVector), static_cast<int32>(INDEX_NONE));
		});

		It("reaparece vivo, sin heridas y conservando lo lento", [this]()
		{
			FSurvivalState Dead;
			Dead.Health = 0.0f;
			Dead.Hunger = 0.0f;
			Dead.BodyTemperature = 31.0f;
			Dead.ScurvySeverity = 0.4f;
			Dead.Wounds.AddDefaulted();
			Dead.AddCondition(ECondition::Bleeding, 3.0f);
			const FSurvivalState Alive = MakeRespawnState(Dead);
			TestFalse(TEXT("Vivo"), Alive.IsDead());
			TestEqual(TEXT("Sin heridas"), Alive.Wounds.Num(), 0);
			TestFalse(TEXT("Sin sangrar"), Alive.HasCondition(ECondition::Bleeding));
			TestEqual(TEXT("Temperatura normal"), Alive.BodyTemperature, 37.0f);
			TestEqual(TEXT("Escorbuto intacto"), Alive.ScurvySeverity, 0.4f);
			TestTrue(TEXT("Hambre mínima"), Alive.Hunger >= 30.0f);
		});
	});

	Describe("Peligro para la música", [this]()
	{
		It("sube con la hipotermia, el calor extremo y la salud baja", [this]()
		{
			FSurvivalState Fine;
			TestEqual(TEXT("Sano"), BodyDanger01(Fine), 0.0f);
			FSurvivalState Cold;
			Cold.BodyTemperature = 33.0f;
			TestEqual(TEXT("Hipotermia grave"), BodyDanger01(Cold), 1.0f);
			FSurvivalState Hurt;
			Hurt.Health = 20.0f;
			TestTrue(TEXT("Herido"), BodyDanger01(Hurt) > 0.4f);
		});

		It("el ciclón asusta más a la intemperie", [this]()
		{
			TestEqual(TEXT("Despejado"), StormDanger01(EWeatherState::Clear, false), 0.0f);
			TestTrue(TEXT("Ciclón fuera"), StormDanger01(EWeatherState::Cyclone, false) > StormDanger01(EWeatherState::Cyclone, true));
			TestTrue(TEXT("Ciclón > galerna"), StormDanger01(EWeatherState::Cyclone, false) > StormDanger01(EWeatherState::Gale, false));
		});

		It("mide la amenaza de los depredadores por estado y distancia", [this]()
		{
			TestEqual(TEXT("Delfín inofensivo"), PredatorThreat01(EFaunaSpecies::Dolphin, EMarineState::Accompany, 0.0f), 0.0f);
			TestEqual(TEXT("Tiburón que se retira"), PredatorThreat01(EFaunaSpecies::ReefShark, EMarineState::Retreat, 100.0f), 0.0f);
			const float Near = PredatorThreat01(EFaunaSpecies::TigerShark, EMarineState::Stalk, 500.0f);
			const float Far = PredatorThreat01(EFaunaSpecies::TigerShark, EMarineState::Stalk, 3500.0f);
			TestTrue(TEXT("Cerca asusta más"), Near > Far);
			TestEqual(TEXT("Embestida encima"), PredatorThreat01(EFaunaSpecies::ReefShark, EMarineState::Attack, 0.0f), 1.0f);
		});
	});

	Describe("Fauna y cuerpo", [this]()
	{
		It("las medusas y las rayas pican; los tiburones cortan", [this]()
		{
			const FFaunaHarm Jelly = HarmFromFauna(EFaunaSpecies::Jellyfish, 5.0f);
			TestTrue(TEXT("Medusa pica"), Jelly.bSting && Jelly.Sting == EStingKind::Jellyfish);
			const FFaunaHarm Ray = HarmFromFauna(EFaunaSpecies::Stingray, 8.0f);
			TestTrue(TEXT("Raya pica"), Ray.bSting && Ray.Sting == EStingKind::Ray);
			const FFaunaHarm Bite = HarmFromFauna(EFaunaSpecies::TigerShark, 60.0f);
			TestFalse(TEXT("Sin picadura"), Bite.bSting);
			TestEqual(TEXT("Corte hondo"), Bite.CutDepth, 1.0f);
			TestTrue(TEXT("Mordisco leve"), HarmFromFauna(EFaunaSpecies::ReefShark, 8.0f).CutDepth < 0.5f);
		});
	});

	Describe("Islas, eventos y días", [this]()
	{
		It("encuentra la isla que se pisa y la presencia de los eventos", [this]()
		{
			const FIslandLayout Layout = FIslandLayout::Provisional();
			for (int32 I = 0; I < Layout.Islands.Num(); ++I)
			{
				TestEqual(TEXT("El centro es de su isla"), FindIslandAt(Layout, Layout.Islands[I].Center), I);
			}
			const FVector2D OpenSea(FIslandLayout::WorldHalfExtent * 20.0f, FIslandLayout::WorldHalfExtent * 20.0f);
			TestEqual(TEXT("En alta mar"), FindIslandAt(Layout, OpenSea), static_cast<int32>(INDEX_NONE));

			FWorldEvent Sea;
			Sea.Island = EIslandArchetype::Count;
			TestTrue(TEXT("El mar se ve desde cualquier sitio"), IsEventWitnessed(Sea, Layout, OpenSea));

			FWorldEvent OnIslet;
			OnIslet.Island = EIslandArchetype::Teeth;
			const FIslandDesc* Islet = Layout.FindIsland(EIslandArchetype::Teeth);
			TestTrue(TEXT("Hay islote rocoso"), Islet != nullptr);
			if (Islet)
			{
				TestTrue(TEXT("En el islote"), IsEventWitnessed(OnIslet, Layout, Islet->Center));
				TestFalse(TEXT("Lejos"), IsEventWitnessed(OnIslet, Layout, OpenSea));
				float Distance = 0.0f;
				TestEqual(TEXT("La más cercana"), Layout.Islands[NearestIsland(Layout, Islet->Center, Distance)].Archetype, EIslandArchetype::Teeth);
				TestEqual(TEXT("Dentro"), Distance, 0.0f);
			}
		});

		It("cuenta los días completos de la partida", [this]()
		{
			TestEqual(TEXT("Recién empezada"), DaysSurvived(4.3f, 4.3f), 0);
			TestEqual(TEXT("Un día y algo"), DaysSurvived(5.9f, 4.3f), 1);
			TestEqual(TEXT("Nunca negativo"), DaysSurvived(1.0f, 4.3f), 0);
		});

		It("no se rompe con valores no finitos", [this]()
		{
			const float Inf = std::numeric_limits<float>::infinity();
			TestEqual(TEXT("Días con reloj NaN"), DaysSurvived(std::numeric_limits<float>::quiet_NaN(), 4.3f), 0);
			TestEqual(TEXT("Días con reloj infinito"), DaysSurvived(Inf, 4.3f), 0);
			TestEqual(TEXT("Días fuera de int32"), DaysSurvived(1.0e30f, 0.0f), TNumericLimits<int32>::Max());

			FSurvivalInputs Inputs;
			FSurvivalLinkInputs Links;
			Links.CarriedWeightRatio = std::numeric_limits<float>::quiet_NaN();
			ApplySurvivalLinks(Inputs, Links);
			TestEqual(TEXT("Carga NaN cuenta como nada"), Inputs.CarriedWeightRatio, 0.0f);
		});
	});

	Describe("Barcos y ciclones", [this]()
	{
		It("daña más con más categoría y menos varado", [this]()
		{
			TestEqual(TEXT("Sin ciclón"), BoatCycloneDamagePerHour(0, false), 0.0f);
			TestTrue(TEXT("Categoría 3 > 1"), BoatCycloneDamagePerHour(3, false) > BoatCycloneDamagePerHour(1, false));
			TestTrue(TEXT("Varado sufre menos"), BoatCycloneDamagePerHour(2, true) < BoatCycloneDamagePerHour(2, false));
		});

		It("el modelo del clima da categoría solo durante los ciclones y siempre la misma", [this]()
		{
			const FWeatherModel Weather(1234u);
			const FWeatherModel Same(1234u);
			int32 CycloneSamples = 0;
			TArray<int32> Seen = {0, 0, 0, 0};
			for (float T = 0.0f; T < 128.0f; T += 0.05f)
			{
				const int32 Category = Weather.CycloneCategoryAt(T);
				TestEqual(TEXT("Determinista"), Same.CycloneCategoryAt(T), Category);
				if (Weather.StateAt(T) == EWeatherState::Cyclone)
				{
					++CycloneSamples;
					TestTrue(TEXT("Entre 1 y 3"), Category >= 1 && Category <= 3);
					++Seen[FMath::Clamp(Category, 0, 3)];
				}
				else
				{
					TestEqual(TEXT("Sin ciclón, 0"), Category, 0);
				}
			}
			TestTrue(TEXT("Hay ciclones en cuatro años"), CycloneSamples > 0);
		});
	});
}

#endif
