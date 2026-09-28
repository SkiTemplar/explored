#include "Fishing/FishingModel.h"

#include "Core/ExploredRandom.h"
#include "Ocean/OceanCurrents.h"
#include "Weather/WeatherModel.h"

namespace FishingModelDetail
{
	using EHab = EFishHabitat;
	using EMet = ECatchMethod;

	/** Ciclo lunar en días (ExploredSky::DaysPerLunarCycle; aquí sin depender del subsistema). */
	constexpr float LunarCycleDays = 12.0f;

	/** Marea por hábitat [bajamar, llenante, pleamar, vaciante]: el arrecife come con la marea que entra. */
	constexpr float ReefTide[] = { 0.5f, 1.2f, 0.9f, 1.0f };
	constexpr float LagoonTide[] = { 0.4f, 0.9f, 1.2f, 1.0f };
	constexpr float OffshoreTide[] = { 0.9f, 1.1f, 1.0f, 1.1f };
	/** Luna [nueva, creciente, llena, menguante]: los nocturnos comen más a oscuras; los pelágicos, con luna llena. */
	constexpr float DarkMoon[] = { 1.2f, 1.0f, 0.8f, 1.0f };
	constexpr float BrightMoon[] = { 0.9f, 1.0f, 1.2f, 1.0f };

	/** FloorToInt sin UB: lo no finito da 0 y lo enorme se acota antes de convertir. */
	int32 SafeFloorToInt(double V)
	{
		return FMath::IsFinite(V) ? static_cast<int32>(FMath::FloorToDouble(FMath::Clamp(V, -2.0e9, 2.0e9))) : 0;
	}

	template <int32 N>
	void Fill(float (&Dst)[N], const float (&Src)[N])
	{
		for (int32 I = 0; I < N; ++I)
		{
			Dst[I] = Src[I];
		}
	}

	FFishSpecies MakeSpecies(const TCHAR* Id, const TCHAR* NameEs, uint8 Habitats, uint8 Methods,
		float MinDepthM, float MaxDepthM, float BitesPerMinute, float MinKg, float MaxKg,
		float StrengthKgf, float StaminaSeconds, float Aggression, float Wariness)
	{
		FFishSpecies S;
		S.Id = FName(Id);
		S.NameEs = NameEs;
		S.HabitatMask = Habitats;
		S.MethodMask = Methods;
		S.MinDepthM = MinDepthM;
		S.MaxDepthM = MaxDepthM;
		S.BitesPerMinute = BitesPerMinute;
		S.MinWeightKg = MinKg;
		S.MaxWeightKg = MaxKg;
		S.StrengthKgf = StrengthKgf;
		S.StaminaSeconds = StaminaSeconds;
		S.Aggression = Aggression;
		S.Wariness = Wariness;
		return S;
	}

