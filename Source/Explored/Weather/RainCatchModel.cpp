#include "Weather/RainCatchModel.h"

#include "Carry/InventoryModel.h"

namespace
{
	struct FRainPoint
	{
		float Rain01;
		float MmPerHour;
	};

	/**
	 * Lluvia 0–1 de FWeatherModel::BaseSample → mm/h, interpolado a tramos:
	 * llovizna (0,3) 2,5 mm/h, chubasco (0,75) 10, tormenta (0,95) 30 y
	 * ciclón (1,0) 50. Valores de lluvia tropical.
	 */
	constexpr FRainPoint RainTable[] = {
		{0.0f, 0.0f}, {0.3f, 2.5f}, {0.75f, 10.0f}, {0.95f, 30.0f}, {1.0f, 50.0f}};

	/** Evaporación de una lámina de agua a pleno sol y cielo despejado (~5,5 mm/día). */
	constexpr float ClearSkyEvaporationMmPerHour = 0.25f;

	int64 FloorDiv(int64 A, int64 B)
	{
		const int64 Q = A / B;
		return (A % B != 0 && ((A < 0) != (B < 0))) ? Q - 1 : Q;
	}

	/** mm/h × m² → µL/min (1 mm sobre 1 m² = 1 L). */
	int64 MicroLPerMinute(double MmPerHour, double AreaM2)
	{
		if (!(MmPerHour > 0.0) || !(AreaM2 > 0.0) || !FMath::IsFinite(MmPerHour * AreaM2))
		{
			return 0;
		}
		return static_cast<int64>(FMath::RoundToDouble(MmPerHour * AreaM2 * static_cast<double>(FRainCatchModel::MicroLPerLiter) / 60.0));
	}

	/**
	 * Quita Amount del total, en proporción. Lo que sale del resto se redondea
	 * hacia arriba: si no, con poca agua ajena el suelo entero la dejaría para
	 * siempre (un rastro de 0,1 % que no se va nunca por mucho que llueva).
	 */
	void RemoveProportional(FRainCatchState& State, int64 Amount, int64* OutRain, int64* OutOther)
	{
		const int64 Total = State.TotalMicroL();
		Amount = FMath::Clamp<int64>(Amount, 0, Total);
		int64 FromOther = 0;
		if (Amount > 0 && State.OtherMicroL > 0)
		{
			if (Total <= FRainCatchModel::MaxCapacityMicroL)
			{
				// Amount y OtherMicroL no pasan de MaxCapacityMicroL (1e9): el producto cabe en int64.
				FromOther = (Amount * State.OtherMicroL + Total - 1) / Total;
			}
			else
			{
				// Estado cargado fuera de rango: sin exactitud, pero sin desbordar.
				FromOther = static_cast<int64>(FMath::CeilToDouble(static_cast<double>(Amount) * (static_cast<double>(State.OtherMicroL) / static_cast<double>(Total))));
			}
			FromOther = FMath::Min(FromOther, State.OtherMicroL);
		}
		const int64 FromRain = FMath::Min(Amount - FromOther, State.RainMicroL);
		State.OtherMicroL -= FromOther;
		State.RainMicroL -= FromRain;
		if (State.OtherMicroL == 0)
		{
			State.OtherKind = ERainCatchLiquid::None;
		}
		if (OutRain) { *OutRain = FromRain; }
		if (OutOther) { *OutOther = FromOther; }
	}
}

const TCHAR* LexToString(ERainCatchQuality Quality)
{
	switch (Quality)
	{
	case ERainCatchQuality::Empty: return TEXT("Empty");
	case ERainCatchQuality::Rain: return TEXT("Rain");
	case ERainCatchQuality::Untreated: return TEXT("Untreated");
	case ERainCatchQuality::Sea: return TEXT("Sea");
	}
	return TEXT("?");
}

