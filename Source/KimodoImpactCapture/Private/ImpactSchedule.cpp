// Copyright Animotive. All Rights Reserved.

#include "ImpactSchedule.h"

#include "ImpactParams.h"
#include "ImpactRandom.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactSchedule)

// Red: stubs returning neutral values so the tests compile and fail.

float FImpactSchedule::TotalScriptedImpulse() const
{
	return 0.0f;
}

bool FImpactScheduleSampler::Sample(
	const FImpactParams& Params, const FImpactDeliveryDistributions& Distributions,
	int32 BatchSeed, int32 ClipIndex, FImpactSchedule& OutSchedule, FText& OutReason)
{
	return true;
}
