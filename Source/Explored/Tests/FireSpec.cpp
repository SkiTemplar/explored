#include "Misc/AutomationTest.h"

#include "Cooking/FireModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace FireTest
{
	/** Fuego encendido con yesca y combustible, listo para simular. */
	FFireState Lit(EFireLevel Level, const TArray<FName>& Fuel)
	{
		const FFireData& Data = FFireData::Default();
		FFireState S;
		S.Level = Level;
		TArray<EFireEvent> Events;
		FFireModel::AddFuel(S, Data, TEXT("fibra_coco"), Events);
		for (const FName& Id : Fuel)
		{
			FFireModel::AddFuel(S, Data, Id, Events);
		}
		FFireModel::TryIgnite(S, Data, EIgnitionMethod::Matches, FFireEnvironment(), 0.0f, Events);
		return S;
	}

	FFireEnvironment Calm(bool bSheltered = false)
	{
		FFireEnvironment Env;
		Env.Rain = 0.0f;
		Env.Wind = 0.0f;
		Env.bSheltered = bSheltered;
		return Env;
	}

	/** Horas hasta que deja de arder (con tope). */
	float HoursUntilNotBurning(FFireState S, const FFireEnvironment& Env, float MaxHours)
	{
		TArray<EFireEvent> Events;
		constexpr float Step = 0.05f;
		float T = 0.0f;
		while (S.Status == EFireStatus::Burning && T < MaxHours)
		{
			FFireModel::Tick(S, FFireData::Default(), Env, Step, Events);
			T += Step;
		}
		return T;
	}
}

