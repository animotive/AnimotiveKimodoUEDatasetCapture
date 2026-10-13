// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Turns one simulated frame into a skeleton pose, in component space.
 *
 * Bodies give rotations; positions come from the source pose's bone lengths, so no joint can
 * separate however far the physics stretched it. Only the top body (the pelvis: no body above
 * it) keeps its simulated position. Bones without a body follow the source animation locally,
 * and the root bone is given (the root tracker's ground projection). Prepared once per clip
 * for a skeleton and its bodies; Rebuild then allocates nothing.
 */
class KIMODOIMPACTCAPTURE_API FImpactPoseRebuild
{
public:
	/** BodyBoneIndices[i] is the skeleton bone driven by body i. */
	FImpactPoseRebuild(const FReferenceSkeleton& Ref, TConstArrayView<int32> BodyBoneIndices);

	/**
	 * Fills OutComponent (one transform per bone) from body transforms in component space (body
	 * order as given to the constructor), the source pose in component space and the root bone.
	 */
	void Rebuild(TConstArrayView<FTransform> BodyComponent, TConstArrayView<FTransform> SourceComponent, const FTransform& Root,
		TArrayView<FTransform> OutComponent) const;

	int32 GetBoneCount() const { return Parents.Num(); }

private:
	TArray<int32> Parents;
	/** Body index driving each bone, or INDEX_NONE. */
	TArray<int32> BodyOf;
	/** True when some ancestor of the bone has a body. */
	TArray<bool> HasBodyAbove;
};
