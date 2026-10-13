// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * One clip's baked poses: component-space bone transforms, frame after frame, in a block reserved
 * at clip start for the clip's longest possible length. The per-step path never allocates; a
 * frame beyond the reserve is refused and the overflow recorded, so the clip is invalidated with
 * a reason instead of silently growing or losing frames.
 */
class KIMODOIMPACTCAPTURE_API FImpactPoseBuffer
{
public:
	/** Reserves MaxFrames x BoneCount transforms and clears any existing content. */
	void Reserve(int32 MaxFrames, int32 BoneCount);

	/**
	 * The next frame's transforms, BoneCount of them, to be filled by the caller. Empty, and the
	 * overflow recorded, when the buffer is full.
	 */
	TArrayView<FTransform> AddFrame();

	int32 Num() const;
	int32 Capacity() const;
	int32 GetBoneCount() const;
	bool IsFull() const;

	/** True once a frame was refused; the reason says how many frames the reserve held. */
	bool HasOverflowed() const;
	FText GetOverflowReason() const;

	/** One recorded frame's transforms. */
	TArrayView<const FTransform> GetFrame(int32 Frame) const;

	/** Clears the frames and the overflow, keeping the reserve. */
	void Reset();

	/** Times the storage moved since the last Reserve. Any value above zero is a defect. */
	int32 GetReallocationCount() const;

private:
	TArray<FTransform> Transforms;
	const FTransform* LastKnownData = nullptr;
	int32 ReallocationCount = 0;
	int32 MaxFrameCount = 0;
	int32 BoneCount = 0;
	int32 FrameCount = 0;
	bool bOverflowed = false;
};