void FRainCatchModel::Sanitize(FRainCatchState& State)
{
	State.RainMicroL = FMath::Clamp<int64>(State.RainMicroL, 0, MaxCapacityMicroL);
	State.OtherMicroL = FMath::Clamp<int64>(State.OtherMicroL, 0, MaxCapacityMicroL);
	if (State.OtherMicroL == 0 || static_cast<uint8>(State.OtherKind) > static_cast<uint8>(ERainCatchLiquid::Sea))
	{
		State.OtherKind = State.OtherMicroL == 0 ? ERainCatchLiquid::None : ERainCatchLiquid::Sea;
	}
	else if (State.OtherKind == ERainCatchLiquid::None)
	{
		// Agua ajena sin tipo: se trata como sin tratar, nunca como limpia.
		State.OtherKind = ERainCatchLiquid::Untreated;
	}
}

float FRainCatchState::TotalLiters() const
{
	return static_cast<float>(static_cast<double>(TotalMicroL()) / static_cast<double>(FRainCatchModel::MicroLPerLiter));
}

float FRainCatchModel::RainMmPerHour(float Rain01)
{
	if (!(Rain01 > RainThreshold))
	{
		return 0.0f;
	}
	const float R = FMath::Min(Rain01, 1.0f);
	for (int32 i = 1; i < UE_ARRAY_COUNT(RainTable); ++i)
	{
		if (R <= RainTable[i].Rain01)
		{
			const FRainPoint& A = RainTable[i - 1];
			const FRainPoint& B = RainTable[i];
			const float Alpha = (R - A.Rain01) / (B.Rain01 - A.Rain01);
			return FMath::Lerp(A.MmPerHour, B.MmPerHour, Alpha);
		}
	}
	return RainTable[UE_ARRAY_COUNT(RainTable) - 1].MmPerHour;
}

float FRainCatchModel::EvaporationMmPerHour(const FWeatherSample& Sample)
{
	if (Sample.Rain > RainThreshold)
	{
		return 0.0f;
	}
	const float Cloud = FMath::IsFinite(Sample.CloudCover) ? FMath::Clamp(Sample.CloudCover, 0.0f, 1.0f) : 1.0f;
	return ClearSkyEvaporationMmPerHour * (1.0f - 0.6f * Cloud);
}

FRainCatchRates FRainCatchModel::RatesFor(const FRainCatchSpec& Spec, const FWeatherSample& Sample)
{
	FRainCatchRates Rates;
	if (!(Spec.MouthAreaM2 > 0.0) || Spec.CapacityMicroL <= 0)
	{
		return Rates;
	}
	if (!Spec.bSheltered)
	{
		Rates.InPerMinute = MicroLPerMinute(RainMmPerHour(FMath::IsFinite(Sample.Rain) ? Sample.Rain : 0.0f), Spec.MouthAreaM2);
	}
	if (Rates.InPerMinute == 0)
	{
		const double Shade = Spec.bSheltered ? ShelteredEvaporationFactor : 1.0;
		Rates.EvaporationPerMinute = MicroLPerMinute(EvaporationMmPerHour(Sample) * Shade, Spec.MouthAreaM2);
	}
	return Rates;
}

double FRainCatchModel::MouthAreaM2ForItem(FName ItemId)
{
	struct FMouth
	{
		const TCHAR* Id;
		double AreaM2;
	};
	// Diámetro de boca razonable de cada objeto: cáscara y cuenco de coco Ø15 cm,
	// vasija Ø16 cm, concha de almeja gigante Ø25 cm, bambú grueso Ø6 cm...
	static const FMouth Mouths[] = {
		{TEXT("cascara_coco"), 0.018},
		{TEXT("recipiente_coco"), 0.018},
		{TEXT("vasija_barro"), 0.020},
		{TEXT("concha_grande"), 0.050},
		{TEXT("concha_pequena"), 0.004},
		{TEXT("caracola"), 0.001},
		{TEXT("bambu_grueso"), 0.003},
		{TEXT("bambu_fino"), 0.001},
		{TEXT("cantimplora"), 0.0007},
	};
	for (const FMouth& M : Mouths)
	{
		if (ItemId == FName(M.Id))
		{
			return M.AreaM2;
		}
	}
	return 0.0;
}

