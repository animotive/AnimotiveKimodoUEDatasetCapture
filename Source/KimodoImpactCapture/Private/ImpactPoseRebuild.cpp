// Copyright Animotive. All Rights Reserved.

#include "ImpactPoseRebuild.h"

#include "ReferenceSkeleton.h"

FImpactPoseRebuild::FImpactPoseRebuild(const FReferenceSkeleton& Ref, TConstArrayView<int32> BodyBoneIndices)
{
}

void FImpactPoseRebuild::Rebuild(TConstArrayView<FTransform> BodyComponent, TConstArrayView<FTransform> SourceComponent, const FTransform& Root,
	TArrayView<FTransform> OutComponent) const
{
}
