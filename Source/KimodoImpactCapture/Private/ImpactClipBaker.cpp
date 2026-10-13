// Copyright Animotive. All Rights Reserved.

#include "ImpactClipBaker.h"

#include "Animation/AnimSequence.h"
#include "ImpactPoseBuffer.h"

FString FImpactClipBaker::PackageName(const FString& OutputRoot, const FString& BatchName, const FString& ClipId)
{
	return FString();
}

UAnimSequence* FImpactClipBaker::Bake(UObject* Outer, FName Name, USkeleton* Skeleton, const FReferenceSkeleton& Ref,
	const FImpactPoseBuffer& ComponentPoses, int32 FrameRate, FText& OutReason)
{
	return nullptr;
}

bool FImpactClipBaker::Save(UAnimSequence* Sequence, const FString& Filename, FText& OutReason)
{
	return false;
}