BEGIN_DEFINE_SPEC(FFireSpec, "Explored.Fire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FFireSpec)

void FFireSpec::Define()
{
	using namespace FireTest;

	Describe("los datos (fuels.json)", [this]()
	{
		It("traen los tres niveles en orden, con más capacidad y calor al subir", [this]()
		{
			const FFireData& Data = FFireData::Default();
			TestEqual(TEXT("Tres niveles"), Data.Levels.Num(), 3);
			const FFireLevelDef& Fogata = Data.GetLevel(EFireLevel::Fogata);
			const FFireLevelDef& Hoguera = Data.GetLevel(EFireLevel::Hoguera);
			const FFireLevelDef& Horno = Data.GetLevel(EFireLevel::HornoArcilla);
			TestEqual(TEXT("Id de la fogata"), Fogata.Id, FName(TEXT("fogata")));
			TestEqual(TEXT("La fogata es la pieza fogata"), Fogata.PieceId, FName(TEXT("fogata")));
			TestTrue(TEXT("Más combustible"), Fogata.MaxFuelHours < Hoguera.MaxFuelHours && Hoguera.MaxFuelHours < Horno.MaxFuelHours);
			TestTrue(TEXT("Más calor"), Fogata.HeatScale < Hoguera.HeatScale && Hoguera.HeatScale < Horno.HeatScale);
			TestTrue(TEXT("El horno es cerrado"), Horno.bEnclosed && !Fogata.bEnclosed);
		});

		It("tienen yesca, leña, combustible verde y las tres formas de encender", [this]()
		{
			const FFireData& Data = FFireData::Default();
			const FFuelDef* Tinder = Data.FindFuel(TEXT("fibra_coco"));
			const FFuelDef* Log = Data.FindFuel(TEXT("tronco_pequeno"));
			const FFuelDef* Palm = Data.FindFuel(TEXT("hoja_palma"));
			TestTrue(TEXT("Fibra de coco es yesca"), Tinder && Tinder->bTinder);
			TestTrue(TEXT("El tronco dura más que una rama"), Log && Data.FindFuel(TEXT("rama_seca"))
				&& Log->BurnHours > Data.FindFuel(TEXT("rama_seca"))->BurnHours);
			TestTrue(TEXT("La hoja de palma es verde"), Palm && Palm->bGreen);
			TestNull(TEXT("Una piedra no arde"), Data.FindFuel(TEXT("canto_rodado")));
			TestNotNull(TEXT("Cerillas"), Data.FindIgnition(EIgnitionMethod::Matches));
			TestNotNull(TEXT("Pedernal"), Data.FindIgnition(EIgnitionMethod::Flint));
			TestNotNull(TEXT("Fricción"), Data.FindIgnition(EIgnitionMethod::Friction));
			const FIgnitionDef* ByTool = Data.FindIgnitionByTool(TEXT("cerillas"));
			TestTrue(TEXT("Las cerillas se reconocen en la mano"), ByTool && ByTool->Method == EIgnitionMethod::Matches);
		});
	});

	Describe("el combustible", [this]()
	{
		It("arde lo que dicen los datos: un tronco en fogata unas 3 h, más rápido en hoguera y más lento en horno", [this]()
		{
			const FFireData& Data = FFireData::Default();
			const float LogHours = Data.FindFuel(TEXT("tronco_pequeno"))->BurnHours;
			const FFireEnvironment Env = Calm(true);
			const float Fogata = HoursUntilNotBurning(Lit(EFireLevel::Fogata, {TEXT("tronco_pequeno")}), Env, 48.0f);
			const float Hoguera = HoursUntilNotBurning(Lit(EFireLevel::Hoguera, {TEXT("tronco_pequeno")}), Env, 48.0f);
			const float Horno = HoursUntilNotBurning(Lit(EFireLevel::HornoArcilla, {TEXT("tronco_pequeno")}), Env, 48.0f);
			const float Tinder = Data.FindFuel(TEXT("fibra_coco"))->BurnHours;
			TestEqual(TEXT("Fogata ≈ horas del tronco + yesca"), Fogata, LogHours + Tinder, 0.11f);
			TestTrue(TEXT("La hoguera lo gasta antes"), Hoguera < Fogata);
			TestTrue(TEXT("El horno lo aprovecha más"), Horno > Fogata);
		});

		It("no admite más combustible del que cabe y la yesca siempre cabe", [this]()
		{
			const FFireData& Data = FFireData::Default();
			FFireState S;
			TArray<EFireEvent> Events;
			TestTrue(TEXT("Primer tronco"), FFireModel::AddFuel(S, Data, TEXT("tronco_pequeno"), Events));
			TestFalse(TEXT("El segundo no cabe en la fogata"), FFireModel::AddFuel(S, Data, TEXT("tronco_pequeno"), Events));
			TestTrue(TEXT("La yesca sí"), FFireModel::AddFuel(S, Data, TEXT("yesca_hongo"), Events));
			TestFalse(TEXT("Una piedra no"), FFireModel::AddFuel(S, Data, TEXT("canto_rodado"), Events));
			TestTrue(TEXT("En una hoguera sí caben dos"), FFireModel::Upgrade(S, EFireLevel::Hoguera)
				&& FFireModel::AddFuel(S, Data, TEXT("tronco_pequeno"), Events));
			TestFalse(TEXT("No se baja de nivel"), FFireModel::Upgrade(S, EFireLevel::Fogata));
		});

		It("calienta más una hoguera con buena leña que una fogata con ramas", [this]()
		{
			const FFireState Hoguera = Lit(EFireLevel::Hoguera, {TEXT("madera_dura")});
			const FFireState Fogata = Lit(EFireLevel::Fogata, {TEXT("rama_seca")});
			TestTrue(TEXT("Arden"), Hoguera.Status == EFireStatus::Burning && Fogata.Status == EFireStatus::Burning);
			TestTrue(TEXT("Más calor"), Hoguera.Heat > Fogata.Heat);
			TestTrue(TEXT("El calor se nota cerca y no lejos"),
				FFireModel::HeatAtDistance(Hoguera, 1.0f) > FFireModel::HeatAtDistance(Hoguera, 3.0f)
				&& FFireModel::HeatAtDistance(Hoguera, 6.0f) == 0.0f);
		});
	});

	Describe("el encendido", [this]()
	{
		It("prende mejor con cerillas que con pedernal y que por fricción, y peor sin yesca o mojado", [this]()
		{
			const FFireData& Data = FFireData::Default();
			FFireState S;
			TArray<EFireEvent> Events;
			FFireModel::AddFuel(S, Data, TEXT("rama_seca"), Events);
			const FFireEnvironment Env = Calm();
			const float NoTinder = FFireModel::IgnitionChance(S, Data, EIgnitionMethod::Matches, Env);
			FFireModel::AddFuel(S, Data, TEXT("fibra_coco"), Events);
			const float Matches = FFireModel::IgnitionChance(S, Data, EIgnitionMethod::Matches, Env);
			const float Flint = FFireModel::IgnitionChance(S, Data, EIgnitionMethod::Flint, Env);
			const float Friction = FFireModel::IgnitionChance(S, Data, EIgnitionMethod::Friction, Env);
			TestTrue(TEXT("Cerillas > pedernal > fricción"), Matches > Flint && Flint > Friction && Friction > 0.0f);
			TestTrue(TEXT("La yesca ayuda"), Matches > NoTinder);

			FFireState Wet = S;
			Wet.Dampness = 0.7f;
			TestTrue(TEXT("Mojado cuesta más"), FFireModel::IgnitionChance(Wet, Data, EIgnitionMethod::Matches, Env) < Matches);
			FFireEnvironment Storm;
			Storm.Rain = 1.0f;
			Storm.Wind = 0.9f;
			TestTrue(TEXT("Con temporal a la intemperie cuesta más"), FFireModel::IgnitionChance(S, Data, EIgnitionMethod::Matches, Storm) < Matches * 0.5f);
		});

		It("no prende sin combustible y la tirada decide; cada intento gasta yesca y cerilla", [this]()
		{
			const FFireData& Data = FFireData::Default();
			FFireState Empty;
			TArray<EFireEvent> Events;
			TestFalse(TEXT("Sin combustible no"), FFireModel::TryIgnite(Empty, Data, EIgnitionMethod::Matches, Calm(), 0.0f, Events).bLit);

			FFireState S;
			FFireModel::AddFuel(S, Data, TEXT("fibra_coco"), Events);
			FFireModel::AddFuel(S, Data, TEXT("rama_seca"), Events);
			const FIgnitionResult Fail = FFireModel::TryIgnite(S, Data, EIgnitionMethod::Matches, Calm(), 0.99f, Events);
			TestFalse(TEXT("Tirada alta: no prende"), Fail.bLit);
			TestTrue(TEXT("Gasta la cerilla"), Fail.bConsumedTool);
			TestEqual(TEXT("Gasta la yesca"), S.TinderCharges, 0);
			TestTrue(TEXT("Evento de fallo"), Events.Contains(EFireEvent::IgnitionFailed));

			FFireModel::AddFuel(S, Data, TEXT("fibra_coco"), Events);
			const FIgnitionResult Ok = FFireModel::TryIgnite(S, Data, EIgnitionMethod::Friction, Calm(), 0.0f, Events);
			TestTrue(TEXT("Tirada baja: prende"), Ok.bLit);
			TestFalse(TEXT("La fricción no gasta la rama"), Ok.bConsumedTool);
			TestTrue(TEXT("Cuesta minutos"), Ok.MinutesSpent > 1.0f);
			TestTrue(TEXT("Arde"), S.Status == EFireStatus::Burning && S.Heat > 0.0f);
		});
	});

	Describe("la lluvia y el viento", [this]()
	{
		It("un chaparrón apaga una fogata a la intemperie pero no bajo techo", [this]()
		{
			FFireEnvironment Shower;
			Shower.Rain = 1.0f;
			Shower.Wind = 0.3f;
			FFireState Outside = Lit(EFireLevel::Fogata, {TEXT("tronco_pequeno")});
			FFireState Inside = Outside;
			TArray<EFireEvent> Events;
			FFireModel::Tick(Outside, FFireData::Default(), Shower, 1.0f, Events);
			TestTrue(TEXT("Apagada"), Outside.Status == EFireStatus::Unlit);
			TestTrue(TEXT("Evento de apagado"), Events.Contains(EFireEvent::Extinguished));
			TestTrue(TEXT("Le queda leña mojada"), Outside.FuelHours > 0.0f && Outside.Dampness >= 1.0f);
			TestFalse(TEXT("Ya no es punto de reaparición"), FFireModel::IsRespawnPoint(Outside));

			Shower.bSheltered = true;
			FFireModel::Tick(Inside, FFireData::Default(), Shower, 1.0f, Events);
			TestTrue(TEXT("Bajo techo sigue ardiendo"), Inside.Status == EFireStatus::Burning);
		});

		It("una llovizna no puede con una hoguera y el horno aguanta el chaparrón a la intemperie", [this]()
		{
			FFireEnvironment Drizzle;
			Drizzle.Rain = 0.3f;
			Drizzle.Wind = 0.2f;
			FFireState Hoguera = Lit(EFireLevel::Hoguera, {TEXT("tronco_pequeno"), TEXT("tronco_pequeno")});
			TArray<EFireEvent> Events;
			FFireModel::Tick(Hoguera, FFireData::Default(), Drizzle, 2.0f, Events);
			TestTrue(TEXT("La hoguera sigue"), Hoguera.Status == EFireStatus::Burning);

			FFireEnvironment Shower;
			Shower.Rain = 1.0f;
			Shower.Wind = 0.6f;
			FFireState Horno = Lit(EFireLevel::HornoArcilla, {TEXT("tronco_pequeno")});
			FFireModel::Tick(Horno, FFireData::Default(), Shower, 2.0f, Events);
			TestTrue(TEXT("El horno sigue"), Horno.Status == EFireStatus::Burning);
		});

		It("el viento aviva el fuego y una galerna apaga la fogata", [this]()
		{
			const FFireData& Data = FFireData::Default();
			FFireEnvironment Breeze;
			Breeze.Wind = 0.5f;
			const FFireState S = Lit(EFireLevel::Fogata, {TEXT("tronco_pequeno")});
			TestTrue(TEXT("Con brisa arde más deprisa"),
				FFireModel::RemainingBurnHours(S, Data, Breeze) < FFireModel::RemainingBurnHours(S, Data, Calm()));

			FFireEnvironment Gale;
			Gale.Wind = 1.0f;
			TestTrue(TEXT("La galerna la apaga"), HoursUntilNotBurning(S, Gale, 3.0f) < 1.5f);
			FFireState Out = S;
			TArray<EFireEvent> Events;
			FFireModel::Tick(Out, Data, Gale, 1.5f, Events);
			TestTrue(TEXT("Apagada, no en brasas"), Out.Status == EFireStatus::Unlit);
		});
	});

	Describe("las brasas", [this]()
	{
		It("quedan al acabarse la leña, dan algo de calor y reviven al echar combustible", [this]()
		{
			const FFireData& Data = FFireData::Default();
			FFireState S = Lit(EFireLevel::Fogata, {TEXT("rama_seca")});
			TArray<EFireEvent> Events;
			FFireModel::Tick(S, Data, Calm(), 1.0f, Events);
			TestTrue(TEXT("Brasas"), S.Status == EFireStatus::Embers);
			TestTrue(TEXT("Evento"), Events.Contains(EFireEvent::BurnedDown));
			TestTrue(TEXT("Algo de calor"), S.Heat > 0.0f && S.Heat < Data.GetLevel(EFireLevel::Fogata).HeatScale * 0.5f);
			TestTrue(TEXT("Sigue siendo punto de reaparición"), FFireModel::IsRespawnPoint(S));

			Events.Reset();
			TestTrue(TEXT("Admite leña"), FFireModel::AddFuel(S, Data, TEXT("rama_seca"), Events));
			TestTrue(TEXT("Revive sin encender"), S.Status == EFireStatus::Burning);
			TestTrue(TEXT("Evento de revivir"), Events.Contains(EFireEvent::Revived));
		});

		It("se enfrían del todo si nadie las atiende y la lluvia las ahoga antes", [this]()
		{
			const FFireData& Data = FFireData::Default();
			FFireState S = Lit(EFireLevel::Fogata, {TEXT("rama_seca")});
			TArray<EFireEvent> Events;
			FFireModel::Tick(S, Data, Calm(), 1.0f, Events);
			FFireState Rained = S;
			FFireModel::Tick(S, Data, Calm(), Data.GetLevel(EFireLevel::Fogata).EmberHours + 0.1f, Events);
			TestTrue(TEXT("Apagada"), S.Status == EFireStatus::Unlit);
			TestTrue(TEXT("Evento"), Events.Contains(EFireEvent::WentOut));

			FFireEnvironment Rain;
			Rain.Rain = 0.6f;
			FFireModel::Tick(Rained, Data, Rain, 0.5f, Events);
			TestTrue(TEXT("La lluvia ahoga las brasas en media hora"), Rained.Status == EFireStatus::Unlit);
			TestFalse(TEXT("Mojadas no reviven"), Rained.Status == EFireStatus::Burning);
		});
	});

	Describe("el humo y la señal", [this]()
	{
		It("la hoja verde en una hoguera hace fuego de señal; en una fogata no, y se acaba", [this]()
		{
			const FFireData& Data = FFireData::Default();
			FFireState Hoguera = Lit(EFireLevel::Hoguera, {TEXT("tronco_pequeno")});
			TestFalse(TEXT("Sin hojas no es señal"), FFireModel::IsSignalFire(Hoguera));
			const float CleanSmoke = Hoguera.Smoke;
			TArray<EFireEvent> Events;
			FFireModel::AddFuel(Hoguera, Data, TEXT("hoja_palma"), Events);
			TestTrue(TEXT("Señal"), FFireModel::IsSignalFire(Hoguera));
			TestTrue(TEXT("Más humo"), Hoguera.Smoke > CleanSmoke);

			FFireState Fogata = Lit(EFireLevel::Fogata, {TEXT("rama_seca"), TEXT("hoja_palma")});
			TestFalse(TEXT("Una fogata no es fuego de señal"), FFireModel::IsSignalFire(Fogata));

			FFireModel::Tick(Hoguera, Data, Calm(), 1.0f, Events);
			TestFalse(TEXT("El humo denso se acaba"), FFireModel::IsSignalFire(Hoguera));
		});
	});

	Describe("las entradas no finitas", [this]()
	{
		It("un paso NaN o infinito no toca el fuego y uno enorme termina y lo deja apagado", [this]()
		{
			const FFireData& Data = FFireData::Default();
			FFireState S = Lit(EFireLevel::Hoguera, {TEXT("tronco_pequeno")});
			const FFireState Before = S;
			TArray<EFireEvent> Events;
			FFireModel::Tick(S, Data, Calm(), std::numeric_limits<float>::quiet_NaN(), Events);
			FFireModel::Tick(S, Data, Calm(), std::numeric_limits<float>::infinity(), Events);
			TestTrue(TEXT("Sin cambios"), S == Before);
			TestEqual(TEXT("Sin eventos"), Events.Num(), 0);

			// 1e9 h: 2e10 pasos desbordarían int32 (y tardarían una eternidad).
			FFireModel::Tick(S, Data, Calm(), 1.0e9f, Events);
			TestTrue(TEXT("Apagado"), S.Status == EFireStatus::Unlit);
			TestTrue(TEXT("Se consumió"), Events.Contains(EFireEvent::BurnedDown) && Events.Contains(EFireEvent::WentOut));
		});

		It("un estado cargado con NaN se sanea: arde, se consume y deja de admitir leña", [this]()
		{
			const FFireData& Data = FFireData::Default();
			constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
			FFireState S;
			S.Status = EFireStatus::Burning;
			S.FuelHours = NaN;
			S.FuelHeat = NaN;
			S.Dampness = NaN;
			S.EmberHours = std::numeric_limits<float>::infinity();
			S.SignalSmokeHours = -3.0f;
			S.Heat = NaN;
			S.Smoke = 7.0f;
			S.TinderCharges = 99;
			FFireModel::Sanitize(S);
			TestEqual(TEXT("Combustible"), S.FuelHours, 0.0f);
			TestEqual(TEXT("Calor del combustible"), S.FuelHeat, 0.0f);
			TestEqual(TEXT("Humedad"), S.Dampness, 0.0f);
			TestEqual(TEXT("Brasas"), S.EmberHours, 0.0f);
			TestEqual(TEXT("Señal"), S.SignalSmokeHours, 0.0f);
			TestEqual(TEXT("Calor"), S.Heat, 0.0f);
			TestEqual(TEXT("Humo"), S.Smoke, 1.0f);
			TestEqual(TEXT("Yesca"), S.TinderCharges, FFireModel::MaxTinderCharges);

			TArray<EFireEvent> Events;
			TestTrue(TEXT("Cabe un tronco"), FFireModel::AddFuel(S, Data, TEXT("tronco_pequeno"), Events));
			TestFalse(TEXT("El segundo no cabe"), FFireModel::AddFuel(S, Data, TEXT("tronco_pequeno"), Events));
			FFireModel::Tick(S, Data, Calm(), 24.0f, Events);
			TestTrue(TEXT("Se apaga"), S.Status == EFireStatus::Unlit);
		});

		It("rechaza un combustible con horas NaN en los datos", [this]()
		{
			FFireData Data = FFireData::Default();
			FFuelDef& Bad = Data.Fuels.AddDefaulted_GetRef();
			Bad.ItemId = TEXT("lena_rota");
			Bad.BurnHours = std::numeric_limits<float>::quiet_NaN();
			FFireState S;
			TArray<EFireEvent> Events;
			TestFalse(TEXT("No se acepta"), FFireModel::AddFuel(S, Data, TEXT("lena_rota"), Events));
			TestEqual(TEXT("El hogar sigue vacío"), S.FuelHours, 0.0f);
		});
	});

	It("es determinista: la misma secuencia da exactamente el mismo estado", [this]()
	{
		const FFireData& Data = FFireData::Default();
		auto Run = [&Data]()
		{
			FFireState S;
			TArray<EFireEvent> Events;
			FFireModel::AddFuel(S, Data, TEXT("algodon_silvestre"), Events);
			FFireModel::AddFuel(S, Data, TEXT("madera_dura"), Events);
			FFireModel::TryIgnite(S, Data, EIgnitionMethod::Flint, FFireEnvironment(), 0.2f, Events);
			FFireEnvironment Env;
			for (int32 I = 0; I < 40; ++I)
			{
				Env.Rain = (I % 7) * 0.1f;
				Env.Wind = (I % 5) * 0.2f;
				Env.bSheltered = (I % 3) == 0;
				FFireModel::Tick(S, Data, Env, 0.13f, Events);
				if (I == 20)
				{
					FFireModel::AddFuel(S, Data, TEXT("rama_seca"), Events);
				}
			}
			return S;
		};
		const FFireState A = Run();
		const FFireState B = Run();
		TestTrue(TEXT("Mismo estado"), A == B);

		// Un tick largo equivale a muchos cortos del mismo tamaño total.
		FFireState Long = Lit(EFireLevel::Hoguera, {TEXT("tronco_pequeno")});
		FFireState Short = Long;
		TArray<EFireEvent> Events;
		FFireModel::Tick(Long, Data, Calm(), 2.0f, Events);
		for (int32 I = 0; I < 40; ++I)
		{
			FFireModel::Tick(Short, Data, Calm(), 0.05f, Events);
		}
		TestEqual(TEXT("Mismo combustible"), Long.FuelHours, Short.FuelHours, 1.0e-3f);
	});
}

#endif
