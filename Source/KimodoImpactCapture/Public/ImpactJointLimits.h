// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * A joint's rotation split the way the physics constraint limits it: twist about the constraint's
 * X axis, then swing1 about Z and swing2 about Y. Degrees.
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
 * 180; a locked axis is 0.
 */
struct KIMODOIMPACTCAPTURE_API FImpactJointLimit
{
	float TwistDeg = 180.0f;
	float Swing1Deg = 180.0f;
	float Swing2Deg = 180.0f;

	/** Degrees the worst axis is past its limit; 0 when every axis is within. */
	float Excess(const FImpactJointAngles& Angles) const;
};