	/**
	 * Tabla de especies. Espejo de Content/Data/fish.json: si cambias un id
	 * aquí, cámbialo allí (DataCheck lo comprueba). Cebos en el orden de
	 * EFishBait: [sin cebo, lombriz, vísceras, fruta fermentada, cangrejo, señuelo].
	 */
	TArray<FFishSpecies> BuildSpecies()
	{
		const uint8 Rod = FishBit(EMet::Rod);
		const uint8 Spear = FishBit(EMet::Spear);
		const uint8 Net = FishBit(EMet::Net);
		const uint8 Trap = FishBit(EMet::Trap);
		const uint8 Hand = FishBit(EMet::Hand);

		TArray<FFishSpecies> Out;

		// --- Arrecife y laguna (8) ---
		{
			FFishSpecies S = MakeSpecies(TEXT("pez_loro"), TEXT("Pez loro"), FishBit(EHab::Reef), Rod | Spear | Net | Trap,
				1.0f, 20.0f, 0.6f, 0.8f, 3.0f, 2.5f, 8.0f, 0.4f, 0.6f);
			Fill(S.PeriodWeight, { 0.6f, 1.0f, 0.5f, 0.02f });
			Fill(S.TideWeight, ReefTide);
			Fill(S.BaitWeight, { 0.15f, 0.4f, 0.2f, 1.0f, 0.3f, 0.1f });
			S.SeaStateSensitivity = 0.6f;
			S.FilletRatio = 0.4f;
			Out.Add(S);
		}
		{
			FFishSpecies S = MakeSpecies(TEXT("pez_cirujano"), TEXT("Pez cirujano"), FishBit(EHab::Reef) | FishBit(EHab::Lagoon),
				Rod | Spear | Net | Trap, 0.5f, 15.0f, 0.8f, 0.3f, 1.2f, 1.5f, 5.0f, 0.4f, 0.5f);
			Fill(S.PeriodWeight, { 0.7f, 1.0f, 0.6f, 0.02f });
			Fill(S.TideWeight, ReefTide);
			Fill(S.BaitWeight, { 0.15f, 0.6f, 0.3f, 0.8f, 0.2f, 0.1f });
			S.SeaStateSensitivity = 0.6f;
			S.FilletRatio = 0.35f;
			Out.Add(S);
		}
		{
			FFishSpecies S = MakeSpecies(TEXT("pargo"), TEXT("Pargo"), FishBit(EHab::Reef) | FishBit(EHab::Slope), Rod | Spear,
				5.0f, 60.0f, 0.5f, 1.0f, 6.0f, 3.5f, 10.0f, 0.5f, 0.5f);
			Fill(S.PeriodWeight, { 1.0f, 0.4f, 1.0f, 0.8f });
			Fill(S.TideWeight, ReefTide);
			Fill(S.MoonWeight, DarkMoon);
			Fill(S.BaitWeight, { 0.15f, 0.7f, 0.9f, 0.1f, 1.0f, 0.5f });
			S.SeaStateSensitivity = 0.4f;
			S.FilletRatio = 0.42f;
			S.OilRatio = 0.015f;
			Out.Add(S);
		}
		{
			FFishSpecies S = MakeSpecies(TEXT("mero"), TEXT("Mero"), FishBit(EHab::Reef) | FishBit(EHab::Slope), Rod | Spear,
				5.0f, 60.0f, 0.25f, 3.0f, 18.0f, 8.0f, 14.0f, 0.5f, 0.7f);
			Fill(S.PeriodWeight, { 0.9f, 0.3f, 1.0f, 0.6f });
			Fill(S.TideWeight, ReefTide);
			Fill(S.MoonWeight, DarkMoon);
			Fill(S.BaitWeight, { 0.1f, 0.3f, 0.8f, 0.0f, 1.0f, 0.3f });
			S.SeaStateSensitivity = 0.3f;
			S.FilletRatio = 0.4f;
			S.OilRatio = 0.02f;
			Out.Add(S);
		}
		{
			FFishSpecies S = MakeSpecies(TEXT("salmonete"), TEXT("Salmonete"), FishBit(EHab::Lagoon), Rod | Spear | Net | Trap,
				0.5f, 8.0f, 0.9f, 0.2f, 0.7f, 0.8f, 4.0f, 0.4f, 0.4f);
			Fill(S.PeriodWeight, { 0.8f, 1.0f, 0.7f, 0.1f });
			Fill(S.TideWeight, LagoonTide);
			Fill(S.BaitWeight, { 0.2f, 1.0f, 0.5f, 0.1f, 0.4f, 0.0f });
			S.SeaStateSensitivity = 0.7f;
			S.FilletRatio = 0.35f;
			Out.Add(S);
		}
		{
			FFishSpecies S = MakeSpecies(TEXT("pez_ballesta"), TEXT("Pez ballesta"), FishBit(EHab::Reef), Rod | Spear | Trap,
				2.0f, 25.0f, 0.5f, 0.5f, 2.5f, 3.0f, 9.0f, 0.5f, 0.3f);
			Fill(S.PeriodWeight, { 0.6f, 1.0f, 0.6f, 0.02f });
			Fill(S.TideWeight, ReefTide);
			Fill(S.BaitWeight, { 0.15f, 0.6f, 0.5f, 0.2f, 1.0f, 0.2f });
			S.SeaStateSensitivity = 0.5f;
			S.FilletRatio = 0.33f;
			Out.Add(S);
		}
		{
			FFishSpecies S = MakeSpecies(TEXT("barracuda"), TEXT("Barracuda"), FishBit(EHab::Lagoon) | FishBit(EHab::Reef), Rod | Spear,
				1.0f, 30.0f, 0.3f, 2.0f, 10.0f, 6.0f, 10.0f, 0.7f, 0.6f);
			Fill(S.PeriodWeight, { 1.0f, 0.7f, 1.0f, 0.3f });
			Fill(S.TideWeight, LagoonTide);
			Fill(S.BaitWeight, { 0.05f, 0.1f, 0.7f, 0.0f, 0.3f, 1.0f });
			S.SeaStateSensitivity = 0.4f;
			S.RainAffinity = 0.3f;
			S.FilletRatio = 0.45f;
			S.OilRatio = 0.02f;
			Out.Add(S);
		}
		{
			FFishSpecies S = MakeSpecies(TEXT("jurel"), TEXT("Jurel"), FishBit(EHab::Reef) | FishBit(EHab::Slope), Rod | Spear | Trap,
				2.0f, 80.0f, 0.4f, 1.0f, 8.0f, 5.0f, 12.0f, 0.7f, 0.5f);
			Fill(S.PeriodWeight, { 1.0f, 0.5f, 1.0f, 0.2f });
			Fill(S.TideWeight, OffshoreTide);
			Fill(S.BaitWeight, { 0.05f, 0.2f, 0.8f, 0.0f, 0.4f, 1.0f });
			S.SeaStateSensitivity = 0.3f;
			S.FilletRatio = 0.45f;
			S.OilRatio = 0.04f;
			Out.Add(S);
		}

		// --- Mar abierto (3) ---
		{
			FFishSpecies S = MakeSpecies(TEXT("bonito"), TEXT("Bonito"), FishBit(EHab::Slope) | FishBit(EHab::Deep), Rod,
				20.0f, 3000.0f, 0.5f, 2.0f, 7.0f, 6.0f, 16.0f, 0.8f, 0.4f);
			Fill(S.PeriodWeight, { 1.0f, 0.4f, 0.9f, 0.1f });
			Fill(S.TideWeight, OffshoreTide);
			Fill(S.MoonWeight, BrightMoon);
			Fill(S.BaitWeight, { 0.05f, 0.1f, 0.6f, 0.0f, 0.2f, 1.0f });
			S.SeaStateSensitivity = 0.2f;
			S.FilletRatio = 0.5f;
			S.OilRatio = 0.05f;
			Out.Add(S);
		}
		{
			FFishSpecies S = MakeSpecies(TEXT("dorado"), TEXT("Dorado"), FishBit(EHab::Deep), Rod,
				60.0f, 3000.0f, 0.35f, 5.0f, 18.0f, 9.0f, 20.0f, 0.8f, 0.3f);
			Fill(S.PeriodWeight, { 0.8f, 1.0f, 0.7f, 0.05f });
			Fill(S.TideWeight, OffshoreTide);
			Fill(S.BaitWeight, { 0.05f, 0.1f, 0.5f, 0.0f, 0.2f, 1.0f });
			S.SeaStateSensitivity = 0.2f;
			S.FilletRatio = 0.5f;
			S.OilRatio = 0.03f;
			Out.Add(S);
		}
		{
			FFishSpecies S = MakeSpecies(TEXT("atun"), TEXT("Atún"), FishBit(EHab::Deep), Rod,
				80.0f, 3000.0f, 0.2f, 10.0f, 40.0f, 14.0f, 30.0f, 0.8f, 0.5f);
			Fill(S.PeriodWeight, { 1.0f, 0.4f, 1.0f, 0.2f });
			Fill(S.TideWeight, OffshoreTide);
			Fill(S.MoonWeight, BrightMoon);
			Fill(S.BaitWeight, { 0.02f, 0.0f, 0.7f, 0.0f, 0.2f, 1.0f });
			S.SeaStateSensitivity = 0.15f;
			S.FilletRatio = 0.55f;
			S.OilRatio = 0.06f;
			Out.Add(S);
		}

		// --- Langosta de arrecife: nocturna; arpón, nasa o a mano, nunca con caña ---
		{
			FFishSpecies S = MakeSpecies(TEXT("langosta"), TEXT("Langosta de arrecife"), FishBit(EHab::Reef), Spear | Trap | Hand,
				2.0f, 25.0f, 0.3f, 0.5f, 2.5f, 0.0f, 0.0f, 0.0f, 0.6f);
			Fill(S.PeriodWeight, { 0.3f, 0.05f, 0.5f, 1.0f });
			Fill(S.TideWeight, ReefTide);
			Fill(S.MoonWeight, DarkMoon);
			Fill(S.BaitWeight, { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f });
			S.FilletRatio = 0.0f;
			S.OilRatio = 0.0f;
			Out.Add(S);
		}
		return Out;
	}

	FLegendaryCatch MakeLegend(const TCHAR* Id, const TCHAR* NameEs, const TCHAR* Spot, uint8 Methods,
		float MinKg, float MaxKg, float StrengthKgf, float StaminaSeconds, float Wariness)
	{
		FLegendaryCatch L;
		L.Id = FName(Id);
		L.NameEs = NameEs;
		L.SpotTag = FName(Spot);
		L.MethodMask = Methods;
		L.MinWeightKg = MinKg;
		L.MaxWeightKg = MaxKg;
		L.StrengthKgf = StrengthKgf;
		L.StaminaSeconds = StaminaSeconds;
		L.Wariness = Wariness;
		return L;
	}

