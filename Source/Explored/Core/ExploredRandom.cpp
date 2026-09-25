#include "Core/ExploredRandom.h"

FExploredRandom::FExploredRandom(uint64 InSeed, uint64 InStream)
{
	Increment = (InStream << 1u) | 1u;
	State = 0;
	NextUInt32();
	State += InSeed;
	NextUInt32();
}

uint32 FExploredRandom::NextUInt32()
{
	const uint64 OldState = State;
	State = OldState * 6364136223846793005ULL + Increment;
	const uint32 XorShifted = static_cast<uint32>(((OldState >> 18u) ^ OldState) >> 27u);
	const uint32 Rot = static_cast<uint32>(OldState >> 59u);
	return (XorShifted >> Rot) | (XorShifted << ((0u - Rot) & 31u));
}

int32 FExploredRandom::RangeInt(int32 Min, int32 Max)
{
	check(Max >= Min);
	const uint64 Span = static_cast<uint64>(static_cast<int64>(Max) - Min) + 1u;
	return static_cast<int32>(Min + static_cast<int64>(NextUInt32() % Span));
}

float FExploredRandom::NextFloat()
{
	// 24 bits de mantisa: valores exactamente representables en [0, 1).
	return static_cast<float>(NextUInt32() >> 8) * (1.0f / 16777216.0f);
}

float FExploredRandom::RangeFloat(float Min, float Max)
{
	return Min + (Max - Min) * NextFloat();
}

bool FExploredRandom::Chance(float P)
{
	return NextFloat() < P;
}

FVector2D FExploredRandom::InsideUnitDisc()
{
	const float Angle = NextFloat() * UE_TWO_PI;
	const float Radius = FMath::Sqrt(NextFloat());
	return FVector2D(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);
}

namespace ExploredHash
{
	uint32 Hash32(uint32 X)
	{
		X ^= X >> 16;
		X *= 0x7FEB352Du;
		X ^= X >> 15;
		X *= 0x846CA68Bu;
		X ^= X >> 16;
		return X;
	}

	uint32 Hash2D(uint32 Seed, int32 X, int32 Y)
	{
		uint32 H = Hash32(Seed ^ 0x9E3779B9u);
		H = Hash32(H ^ static_cast<uint32>(X) * 0x85EBCA6Bu);
		H = Hash32(H ^ static_cast<uint32>(Y) * 0xC2B2AE35u);
		return H;
	}

	uint32 Hash3D(uint32 Seed, int32 X, int32 Y, int32 Z)
	{
		const uint32 H = Hash2D(Seed, X, Y);
		return Hash32(H ^ static_cast<uint32>(Z) * 0x27D4EB2Fu);
	}

	float ToUnitFloat(uint32 H)
	{
		return static_cast<float>(H >> 8) * (1.0f / 16777216.0f);
	}
}
