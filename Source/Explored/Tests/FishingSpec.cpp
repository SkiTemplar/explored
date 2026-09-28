#include "Misc/AutomationTest.h"

#include "Fishing/FishingModel.h"
#include "Fishing/FishingTension.h"
#include "Ocean/OceanCurrents.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace FishingTest
{
	/** Arrecife a 8 m al amanecer, marea entrando, mar en calma. */
	FFishingConditions ReefDawn()
	{
		FFishingConditions C;
		C.Habitat = EFishHabitat::Reef;
		C.DepthM = 8.0f;
		C.Hours = 6.0f;
		C.TideLevel = 0.0f;
		C.TideFlow = 1.0f;
		C.MoonPhase01 = 0.25f;
		C.SeaState = 0.15f;
		C.Method = ECatchMethod::Rod;
		C.Bait = EFishBait::Cangrejo;
		return C;
	}

	float Rate(FName Id, const FFishingConditions& C)
	{
		const FFishSpecies* S = FFishingModel::FindSpecies(Id);
		return S ? FFishingModel::BiteRatePerSecond(*S, C) : -1.0f;
	}

	/** Jugador razonable: recoge con el sedal flojo, aguanta en medio y cede cuando se tensa. */
	float GoodPlayer(const FFishFightState& S)
	{
		if (S.Tension01 > 0.8f)
		{
			return -1.0f;
		}
		return S.Tension01 < 0.55f ? 1.0f : 0.0f;
	}

	enum class EPolicy : uint8 { Good, ReelAlways, GiveAlways };

	/** Pelea un ejemplar mediano de la especie y devuelve cuántas veces de N acaba en cada resultado. */
	void Fight(float StrengthKgf, float StaminaSeconds, float Aggression, const FFishingTackle& Tackle, float Abrasion,
		EPolicy Policy, int32 Runs, int32& OutCaught, int32& OutEscaped, int32& OutSnapped)
	{
		OutCaught = OutEscaped = OutSnapped = 0;
		for (int32 Run = 0; Run < Runs; ++Run)
		{
			FFishFightParams P;
			P.StrengthKgf = StrengthKgf;
			P.StaminaSeconds = StaminaSeconds;
			P.Aggression = Aggression;
			P.ApplyTackle(Tackle, Abrasion);
			FFishFight Fight(P, 1000u + static_cast<uint32>(Run) * 7919u);
			while (!Fight.IsFinished())
			{
				float Input = 0.0f;
				switch (Policy)
				{
				case EPolicy::Good: Input = GoodPlayer(Fight.GetState()); break;
				case EPolicy::ReelAlways: Input = 1.0f; break;
				case EPolicy::GiveAlways: Input = -1.0f; break;
				}
				Fight.Tick(1.0f / 30.0f, Input);
			}
			switch (Fight.GetState().Outcome)
			{
			case EFishFightOutcome::Caught: ++OutCaught; break;
			case EFishFightOutcome::Escaped: ++OutEscaped; break;
			default: ++OutSnapped; break;
			}
		}
	}

	void FightSpecies(FName Id, const FFishingTackle& Tackle, EPolicy Policy, int32 Runs,
		int32& OutCaught, int32& OutEscaped, int32& OutSnapped)
	{
		const FFishSpecies* S = FFishingModel::FindSpecies(Id);
		check(S);
		Fight(S->StrengthKgf, S->StaminaSeconds, S->Aggression, Tackle, 0.0f, Policy, Runs, OutCaught, OutEscaped, OutSnapped);
	}

	/** Cuenta picadas en N esperas de 60 s con semillas distintas. */
	int32 CountBites(const FFishingConditions& C, int32 Runs, FName OnlyId = NAME_None)
	{
		int32 Bites = 0;
		for (int32 Run = 0; Run < Runs; ++Run)
		{
			FFishBite Bite;
			if (FFishingModel::WaitForBite(C, nullptr, 77u + static_cast<uint32>(Run), 12, 10.3f, 60.0f, Bite)
				&& (OnlyId.IsNone() || Bite.Id == OnlyId))
			{
				++Bites;
			}
		}
		return Bites;
	}

	bool OnlyItems(const TArray<FTrapCatch>& Catches, const TArray<FName>& Allowed)
	{
		for (const FTrapCatch& Catch : Catches)
		{
			if (!Allowed.Contains(Catch.ItemId))
			{
				return false;
			}
		}
		return true;
	}
}