	/** Las cinco legendarias de la biblia §4.6. */
	TArray<FLegendaryCatch> BuildLegendaries()
	{
		const uint8 Rod = FishBit(EMet::Rod);
		const uint8 Spear = FishBit(EMet::Spear);
		TArray<FLegendaryCatch> Out;
		{
			// Rompe sedales: roza la roca de su cueva. Hace falta nailon y anzuelo de alambre.
			FLegendaryCatch L = MakeLegend(TEXT("el_viejo"), TEXT("El Viejo"), TEXT("cueva_arenas_blancas"), Rod,
				60.0f, 90.0f, 15.0f, 45.0f, 0.4f);
			Fill(L.PeriodWeight, { 0.8f, 0.3f, 1.0f, 0.8f });
			L.PerMinute = 0.012f;
			L.Aggression = 0.6f;
			L.RockAbrasionPerSecond = 0.08f;
			L.Rewards = { FName(TEXT("trofeo_el_viejo")) };
			Out.Add(L);
		}
		{
			FLegendaryCatch L = MakeLegend(TEXT("sombra"), TEXT("Sombra"), TEXT("canal_profundo"), Spear,
				250.0f, 400.0f, 30.0f, 60.0f, 0.2f);
			Fill(L.PeriodWeight, { 1.0f, 0.6f, 1.0f, 0.8f });
			L.bRequiresBoat = true;
			L.HarpoonHits = 3;
			L.FilletRatio = 0.3f;
			L.Rewards = { FName(TEXT("dientes_sombra")) };
			Out.Add(L);
		}
		{
			// Emboscada en aguas turbias: solo se la ve por su sombra con el agua clara.
			FLegendaryCatch L = MakeLegend(TEXT("manta_negra"), TEXT("La Manta Negra"), TEXT("bajios_manglar"), Spear,
				120.0f, 200.0f, 20.0f, 50.0f, 0.6f);
			Fill(L.PeriodWeight, { 0.8f, 1.0f, 0.8f, 0.3f });
			L.HarpoonHits = 2;
			L.FilletRatio = 0.25f;
			L.Rewards = { FName(TEXT("aguijon_manta_negra")) };
			Out.Add(L);
		}
		{
			FLegendaryCatch L = MakeLegend(TEXT("el_errante"), TEXT("El Errante"), TEXT("arrecife_arenas_blancas"), Spear | Rod,
				40.0f, 60.0f, 11.0f, 35.0f, 0.9f);
			Fill(L.PeriodWeight, { 1.0f, 0.6f, 1.0f, 0.3f });
			L.bSpooksEasily = true;
			L.Rewards = { FName(TEXT("piel_el_errante")) };
			Out.Add(L);
		}
		{
			FLegendaryCatch L = MakeLegend(TEXT("rey_de_plata"), TEXT("El Rey de Plata"), TEXT("mar_abierto"), Rod,
				80.0f, 150.0f, 13.0f, 60.0f, 0.4f);
			Fill(L.PeriodWeight, { 1.0f, 0.5f, 1.0f, 0.3f });
			L.bRequiresBoat = true;
			L.Aggression = 0.9f;
			L.FilletRatio = 0.5f;
			L.Rewards = { FName(TEXT("sedal_legendario")), FName(TEXT("anzuelo_legendario")) };
			Out.Add(L);
		}
		return Out;
	}

	/** Entrada de la tabla de trampas: qué cae, dónde y con qué cebo. */
	struct FTrapEntry
	{
		ETrapKind Kind;
		const TCHAR* ItemId;
		uint8 HabitatMask;
		float PerHour;
		float NightMultiplier;
		EFishBait FavouriteBait;
		float MinKg;
		float MaxKg;
	};

	/** Espejo de «traps» en fish.json. */
	const FTrapEntry TrapTable[] = {
		{ ETrapKind::Nasa, TEXT("salmonete"), FishBit(EHab::Lagoon) | FishBit(EHab::Shore), 0.018f, 0.6f, EFishBait::Lombriz, 0.2f, 0.7f },
		{ ETrapKind::Nasa, TEXT("pez_cirujano"), FishBit(EHab::Lagoon) | FishBit(EHab::Reef), 0.014f, 0.5f, EFishBait::FrutaFermentada, 0.3f, 1.2f },
		{ ETrapKind::Nasa, TEXT("pez_loro"), FishBit(EHab::Reef), 0.009f, 0.4f, EFishBait::FrutaFermentada, 0.8f, 2.0f },
		{ ETrapKind::Nasa, TEXT("pez_ballesta"), FishBit(EHab::Reef), 0.007f, 0.4f, EFishBait::Cangrejo, 0.5f, 2.0f },
		{ ETrapKind::Nasa, TEXT("langosta"), FishBit(EHab::Reef), 0.011f, 2.5f, EFishBait::Visceras, 0.5f, 2.5f },
		{ ETrapKind::Nasa, TEXT("pulpo"), FishBit(EHab::Reef) | FishBit(EHab::Shore), 0.007f, 1.5f, EFishBait::Cangrejo, 0.5f, 2.5f },
		{ ETrapKind::Nasa, TEXT("cangrejo"), FishBit(EHab::Shore) | FishBit(EHab::Lagoon), 0.011f, 1.5f, EFishBait::Visceras, 0.15f, 0.4f },
		{ ETrapKind::CrabTrap, TEXT("cangrejo"), FishBit(EHab::Shore) | FishBit(EHab::Lagoon), 0.028f, 1.5f, EFishBait::Visceras, 0.15f, 0.4f },
		{ ETrapKind::CrabTrap, TEXT("cangrejo_cocotero"), FishBit(EHab::Shore), 0.009f, 2.5f, EFishBait::FrutaFermentada, 1.0f, 3.0f },
		{ ETrapKind::CrabTrap, TEXT("pulpo"), FishBit(EHab::Shore) | FishBit(EHab::Lagoon), 0.005f, 1.5f, EFishBait::Cangrejo, 0.5f, 2.0f },
		{ ETrapKind::CrabTrap, TEXT("langosta"), FishBit(EHab::Lagoon), 0.004f, 2.0f, EFishBait::Visceras, 0.5f, 2.0f },
		{ ETrapKind::StoneCorral, TEXT("salmonete"), FishBit(EHab::Shore), 0.05f, 1.0f, EFishBait::None, 0.2f, 0.7f },
		{ ETrapKind::StoneCorral, TEXT("pez_cirujano"), FishBit(EHab::Shore), 0.035f, 1.0f, EFishBait::None, 0.3f, 1.2f },
		{ ETrapKind::StoneCorral, TEXT("pez_loro"), FishBit(EHab::Shore), 0.018f, 1.0f, EFishBait::None, 0.8f, 2.0f },
		{ ETrapKind::StoneCorral, TEXT("jurel"), FishBit(EHab::Shore), 0.011f, 1.0f, EFishBait::None, 0.5f, 2.0f },
		{ ETrapKind::StoneCorral, TEXT("cangrejo"), FishBit(EHab::Shore), 0.014f, 1.0f, EFishBait::None, 0.15f, 0.4f },
	};

	/** Lo que da una poza de marea en bajamar (espejo de «tidePool» en fish.json). */
	struct FPoolEntry
	{
		const TCHAR* ItemId;
		float Weight;
		float MinKg;
		float MaxKg;
	};

