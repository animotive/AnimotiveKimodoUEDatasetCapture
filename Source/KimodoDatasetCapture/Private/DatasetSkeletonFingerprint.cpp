// Copyright Animotive. All Rights Reserved.

#include "DatasetSkeletonFingerprint.h"

#include "ReferenceSkeleton.h"

// RED PHASE: neutral bodies so the tests fail at runtime rather than failing to compile.

FString FDatasetSkeletonFingerprint::Compute(TArrayView<const FDatasetBoneEntry> Bones)
{
	return FString();
}

FString FDatasetSkeletonFingerprint::Compute(const FReferenceSkeleton& ReferenceSkeleton)
{
	return FString();
}

TArray<FDatasetBoneEntry> FDatasetSkeletonFingerprint::ExtractBones(const FReferenceSkeleton& ReferenceSkeleton)
{
	return TArray<FDatasetBoneEntry>();
}
