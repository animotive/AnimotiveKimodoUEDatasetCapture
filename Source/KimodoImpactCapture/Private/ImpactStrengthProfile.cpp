// Copyright Animotive. All Rights Reserved.

#include "ImpactStrengthProfile.h"

#include "ImpactRandom.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactStrengthProfile)

// Red: stubs returning neutral values so the tests compile and fail.

FImpactStrengthProfile FImpactStrengthProfile::Sample(const FImpactStrengthDistribution& Distribution, FImpactRandom& Random)
{
	return FImpactStrengthProfile();
}

FImpactBodyStrength FImpactStrengthProfile::Evaluate(const FImpactBodyStrengthQuery& Body, int32 FramesSinceHit) const
{
	return FImpactBodyStrength();
}
