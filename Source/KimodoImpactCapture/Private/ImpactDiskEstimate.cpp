// Copyright Animotive. All Rights Reserved.

#include "ImpactDiskEstimate.h"

#include "ImpactBatchManifest.h"

int64 FImpactDiskEstimate::BytesPerClip(int32 Bones, int32 MaxFrames)
{
	return 0;
}

int64 FImpactDiskEstimate::PerClip(const FImpactBatchManifest& Manifest, int32 Bones, int32 MaxFrames)
{
	return 0;
}

int64 FImpactDiskEstimate::Estimate(int32 PendingClips, int64 PerClipBytes)
{
	return 0;
}

bool FImpactDiskEstimate::ShouldWarn(int64 EstimateBytes, int64 FreeBytes, FText& OutWarning)
{
	return false;
}

int64 FImpactDiskEstimate::FreeBytes(const FString& Path)
{
	return -1;
}