	const FPoolEntry TidePoolTable[] = {
		{ TEXT("cangrejo"), 0.35f, 0.15f, 0.4f },
		{ TEXT("lapa"), 0.3f, 0.02f, 0.06f },
		{ TEXT("erizo"), 0.2f, 0.1f, 0.3f },
		{ TEXT("pulpo"), 0.15f, 0.5f, 2.0f },
	};

	/** Id de objeto de cada cebo (items.json), en el orden de EFishBait. */
	const TCHAR* const BaitItems[] = { TEXT(""), TEXT("lombriz"), TEXT("visceras_pescado"), TEXT("fruta_fermentada"),
		TEXT("cangrejo"), TEXT("senuelo_tallado") };
	static_assert(static_cast<int32>(UE_ARRAY_COUNT(BaitItems)) == static_cast<int32>(EFishBait::Count), "Un objeto por cebo");

	constexpr uint32 BiteSalt = 0xB17E0001u;
	constexpr uint32 PickSalt = 0x5EC10002u;
	constexpr uint32 SizeSalt = 0x51E50003u;
	constexpr uint32 FightSalt = 0xF1670004u;
	constexpr uint32 TrapSalt = 0x7AA90005u;
	constexpr uint32 PoolSalt = 0x9001D006u;
	constexpr uint32 NetSalt = 0x0E7C0007u;

	float Unit(uint32 Seed, int32 X, int32 Y, int32 Z)
	{
		return ExploredHash::ToUnitFloat(ExploredHash::Hash3D(Seed, X, Y, Z));
	}

	/** Tamaño sesgado hacia lo pequeño: los ejemplares grandes son raros. */
	float SizeT(float U)
	{
		return FMath::Pow(FMath::Clamp(U, 0.0f, 1.0f), 1.6f);
	}

	/** 1 dentro del rango de profundidad; cae suave fuera. */
	float DepthFactor(float DepthM, float MinDepthM, float MaxDepthM)
	{
		if (DepthM < MinDepthM)
		{
			return FMath::SmoothStep(MinDepthM * 0.6f, MinDepthM, DepthM);
		}
		if (DepthM > MaxDepthM)
		{
			return 1.0f - FMath::SmoothStep(MaxDepthM, MaxDepthM * 1.3f, DepthM);
		}
		return 1.0f;
	}

	float NoiseFactor(float Wariness, float Noise01)
	{
		return FMath::Square(FMath::Clamp(1.0f - Wariness * FMath::Clamp(Noise01, 0.0f, 1.0f), 0.0f, 1.0f));
	}

	float WeatherFactor(float SeaSensitivity, float RainAffinity, const FFishingConditions& C)
	{
		const float Rough = FMath::Max(0.0f, C.SeaState - 0.3f) / 0.7f;
		const float Sea = 1.0f - FMath::Clamp(SeaSensitivity, 0.0f, 1.0f) * FMath::Clamp(Rough, 0.0f, 1.0f);
		return FMath::Max(0.0f, Sea * (1.0f + RainAffinity * FMath::Clamp(C.Rain, 0.0f, 1.0f)));
	}

	bool IsNight(float Hours)
	{
		return Hours < 6.0f || Hours >= 19.0f;
	}

	FFishFightParams MakeFight(float StrengthKgf, float StaminaSeconds, float Aggression, float Size01,
		const FFishingTackle& Tackle, float Abrasion)
	{
		FFishFightParams P;
		P.StrengthKgf = StrengthKgf * (0.75f + 0.5f * Size01);
		P.StaminaSeconds = StaminaSeconds * (0.8f + 0.4f * Size01);
		P.Aggression = Aggression;
		P.StartDistanceM = 12.0f;
		P.ApplyTackle(Tackle, Abrasion);
		return P;
	}
}

// --- Estado guardable ----------------------------------------------------------

FPlacedTrap& FFishingSaveState::PlaceTrap(ETrapKind Kind, EFishHabitat Habitat, const FVector& Location, EFishBait Bait, float NowDays)
{
	FPlacedTrap& Trap = Traps.AddDefaulted_GetRef();
	Trap.Id = NextTrapId++;
	Trap.Kind = Kind;
	Trap.Habitat = Habitat;
	Trap.Location = Location;
	Trap.Bait = Bait;
	Trap.BaitLeft01 = Bait == EFishBait::None ? 0.0f : 1.0f;
	Trap.PlacedAtDays = NowDays;
	Trap.SimulatedToDays = NowDays;
	return Trap;
}

FPlacedTrap* FFishingSaveState::FindTrap(int32 TrapId)
{
	return Traps.FindByPredicate([TrapId](const FPlacedTrap& T) { return T.Id == TrapId; });
}

bool FFishingSaveState::RemoveTrap(int32 TrapId)
{
	return Traps.RemoveAll([TrapId](const FPlacedTrap& T) { return T.Id == TrapId; }) > 0;
}

bool FFishingSaveState::IsLegendarySpooked(FName Id, float NowDays) const
{
	return Spooked.ContainsByPredicate([Id, NowDays](const FLegendarySpook& S) { return S.Id == Id && NowDays < S.UntilDays; });
}

// --- Condiciones -----------------------------------------------------------------

void FFishingConditions::SetTime(float TotalDays)
{
	Hours = FMath::Frac(TotalDays) * 24.0f;
	TideLevel = FOceanTide::Level(TotalDays);
	TideFlow = FOceanTide::Flow(TotalDays);
	MoonPhase01 = FMath::Frac(TotalDays / FishingModelDetail::LunarCycleDays);
}

void FFishingConditions::SetWeather(const FWeatherSample& Weather)
{
	Rain = Weather.Rain;
	SeaState = Weather.SeaState;
}

// --- Tablas ----------------------------------------------------------------------

const TArray<FFishSpecies>& FFishingModel::Species()
{
	static const TArray<FFishSpecies> Table = FishingModelDetail::BuildSpecies();
	return Table;
}

const FFishSpecies* FFishingModel::FindSpecies(FName Id)
{
	return Species().FindByPredicate([Id](const FFishSpecies& S) { return S.Id == Id; });
}

const TArray<FLegendaryCatch>& FFishingModel::Legendaries()
{
	static const TArray<FLegendaryCatch> Table = FishingModelDetail::BuildLegendaries();
	return Table;
}

const FLegendaryCatch* FFishingModel::FindLegendary(FName Id)
{
	return Legendaries().FindByPredicate([Id](const FLegendaryCatch& L) { return L.Id == Id; });
}

FName FFishingModel::BaitItemId(EFishBait Bait)
{
	const int32 Index = static_cast<int32>(Bait);
	if (Bait == EFishBait::None || Index < 0 || Index >= static_cast<int32>(EFishBait::Count))
	{
		return NAME_None;
	}
	return FName(FishingModelDetail::BaitItems[Index]);
}

EFishBait FFishingModel::BaitFromItemId(FName ItemId)
{
	for (int32 I = 1; I < static_cast<int32>(EFishBait::Count); ++I)
	{
		if (ItemId == FName(FishingModelDetail::BaitItems[I]))
		{
			return static_cast<EFishBait>(I);
		}
	}
	return EFishBait::None;
}

