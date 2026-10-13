// Copyright Animotive. All Rights Reserved.

#include "ImpactLegIK.h"

#include "ReferenceSkeleton.h"

TOptional<FVector> FImpactLegIK::ChooseEffector(bool bReleased, bool bFootPlaced, const FVector& PlacedTarget, bool bMovingSource,
	const FVector& SourceFoot, const FVector& CurrentFoot)
{
	return TOptional<FVector>();
}

void FImpactLegIK::Solve(const FReferenceSkeleton& Ref, const FImpactLegChain& Leg, const FVector& Effector, const FVector& Forward,
	TConstArrayView<FTransform> Source, TArrayView<FTransform> Pose)
{
}