FRainCatchSpec FRainCatchModel::SpecForItem(FName ItemId, float RecipienteValue, bool bSheltered)
{
	FRainCatchSpec Spec;
	Spec.MouthAreaM2 = MouthAreaM2ForItem(ItemId);
	const float Liters = FMath::IsFinite(RecipienteValue) ? FInventoryModel::LiquidCapacityFromRecipiente(RecipienteValue) : 0.0f;
	Spec.CapacityMicroL = Spec.MouthAreaM2 > 0.0 ? static_cast<int64>(FMath::RoundToDouble(static_cast<double>(Liters) * MicroLPerLiter)) : 0;
	Spec.bSheltered = bSheltered;
	return Spec;
}

void FRainCatchModel::StepMinutes(FRainCatchState& State, const FRainCatchSpec& Spec, const FRainCatchRates& Rates, int64 NMinutes)
{
	const int64 Capacity = FMath::Clamp<int64>(Spec.CapacityMicroL, 0, MaxCapacityMicroL);
	const int64 In = FMath::Clamp<int64>(Rates.InPerMinute, 0, MaxCapacityMicroL);
	const int64 Evap = FMath::Clamp<int64>(Rates.EvaporationPerMinute, 0, MaxCapacityMicroL);
	Sanitize(State);
	int64 Left = FMath::Clamp<int64>(NMinutes, 0, MaxCatchUpMinutes);
	// Minuto a minuto mientras haya mezcla o sobre agua: el reparto proporcional no tiene forma cerrada.
	while (Left > 0 && (State.OtherMicroL > 0 || State.TotalMicroL() > Capacity))
	{
		if (In > 0)
		{
			State.RainMicroL += In;
			State.CaughtMicroL += In;
		}
		else if (Evap > 0 && State.TotalMicroL() > 0)
		{
			const int64 Before = State.TotalMicroL();
			RemoveProportional(State, Evap, nullptr, nullptr);
			State.EvaporatedMicroL += Before - State.TotalMicroL();
		}
		const int64 Excess = State.TotalMicroL() - Capacity;
		if (Excess > 0)
		{
			RemoveProportional(State, Excess, nullptr, nullptr);
			State.SpilledMicroL += Excess;
		}
		--Left;
	}
	if (Left == 0)
	{
		return;
	}
	// Solo lluvia y dentro de la capacidad: lo mismo que el bucle, de una vez
	// (Left ≤ 60 días e In ≤ 1e9: el producto cabe en int64).
	if (In > 0)
	{
		const int64 Added = Left * In;
		const int64 After = FMath::Min(State.RainMicroL + Added, Capacity);
		State.CaughtMicroL += Added;
		State.SpilledMicroL += State.RainMicroL + Added - After;
		State.RainMicroL = After;
	}
	else if (Evap > 0)
	{
		const int64 After = FMath::Max<int64>(0, State.RainMicroL - Left * Evap);
		State.EvaporatedMicroL += State.RainMicroL - After;
		State.RainMicroL = After;
	}
}

FWeatherSample FRainCatchSky::SampleForSlot(int64 Slot) const
{
	if (const FWeatherSample* Found = Cache.Find(Slot))
	{
		return *Found;
	}
	if (Cache.Num() >= MaxCachedSlots)
	{
		Cache.Reset();
	}
	const double CenterMinute = static_cast<double>(Slot) * FRainCatchModel::SlotMinutes + 0.5 * FRainCatchModel::SlotMinutes;
	const FWeatherSample Sample = Weather.SampleAt(static_cast<float>(CenterMinute / static_cast<double>(FRainCatchModel::MinutesPerDay)));
	Cache.Add(Slot, Sample);
	return Sample;
}