int32 FFishingModel::TrapCapacity(ETrapKind Kind)
{
	switch (Kind)
	{
	case ETrapKind::Nasa: return 4;
	case ETrapKind::CrabTrap: return 3;
	case ETrapKind::StoneCorral: return 6;
	default: return 0;
	}
}

// --- Entorno ---------------------------------------------------------------------

EDayPeriod FFishingModel::DayPeriod(float Hours)
{
	const float Clock = FMath::Fmod(FMath::Fmod(Hours, 24.0f) + 24.0f, 24.0f);
	if (Clock >= 5.0f && Clock < 7.5f)
	{
		return EDayPeriod::Dawn;
	}
	if (Clock >= 7.5f && Clock < 16.5f)
	{
		return EDayPeriod::Day;
	}
	if (Clock >= 16.5f && Clock < 19.0f)
	{
		return EDayPeriod::Dusk;
	}
	return EDayPeriod::Night;
}

ETidePhase FFishingModel::TidePhase(float TideLevel, float TideFlow)
{
	if (TideLevel <= -0.6f)
	{
		return ETidePhase::Low;
	}
	if (TideLevel >= 0.6f)
	{
		return ETidePhase::High;
	}
	return TideFlow >= 0.0f ? ETidePhase::Rising : ETidePhase::Falling;
}

EMoonQuarter FFishingModel::MoonQuarter(float MoonPhase01)
{
	const float P = FMath::Frac(MoonPhase01);
	if (P < 0.125f || P >= 0.875f)
	{
		return EMoonQuarter::New;
	}
	if (P < 0.375f)
	{
		return EMoonQuarter::Waxing;
	}
	if (P < 0.625f)
	{
		return EMoonQuarter::Full;
	}
	return EMoonQuarter::Waning;
}

EFishHabitat FFishingModel::HabitatForDepth(float DepthM, bool bReefNearby)
{
	if (DepthM < 1.0f)
	{
		return EFishHabitat::Shore;
	}
	if (DepthM <= 25.0f)
	{
		return bReefNearby ? EFishHabitat::Reef : EFishHabitat::Lagoon;
	}
	if (DepthM <= 80.0f)
	{
		return EFishHabitat::Slope;
	}
	return EFishHabitat::Deep;
}

float FFishingModel::PlayerNoise(float CarriedWeightRatio, float MoveSpeed01, float SplashesPerMinute, bool bWading)
{
	// Cargado se pisa fuerte y se hace ruido al moverse (GDD §8.2); por debajo
	// de un tercio de la capacidad cómoda no se nota.
	const float Weight = FMath::Clamp((CarriedWeightRatio - 0.3f) / 0.9f, 0.0f, 1.0f) * 0.4f;
	const float Move = FMath::Clamp(MoveSpeed01, 0.0f, 1.0f) * 0.3f;
	const float Splash = FMath::Clamp(SplashesPerMinute / 6.0f, 0.0f, 1.0f) * 0.5f;
	const float Wading = bWading ? 0.1f : 0.0f;
	return FMath::Clamp(Weight + Move + Splash + Wading, 0.0f, 1.0f);
}

// --- Picadas ---------------------------------------------------------------------

float FFishingModel::BiteRatePerSecond(const FFishSpecies& S, const FFishingConditions& C)
{
	using namespace FishingModelDetail;
	if (!S.LivesIn(C.Habitat) || !S.CaughtBy(C.Method))
	{
		return 0.0f;
	}

	float Bait = 1.0f;
	switch (C.Method)
	{
	case ECatchMethod::Rod:
		Bait = S.BaitWeight[FMath::Clamp(static_cast<int32>(C.Bait), 0, static_cast<int32>(EFishBait::Count) - 1)];
		break;
	case ECatchMethod::Spear:
		// Con arpón hay que verlo: aguas someras, o desde la barca a la superficie.
		if (C.DepthM > SpearMaxDepthM && !C.bFromBoat)
		{
			return 0.0f;
		}
		break;
	case ECatchMethod::Net:
		if (C.DepthM > 3.0f)
		{
			return 0.0f;
		}
		break;
	default:
		break;
	}

	const float Depth = C.Method == ECatchMethod::Rod ? DepthFactor(C.DepthM, S.MinDepthM, S.MaxDepthM) : 1.0f;
	const float Period = S.PeriodWeight[static_cast<int32>(DayPeriod(C.Hours))];
	const float Tide = S.TideWeight[static_cast<int32>(TidePhase(C.TideLevel, C.TideFlow))];
	const float Moon = S.MoonWeight[static_cast<int32>(MoonQuarter(C.MoonPhase01))];
	const float Weather = WeatherFactor(S.SeaStateSensitivity, S.RainAffinity, C);
	const float Noise = NoiseFactor(S.Wariness, C.Noise01);
	const float Population = 1.0f - 0.85f * FMath::Clamp(C.Depletion01, 0.0f, 1.0f);
	return S.BitesPerMinute / 60.0f * Bait * Depth * Period * Tide * Moon * Weather * Noise * Population;
}

float FFishingModel::LegendaryRatePerSecond(const FLegendaryCatch& L, const FFishingConditions& C,
	const FFishingSaveState* State, float NowDays)
{
	using namespace FishingModelDetail;
	if (C.SpotTag.IsNone() || C.SpotTag != L.SpotTag || !L.CaughtBy(C.Method))
	{
		return 0.0f;
	}
	if (L.bRequiresBoat && !C.bFromBoat)
	{
		return 0.0f;
	}
	if (State && (State->IsLegendaryCaught(L.Id) || State->IsLegendarySpooked(L.Id, NowDays)))
	{
		return 0.0f;
	}
	// Las legendarias se ven por su sombra: agua clara (biblia §4.6, La Manta Negra).
	const float Murk = FMath::Max(FMath::Clamp(C.Rain, 0.0f, 1.0f), FMath::Clamp(C.SeaState - 0.3f, 0.0f, 1.0f));
	const float Clear = 1.0f - 0.7f * Murk;
	const float Bait = C.Method == ECatchMethod::Rod && C.Bait == EFishBait::None ? 0.2f : 1.0f;
	const float Period = L.PeriodWeight[static_cast<int32>(DayPeriod(C.Hours))];
	return L.PerMinute / 60.0f * Clear * Bait * Period * NoiseFactor(L.Wariness, C.Noise01);
}

float FFishingModel::TotalBiteRatePerSecond(const FFishingConditions& C, const FFishingSaveState* State, float NowDays)
{
	float Total = 0.0f;
	for (const FFishSpecies& S : Species())
	{
		Total += BiteRatePerSecond(S, C);
	}
	for (const FLegendaryCatch& L : Legendaries())
	{
		Total += LegendaryRatePerSecond(L, C, State, NowDays);
	}
	return Total;
}

