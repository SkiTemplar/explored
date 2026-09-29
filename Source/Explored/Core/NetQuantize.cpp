#include "Core/NetQuantize.h"

namespace ExploredNet
{
	void FByteWriter::U16(uint16 V)
	{
		Out.Add(static_cast<uint8>(V & 0xFF));
		Out.Add(static_cast<uint8>(V >> 8));
	}

	void FByteWriter::U32(uint32 V)
	{
		U16(static_cast<uint16>(V & 0xFFFF));
		U16(static_cast<uint16>(V >> 16));
	}

	uint8 FByteReader::U8()
	{
		if (!bOk || Pos >= In.Num())
		{
			bOk = false;
			return 0;
		}
		return In[Pos++];
	}

	uint16 FByteReader::U16()
	{
		const uint16 Lo = U8();
		const uint16 Hi = U8();
		return static_cast<uint16>(Lo | (Hi << 8));
	}

	uint32 FByteReader::U32()
	{
		const uint32 Lo = U16();
		const uint32 Hi = U16();
		return Lo | (Hi << 16);
	}

	double Finite(double V, double Fallback)
	{
		return FMath::IsFinite(V) ? V : Fallback;
	}

	int64 RoundClamped(double V, int64 Lo, int64 Hi)
	{
		// Se acota en double antes de convertir: convertir un double fuera de rango a
		// entero es comportamiento indefinido (UBSan lo caza con float-cast-overflow).
		const double Safe = Finite(V, 0.0);
		const double Clamped = FMath::Clamp(Safe, static_cast<double>(Lo), static_cast<double>(Hi));
		const double Rounded = FMath::FloorToDouble(Clamped + 0.5);
		// double(INT64_MAX) redondea a 2^63, que ya no cabe en int64: los extremos se
		// devuelven sin convertir.
		if (Rounded >= static_cast<double>(Hi))
		{
			return Hi;
		}
		if (Rounded <= static_cast<double>(Lo))
		{
			return Lo;
		}
		return static_cast<int64>(Rounded);
	}

	uint8 Quantize01(float V)
	{
		return static_cast<uint8>(RoundClamped(static_cast<double>(V) * 255.0, 0, 255));
	}

	float Dequantize01(uint8 V)
	{
		return static_cast<float>(V) / 255.0f;
	}

	namespace
	{
		/** Grados → pasos de una vuelta de Steps, envuelto a [0, Steps). */
		int64 WrapAngleSteps(float Deg, int64 Steps)
		{
			const double Safe = Finite(static_cast<double>(Deg), 0.0);
			double Turns = FMath::Fmod(Safe / 360.0, 1.0);
			if (Turns < 0.0)
			{
				Turns += 1.0;
			}
			const int64 Q = RoundClamped(Turns * static_cast<double>(Steps), 0, Steps);
			return Q % Steps;
		}
	}

	uint8 QuantizeAngle8(float Deg)
	{
		return static_cast<uint8>(WrapAngleSteps(Deg, 256));
	}

	float DequantizeAngle8(uint8 V)
	{
		return static_cast<float>(V) * (360.0f / 256.0f);
	}

	uint16 QuantizeAngle16(float Deg)
	{
		return static_cast<uint16>(WrapAngleSteps(Deg, 65536));
	}

	float DequantizeAngle16(uint16 V)
	{
		return static_cast<float>(static_cast<double>(V) * (360.0 / 65536.0));
	}

	float AngleDeltaDeg(float A, float B)
	{
		double D = FMath::Fmod(Finite(static_cast<double>(B) - static_cast<double>(A), 0.0), 360.0);
		if (D <= -180.0)
		{
			D += 360.0;
		}
		else if (D > 180.0)
		{
			D -= 360.0;
		}
		return static_cast<float>(D);
	}

	namespace
	{
		constexpr int64 XYMax = (int64(1) << (PositionXYBits - 1)) - 1;
		constexpr int64 ZMax = (int64(1) << (PositionZBits - 1)) - 1;
		constexpr uint64 XYMask = (uint64(1) << PositionXYBits) - 1;
		constexpr uint64 ZMask = (uint64(1) << PositionZBits) - 1;

		int64 SignExtend(uint64 Raw, int32 Bits)
		{
			const uint64 SignBit = uint64(1) << (Bits - 1);
			return static_cast<int64>((Raw ^ SignBit)) - static_cast<int64>(SignBit);
		}
	}

	void WritePosition7(FByteWriter& W, const FVector& PositionCm)
	{
		const int64 X = RoundClamped(PositionCm.X / PositionXYStepCm, -XYMax, XYMax);
		const int64 Y = RoundClamped(PositionCm.Y / PositionXYStepCm, -XYMax, XYMax);
		const int64 Z = RoundClamped(PositionCm.Z / PositionZStepCm, -ZMax, ZMax);
		const uint64 Packed = (static_cast<uint64>(X) & XYMask)
			| ((static_cast<uint64>(Y) & XYMask) << PositionXYBits)
			| ((static_cast<uint64>(Z) & ZMask) << (2 * PositionXYBits));
		for (int32 i = 0; i < PositionBytes; ++i)
		{
			W.U8(static_cast<uint8>((Packed >> (8 * i)) & 0xFF));
		}
	}

	FVector ReadPosition7(FByteReader& R)
	{
		uint64 Packed = 0;
		for (int32 i = 0; i < PositionBytes; ++i)
		{
			Packed |= static_cast<uint64>(R.U8()) << (8 * i);
		}
		const int64 X = SignExtend(Packed & XYMask, PositionXYBits);
		const int64 Y = SignExtend((Packed >> PositionXYBits) & XYMask, PositionXYBits);
		const int64 Z = SignExtend((Packed >> (2 * PositionXYBits)) & ZMask, PositionZBits);
		// El mínimo de complemento a dos no lo escribe nunca WritePosition7 (el rango es
		// simétrico): un paquete que lo trae está manipulado y no se aceptaría al reenviarlo.
		if (X < -XYMax || Y < -XYMax || Z < -ZMax)
		{
			R.bOk = false;
		}
		return FVector(X * PositionXYStepCm, Y * PositionXYStepCm, Z * PositionZStepCm);
	}

	FVector QuantizePosition7(const FVector& PositionCm)
	{
		TArray<uint8> Bytes;
		FByteWriter W(Bytes);
		WritePosition7(W, PositionCm);
		FByteReader R(Bytes);
		return ReadPosition7(R);
	}
}
