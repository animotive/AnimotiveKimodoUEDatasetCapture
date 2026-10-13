// Copyright Animotive. All Rights Reserved.

#include "ImpactJointLimits.h"

FImpactJointAngles FImpactJointAngles::FromRelative(const FQuat& ChildInParent)
{
	return FImpactJointAngles();
}

float FImpactJointLimit::Excess(const FImpactJointAngles& Angles) const
{
	return 0.0f;
}