bool FFishingModel::WaitForBite(const FFishingConditions& C, const FFishingSaveState* State, uint32 Seed,
	int32 SpotKey, float StartDays, float MaxWaitSeconds, FFishBite& OutBite)
{
	using namespace FishingModelDetail;
	const TArray<FFishSpecies>& AllSpecies = Species();
	const TArray<FLegendaryCatch>& AllLegends = Legendaries();

	TArray<float> Rates;
	Rates.Reserve(AllSpecies.Num() + AllLegends.Num());
	float Total = 0.0f;
	for (const FFishSpecies& S : AllSpecies)
	{
		Total += Rates.Add_GetRef(BiteRatePerSecond(S, C));
	}
	for (const FLegendaryCatch& L : AllLegends)
	{
		Total += Rates.Add_GetRef(LegendaryRatePerSecond(L, C, State, StartDays));
	}
	// IsFinite explícito: con NaN (condiciones o tiempo corruptos) Total <= 0 es falso y
	// picaría en el primer segundo con un pez NaN.
	if (!FMath::IsFinite(Total) || Total <= 0.0f || !FMath::IsFinite(StartDays) || !FMath::IsFinite(MaxWaitSeconds))
	{
		return false;
	}

	// El instante se discretiza en minutos de juego: misma semilla, sitio y
	// minuto dan la misma espera, y un minuto después, otra distinta.
	const int32 Minute = static_cast<int32>(FMath::FloorToDouble(FMath::Clamp(static_cast<double>(StartDays) * 1440.0, -2.0e9, 2.0e9)));
	const float PerSecond = 1.0f - FMath::Exp(-Total);
	// Acotado antes de convertir: una espera enorme daría hasta 2^31 vueltas.
	const int32 MaxSeconds = FMath::FloorToInt(FMath::Clamp(MaxWaitSeconds, 0.0f, MaxBiteWaitLimitSeconds));
	for (int32 Second = 0; Second < MaxSeconds; ++Second)
	{
		if (Unit(Seed ^ BiteSalt, SpotKey, Minute, Second) >= PerSecond)
		{
			continue;
		}

		float Pick = Unit(Seed ^ PickSalt, SpotKey, Minute, Second) * Total;
		int32 Chosen = Rates.Num() - 1;
		for (int32 I = 0; I < Rates.Num(); ++I)
		{
			if (Rates[I] > 0.0f && Pick < Rates[I])
			{
				Chosen = I;
				break;
			}
			Pick -= Rates[I];
		}
		while (Chosen > 0 && Rates[Chosen] <= 0.0f)
		{
			--Chosen;
		}

		const float Size01 = SizeT(Unit(Seed ^ SizeSalt, SpotKey, Minute, Second));
		OutBite = FFishBite();
		OutBite.WaitSeconds = static_cast<float>(Second + 1);
		OutBite.FightSeed = ExploredHash::Hash3D(Seed ^ FightSalt, SpotKey, Minute, Second);
		if (Chosen < AllSpecies.Num())
		{
			const FFishSpecies& S = AllSpecies[Chosen];
			OutBite.Id = S.Id;
			OutBite.WeightKg = FMath::Lerp(S.MinWeightKg, S.MaxWeightKg, Size01);
			OutBite.Fight = MakeFight(S.StrengthKgf, S.StaminaSeconds, S.Aggression, Size01, C.Tackle, 0.0f);
		}
		else
		{
			const FLegendaryCatch& L = AllLegends[Chosen - AllSpecies.Num()];
			OutBite.Id = L.Id;
			OutBite.bLegendary = true;
			OutBite.WeightKg = FMath::Lerp(L.MinWeightKg, L.MaxWeightKg, Size01);
			OutBite.Fight = MakeFight(L.StrengthKgf, L.StaminaSeconds, L.Aggression, Size01, C.Tackle, L.RockAbrasionPerSecond);
		}
		return true;
	}
	return false;
}

// --- Arpón y red -------------------------------------------------------------------

float FFishingModel::TrueDepthFromApparent(float EyeHeightM, float HorizontalDistM, float ApparentDepthM)
{
	const float Depth = FMath::Max(0.0f, ApparentDepthM);
	const float Eye = FMath::Max(0.01f, EyeHeightM);
	const float Dist = FMath::Abs(HorizontalDistM);
	// El rayo que llega al ojo cruza la superficie en Xs; por encima va recto
	// (ángulo Theta1 con la vertical) y por debajo se cierra (Theta2, Snell).
	// El pez está en la vertical de su imagen, más hondo en Tan1 / Tan2.
	const float Xs = Dist * Eye / (Eye + Depth);
	const float Tan1 = Xs / Eye;
	if (Tan1 < 1e-4f)
	{
		return Depth * WaterRefractiveIndex;
	}
	const float Sin1 = Tan1 / FMath::Sqrt(1.0f + Tan1 * Tan1);
	const float Sin2 = Sin1 / WaterRefractiveIndex;
	const float Tan2 = Sin2 / FMath::Sqrt(FMath::Max(1e-6f, 1.0f - Sin2 * Sin2));
	return Depth * Tan1 / Tan2;
}

float FFishingModel::SpearHitChance(ESpearStrike Strike, float DistanceM, float AimErrorM, float Wariness, bool bUnderwater)
{
	const float Dist = FMath::Max(0.0f, DistanceM);
	const float Wary = FMath::Clamp(Wariness, 0.0f, 1.0f);
	float Base = 0.0f;
	float Range = 0.0f;
	float Tolerance = 0.0f;
	if (Strike == ESpearStrike::Thrust)
	{
		// Empuje: seguro pero solo al alcance del brazo más la lanza.
		if (Dist > 2.2f)
		{
			return 0.0f;
		}
		Base = 0.85f * (1.0f - 0.2f * Wary);
		Range = 1.0f;
		Tolerance = 0.15f + 0.02f * Dist;
	}
	else
	{
		const float MaxRange = bUnderwater ? 3.5f : 10.0f;
		Base = 0.7f * (1.0f - 0.35f * Wary);
		Range = 1.0f - FMath::SmoothStep(0.5f * MaxRange, MaxRange, Dist);
		Tolerance = 0.15f + 0.04f * Dist;
	}
	const float Aim = FMath::Exp(-FMath::Square(FMath::Max(0.0f, AimErrorM) / Tolerance));
	return FMath::Clamp(Base * Range * Aim, 0.0f, 1.0f);
}