BEGIN_DEFINE_SPEC(FFishingSpec, "Explored.Fishing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FFishingSpec)

void FFishingSpec::Define()
{
	using namespace FishingTest;

	Describe("Tablas", [this]()
	{
		It("tiene 11 peces, la langosta de arrecife y 5 legendarias (GDD §8.8, biblia §4.6)", [this]()
		{
			int32 Fish = 0;
			for (const FFishSpecies& S : FFishingModel::Species())
			{
				Fish += S.IsFish() ? 1 : 0;
				TestTrue(TEXT("Rango de peso válido"), S.MinWeightKg > 0.0f && S.MinWeightKg < S.MaxWeightKg);
				TestTrue(TEXT("Rango de profundidad válido"), S.MinDepthM < S.MaxDepthM);
			}
			TestEqual(TEXT("Peces con caña"), Fish, 11);
			TestNotNull(TEXT("Langosta"), FFishingModel::FindSpecies(TEXT("langosta")));
			TestFalse(TEXT("La langosta no se pesca con caña"), FFishingModel::FindSpecies(TEXT("langosta"))->CaughtBy(ECatchMethod::Rod));
			TestEqual(TEXT("Legendarias"), FFishingModel::Legendaries().Num(), 5);
			for (const FLegendaryCatch& L : FFishingModel::Legendaries())
			{
				TestTrue(TEXT("Cada legendaria tiene recompensa y sitio"), L.Rewards.Num() > 0 && !L.SpotTag.IsNone());
			}
		});

		It("traduce cebos a objetos y de vuelta", [this]()
		{
			for (int32 I = 1; I < static_cast<int32>(EFishBait::Count); ++I)
			{
				const EFishBait Bait = static_cast<EFishBait>(I);
				TestEqual(TEXT("Ida y vuelta"), FFishingModel::BaitFromItemId(FFishingModel::BaitItemId(Bait)), Bait);
			}
			TestTrue(TEXT("Sin cebo no hay objeto"), FFishingModel::BaitItemId(EFishBait::None).IsNone());
		});
	});

	Describe("Hábitat, hora, marea y luna", [this]()
	{
		It("no hay especies fuera de su hábitat ni con el método equivocado", [this]()
		{
			FFishingConditions Reef = ReefDawn();
			FFishingConditions Deep = ReefDawn();
			Deep.Habitat = EFishHabitat::Deep;
			Deep.DepthM = 400.0f;
			Deep.Bait = EFishBait::Senuelo;
			TestTrue(TEXT("Pez loro en el arrecife"), Rate(TEXT("pez_loro"), Reef) > 0.0f);
			TestEqual(TEXT("Pez loro no en mar abierto"), Rate(TEXT("pez_loro"), Deep), 0.0f);
			TestEqual(TEXT("Atún no en el arrecife"), Rate(TEXT("atun"), Reef), 0.0f);
			TestTrue(TEXT("Atún en mar abierto"), Rate(TEXT("atun"), Deep) > 0.0f);
			TestEqual(TEXT("La langosta no pica con caña"), Rate(TEXT("langosta"), Reef), 0.0f);

			FFishingConditions TooShallow = Reef;
			TooShallow.DepthM = 1.0f;
			TestTrue(TEXT("El pargo no sube a 1 m"), Rate(TEXT("pargo"), TooShallow) < Rate(TEXT("pargo"), Reef) * 0.05f);
		});

		It("respeta las horas de actividad de cada especie", [this]()
		{
			FFishingConditions Noon = ReefDawn();
			Noon.Hours = 12.0f;
			Noon.Bait = EFishBait::FrutaFermentada;
			FFishingConditions Midnight = Noon;
			Midnight.Hours = 0.5f;
			TestTrue(TEXT("El pez loro come de día"), Rate(TEXT("pez_loro"), Noon) > 10.0f * Rate(TEXT("pez_loro"), Midnight));

			FFishingConditions Dawn = ReefDawn();
			FFishingConditions Day = ReefDawn();
			Day.Hours = 12.0f;
			TestTrue(TEXT("El pargo, al amanecer mejor que a mediodía"), Rate(TEXT("pargo"), Dawn) > 2.0f * Rate(TEXT("pargo"), Day));

			TestEqual(TEXT("Amanecer"), FFishingModel::DayPeriod(6.0f), EDayPeriod::Dawn);
			TestEqual(TEXT("Día"), FFishingModel::DayPeriod(12.0f), EDayPeriod::Day);
			TestEqual(TEXT("Atardecer"), FFishingModel::DayPeriod(18.0f), EDayPeriod::Dusk);
			TestEqual(TEXT("Noche"), FFishingModel::DayPeriod(23.0f), EDayPeriod::Night);
		});

		It("lee la marea de FOceanTide y el arrecife come más con la marea entrando", [this]()
		{
			FFishingConditions C;
			C.SetTime(10.375f);
			TestEqual(TEXT("Bajamar a las 9:00"), FFishingModel::TidePhase(C.TideLevel, C.TideFlow), ETidePhase::Low);
			C.SetTime(10.125f);
			TestEqual(TEXT("Pleamar a las 3:00"), FFishingModel::TidePhase(C.TideLevel, C.TideFlow), ETidePhase::High);
			C.SetTime(10.5f);
			TestEqual(TEXT("Llenante a mediodía"), FFishingModel::TidePhase(C.TideLevel, C.TideFlow), ETidePhase::Rising);
			C.SetTime(10.25f);
			TestEqual(TEXT("Vaciante a las 6:00"), FFishingModel::TidePhase(C.TideLevel, C.TideFlow), ETidePhase::Falling);

			FFishingConditions Rising = ReefDawn();
			FFishingConditions Low = ReefDawn();
			Low.TideLevel = -1.0f;
			Low.TideFlow = 0.0f;
			TestTrue(TEXT("Marea entrando mejor que bajamar"), Rate(TEXT("pargo"), Rising) > 1.5f * Rate(TEXT("pargo"), Low));
		});

		It("la luna y el tiempo cambian la picada", [this]()
		{
			FFishingConditions NewMoon = ReefDawn();
			NewMoon.Hours = 22.0f;
			NewMoon.MoonPhase01 = 0.0f;
			FFishingConditions FullMoon = NewMoon;
			FullMoon.MoonPhase01 = 0.5f;
			TestTrue(TEXT("El pargo nocturno prefiere la luna nueva"), Rate(TEXT("pargo"), NewMoon) > Rate(TEXT("pargo"), FullMoon));

			FFishingConditions Storm = ReefDawn();
			Storm.SeaState = 1.0f;
			TestTrue(TEXT("La mar gruesa quita picadas"), Rate(TEXT("pargo"), Storm) < 0.8f * Rate(TEXT("pargo"), ReefDawn()));

			FFishingConditions Murky = ReefDawn();
			Murky.Bait = EFishBait::Senuelo;
			FFishingConditions Clear = Murky;
			Murky.Rain = 1.0f;
			TestTrue(TEXT("La barracuda caza mejor en agua turbia"), Rate(TEXT("barracuda"), Murky) > Rate(TEXT("barracuda"), Clear));
		});
	});

	Describe("Cebo y ruido", [this]()
	{
		It("el cebo favorito multiplica las picadas", [this]()
		{
			FFishingConditions Fruit = ReefDawn();
			Fruit.Hours = 12.0f;
			Fruit.Bait = EFishBait::FrutaFermentada;
			FFishingConditions Bare = Fruit;
			Bare.Bait = EFishBait::None;
			TestTrue(TEXT("Pez loro con fruta fermentada"), Rate(TEXT("pez_loro"), Fruit) > 5.0f * Rate(TEXT("pez_loro"), Bare));

			FFishingConditions Lagoon = Fruit;
			Lagoon.Habitat = EFishHabitat::Lagoon;
			Lagoon.DepthM = 3.0f;
			FFishingConditions Worm = Lagoon;
			Worm.Bait = EFishBait::Lombriz;
			TestTrue(TEXT("El salmonete prefiere lombriz"), Rate(TEXT("salmonete"), Worm) > 3.0f * Rate(TEXT("salmonete"), Lagoon));
		});

		It("el peso que se carga, moverse y chapotear hacen ruido", [this]()
		{
			TestEqual(TEXT("Quieto y ligero, en silencio"), FFishingModel::PlayerNoise(0.2f, 0.0f, 0.0f, false), 0.0f);
			TestTrue(TEXT("Cargado hace ruido"), FFishingModel::PlayerNoise(1.2f, 0.0f, 0.0f, false) > 0.3f);
			TestTrue(TEXT("Chapotear es lo peor"), FFishingModel::PlayerNoise(0.2f, 0.0f, 6.0f, true)
				> FFishingModel::PlayerNoise(0.2f, 1.0f, 0.0f, false));
			TestTrue(TEXT("Acotado"), FFishingModel::PlayerNoise(3.0f, 1.0f, 30.0f, true) <= 1.0f);
		});

		It("el ruido reduce las picadas (estadística sobre 400 esperas)", [this]()
		{
			FFishingConditions Quiet = ReefDawn();
			FFishingConditions Noisy = Quiet;
			Noisy.Noise01 = FFishingModel::PlayerNoise(1.3f, 0.5f, 4.0f, true);
			TestTrue(TEXT("Menos tasa con ruido"), FFishingModel::TotalBiteRatePerSecond(Noisy, nullptr, 0.0f)
				< 0.5f * FFishingModel::TotalBiteRatePerSecond(Quiet, nullptr, 0.0f));
			const int32 QuietBites = CountBites(Quiet, 400);
			const int32 NoisyBites = CountBites(Noisy, 400);
			AddInfo(FString::Printf(TEXT("Picadas en 60 s: %d en silencio, %d con ruido"), QuietBites, NoisyBites));
			TestTrue(TEXT("Más picadas en silencio"), QuietBites > NoisyBites + 40);
		});
	});

	Describe("Minijuego de tensión", [this]()
	{
		It("un jugador razonable saca casi siempre un pez típico", [this]()
		{
			int32 Caught = 0, Escaped = 0, Snapped = 0;
			FightSpecies(TEXT("pargo"), FFishingTackle(), EPolicy::Good, 300, Caught, Escaped, Snapped);
			AddInfo(FString::Printf(TEXT("Pargo: %d sacados, %d escapados, %d rotos"), Caught, Escaped, Snapped));
			TestTrue(TEXT("Al menos el 90 %"), Caught >= 270);
			FightSpecies(TEXT("pez_loro"), FFishingTackle(), EPolicy::Good, 300, Caught, Escaped, Snapped);
			TestTrue(TEXT("Pez loro, al menos el 90 %"), Caught >= 270);
		});

		It("recoger sin parar parte el sedal contra un pez fuerte", [this]()
		{
			int32 Caught = 0, Escaped = 0, Snapped = 0;
			FightSpecies(TEXT("mero"), FFishingTackle(), EPolicy::ReelAlways, 200, Caught, Escaped, Snapped);
			TestTrue(TEXT("Mero: casi siempre se parte"), Snapped >= 190);

			FFishingTackle Nylon;
			Nylon.Line = EFishingLine::Nailon;
			Nylon.Hook = EFishHook::Alambre;
			FightSpecies(TEXT("atun"), Nylon, EPolicy::ReelAlways, 200, Caught, Escaped, Snapped);
			TestTrue(TEXT("Atún: se parte hasta con nailon"), Snapped >= 190);

			// Con un pez pequeño recoger a lo bruto sí funciona: no hay que complicarse.
			FightSpecies(TEXT("salmonete"), FFishingTackle(), EPolicy::ReelAlways, 200, Caught, Escaped, Snapped);
			TestTrue(TEXT("Salmonete a lo bruto"), Caught >= 180);
		});

		It("soltar sedal sin parar deja escapar el pez", [this]()
		{
			int32 Caught = 0, Escaped = 0, Snapped = 0;
			FightSpecies(TEXT("pargo"), FFishingTackle(), EPolicy::GiveAlways, 200, Caught, Escaped, Snapped);
			TestEqual(TEXT("Nunca se saca"), Caught, 0);
			TestTrue(TEXT("Se suelta por el sedal flojo"), Escaped >= 150);
		});

		It("el aparejo cambia los umbrales", [this]()
		{
			FFishingTackle Poor;
			Poor.RodQuality01 = 0.0f;
			FFishingTackle Good;
			Good.RodQuality01 = 1.0f;
			TestTrue(TEXT("Mejor caña, más carga"), Good.BreakKgf() > Poor.BreakKgf());
			TestTrue(TEXT("Mejor caña, tirones más suaves"), Good.ShockRate() < Poor.ShockRate());
			FFishingTackle Nylon = Good;
			Nylon.Line = EFishingLine::Nailon;
			TestTrue(TEXT("Con nailon manda el anzuelo de hueso"), FMath::IsNearlyEqual(Nylon.BreakKgf(), Nylon.HookHoldKgf()));
			Nylon.Hook = EFishHook::Alambre;
			TestTrue(TEXT("Nailon y alambre aguantan el doble"), Nylon.BreakKgf() > 2.0f * Good.BreakKgf());
			FFishingTackle Wire = Poor;
			Wire.Hook = EFishHook::Alambre;
			TestTrue(TEXT("El alambre perdona más holgura"), Wire.SlackGraceSeconds() > Poor.SlackGraceSeconds());
		});

		It("El Viejo rompe la fibra y solo sale con nailon y alambre", [this]()
		{
			const FLegendaryCatch* Viejo = FFishingModel::FindLegendary(TEXT("el_viejo"));
			TestNotNull(TEXT("Existe"), Viejo);
			if (!Viejo)
			{
				return;
			}
			int32 Caught = 0, Escaped = 0, Snapped = 0;
			Fight(Viejo->StrengthKgf, Viejo->StaminaSeconds, Viejo->Aggression, FFishingTackle(), Viejo->RockAbrasionPerSecond,
				EPolicy::Good, 100, Caught, Escaped, Snapped);
			TestEqual(TEXT("Con fibra, nunca"), Caught, 0);

			FFishingTackle NylonBone;
			NylonBone.Line = EFishingLine::Nailon;
			Fight(Viejo->StrengthKgf, Viejo->StaminaSeconds, Viejo->Aggression, NylonBone, Viejo->RockAbrasionPerSecond,
				EPolicy::Good, 100, Caught, Escaped, Snapped);
			TestTrue(TEXT("Con anzuelo de hueso casi nunca"), Caught <= 10);

			FFishingTackle NylonWire = NylonBone;
			NylonWire.Hook = EFishHook::Alambre;
			Fight(Viejo->StrengthKgf, Viejo->StaminaSeconds, Viejo->Aggression, NylonWire, Viejo->RockAbrasionPerSecond,
				EPolicy::Good, 100, Caught, Escaped, Snapped);
			TestTrue(TEXT("Con nailon y alambre, casi siempre"), Caught >= 80);
		});

		It("es determinista y apenas depende del paso de tiempo", [this]()
		{
			FFishFightParams P;
			P.StrengthKgf = 5.0f;
			P.ApplyTackle(FFishingTackle());
			FFishFight A(P, 42u);
			FFishFight B(P, 42u);
			while (!A.IsFinished())
			{
				A.Tick(1.0f / 30.0f, GoodPlayer(A.GetState()));
				B.Tick(1.0f / 30.0f, GoodPlayer(B.GetState()));
			}
			TestEqual(TEXT("Mismo resultado"), A.GetState().Outcome, B.GetState().Outcome);
			TestEqual(TEXT("Mismo tiempo"), A.GetState().ElapsedSeconds, B.GetState().ElapsedSeconds);

			// Un fotograma largo se trocea: la tensión nunca salta de golpe.
			FFishFight Long(P, 7u);
			Long.Tick(0.5f, 1.0f);
			TestTrue(TEXT("Paso largo troceado"), Long.GetState().ElapsedSeconds > 0.49f);
		});
	});

	Describe("Arpón", [this]()
	{
		It("el agua sube los peces: hay que apuntar por debajo", [this]()
		{
			const float Vertical = FFishingModel::TrueDepthFromApparent(1.6f, 0.0f, 1.0f);
			TestTrue(TEXT("En vertical, n veces más hondo"), FMath::IsNearlyEqual(Vertical, FFishingModel::WaterRefractiveIndex, 0.01f));
			const float Oblique = FFishingModel::TrueDepthFromApparent(1.6f, 4.0f, 1.0f);
			TestTrue(TEXT("En oblicuo, todavía más"), Oblique > Vertical);
			for (int32 I = 0; I < 20; ++I)
			{
				const float Apparent = 0.2f + 0.2f * I;
				TestTrue(TEXT("Siempre por debajo"), FFishingModel::TrueDepthFromApparent(1.7f, 0.3f * I, Apparent) > Apparent);
			}
		});

		It("apuntar a la imagen falla más que corregir la refracción", [this]()
		{
			const float Apparent = 0.8f;
			const float True = FFishingModel::TrueDepthFromApparent(1.6f, 1.5f, Apparent);
			const float Naive = FFishingModel::SpearHitChance(ESpearStrike::Thrust, 1.8f, True - Apparent, 0.5f, false);
			const float Corrected = FFishingModel::SpearHitChance(ESpearStrike::Thrust, 1.8f, 0.0f, 0.5f, false);
			TestTrue(TEXT("Corregido acierta mucho más"), Corrected > Naive * 2.0f);
		});

		It("empuje de cerca, lanzamiento de lejos; bajo el agua el lanzamiento no llega", [this]()
		{
			const float ThrustNear = FFishingModel::SpearHitChance(ESpearStrike::Thrust, 1.0f, 0.0f, 0.5f, false);
			const float ThrowNear = FFishingModel::SpearHitChance(ESpearStrike::Throw, 1.0f, 0.0f, 0.5f, false);
			TestTrue(TEXT("De cerca, mejor empujar"), ThrustNear > ThrowNear);
			TestEqual(TEXT("El empuje no llega a 4 m"), FFishingModel::SpearHitChance(ESpearStrike::Thrust, 4.0f, 0.0f, 0.5f, false), 0.0f);
			TestTrue(TEXT("El lanzamiento sí"), FFishingModel::SpearHitChance(ESpearStrike::Throw, 4.0f, 0.0f, 0.5f, false) > 0.3f);
			TestTrue(TEXT("Bajo el agua, el lanzamiento se queda corto"),
				FFishingModel::SpearHitChance(ESpearStrike::Throw, 4.0f, 0.0f, 0.5f, true) < 0.01f);
			TestTrue(TEXT("Los peces cautos esquivan más"), FFishingModel::SpearHitChance(ESpearStrike::Throw, 4.0f, 0.0f, 1.0f, false)
				< FFishingModel::SpearHitChance(ESpearStrike::Throw, 4.0f, 0.0f, 0.0f, false));
		});

		It("con arpón no hace falta cebo, pero sí aguas someras", [this]()
		{
			FFishingConditions Shallow = ReefDawn();
			Shallow.Method = ECatchMethod::Spear;
			Shallow.Bait = EFishBait::None;
			Shallow.DepthM = 3.0f;
			Shallow.Hours = 23.0f;
			TestTrue(TEXT("Langosta de noche con arpón"), Rate(TEXT("langosta"), Shallow) > 0.0f);
			FFishingConditions DeepWater = Shallow;
			DeepWater.DepthM = 20.0f;
			TestEqual(TEXT("A 20 m no se ve"), Rate(TEXT("langosta"), DeepWater), 0.0f);
		});
	});

	Describe("Red, trampas y pozas", [this]()
	{
		It("la red de mano solo trabaja en lo somero", [this]()
		{
			FFishingConditions Lagoon = ReefDawn();
			Lagoon.Habitat = EFishHabitat::Lagoon;
			Lagoon.DepthM = 1.5f;
			Lagoon.Hours = 12.0f;
			int32 Total = 0;
			for (int32 Spot = 0; Spot < 100; ++Spot)
			{
				Total += FFishingModel::CastNet(Lagoon, 9u, Spot, 3.2f).Num();
			}
			TestTrue(TEXT("Algo cae en la laguna"), Total > 10);
			FFishingConditions Deep = Lagoon;
			Deep.DepthM = 12.0f;
			TestEqual(TEXT("Nada a 12 m"), FFishingModel::CastNet(Deep, 9u, 1, 3.2f).Num(), 0);
		});

		It("una nasa acumula capturas con el tiempo sin pasar de su capacidad", [this]()
		{
			FFishingSaveState State;
			FPlacedTrap& Trap = State.PlaceTrap(ETrapKind::Nasa, EFishHabitat::Reef, FVector::ZeroVector, EFishBait::Visceras, 2.0f);
			FFishingModel::AdvanceTrap(Trap, 2.05f, 5u);
			const int32 AfterAnHour = Trap.Contents.Num();
			FFishingModel::AdvanceTrap(Trap, 3.0f, 5u);
			const int32 AfterADay = Trap.Contents.Num();
			FFishingModel::AdvanceTrap(Trap, 30.0f, 5u);
			TestTrue(TEXT("Crece o se mantiene"), AfterAnHour <= AfterADay);
			TestEqual(TEXT("Llena al cabo de muchos días"), Trap.Contents.Num(), FFishingModel::TrapCapacity(ETrapKind::Nasa));
			TestTrue(TEXT("El cebo se acaba"), Trap.BaitLeft01 <= 0.0f);

			const TArray<FTrapCatch> Taken = FFishingModel::CollectTrap(Trap, 30.0f, 5u);
			TestEqual(TEXT("Se lleva todo"), Taken.Num(), FFishingModel::TrapCapacity(ETrapKind::Nasa));
			TestEqual(TEXT("Vacía tras revisarla"), Trap.Contents.Num(), 0);
			for (const FTrapCatch& Catch : Taken)
			{
				TestEqual(TEXT("Fresco al sacarlo"), Catch.CaughtAtDays, 30.0f);
			}
		});

		It("un tiempo NaN, infinito o enorme no cuelga la trampa", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			const float Inf = std::numeric_limits<float>::infinity();
			FFishingSaveState State;
			FPlacedTrap& Trap = State.PlaceTrap(ETrapKind::Nasa, EFishHabitat::Reef, FVector::ZeroVector, EFishBait::Visceras, 2.0f);
			TestEqual(TEXT("revisar en NaN no se lleva nada"), FFishingModel::CollectTrap(Trap, NaN, 5u).Num(), 0);
			TestEqual(TEXT("ni guarda el NaN"), Trap.SimulatedToDays, 2.0f);
			FFishingModel::AdvanceTrap(Trap, Inf, 5u);
			TestEqual(TEXT("infinito no avanza"), Trap.SimulatedToDays, 2.0f);

			// Guardado corrupto: se recupera como mucho MaxTrapCatchUpDays.
			Trap.SimulatedToDays = NaN;
			FFishingModel::AdvanceTrap(Trap, 3.0f, 5u);
			TestEqual(TEXT("NaN guardado: sigue desde ahora"), Trap.SimulatedToDays, 3.0f);
			TestTrue(TEXT("sin pasar de la capacidad"), Trap.Contents.Num() <= FFishingModel::TrapCapacity(ETrapKind::Nasa));

			Trap.SimulatedToDays = -Inf;
			FFishingModel::AdvanceTrap(Trap, 1.0e30f, 5u);
			TestEqual(TEXT("un salto enorme termina"), Trap.SimulatedToDays, 1.0e30f);
			const int32 Inside = Trap.Contents.Num();
			TestEqual(TEXT("revisarla antes no simula hacia atrás"), FFishingModel::CollectTrap(Trap, 4.0f, 5u).Num(), Inside);
		});

		It("el cebo llena antes las trampas (media de 200 nasas en un día)", [this]()
		{
			FFishingSaveState State;
			int32 Baited = 0;
			int32 Bare = 0;
			for (int32 I = 0; I < 200; ++I)
			{
				FPlacedTrap& A = State.PlaceTrap(ETrapKind::Nasa, EFishHabitat::Reef, FVector::ZeroVector, EFishBait::Visceras, 1.0f);
				FFishingModel::AdvanceTrap(A, 2.0f, 11u);
				Baited += A.Contents.Num();
				FPlacedTrap& B = State.PlaceTrap(ETrapKind::Nasa, EFishHabitat::Reef, FVector::ZeroVector, EFishBait::None, 1.0f);
				FFishingModel::AdvanceTrap(B, 2.0f, 11u);
				Bare += B.Contents.Num();
			}
			AddInfo(FString::Printf(TEXT("Capturas en un día: %d con cebo, %d sin cebo"), Baited, Bare));
			TestTrue(TEXT("Con cebo, bastante más"), Baited > Bare * 3 / 2);
		});

		It("simular de una vez o a trozos da lo mismo", [this]()
		{
			FFishingSaveState State;
			FPlacedTrap& Once = State.PlaceTrap(ETrapKind::CrabTrap, EFishHabitat::Shore, FVector::ZeroVector, EFishBait::Visceras, 4.1f);
			FPlacedTrap Pieces = Once;
			FFishingModel::AdvanceTrap(Once, 6.6f, 3u);
			for (float T = 4.2f; T <= 6.6f; T += 0.37f)
			{
				FFishingModel::AdvanceTrap(Pieces, T, 3u);
			}
			FFishingModel::AdvanceTrap(Pieces, 6.6f, 3u);
			TestEqual(TEXT("Mismo número"), Once.Contents.Num(), Pieces.Contents.Num());
			for (int32 I = 0; I < FMath::Min(Once.Contents.Num(), Pieces.Contents.Num()); ++I)
			{
				TestTrue(TEXT("Misma captura"), Once.Contents[I].ItemId == Pieces.Contents[I].ItemId
					&& Once.Contents[I].WeightKg == Pieces.Contents[I].WeightKg);
			}
		});

		It("la trampa de cangrejos solo da marisco y el corral pesca con la marea bajando", [this]()
		{
			FFishingSaveState State;
			const TArray<FName> Shellfish = { TEXT("cangrejo"), TEXT("cangrejo_cocotero"), TEXT("pulpo"), TEXT("langosta") };
			for (int32 I = 0; I < 50; ++I)
			{
				FPlacedTrap& Crab = State.PlaceTrap(ETrapKind::CrabTrap, EFishHabitat::Shore, FVector::ZeroVector, EFishBait::FrutaFermentada, 1.0f);
				FFishingModel::AdvanceTrap(Crab, 3.0f, 21u);
				TestTrue(TEXT("Sin peces en la trampa de cangrejos"), OnlyItems(Crab.Contents, Shellfish));

				FPlacedTrap& Corral = State.PlaceTrap(ETrapKind::StoneCorral, EFishHabitat::Shore, FVector::ZeroVector, EFishBait::None, 1.0f);
				FFishingModel::AdvanceTrap(Corral, 3.0f, 21u);
				for (const FTrapCatch& Catch : Corral.Contents)
				{
					TestTrue(TEXT("El corral atrapa al vaciar"), FOceanTide::Flow(Catch.CaughtAtDays) < 0.0f);
				}
			}
		});

		It("las pozas de marea solo dan marisco en bajamar, una vez por bajamar", [this]()
		{
			FFishingSaveState State;
			const TArray<FName> PoolItems = { TEXT("cangrejo"), TEXT("lapa"), TEXT("erizo"), TEXT("pulpo") };
			TestEqual(TEXT("Nada en pleamar"), FFishingModel::GatherTidePool(State, 1, 10.125f, 8u).Num(), 0);
			TestEqual(TEXT("Nada a media marea"), FFishingModel::GatherTidePool(State, 1, 10.0f, 8u).Num(), 0);
			const TArray<FTrapCatch> Low = FFishingModel::GatherTidePool(State, 1, 10.375f, 8u);
			TestTrue(TEXT("Algo en bajamar"), Low.Num() >= 1 && Low.Num() <= 4);
			TestTrue(TEXT("Solo marisco de poza"), OnlyItems(Low, PoolItems));
			TestEqual(TEXT("Ya vaciada esta bajamar"), FFishingModel::GatherTidePool(State, 1, 10.39f, 8u).Num(), 0);
			TestTrue(TEXT("Otra poza sí"), FFishingModel::GatherTidePool(State, 2, 10.39f, 8u).Num() >= 1);
			TestTrue(TEXT("En la siguiente bajamar se rellena"), FFishingModel::GatherTidePool(State, 1, 10.875f, 8u).Num() >= 1);
			TestEqual(TEXT("Dos bajamares al día"), FFishingModel::LowTideIndex(10.875f) - FFishingModel::LowTideIndex(10.375f), 1);
		});
	});

	Describe("Legendarias", [this]()
	{
		It("solo aparecen en su sitio y con sus condiciones", [this]()
		{
			FFishingConditions Anywhere = ReefDawn();
			for (const FLegendaryCatch& L : FFishingModel::Legendaries())
			{
				TestEqual(TEXT("Nunca fuera de su sitio"), FFishingModel::LegendaryRatePerSecond(L, Anywhere, nullptr, 0.0f), 0.0f);
			}
			const FLegendaryCatch* Rey = FFishingModel::FindLegendary(TEXT("rey_de_plata"));
			FFishingConditions Open = ReefDawn();
			Open.SpotTag = TEXT("mar_abierto");
			Open.Habitat = EFishHabitat::Deep;
			Open.DepthM = 500.0f;
			Open.Bait = EFishBait::Senuelo;
			TestEqual(TEXT("El Rey de Plata no se pesca desde tierra"), FFishingModel::LegendaryRatePerSecond(*Rey, Open, nullptr, 0.0f), 0.0f);
			Open.bFromBoat = true;
			TestTrue(TEXT("Desde la canoa, sí"), FFishingModel::LegendaryRatePerSecond(*Rey, Open, nullptr, 0.0f) > 0.0f);
		});

		It("son raras incluso en su sitio y no vuelven una vez capturadas", [this]()
		{
			FFishingConditions Cave = ReefDawn();
			Cave.SpotTag = TEXT("cueva_arenas_blancas");
			Cave.Hours = 18.0f;
			int32 Bites = 0;
			int32 Legendary = 0;
			for (int32 Run = 0; Run < 3000; ++Run)
			{
				FFishBite Bite;
				if (FFishingModel::WaitForBite(Cave, nullptr, 1u + static_cast<uint32>(Run), 3, 5.0f, 90.0f, Bite))
				{
					++Bites;
					Legendary += Bite.bLegendary ? 1 : 0;
					if (Bite.bLegendary)
					{
						TestTrue(TEXT("Es El Viejo"), Bite.Id == FName(TEXT("el_viejo")));
						TestTrue(TEXT("Grande"), Bite.WeightKg >= 60.0f);
					}
				}
			}
			AddInfo(FString::Printf(TEXT("El Viejo: %d de %d picadas"), Legendary, Bites));
			TestTrue(TEXT("Aparece alguna vez"), Legendary > 0);
			TestTrue(TEXT("Menos del 5 % de las picadas"), Legendary * 20 < Bites);

			FFishingSaveState State;
			State.MarkLegendaryCaught(TEXT("el_viejo"));
			const FLegendaryCatch* Viejo = FFishingModel::FindLegendary(TEXT("el_viejo"));
			TestEqual(TEXT("Capturado, no vuelve"), FFishingModel::LegendaryRatePerSecond(*Viejo, Cave, &State, 5.0f), 0.0f);
		});

		It("El Errante huye unos días si te ha oído", [this]()
		{
			FFishingSaveState State;
			FFishingConditions Reef = ReefDawn();
			Reef.SpotTag = TEXT("arrecife_arenas_blancas");
			Reef.Method = ECatchMethod::Spear;
			Reef.DepthM = 3.0f;
			const FLegendaryCatch* Errante = FFishingModel::FindLegendary(TEXT("el_errante"));
			TestTrue(TEXT("Presente al principio"), FFishingModel::LegendaryRatePerSecond(*Errante, Reef, &State, 4.0f) > 0.0f);
			FFishingModel::NoteLegendaryPresence(State, TEXT("el_errante"), 0.1f, 4.0f);
			TestTrue(TEXT("Un pescador silencioso no lo espanta"), FFishingModel::LegendaryRatePerSecond(*Errante, Reef, &State, 4.0f) > 0.0f);
			FFishingModel::NoteLegendaryPresence(State, TEXT("el_errante"), 0.7f, 4.0f);
			TestEqual(TEXT("Espantado"), FFishingModel::LegendaryRatePerSecond(*Errante, Reef, &State, 5.0f), 0.0f);
			TestTrue(TEXT("Vuelve a los tres días"), FFishingModel::LegendaryRatePerSecond(*Errante, Reef, &State, 7.5f) > 0.0f);
		});
	});

	Describe("Despiece", [this]()
	{
		It("hace falta cuchillo y el marisco se cocina entero", [this]()
		{
			FButcherResult Result;
			TestFalse(TEXT("Sin filo no"), FFishingModel::Butcher(TEXT("pargo"), 3.0f, 0.0f, 1.0f, Result));
			TestFalse(TEXT("La langosta no se despieza"), FFishingModel::Butcher(TEXT("langosta"), 1.0f, 0.6f, 1.0f, Result));
			TestFalse(TEXT("Algo desconocido tampoco"), FFishingModel::Butcher(TEXT("coco_maduro"), 1.0f, 0.6f, 1.0f, Result));
		});

		It("saca filetes, espinas, piel, vísceras y aceite; lo crudo se pudre en un día", [this]()
		{
			FButcherResult Result;
			TestTrue(TEXT("Se despieza"), FFishingModel::Butcher(TEXT("bonito"), 6.0f, 0.6f, 2.5f, Result));
			auto Count = [&Result](const TCHAR* Id)
			{
				const FButcherYield* Y = Result.Yields.FindByPredicate([Id](const FButcherYield& Yield) { return Yield.ItemId == FName(Id); });
				return Y ? Y->Count : 0;
			};
			TestTrue(TEXT("Filetes"), Count(TEXT("filete_pescado")) >= 5);
			TestTrue(TEXT("Espinas"), Count(TEXT("espina_pescado")) >= 1);
			TestTrue(TEXT("Piel"), Count(TEXT("piel_pescado")) >= 1);
			TestTrue(TEXT("Vísceras para cebo"), Count(TEXT("visceras_pescado")) >= 1);
			TestTrue(TEXT("Aceite del pescado azul"), Count(TEXT("aceite_pescado")) >= 1);
			TestEqual(TEXT("El frescor empieza ahora"), Result.FreshSinceDays, 2.5f);
			for (const FButcherYield& Y : Result.Yields)
			{
				const bool bPerishable = Y.ItemId == FName(TEXT("filete_pescado")) || Y.ItemId == FName(TEXT("visceras_pescado"));
				TestEqual(TEXT("Lo crudo, un día; lo demás no se pudre"), Y.SpoilAfterDays, bPerishable ? FFishingModel::RawSpoilDays : -1.0f);
			}

			FButcherResult Blunt;
			FButcherResult Sharp;
			FFishingModel::Butcher(TEXT("mero"), 12.0f, 0.2f, 0.0f, Blunt);
			FFishingModel::Butcher(TEXT("mero"), 12.0f, 1.0f, 0.0f, Sharp);
			TestTrue(TEXT("Buen filo, más rápido"), Sharp.Seconds < Blunt.Seconds);
			TestTrue(TEXT("Buen filo, más carne"), Sharp.Yields[0].Count > Blunt.Yields[0].Count);
		});

		It("una legendaria da su recompensa exclusiva", [this]()
		{
			FButcherResult Result;
			TestTrue(TEXT("Se despieza"), FFishingModel::Butcher(TEXT("rey_de_plata"), 100.0f, 0.8f, 0.0f, Result));
			TestTrue(TEXT("Sedal legendario"), Result.Yields.ContainsByPredicate([](const FButcherYield& Y) { return Y.ItemId == FName(TEXT("sedal_legendario")); }));
			TestTrue(TEXT("Anzuelo legendario"), Result.Yields.ContainsByPredicate([](const FButcherYield& Y) { return Y.ItemId == FName(TEXT("anzuelo_legendario")); }));
		});

		It("un peso NaN, infinito o enorme da un despiece acotado", [this]()
		{
			for (const float Bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 1.0e30f })
			{
				FButcherResult Result;
				TestTrue(TEXT("se despieza"), FFishingModel::Butcher(TEXT("bonito"), Bad, 0.6f, 1.0f, Result));
				for (const FButcherYield& Y : Result.Yields)
				{
					TestTrue(FString::Printf(TEXT("%s entre 1 y 60"), *Y.ItemId.ToString()), Y.Count >= 1 && Y.Count <= 60);
				}
				TestTrue(TEXT("tiempo finito y acotado"), FMath::IsFinite(Result.Seconds) && Result.Seconds <= 600.0f);
			}
		});
	});

	Describe("Determinismo y ecosistema", [this]()
	{
		It("misma semilla, sitio y hora dan la misma picada", [this]()
		{
			const FFishingConditions C = ReefDawn();
			FFishBite A;
			FFishBite B;
			const bool bA = FFishingModel::WaitForBite(C, nullptr, 123u, 45, 7.25f, 300.0f, A);
			const bool bB = FFishingModel::WaitForBite(C, nullptr, 123u, 45, 7.25f, 300.0f, B);
			TestTrue(TEXT("Pica en 5 minutos"), bA);
			TestEqual(TEXT("Igual"), bA, bB);
			TestTrue(TEXT("Misma especie"), A.Id == B.Id);
			TestEqual(TEXT("Misma espera"), A.WaitSeconds, B.WaitSeconds);
			TestEqual(TEXT("Mismo peso"), A.WeightKg, B.WeightKg);
			TestEqual(TEXT("Misma pelea"), A.FightSeed, B.FightSeed);

			int32 Different = 0;
			for (int32 Spot = 0; Spot < 20; ++Spot)
			{
				FFishBite Other;
				FFishingModel::WaitForBite(C, nullptr, 123u, 1000 + Spot, 7.25f, 300.0f, Other);
				Different += (Other.WaitSeconds != A.WaitSeconds || Other.Id != A.Id) ? 1 : 0;
			}
			TestTrue(TEXT("Otro sitio, otra picada"), Different >= 15);
		});

		It("una espera enorme o un instante o condiciones NaN no rompen la picada", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			const FFishingConditions C = ReefDawn();
			FFishBite Normal;
			FFishBite Huge;
			TestTrue(TEXT("pica en 5 minutos"), FFishingModel::WaitForBite(C, nullptr, 123u, 45, 7.25f, 300.0f, Normal));
			TestTrue(TEXT("con espera enorme también"), FFishingModel::WaitForBite(C, nullptr, 123u, 45, 7.25f, 1.0e30f, Huge));
			TestEqual(TEXT("y es la misma picada"), Huge.WaitSeconds, Normal.WaitSeconds);
			FFishBite Bite;
			TestFalse(TEXT("instante NaN: no pica"), FFishingModel::WaitForBite(C, nullptr, 123u, 45, NaN, 300.0f, Bite));
			TestFalse(TEXT("espera NaN: no pica"), FFishingModel::WaitForBite(C, nullptr, 123u, 45, 7.25f, NaN, Bite));
			FFishingConditions Broken = C;
			Broken.Hours = NaN;
			Broken.DepthM = NaN;
			if (FFishingModel::WaitForBite(Broken, nullptr, 123u, 45, 7.25f, 300.0f, Bite))
			{
				TestTrue(TEXT("condiciones NaN: si pica, con peso finito"), FMath::IsFinite(Bite.WeightKg) && Bite.Id != NAME_None);
			}
		});

		It("los ejemplares grandes son raros y pelean más", [this]()
		{
			const FFishingConditions C = ReefDawn();
			int32 Big = 0;
			int32 Total = 0;
			for (int32 Run = 0; Run < 600; ++Run)
			{
				FFishBite Bite;
				if (FFishingModel::WaitForBite(C, nullptr, 900u + static_cast<uint32>(Run), 5, 3.0f, 120.0f, Bite) && Bite.Id == FName(TEXT("pargo")))
				{
					++Total;
					const FFishSpecies* S = FFishingModel::FindSpecies(Bite.Id);
					const bool bBig = Bite.WeightKg > FMath::Lerp(S->MinWeightKg, S->MaxWeightKg, 0.75f);
					Big += bBig ? 1 : 0;
					TestTrue(TEXT("Fuerza acorde al tamaño"), Bite.Fight.StrengthKgf >= S->StrengthKgf * 0.75f - 1e-3f);
				}
			}
			TestTrue(TEXT("Hay pargos"), Total > 20);
			TestTrue(TEXT("Menos de un cuarto son grandes"), Big * 4 < Total);
		});

		It("sobrepescar vacía la zona y se recupera sola en días", [this]()
		{
			FFishingSaveState State;
			const int32 Zone = FFishingModel::ZoneKeyAt(FVector2D(12345.0, -6789.0));
			TestEqual(TEXT("Misma zona a pocos metros"), FFishingModel::ZoneKeyAt(FVector2D(12400.0, -6700.0)), Zone);
			for (int32 I = 0; I < 8; ++I)
			{
				FFishingModel::RegisterCatch(State, Zone, 2.0f);
			}
			const float Depleted = FFishingModel::ZoneDepletion(State, Zone, 2.0f);
			TestTrue(TEXT("Zona vaciada"), Depleted > 0.8f);
			FFishingConditions Full = ReefDawn();
			FFishingConditions Empty = Full;
			Empty.Depletion01 = Depleted;
			TestTrue(TEXT("Menos picadas"), Rate(TEXT("pargo"), Empty) < 0.4f * Rate(TEXT("pargo"), Full));
			TestTrue(TEXT("Recuperada en una semana"), FFishingModel::ZoneDepletion(State, Zone, 9.0f) < 0.05f);
		});

		It("guarda las trampas como datos planos", [this]()
		{
			FFishingSaveState State;
			const int32 A = State.PlaceTrap(ETrapKind::Nasa, EFishHabitat::Reef, FVector(1, 2, 3), EFishBait::Lombriz, 0.0f).Id;
			const int32 B = State.PlaceTrap(ETrapKind::CrabTrap, EFishHabitat::Shore, FVector(4, 5, 6), EFishBait::None, 0.0f).Id;
			TestNotEqual(TEXT("Ids distintos"), A, B);
			TestNotNull(TEXT("Se encuentra"), State.FindTrap(B));
			FFishingSaveState Copy = State;
			TestTrue(TEXT("La copia conserva la trampa"), Copy.FindTrap(A) && Copy.FindTrap(A)->Bait == EFishBait::Lombriz);
			TestTrue(TEXT("Se quita"), State.RemoveTrap(A));
			TestNull(TEXT("Ya no está"), State.FindTrap(A));
			TestFalse(TEXT("No se quita dos veces"), State.RemoveTrap(A));
		});
	});
}

#endif
