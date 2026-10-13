// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Limits beyond which a frame means the simulation has broken. A value on a limit is still valid. */
struct KIMODOIMPACTCAPTURE_API FImpactValidityThresholds
{
	/**
	 * 60 m/s. A ragdoll hit by a car legitimately whips a hand past 40 m/s (observed in the Phase 3
	 * review); solver explosions run to hundreds.
	 */
	float MaxBodySpeedCmPerSecond = 6000.0f;
	/**
	 * 10,000 deg/s. A ragdolled hand hit by a car spun at 6,700 in the Phase 3 review; solver
	 * explosions spin in the tens of thousands.
	 */
	float MaxAngularSpeedDegPerSecond = 10000.0f;
	/** How far a body may sink below the floor before the contact is considered broken. */
	float MaxGroundPenetrationCm = 15.0f;
	/** Raw joint separation; the worst legitimate clip after the Phase 1 fixes measured 20 cm. */
	float MaxJointGapCm = 35.0f;
	/**
	 * Degrees a hinge (knee, elbow) may sit past its physics asset limit: beyond this the limb has
	 * folded the wrong way. Hinges held to 8 degrees in the Phase 3 review once the ragdoll's limits
	 * were hard and its joints toned; limp under soft limits they reached 27.
	 */
	float MaxHingeLimitExcessDeg = 20.0f;
	/**
	 * Degrees any other joint may sit past its limit. Shoulders and wrists legitimately overshoot
	 * the asset's conservative cones by up to 30 when a car strikes or the body lands on an arm;
	 * beyond this the joint has broken.
	 */
	float MaxJointLimitExcessDeg = 35.0f;
	/**
	 * Frames of recent targets a joint is measured against. A held joint trails its target: on the
	 * one-cycle Kimodo run an elbow sat 20-26 degrees past this frame's target while matching one
	 * a few frames old. 6 frames is 0.1 s at 60 Hz.
	 */
	int32 TargetWindowFrames = 6;
};

/** One body on one frame, as measured from the physics scene. */
struct KIMODOIMPACTCAPTURE_API FImpactBodySample
{
	FName Name;
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FVector LinearVelocity = FVector::ZeroVector;
	FVector AngularVelocityDegPerSecond = FVector::ZeroVector;
	/** Distance between this body and where its parent joint says it should be; 0 for the root body. */
	float JointGapCm = 0.0f;
	/** Degrees past this body's joint limit (FImpactJointLimit::Excess); 0 for the root body. */
	float JointLimitExcessDeg = 0.0f;
	/** The joint is a hinge (FImpactJointLimit::IsHinge): excess means a wrong-way fold. */
	bool bHingeJoint = false;
};

/**
 * Decides whether a simulated frame is usable. A clip with any unusable frame is abandoned with
 * the reason recorded, never baked: a broken simulation is worse than no clip.
 */
struct KIMODOIMPACTCAPTURE_API FImpactValidity
{
	/** True when every body is finite and within limits. Otherwise OutReason names the body and value. */
	static bool Check(TConstArrayView<FImpactBodySample> Bodies, float GroundHeight, const FImpactValidityThresholds& Thresholds, FText& OutReason);
};
