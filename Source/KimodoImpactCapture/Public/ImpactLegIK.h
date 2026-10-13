// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** The three bones of one leg, as skeleton bone indices. */
struct KIMODOIMPACTCAPTURE_API FImpactLegChain
{
	int32 Thigh = INDEX_NONE;
	int32 Calf = INDEX_NONE;
	int32 Foot = INDEX_NONE;

	bool IsValid() const { return Thigh != INDEX_NONE && Calf != INDEX_NONE && Foot != INDEX_NONE; }
};

/**
 * Leg IK for the recorded pose: feet the balance planner placed land exactly where it put them,
 * and on a moving source a foot the source has on the ground goes to the source's ground height.
 * A ragdoll's feet are left to physics.
 */
struct KIMODOIMPACTCAPTURE_API FImpactLegIK
{
	/** Height below which a source foot counts as on the ground, in centimetres. */
	static constexpr float SourceContactHeightCm = 12.0f;

	/**
	 * Where the foot should go this frame, all in component space, or unset when the leg is left
	 * as simulated: a placed foot goes to its target; a moving source's grounded foot keeps its
	 * position across the ground and takes the source's height.
	 */
	static TOptional<FVector> ChooseEffector(bool bReleased, bool bFootPlaced, const FVector& PlacedTarget, bool bMovingSource,
		const FVector& SourceFoot, const FVector& CurrentFoot);

	/**
	 * Two-bone IK on Pose (component space, one transform per bone): the foot reaches Effector,
	 * the knee bends toward Forward, the foot keeps its rotation, bone lengths never change (an
	 * unreachable target leaves the leg straight toward it), and every bone below the foot keeps
	 * its local transform from Source.
	 */
	static void Solve(const FReferenceSkeleton& Ref, const FImpactLegChain& Leg, const FVector& Effector, const FVector& Forward,
		TConstArrayView<FTransform> Source, TArrayView<FTransform> Pose);
};
