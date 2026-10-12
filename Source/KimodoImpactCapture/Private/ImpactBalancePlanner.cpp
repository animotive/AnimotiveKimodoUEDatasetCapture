// Copyright Animotive. All Rights Reserved.

#include "ImpactBalancePlanner.h"

// Red: stubs returning neutral values so the tests compile and fail.

FImpactBalancePlanner::FImpactBalancePlanner(const FImpactBalanceConfig& InConfig)
	: Config(InConfig)
{
}

FImpactBalanceOutput FImpactBalancePlanner::Step(const FImpactBalanceInput& Input)
{
	return FImpactBalanceOutput();
}

FVector FImpactBalancePlanner::CapturePoint(const FVector& Com, const FVector& ExcessVelocity, float GravityCmPerSecondSquared)
{
	return FVector::ZeroVector;
}
