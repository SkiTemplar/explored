#include "Boats/BoatNetState.h"

namespace BoatNetStateDetail
{
	/** Redondeo al entero más cercano recortado a [Lo, Hi]; NaN e infinitos dan 0 recortado. */
	int64 QuantizeClamped(double Value, double Step, int64 Lo, int64 Hi)
	{
		if (!FMath::IsFinite(Value))
		{
			return FMath::Clamp<int64>(0, Lo, Hi);
		}
		const double Q = FMath::RoundToDouble(Value / Step);
		if (Q <= static_cast<double>(Lo))
		{
			return Lo;
		}
		if (Q >= static_cast<double>(Hi))
		{
			return Hi;
		}
		return static_cast<int64>(Q);
	}

	/** Extiende el signo de un campo de Bits bits. */
	int32 SignExtend(uint64 Raw, int32 Bits)
	{
		const uint64 Mask = (uint64(1) << Bits) - 1;
		const uint64 SignBit = uint64(1) << (Bits - 1);
		const uint64 V = Raw & Mask;
		return static_cast<int32>(static_cast<int64>(V ^ SignBit) - static_cast<int64>(SignBit));
	}

	void PutU16(uint8* Out, uint16 V)
	{
		Out[0] = static_cast<uint8>(V & 0xFF);
		Out[1] = static_cast<uint8>(V >> 8);
	}

	uint16 GetU16(const uint8* In)
	{
		return static_cast<uint16>(In[0] | (In[1] << 8));
	}
}

FExploredBoatNetState FExploredBoatNetState::Quantize(const FBoatState& State, float SailTrim01, int32 Breaches)
{
	using namespace BoatNetStateDetail;
	FExploredBoatNetState N;
	N.PosX = static_cast<int32>(QuantizeClamped(State.LocationCm.X, 1.0, MinPositionXYCm, MaxPositionXYCm));
	N.PosY = static_cast<int32>(QuantizeClamped(State.LocationCm.Y, 1.0, MinPositionXYCm, MaxPositionXYCm));
	N.PosZ = static_cast<int32>(QuantizeClamped(State.LocationCm.Z, 1.0, MinPositionZCm, MaxPositionZCm));
	N.VelX = static_cast<int16>(QuantizeClamped(State.VelocityCmS.X, VelocityStepCmS, -32767, 32767));
	N.VelY = static_cast<int16>(QuantizeClamped(State.VelocityCmS.Y, VelocityStepCmS, -32767, 32767));

	// Rumbo: se envuelve a [0, 360) antes de cuantizar; 359,999° da la vuelta a 0.
	double Yaw = FMath::IsFinite(State.YawDeg) ? FMath::Fmod(static_cast<double>(State.YawDeg), 360.0) : 0.0;
	if (Yaw < 0.0)
	{
		Yaw += 360.0;
	}
	N.Heading = static_cast<uint16>(QuantizeClamped(Yaw, 360.0 / 65536.0, 0, 65536) & 0xFFFF);

	N.Heel = static_cast<int8>(QuantizeClamped(State.RollDeg, 1.0, -MaxHeelDeg, MaxHeelDeg));
	const int64 Trim = QuantizeClamped(SailTrim01, 1.0 / MaxTrimSteps, 0, MaxTrimSteps);
	N.Sail = static_cast<uint8>((State.bSailRaised ? 0x80 : 0x00) | Trim);
	N.WaterDeciKg = static_cast<uint16>(QuantizeClamped(State.WaterInHullKg, WaterStepKg, 0, 65535));
	const double Intact = FMath::IsFinite(State.HullDamage01) ? 1.0 - State.HullDamage01 : 1.0;
	N.Integrity = static_cast<uint8>(QuantizeClamped(Intact, 1.0 / 255.0, 0, 255));
	const int32 Condition = FMath::Clamp(static_cast<int32>(State.Condition), 0, 3);
	N.Status = static_cast<uint8>(FMath::Clamp(Breaches, 0, MaxBreaches) | (Condition << 6));
	return N;
}

FBoatNetSnapshot FExploredBoatNetState::Dequantize() const
{
	FBoatNetSnapshot S;
	S.LocationCm = FVector(PosX, PosY, PosZ);
	S.VelocityCmS = FVector2D(VelX * VelocityStepCmS, VelY * VelocityStepCmS);
	S.YawDeg = static_cast<float>(Heading * (360.0 / 65536.0));
	S.RollDeg = static_cast<float>(Heel);
	S.bSailRaised = (Sail & 0x80) != 0;
	S.SailTrim01 = static_cast<float>(Sail & 0x7F) / MaxTrimSteps;
	S.WaterInHullKg = WaterDeciKg * WaterStepKg;
	S.HullDamage01 = 1.0f - Integrity / 255.0f;
	S.Breaches = Status & 0x3F;
	S.Condition = static_cast<EBoatCondition>(Status >> 6);
	return S;
}

