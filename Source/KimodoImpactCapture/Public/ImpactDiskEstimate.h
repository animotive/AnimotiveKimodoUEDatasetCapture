// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FImpactBatchManifest;

/**
 * How much disk a batch will take, checked before it starts. Disk, not time, is what limits a
 * batch: the Phase 1 spike measured ~1.6 MB per 3 s clip (tech-stack.md).
 */
struct KIMODOIMPACTCAPTURE_API FImpactDiskEstimate
{
	/**
	 * Bytes one bone takes per frame in a saved clip, sidecar included, from the spike's measured
	 * ~1.6 MB for 180 frames of 88 bones.
	 */
	static constexpr int64 BytesPerBoneFrame = 104;

	/** Upper bound for one clip of MaxFrames frames over Bones bones, before any clip is measured. */
	static int64 BytesPerClip(int32 Bones, int32 MaxFrames);

	/**
	 * Bytes per clip for the rest of a batch: the mean of the clips already on disk when the
	 * manifest has any, else the upper bound.
	 */
	static int64 PerClip(const FImpactBatchManifest& Manifest, int32 Bones, int32 MaxFrames);

	static int64 Estimate(int32 PendingClips, int64 PerClipBytes);

	/** True with a warning when EstimateBytes will not fit in FreeBytes. Unknown free space (negative) never warns. */
	static bool ShouldWarn(int64 EstimateBytes, int64 FreeBytes, FText& OutWarning);

	/** Free bytes on the drive holding Path, or -1 when it cannot be read. */
	static int64 FreeBytes(const FString& Path);
};
