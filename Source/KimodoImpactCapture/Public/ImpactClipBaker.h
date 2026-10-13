// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FImpactPoseBuffer;
class UAnimSequence;
class USkeleton;

/**
 * Turns a recorded clip into an animation asset on the source skeleton, through
 * IAnimationDataController, and saves it under <OutputRoot>/<BatchName>/<ClipId>.
 */
struct KIMODOIMPACTCAPTURE_API FImpactClipBaker
{
	/** The clip's long package name: OutputRoot/BatchName/ClipId, without doubled separators. */
	static FString PackageName(const FString& OutputRoot, const FString& BatchName, const FString& ClipId);

	/**
	 * A new sequence named Name in Outer, one key per recorded frame at FrameRate, one bone track
	 * per skeleton bone. ComponentPoses holds component-space poses (one transform per bone of
	 * Ref). Null with a reason when there is nothing to bake or the inputs disagree.
	 */
	static UAnimSequence* Bake(UObject* Outer, FName Name, USkeleton* Skeleton, const FReferenceSkeleton& Ref,
		const FImpactPoseBuffer& ComponentPoses, int32 FrameRate, FText& OutReason);

	/** Saves the sequence's package to Filename. False with a reason on failure. */
	static bool Save(UAnimSequence* Sequence, const FString& Filename, FText& OutReason);
};
