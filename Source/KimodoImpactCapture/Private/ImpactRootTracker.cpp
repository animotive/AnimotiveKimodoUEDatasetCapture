// Copyright Animotive. All Rights Reserved.

#include "ImpactRootTracker.h"

// Red: stubs returning neutral values so the tests compile and fail.

FImpactRootTracker::FImpactRootTracker(const FTransform& ReferencePelvis, const FVector& InMeshForward, float InGroundHeight, const FImpactRootTrackerConfig& InConfig)
	: Config(InConfig)
{
}

FTransform FImpactRootTracker::Project(const FTransform& Pelvis) const
{
	return FTransform::Identity;
}

FTransform FImpactRootTracker::Step(const FTransform& SourceRoot, const FTransform& Pelvis, bool bReleased)
{
	return FTransform::Identity;
}

float FImpactRootTracker::PelvisYaw(const FTransform& Pelvis, bool& bOutValid) const
{
	bOutValid = false;
	return 0.0f;
}