void FExploredBoatNetState::ApplyTo(FBoatState& State) const
{
	const FBoatNetSnapshot S = Dequantize();
	State.LocationCm = S.LocationCm;
	State.VelocityCmS = S.VelocityCmS;
	State.YawDeg = S.YawDeg;
	State.RollDeg = S.RollDeg;
	State.bSailRaised = S.bSailRaised;
	State.WaterInHullKg = S.WaterInHullKg;
	State.HullDamage01 = S.HullDamage01;
	State.Condition = S.Condition;
}

void FExploredBoatNetState::ToBytes(uint8 (&Out)[SizeBytes]) const
{
	using namespace BoatNetStateDetail;
	const uint64 XYMask = (uint64(1) << PositionXYBits) - 1;
	const uint64 ZMask = (uint64(1) << PositionZBits) - 1;
	const uint64 Packed = (static_cast<uint64>(static_cast<uint32>(PosX)) & XYMask)
		| ((static_cast<uint64>(static_cast<uint32>(PosY)) & XYMask) << PositionXYBits)
		| ((static_cast<uint64>(static_cast<uint32>(PosZ)) & ZMask) << (2 * PositionXYBits));
	for (int32 I = 0; I < 7; ++I)
	{
		Out[I] = static_cast<uint8>((Packed >> (8 * I)) & 0xFF);
	}
	PutU16(Out + 7, static_cast<uint16>(VelX));
	PutU16(Out + 9, static_cast<uint16>(VelY));
	PutU16(Out + 11, Heading);
	Out[13] = static_cast<uint8>(Heel);
	Out[14] = Sail;
	PutU16(Out + 15, WaterDeciKg);
	Out[17] = Integrity;
	Out[18] = Status;
}

TArray<uint8> FExploredBoatNetState::ToBytes() const
{
	uint8 Raw[SizeBytes];
	ToBytes(Raw);
	TArray<uint8> Bytes;
	Bytes.Append(Raw, SizeBytes);
	return Bytes;
}

bool FExploredBoatNetState::FromBytes(const uint8* Data, int32 Num, FExploredBoatNetState& Out)
{
	using namespace BoatNetStateDetail;
	if (Data == nullptr || Num != SizeBytes)
	{
		return false;
	}
	uint64 Packed = 0;
	for (int32 I = 0; I < 7; ++I)
	{
		Packed |= static_cast<uint64>(Data[I]) << (8 * I);
	}
	FExploredBoatNetState N;
	N.PosX = SignExtend(Packed, PositionXYBits);
	N.PosY = SignExtend(Packed >> PositionXYBits, PositionXYBits);
	N.PosZ = SignExtend(Packed >> (2 * PositionXYBits), PositionZBits);
	N.VelX = static_cast<int16>(GetU16(Data + 7));
	N.VelY = static_cast<int16>(GetU16(Data + 9));
	N.Heading = GetU16(Data + 11);
	N.Heel = static_cast<int8>(Data[13]);
	N.Sail = Data[14];
	N.WaterDeciKg = GetU16(Data + 15);
	N.Integrity = Data[17];
	N.Status = Data[18];
	// -128 no lo produce Quantize: un paquete con él viene corrupto, pero se recorta en vez de rechazarlo.
	N.Heel = static_cast<int8>(FMath::Max<int32>(N.Heel, -MaxHeelDeg));
	N.VelX = static_cast<int16>(FMath::Max<int32>(N.VelX, -32767));
	N.VelY = static_cast<int16>(FMath::Max<int32>(N.VelY, -32767));
	Out = N;
	return true;
}

bool FExploredBoatNetState::operator==(const FExploredBoatNetState& Other) const
{
	return PosX == Other.PosX && PosY == Other.PosY && PosZ == Other.PosZ && VelX == Other.VelX && VelY == Other.VelY
		&& Heading == Other.Heading && Heel == Other.Heel && Sail == Other.Sail && WaterDeciKg == Other.WaterDeciKg
		&& Integrity == Other.Integrity && Status == Other.Status;
}
