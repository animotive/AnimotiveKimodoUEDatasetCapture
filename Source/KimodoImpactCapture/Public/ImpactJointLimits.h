// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * A joint's rotation split the way the physics constraint limits it: twist about the constraint's
 * X axis, then a swing whose angle is split along Z (swing1) and Y (swing2). Degrees.
 */
struct KIMODOIMPACTCAPTURE_API FImpactJointAngles
{
	float TwistDeg = 0.0f;
	float Swing1Deg = 0.0f;
	float Swing2Deg = 0.0f;

	/** From the child's constraint frame expressed in the parent's constraint frame. */
	static FImpactJointAngles FromRelative(const FQuat& ChildInParent);
};

/**
 * A joint's symmetric angular limits, degrees either side of the constraint frame. A free axis is
 * 180; a locked axis is 0. With both swings limited the swing limit is an elliptical cone, as in
 * Chaos; otherwise each axis is limited on its own.
 */
struct KIMODOIMPACTCAPTURE_API FImpactJointLimit
{
	float TwistDeg = 180.0f;
	float Swing1Deg = 180.0f;
	float Swing2Deg = 180.0f;

	/** Degrees the worst axis is past its limit; 0 when every axis is within. */
	float Excess(const FImpactJointAngles& Angles) const;

	/**
	 * Excess past this limit widened just enough to hold Allowed: each axis opened to Allowed's
	 * angle, an elliptical cone scaled until Allowed is on it. A source pose that already sits
	 * outside the asset's limits is the motion asked for, not a fold; only what the physics adds
	 * beyond it counts.
	 */
	float ExcessBeyond(const FImpactJointAngles& Angles, const FImpactJointAngles& Allowed) const;

	/**
	 * The least ExcessBeyond over several allowed angles: the targets a held joint was given over
	 * the last few frames, since it trails them. Plain Excess when Allowed is empty.
	 */
	float ExcessBeyondAny(const FImpactJointAngles& Angles, TConstArrayView<FImpactJointAngles> Allowed) const;

	/**
	 * This limit opened just enough to hold every angle in Source, plus MarginDeg: each locked or
	 * limited axis a source angle exceeds is opened to the largest such angle plus the margin, and
	 * a cone is then scaled until every source angle is inside it. Axes the source keeps within,
	 * and free axes, are unchanged. Limits stay symmetric, as physics limits are.
	 */
	FImpactJointLimit OpenedToHold(TConstArrayView<FImpactJointAngles> Source, float MarginDeg) const;

	/** One swing limited, the other swing and the twist locked: a knee or an elbow. */
	bool IsHinge() const;
};

/**
 * A ragdoll's range on one swing axis, as degrees either side of the joint's angle in the
 * reference (standing) pose. Physics limits are symmetric about the constraint frame, so an
 * uneven range (a hip bends far forward, little back) is made by turning the frame to the range's
 * centre and limiting to its half-width.
 */
struct KIMODOIMPACTCAPTURE_API FImpactJointRange
{
	float MinDeg = -180.0f;
	float MaxDeg = 180.0f;

	/**
	 * The range on an axis whose asset limit is AssetLimitDeg either side of the frame, with the
	 * reference pose at ReferenceDeg, never wider than the asset allows. OutCentreDeg is where the
	 * frame turns to, OutHalfDeg the new symmetric limit. False when the two do not overlap.
	 */
	bool Fit(float AssetLimitDeg, float ReferenceDeg, float& OutCentreDeg, float& OutHalfDeg) const;

	/** The parent constraint frame turned about its Z (swing1) axis to CentreDeg. */
	static FQuat TurnSwing1(const FQuat& ParentFrame, float CentreDeg);
};

/**
 * Hip ranges a ragdoll is held to, from the standing pose. The physics asset allows a dancer's
 * range (125 degrees of flexion); limp legs went into the splits in the Phase 3 car and trip
 * clips, the front leg flexed 104 degrees. On the UEFN mannequin hip flexion is negative swing1.
 * Values are the "Loose" look approved in the Phase 3 review (70 / 15 / 20 read as too rigid).
 */
struct KIMODOIMPACTCAPTURE_API FImpactRagdollRanges
{
	FImpactJointRange HipSwing1 = { -90.0f, 25.0f };
	/** Sideways, either way. */
	float HipSwing2Deg = 30.0f;
	float HipTwistDeg = 30.0f;
};