TArray<FTrapCatch> FFishingModel::CastNet(const FFishingConditions& Conditions, uint32 Seed, int32 SpotKey, float NowDays)
{
	using namespace FishingModelDetail;
	TArray<FTrapCatch> Out;
	FFishingConditions C = Conditions;
	C.Method = ECatchMethod::Net;
	const TArray<FFishSpecies>& All = Species();
	TArray<float> Rates;
	float Total = 0.0f;
	for (const FFishSpecies& S : All)
	{
		Total += Rates.Add_GetRef(BiteRatePerSecond(S, C));
	}
	if (Total <= 0.0f)
	{
		return Out;
	}
	const int32 Minute = SafeFloorToInt(static_cast<double>(NowDays) * 1440.0);
	const float Expected = FMath::Min(3.0f, Total * 45.0f);
	for (int32 Try = 0; Try < 3; ++Try)
	{
		if (Unit(Seed ^ NetSalt, SpotKey, Minute, Try * 3) >= Expected / 3.0f)
		{
			continue;
		}
		float Pick = Unit(Seed ^ NetSalt, SpotKey, Minute, Try * 3 + 1) * Total;
		for (int32 I = 0; I < All.Num(); ++I)
		{
			if (Rates[I] > 0.0f && Pick < Rates[I])
			{
				FTrapCatch& Catch = Out.AddDefaulted_GetRef();
				Catch.ItemId = All[I].Id;
				Catch.WeightKg = FMath::Lerp(All[I].MinWeightKg, All[I].MaxWeightKg,
					SizeT(Unit(Seed ^ NetSalt, SpotKey, Minute, Try * 3 + 2)));
				Catch.CaughtAtDays = NowDays;
				break;
			}
			Pick -= Rates[I];
		}
	}
	return Out;
}

// --- Trampas y pozas -----------------------------------------------------------------

void FFishingModel::AdvanceTrap(FPlacedTrap& Trap, float ToDays, uint32 WorldSeed)
{
	using namespace FishingModelDetail;
	// Un guardado corrupto puede traer NaN o infinitos: sin IsFinite, FloorToInt(NaN)
	// da INT_MIN y el bucle de horas no acaba nunca.
	const bool bSimulatedValid = FMath::IsFinite(Trap.SimulatedToDays);
	if (!FMath::IsFinite(ToDays) || (bSimulatedValid && ToDays <= Trap.SimulatedToDays))
	{
		return;
	}
	// Horas enteras de juego en (SimulatedToDays, ToDays]: simular de una vez
	// o a trozos da exactamente lo mismo. Se acota antes de convertir a entero.
	constexpr double HourLimit = 1.0e9;
	const int64 LastHour = FMath::FloorToInt64(FMath::Clamp(static_cast<double>(ToDays) * 24.0, -HourLimit, HourLimit));
	int64 FirstHour = LastHour - 24 * static_cast<int64>(MaxTrapCatchUpDays) + 1;
	if (bSimulatedValid)
	{
		FirstHour = FMath::Max(FirstHour,
			FMath::FloorToInt64(FMath::Clamp(static_cast<double>(Trap.SimulatedToDays) * 24.0, -HourLimit, HourLimit)) + 1);
	}
	const int32 Capacity = TrapCapacity(Trap.Kind);
	const uint32 Salt = WorldSeed ^ TrapSalt ^ (static_cast<uint32>(Trap.Kind) << 24);

	for (int64 Hour64 = FirstHour; Hour64 <= LastHour; ++Hour64)
	{
		const int32 Hour = static_cast<int32>(Hour64);
		const float Days = static_cast<float>(static_cast<double>(Hour) / 24.0);
		const float Level = FOceanTide::Level(Days);
		const float Flow = FOceanTide::Flow(Days);
		const bool bNight = IsNight(FMath::Frac(Days) * 24.0f);

		// La nasa en la orilla solo pesca cubierta de agua; el corral, al vaciar la marea.
		const bool bWorks = !(Trap.Kind == ETrapKind::Nasa && Trap.Habitat == EFishHabitat::Shore && Level < -0.3f)
			&& !(Trap.Kind == ETrapKind::StoneCorral && Flow >= 0.0f);
		const bool bBaited = Trap.Bait != EFishBait::None && Trap.BaitLeft01 > 0.0f;

		// Llena, ya no entra nada; el cebo se sigue gastando igual.
		if (bWorks && Trap.Contents.Num() < Capacity)
		{
			for (int32 E = 0; E < static_cast<int32>(UE_ARRAY_COUNT(TrapTable)); ++E)
			{
				const FTrapEntry& Entry = TrapTable[E];
				if (Entry.Kind != Trap.Kind || (Entry.HabitatMask & FishBit(Trap.Habitat)) == 0)
				{
					continue;
				}
				float Rate = Entry.PerHour * (bNight ? Entry.NightMultiplier : 1.0f);
				if (bBaited)
				{
					Rate *= Trap.Bait == Entry.FavouriteBait ? 3.0f : 1.6f;
				}
				if (Unit(Salt, Trap.Id, Hour, E * 2) < 1.0f - FMath::Exp(-Rate))
				{
					FTrapCatch& Catch = Trap.Contents.AddDefaulted_GetRef();
					Catch.ItemId = FName(Entry.ItemId);
					Catch.WeightKg = FMath::Lerp(Entry.MinKg, Entry.MaxKg, SizeT(Unit(Salt, Trap.Id, Hour, E * 2 + 1)));
					Catch.CaughtAtDays = Days;
					if (Trap.Contents.Num() >= Capacity)
					{
						break;
					}
				}
			}
		}
		if (bBaited)
		{
			Trap.BaitLeft01 = FMath::Max(0.0f, Trap.BaitLeft01 - 1.0f / 24.0f);
		}
	}
	Trap.SimulatedToDays = ToDays;
}

TArray<FTrapCatch> FFishingModel::CollectTrap(FPlacedTrap& Trap, float NowDays, uint32 WorldSeed)
{
	if (!FMath::IsFinite(NowDays))
	{
		return {};
	}
	AdvanceTrap(Trap, NowDays, WorldSeed);
	TArray<FTrapCatch> Out = MoveTemp(Trap.Contents);
	Trap.Contents.Reset();
	// Lo que sigue vivo en la nasa está fresco al sacarlo.
	for (FTrapCatch& Catch : Out)
	{
		Catch.CaughtAtDays = NowDays;
	}
	return Out;
}

int32 FFishingModel::LowTideIndex(float TotalDays)
{
	// Level = sin(4·pi·T): bajamares en T = 3/8 + k/2.
	return FishingModelDetail::SafeFloorToInt((static_cast<double>(TotalDays) - 0.375) * 2.0 + 0.5);
}

TArray<FTrapCatch> FFishingModel::GatherTidePool(FFishingSaveState& State, int32 PoolId, float TotalDays, uint32 Seed)
{
	using namespace FishingModelDetail;
	TArray<FTrapCatch> Out;
	if (!FMath::IsFinite(TotalDays) || FOceanTide::Level(TotalDays) > TidePoolOpenLevel)
	{
		return Out;
	}
	const int32 LowTide = LowTideIndex(TotalDays);
	FTidePoolRecord* Record = State.TidePools.FindByPredicate([PoolId](const FTidePoolRecord& R) { return R.PoolId == PoolId; });
	if (!Record)
	{
		Record = &State.TidePools.AddDefaulted_GetRef();
		Record->PoolId = PoolId;
	}
	if (Record->LastLowTideIndex == LowTide)
	{
		return Out;
	}
	Record->LastLowTideIndex = LowTide;

	FExploredRandom Rng(ExploredHash::Hash3D(Seed ^ PoolSalt, PoolId, LowTide, 0));
	const float Spring = FOceanTide::SpringNeapFactor(FMath::Frac(TotalDays / LunarCycleDays));
	const int32 Count = Rng.RangeInt(1, 3) + (Spring > 0.9f ? 1 : 0);
	float TotalWeight = 0.0f;
	for (const FPoolEntry& E : TidePoolTable)
	{
		TotalWeight += E.Weight;
	}
	for (int32 I = 0; I < Count; ++I)
	{
		float Pick = Rng.NextFloat() * TotalWeight;
		const FPoolEntry* Chosen = &TidePoolTable[0];
		for (const FPoolEntry& E : TidePoolTable)
		{
			if (Pick < E.Weight)
			{
				Chosen = &E;
				break;
			}
			Pick -= E.Weight;
		}
		FTrapCatch& Catch = Out.AddDefaulted_GetRef();
		Catch.ItemId = FName(Chosen->ItemId);
		Catch.WeightKg = Rng.RangeFloat(Chosen->MinKg, Chosen->MaxKg);
		Catch.CaughtAtDays = TotalDays;
	}
	return Out;
}

