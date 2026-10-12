// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Limits beyond which a frame means the simulation has broken. A value on a limit is still valid. */
struct KIMODOIMPACTCAPTURE_API FImpactValidityThresholds
{
	/** 40 m/s: far beyond anything a struck body reaches; above it the solver has exploded. */
	float MaxBodySpeedCmPerSecond = 4000.0f;
	float MaxAngularSpeedDegPerSecond = 5000.0f;
	/** How far a body may sink below the floor before the contact is considered broken. */
	float MaxGroundPenetrationCm = 15.0f;
	/** Raw joint separation; the worst legitimate clip after the Phase 1 fixes measured 20 cm. */
	float MaxJointGapCm = 35.0f;
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
