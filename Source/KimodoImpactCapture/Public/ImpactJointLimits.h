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

	/** One swing limited, the other swing and the twist locked: a knee or an elbow. */
	bool IsHinge() const;
};