int64 FRainCatchModel::Advance(FRainCatchState& State, const FRainCatchSpec& Spec, const FWeatherModel& Weather, int64 NowMinute)
{
	const FRainCatchSky Sky(Weather);
	return Advance(State, Spec, Sky, NowMinute);
}

int64 FRainCatchModel::Advance(FRainCatchState& State, const FRainCatchSpec& Spec, const FRainCatchSky& Sky, int64 NowMinute)
{
	if (NowMinute <= State.LastUpdateMinute)
	{
		return 0;
	}
	if (NowMinute > MaxSupportedMinute || NowMinute < -MaxSupportedMinute)
	{
		// Reloj fuera de toda partida posible (estado corrupto): FWeatherModel no
		// admite esos días. Se adopta la hora sin simular nada.
		State.LastUpdateMinute = NowMinute;
		return 0;
	}
	// Comparar en vez de restar: con un LastUpdateMinute cargado corrupto la resta desborda.
	const int64 Earliest = NowMinute - MaxCatchUpMinutes;
	if (State.LastUpdateMinute < Earliest)
	{
		State.LastUpdateMinute = Earliest;
	}
	const int64 CaughtBefore = State.CaughtMicroL;
	int64 T = State.LastUpdateMinute;
	while (T < NowMinute)
	{
		const int64 Slot = FloorDiv(T, SlotMinutes);
		const int64 SlotEnd = (Slot + 1) * SlotMinutes;
		const int64 Until = FMath::Min(SlotEnd, NowMinute);
		StepMinutes(State, Spec, RatesFor(Spec, Sky.SampleForSlot(Slot)), Until - T);
		T = Until;
	}
	State.LastUpdateMinute = NowMinute;
	return State.CaughtMicroL - CaughtBefore;
}

int64 FRainCatchModel::Pour(FRainCatchState& State, const FRainCatchSpec& Spec, int64 MicroL, ERainCatchLiquid Kind)
{
	Sanitize(State);
	const int64 Capacity = FMath::Clamp<int64>(Spec.CapacityMicroL, 0, MaxCapacityMicroL);
	const int64 Room = FMath::Max<int64>(0, Capacity - State.TotalMicroL());
	const int64 Accepted = FMath::Clamp<int64>(MicroL, 0, Room);
	if (Accepted == 0)
	{
		return 0;
	}
	if (Kind == ERainCatchLiquid::None)
	{
		State.RainMicroL += Accepted;
	}
	else
	{
		State.OtherMicroL += Accepted;
		State.OtherKind = static_cast<ERainCatchLiquid>(FMath::Max(static_cast<uint8>(State.OtherKind), static_cast<uint8>(Kind)));
	}
	return Accepted;
}

int64 FRainCatchModel::Take(FRainCatchState& State, int64 MicroL, int64* OutRainMicroL, int64* OutOtherMicroL)
{
	Sanitize(State);
	int64 R = 0;
	int64 O = 0;
	RemoveProportional(State, MicroL, &R, &O);
	if (OutRainMicroL) { *OutRainMicroL = R; }
	if (OutOtherMicroL) { *OutOtherMicroL = O; }
	return R + O;
}

ERainCatchQuality FRainCatchModel::Quality(const FRainCatchState& InState)
{
	FRainCatchState State = InState;
	Sanitize(State);
	const int64 Total = State.TotalMicroL();
	if (Total <= 0)
	{
		return ERainCatchQuality::Empty;
	}
	if (State.OtherMicroL <= 0)
	{
		return ERainCatchQuality::Rain;
	}
	if (State.OtherKind == ERainCatchLiquid::Sea &&
		static_cast<double>(State.OtherMicroL) >= SeaFractionForSalty * static_cast<double>(Total))
	{
		return ERainCatchQuality::Sea;
	}
	return ERainCatchQuality::Untreated;
}
