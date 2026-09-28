#include "Boats/BoatNetStateModel.h"

#include "Core/NetQuantize.h"

namespace BoatNetDetail
{
	constexpr uint8 FlagAutoTrim = 1 << 0;
	constexpr uint8 FlagBailing = 1 << 1;
	constexpr uint8 KnownFlags = FlagAutoTrim | FlagBailing;
}

FBoatNetState FBoatNetStateModel::FromBoatState(const FBoatState& State, float SailTrim01)
{
	FBoatNetState Out;
	Out.PositionCm = State.LocationCm;
	Out.VelocityCmS = State.VelocityCmS;
	Out.YawDeg = State.YawDeg;
	Out.HeelDeg = State.RollDeg;
	Out.bSailRaised = State.bSailRaised;
	Out.SailTrim01 = SailTrim01;
	Out.SwampWaterKg = State.WaterInHullKg;
	Out.HullIntegrity01 = 1.0f - State.HullDamage01;
	return Out;
}

void FBoatNetStateModel::EncodeState(const FBoatNetState& State, TArray<uint8>& Out)
{
	using namespace ExploredNet;
	Out.Reset();
	FByteWriter W(Out);
	WritePosition7(W, State.PositionCm);
	W.I16(static_cast<int16>(RoundClamped(State.VelocityCmS.X / VelocityStepCmS, MIN_int16 + 1, MAX_int16)));
	W.I16(static_cast<int16>(RoundClamped(State.VelocityCmS.Y / VelocityStepCmS, MIN_int16 + 1, MAX_int16)));
	W.U16(QuantizeAngle16(State.YawDeg));
	// La escora se envuelve a (−180, 180] antes de acotar: 350° es −10°, no +127°.
	W.I8(static_cast<int8>(RoundClamped(AngleDeltaDeg(0.0f, State.HeelDeg), -127, 127)));
	const uint8 Trim = static_cast<uint8>(RoundClamped(static_cast<double>(State.SailTrim01) * 127.0, 0, 127));
	W.U8(static_cast<uint8>((State.bSailRaised ? 0x80 : 0) | Trim));
	W.U16(static_cast<uint16>(RoundClamped(static_cast<double>(State.SwampWaterKg) / SwampStepKg, 0, MAX_uint16)));
	W.U8(Quantize01(State.HullIntegrity01));
	W.U8(0);
}

bool FBoatNetStateModel::DecodeState(const TArray<uint8>& In, FBoatNetState& OutState)
{
	using namespace ExploredNet;
	if (In.Num() != StateBytes)
	{
		return false;
	}
	FByteReader R(In);
	FBoatNetState S;
	S.PositionCm = ReadPosition7(R);
	const int16 Vx = R.I16();
	const int16 Vy = R.I16();
	S.YawDeg = DequantizeAngle16(R.U16());
	const int8 Heel = R.I8();
	const uint8 Sail = R.U8();
	S.SwampWaterKg = static_cast<float>(R.U16() * SwampStepKg);
	S.HullIntegrity01 = Dequantize01(R.U8());
	const uint8 Reserved = R.U8();
	if (!R.IsDone() || Reserved != 0 || Vx == MIN_int16 || Vy == MIN_int16 || Heel == -128)
	{
		return false;
	}
	S.VelocityCmS = FVector2D(Vx * VelocityStepCmS, Vy * VelocityStepCmS);
	S.HeelDeg = Heel;
	S.bSailRaised = (Sail & 0x80) != 0;
	S.SailTrim01 = static_cast<float>(Sail & 0x7F) / 127.0f;
	OutState = S;
	return true;
}

FBoatNetState FBoatNetStateModel::Quantize(const FBoatNetState& State)
{
	TArray<uint8> Bytes;
	EncodeState(State, Bytes);
	FBoatNetState Out;
	DecodeState(Bytes, Out);
	return Out;
}

void FBoatNetStateModel::EncodeControls(const FBoatNetControls& In, TArray<uint8>& Out)
{
	using namespace ExploredNet;
	using namespace BoatNetDetail;
	Out.Reset();
	FByteWriter W(Out);
	W.U8(Quantize01(In.Controls.SailTrim01));
	W.I8(static_cast<int8>(RoundClamped(static_cast<double>(In.Controls.Rudder) * 127.0, -127, 127)));
	W.U8(static_cast<uint8>((In.Controls.bAutoTrim ? FlagAutoTrim : 0) | (In.Controls.bBailing ? FlagBailing : 0)));
	const uint8 Stroke = static_cast<uint8>(In.Stroke) < static_cast<uint8>(EBoatNetStroke::Count) ? static_cast<uint8>(In.Stroke) : 0;
	W.U8(Stroke);
}

bool FBoatNetStateModel::DecodeControls(const TArray<uint8>& In, FBoatNetControls& OutControls)
{
	using namespace ExploredNet;
	using namespace BoatNetDetail;
	if (In.Num() != ControlsBytes)
	{
		return false;
	}
	FByteReader R(In);
	const uint8 Sheet = R.U8();
	const int8 Rudder = R.I8();
	const uint8 Flags = R.U8();
	const uint8 Stroke = R.U8();
	if (!R.IsDone() || Rudder == -128 || (Flags & ~KnownFlags) != 0 || Stroke >= static_cast<uint8>(EBoatNetStroke::Count))
	{
		return false;
	}
	FBoatNetControls C;
	C.Controls.SailTrim01 = Dequantize01(Sheet);
	C.Controls.Rudder = static_cast<float>(Rudder) / 127.0f;
	C.Controls.bAutoTrim = (Flags & FlagAutoTrim) != 0;
	C.Controls.bBailing = (Flags & FlagBailing) != 0;
	C.Stroke = static_cast<EBoatNetStroke>(Stroke);
	OutControls = C;
	return true;
}

float FBoatNetStateModel::CorrectionAlpha(float SecondsSinceState, float DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return 0.0f;
	}
	const float Elapsed = FMath::IsFinite(SecondsSinceState) ? FMath::Max(0.0f, SecondsSinceState) : CorrectionSeconds;
	const float Remaining = CorrectionSeconds - Elapsed;
	if (Remaining <= DeltaSeconds + 1e-4f)
	{
		return 1.0f;
	}
	return DeltaSeconds / Remaining;
}
