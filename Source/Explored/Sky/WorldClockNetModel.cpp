#include "Sky/WorldClockNetModel.h"

#include "Core/NetQuantize.h"

namespace WorldClockNetDetail
{
	using namespace ExploredNet;

	constexpr int64 HourSteps = 24000;
	constexpr int32 StormIntensitySteps = 31;

	uint8 PackSky(const FWorldClockNetState& S)
	{
		const uint8 Season = static_cast<uint8>(S.Season) < static_cast<uint8>(ESeason::Count) ? static_cast<uint8>(S.Season) : 0;
		const uint8 Weather = static_cast<uint8>(S.Weather) < static_cast<uint8>(EWeatherState::Count) ? static_cast<uint8>(S.Weather) : 0;
		return static_cast<uint8>(Season | (Weather << 2));
	}

	uint8 PackStorm(const FWorldClockNetState& S)
	{
		const uint8 Intensity = static_cast<uint8>(RoundClamped(static_cast<double>(S.StormIntensity01) * StormIntensitySteps, 0, StormIntensitySteps));
		const uint8 Category = static_cast<uint8>(FMath::Clamp(S.CycloneCategory, 0, FWorldClockNetModel::MaxCycloneCategory));
		return static_cast<uint8>(Intensity | (Category << 5));
	}

	uint8 PackTimeScale(float Scale)
	{
		return static_cast<uint8>(RoundClamped(static_cast<double>(Scale) * 2.0, 0, 255));
	}
}

void FWorldClockNetModel::Encode(const FWorldClockNetState& State, TArray<uint8>& Out)
{
	using namespace ExploredNet;
	using namespace WorldClockNetDetail;

	int64 Day = RoundClamped(static_cast<double>(State.Day), 0, 65535);
	double Hours = Finite(static_cast<double>(State.Hours), 0.0);
	Hours = FMath::Fmod(Hours, 24.0);
	if (Hours < 0.0)
	{
		Hours += 24.0;
	}
	int64 HourQ = RoundClamped(Hours * 1000.0, 0, HourSteps);
	if (HourQ >= HourSteps)
	{
		// 23,9996 h redondea a las 24:00: es la medianoche del día siguiente, no un 24:00.
		if (Day < 65535)
		{
			++Day;
			HourQ = 0;
		}
		else
		{
			HourQ = HourSteps - 1;
		}
	}

	Out.Reset();
	FByteWriter W(Out);
	W.U16(static_cast<uint16>(Day));
	W.U16(static_cast<uint16>(HourQ));
	W.U8(PackTimeScale(State.TimeScale));
	W.U8(PackSky(State));
	W.U8(Quantize01(State.Wind01));
	W.U8(QuantizeAngle8(State.WindFromDeg));
	W.U8(Quantize01(State.Rain01));
	W.U8(Quantize01(State.SeaState01));
	W.U8(PackStorm(State));
}

bool FWorldClockNetModel::Decode(const TArray<uint8>& In, FWorldClockNetState& OutState)
{
	using namespace ExploredNet;
	using namespace WorldClockNetDetail;

	if (In.Num() != PacketBytes)
	{
		return false;
	}
	FByteReader R(In);
	const uint16 Day = R.U16();
	const uint16 HourQ = R.U16();
	const uint8 Scale = R.U8();
	const uint8 Sky = R.U8();
	const uint8 Wind = R.U8();
	const uint8 WindFrom = R.U8();
	const uint8 Rain = R.U8();
	const uint8 Sea = R.U8();
	const uint8 Storm = R.U8();
	if (!R.IsDone() || HourQ >= HourSteps)
	{
		return false;
	}
	const uint8 Season = Sky & 0x03;
	const uint8 Weather = (Sky >> 2) & 0x0F;
	const uint8 Category = Storm >> 5;
	if ((Sky & 0xC0) != 0 || Season >= static_cast<uint8>(ESeason::Count) || Weather >= static_cast<uint8>(EWeatherState::Count)
		|| Category > MaxCycloneCategory)
	{
		return false;
	}

	FWorldClockNetState S;
	S.Day = Day;
	S.Hours = static_cast<float>(HourQ) / 1000.0f;
	S.TimeScale = static_cast<float>(Scale) * 0.5f;
	S.Season = static_cast<ESeason>(Season);
	S.Weather = static_cast<EWeatherState>(Weather);
	S.Wind01 = Dequantize01(Wind);
	S.WindFromDeg = DequantizeAngle8(WindFrom);
	S.Rain01 = Dequantize01(Rain);
	S.SeaState01 = Dequantize01(Sea);
	S.StormIntensity01 = static_cast<float>(Storm & 0x1F) / static_cast<float>(StormIntensitySteps);
	S.CycloneCategory = Category;
	OutState = S;
	return true;
}

FWorldClockNetState FWorldClockNetModel::Quantize(const FWorldClockNetState& State)
{
	TArray<uint8> Bytes;
	Encode(State, Bytes);
	FWorldClockNetState Out;
	Decode(Bytes, Out);
	return Out;
}

bool FWorldClockNetModel::NeedsImmediateSend(const FWorldClockNetState& LastSent, const FWorldClockNetState& Now)
{
	using namespace WorldClockNetDetail;
	return PackTimeScale(LastSent.TimeScale) != PackTimeScale(Now.TimeScale)
		|| PackSky(LastSent) != PackSky(Now)
		|| PackStorm(LastSent) != PackStorm(Now);
}

bool FWorldClockNetModel::ShouldSend(const FWorldClockNetState& LastSent, const FWorldClockNetState& Now, double SecondsSinceLastSend)
{
	// NaN o negativo: nunca se ha enviado (o el reloj de red se ha corrompido): se manda.
	if (!FMath::IsFinite(SecondsSinceLastSend) || SecondsSinceLastSend < 0.0 || SecondsSinceLastSend >= SendIntervalSeconds)
	{
		return true;
	}
	return NeedsImmediateSend(LastSent, Now);
}

FWorldClockCorrection FWorldClockNetModel::Correct(double LocalTotalHours, double ServerTotalHours)
{
	FWorldClockCorrection Out;
	if (!FMath::IsFinite(LocalTotalHours) || !FMath::IsFinite(ServerTotalHours))
	{
		Out.bHardSnap = true;
		return Out;
	}
	Out.ErrorHours = ServerTotalHours - LocalTotalHours;
	if (FMath::Abs(Out.ErrorHours) > HardSnapErrorHours)
	{
		Out.bHardSnap = true;
		return Out;
	}
	// Proporcional: con el error máximo antes del salto, el 5 % entero.
	const double Fraction = Out.ErrorHours / HardSnapErrorHours;
	Out.LocalRateFactor = 1.0f + MaxRateDeviation * static_cast<float>(FMath::Clamp(Fraction, -1.0, 1.0));
	return Out;
}
