// Copyright Animotive. All Rights Reserved.

#include "ImpactParams.h"

#include "ImpactRandom.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactParams)

// Red: stubs returning neutral values so the tests compile and fail.

EImpactIntensity FImpactIntensityThresholds::Classify(float ReactionImpulseKgCmPerSecond) const
{
	return EImpactIntensity::Tap;
}

float FImpactParamsSampler::TransferredImpulse(float ImpactorMassKg, float SpeedMetresPerSecond, float BodyMassKg, float Restitution)
{
	return 0.0f;
}

bool FImpactParamsSampler::Sample(
	const UImpactorCatalog& Catalog, const FImpactDistributions& Distributions, float CharacterMassKg,
	int32 BatchSeed, int32 ClipIndex, FImpactParams& OutParams, FText& OutReason)
{
	return true;
}
