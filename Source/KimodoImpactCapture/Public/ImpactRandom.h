// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Independent random streams for one synthetic clip. Each parameter family draws from its own
 * stream, so adding a parameter, or drawing more values from one family, never shifts the
 * values another family sees. Numbered explicitly: the numbers are part of the seed, so
 * renumbering would change every clip ever generated. Append only.
 */
enum class EImpactRandomStream : uint8
{
	SourceAnimation = 0,
	Impactor = 1,
	BodyRegion = 2,
	Direction = 3,
	Timing = 4,
	Delivery = 5,
	StrengthProfile = 6,
	Caption = 7,
};

/**
 * Deterministic random stream for one parameter family of one clip.
 *
 * A clip is a pure function of (batch seed, clip index): the stream is derived from those two
 * values and the stream id alone, never from any other clip or from evaluation order, so any
 * clip in a batch regenerates on its own and shards can run in any order.
 *
 * SplitMix64 throughout: it is tiny, well distributed even for adjacent seeds, and fully
 * specified, so the sequence is identical on every platform and compiler. The test suite pins
 * exact output values; changing the algorithm is a dataset-breaking change.
 */
struct KIMODOIMPACTCAPTURE_API FImpactRandom
{
	FImpactRandom(int32 BatchSeed, int32 ClipIndex, EImpactRandomStream Stream);

	/** Next raw 64-bit value. */
	uint64 NextUInt64();

	/** Uniform in [0, 1). */
	double NextUnit();

	/** Uniform in [Min, Max). Returns Min when the range is empty or inverted. */
	double Range(double Min, double Max);

	/** Uniform integer in [Min, Max], both inclusive. Returns Min when Max < Min. */
	int32 RangeInt(int32 Min, int32 Max);

	/** Uniformly distributed direction on the unit sphere. */
	FVector UnitVector();

private:
	uint64 State = 0;
};
