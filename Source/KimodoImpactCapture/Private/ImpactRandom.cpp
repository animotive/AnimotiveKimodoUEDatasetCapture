// Copyright Animotive. All Rights Reserved.

#include "ImpactRandom.h"

namespace
{
	constexpr uint64 GoldenGamma = 0x9E3779B97F4A7C15ull;

	/** SplitMix64 finaliser: one step of the generator applied to an arbitrary value. */
	uint64 Mix(uint64 Value)
	{
		Value += GoldenGamma;
		uint64 Z = Value;
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		return Z ^ (Z >> 31);
	}

	// Each input lives in its own 2^32 band before mixing, so (seed, index) = (1, 2) and (2, 1)
	// can never collide, and neither can a stream id with either of them.
	constexpr uint64 ClipIndexBand = 0x100000000ull;
	constexpr uint64 StreamBand = 0x200000000ull;
}

FImpactRandom::FImpactRandom(int32 BatchSeed, int32 ClipIndex, EImpactRandomStream Stream)
{
	State = Mix(uint64(uint32(BatchSeed)))
		^ Mix(ClipIndexBand + uint64(uint32(ClipIndex)))
		^ Mix(StreamBand + uint64(uint8(Stream)));
	State = Mix(State);
}

uint64 FImpactRandom::NextUInt64()
{
	State += GoldenGamma;
	uint64 Z = State;
	Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
	Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
	return Z ^ (Z >> 31);
}

double FImpactRandom::NextUnit()
{
	// Top 53 bits: every representable double in [0, 1) at that spacing, never 1.0.
	return double(NextUInt64() >> 11) * (1.0 / 9007199254740992.0);
}

double FImpactRandom::Range(double Min, double Max)
{
	if (!(Max > Min))
	{
		return Min;
	}
	return Min + (Max - Min) * NextUnit();
}

int32 FImpactRandom::RangeInt(int32 Min, int32 Max)
{
	if (Max <= Min)
	{
		return Min;
	}
	const int64 Span = int64(Max) - int64(Min) + 1;
	const int64 Offset = FMath::Min<int64>(int64(NextUnit() * double(Span)), Span - 1);
	return int32(int64(Min) + Offset);
}

FVector FImpactRandom::UnitVector()
{
	// Archimedes: uniform height on [-1, 1] plus uniform azimuth is uniform on the sphere.
	const double Z = Range(-1.0, 1.0);
	const double Azimuth = Range(0.0, 2.0 * UE_DOUBLE_PI);
	const double Radius = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
	return FVector(Radius * FMath::Cos(Azimuth), Radius * FMath::Sin(Azimuth), Z);
}