// --- Despiece ------------------------------------------------------------------------

bool FFishingModel::Butcher(FName CatchId, float WeightKg, float KnifeEdge01, float NowDays, FButcherResult& OutResult)
{
	OutResult = FButcherResult();
	const float Edge = FMath::Clamp(KnifeEdge01, 0.0f, 1.0f);
	if (Edge <= 0.0f)
	{
		return false;
	}
	const FFishSpecies* SpeciesDef = FindSpecies(CatchId);
	const FLegendaryCatch* Legend = SpeciesDef ? nullptr : FindLegendary(CatchId);
	if ((!SpeciesDef || !SpeciesDef->IsFish()) && !Legend)
	{
		return false;
	}

	// Se acota antes de convertir a entero: RoundToInt de NaN o de 1e30 es UB. La mayor
	// legendaria no llega a 400 kg.
	const float W = FMath::IsFinite(WeightKg) ? FMath::Clamp(WeightKg, 0.05f, 2000.0f) : 0.05f;
	const float FilletRatio = SpeciesDef ? SpeciesDef->FilletRatio : Legend->FilletRatio;
	const float OilRatio = SpeciesDef ? SpeciesDef->OilRatio : 0.03f;

	auto AddYield = [&OutResult](const TCHAR* Id, int32 Count, float SpoilDays)
	{
		if (Count > 0)
		{
			FButcherYield& Y = OutResult.Yields.AddDefaulted_GetRef();
			Y.ItemId = FName(Id);
			Y.Count = Count;
			Y.SpoilAfterDays = SpoilDays;
		}
	};

	// Un buen filo saca más carne (biblia §4.4); cada filete ronda 0,25 kg.
	const float FilletKg = W * FilletRatio * (0.6f + 0.4f * Edge);
	AddYield(TEXT("filete_pescado"), FMath::Clamp(FMath::RoundToInt(FilletKg / 0.25f), 1, 60), RawSpoilDays);
	AddYield(TEXT("espina_pescado"), 1 + (W >= 3.0f ? 1 : 0) + (W >= 15.0f ? 1 : 0), -1.0f);
	AddYield(TEXT("piel_pescado"), W >= 1.0f ? (W >= 10.0f ? 2 : 1) : 0, -1.0f);
	AddYield(TEXT("visceras_pescado"), FMath::Clamp(FMath::RoundToInt(W * 0.08f / 0.2f), 1, 5), RawSpoilDays);
	AddYield(TEXT("aceite_pescado"), FMath::Min(10, FMath::FloorToInt(W * OilRatio / 0.1f)), -1.0f);
	if (Legend)
	{
		for (const FName& Reward : Legend->Rewards)
		{
			FButcherYield& Y = OutResult.Yields.AddDefaulted_GetRef();
			Y.ItemId = Reward;
			Y.Count = 1;
		}
	}

	OutResult.Seconds = FMath::Min(600.0f, (15.0f + 8.0f * W) / (0.5f + Edge));
	OutResult.FreshSinceDays = NowDays;
	return true;
}

// --- Ecosistema ------------------------------------------------------------------------

int32 FFishingModel::ZoneKeyAt(const FVector2D& LocationCm)
{
	const int32 X = FishingModelDetail::SafeFloorToInt(LocationCm.X / 10000.0);
	const int32 Y = FishingModelDetail::SafeFloorToInt(LocationCm.Y / 10000.0);
	return static_cast<int32>(ExploredHash::Hash2D(0x20AE5EEDu, X, Y) & 0x7FFFFFFFu);
}

int32 FFishingModel::SpotKeyAt(const FVector2D& LocationCm)
{
	const int32 X = FishingModelDetail::SafeFloorToInt(LocationCm.X / 500.0);
	const int32 Y = FishingModelDetail::SafeFloorToInt(LocationCm.Y / 500.0);
	return static_cast<int32>(ExploredHash::Hash2D(0x5B07CAFEu, X, Y) & 0x7FFFFFFFu);
}

float FFishingModel::ZoneDepletion(const FFishingSaveState& State, int32 ZoneKey, float NowDays)
{
	const FZonePopulation* Zone = State.Zones.FindByPredicate([ZoneKey](const FZonePopulation& Z) { return Z.ZoneKey == ZoneKey; });
	if (!Zone)
	{
		return 0.0f;
	}
	// Se recupera sola: la mitad en algo menos de día y medio.
	const float Elapsed = FMath::Max(0.0f, NowDays - Zone->UpdatedAtDays);
	return Zone->Depletion01 * FMath::Exp(-Elapsed / 2.0f);
}

void FFishingModel::RegisterCatch(FFishingSaveState& State, int32 ZoneKey, float NowDays)
{
	const float Current = ZoneDepletion(State, ZoneKey, NowDays);
	FZonePopulation* Zone = State.Zones.FindByPredicate([ZoneKey](const FZonePopulation& Z) { return Z.ZoneKey == ZoneKey; });
	if (!Zone)
	{
		Zone = &State.Zones.AddDefaulted_GetRef();
		Zone->ZoneKey = ZoneKey;
	}
	Zone->Depletion01 = FMath::Min(1.0f, Current + 0.12f);
	Zone->UpdatedAtDays = NowDays;
}

void FFishingModel::NoteLegendaryPresence(FFishingSaveState& State, FName LegendId, float Noise01, float NowDays)
{
	const FLegendaryCatch* Legend = FindLegendary(LegendId);
	if (!Legend || !Legend->bSpooksEasily || Noise01 <= 0.25f || State.IsLegendaryCaught(LegendId))
	{
		return;
	}
	constexpr float SpookDays = 3.0f;
	if (FLegendarySpook* Existing = State.Spooked.FindByPredicate([LegendId](const FLegendarySpook& S) { return S.Id == LegendId; }))
	{
		Existing->UntilDays = FMath::Max(Existing->UntilDays, NowDays + SpookDays);
		return;
	}
	FLegendarySpook& Spook = State.Spooked.AddDefaulted_GetRef();
	Spook.Id = LegendId;
	Spook.UntilDays = NowDays + SpookDays;
}
