// Copyright Animotive. All Rights Reserved.

#include "ImpactRandom.h"

// Red: stubs returning neutral values so the tests compile and fail.

FImpactRandom::FImpactRandom(int32 BatchSeed, int32 ClipIndex, EImpactRandomStream Stream)
{
}

uint64 FImpactRandom::NextUInt64()
{
	return 0;
}

double FImpactRandom::NextUnit()
{
	return 0.0;
}

double FImpactRandom::Range(double Min, double Max)
{
	return Min;
}

int32 FImpactRandom::RangeInt(int32 Min, int32 Max)
{
	return Min;
}

FVector FImpactRandom::UnitVector()
{
	return FVector::ZeroVector;
}
