// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct KIMODOIMPACTCAPTURE_API FImpactRootTrackerConfig
{
	/** Frames over which the root moves from the source path to under the ragdoll. */
	int32 BlendFrames = 10;
	/** Per-frame low-pass on facing, 1 = none. */
	float YawSmoothing = 0.25f;
	/** Below this flattened length the pelvis is too vertical to give a facing; the last one is kept. */
	float MinFlatForward = 0.3f;
};

/**
 * Root bone for a ragdolled character. While the controls hold, the root follows the source
 * animation. Once they release, the root moves under the pelvis at ground height, facing the
 * pelvis's flattened forward, so a body lying two metres away is not baked with its root still
 * on the source path.
 */
class KIMODOIMPACTCAPTURE_API FImpactRootTracker
{
public:
	/**
	 * ReferencePelvis is the pelvis in component space in the reference pose; MeshForward is the
	 * component-space direction the mesh faces (+Y for the UEFN mannequin). Together they fix which
	 * pelvis axis means "forward" for this rig.
	 */
	FImpactRootTracker(const FTransform& ReferencePelvis, const FVector& MeshForward, float GroundHeight, const FImpactRootTrackerConfig& InConfig);

	/** The pelvis axis, in pelvis space, that points forward in the reference pose. */
	const FVector& GetPelvisForwardAxis() const { return PelvisForwardAxis; }

	/** Root on the ground under the pelvis, facing its flattened forward, without blending or smoothing. */
	FTransform Project(const FTransform& Pelvis) const;

	/** Root for this frame: the source root until bReleased, then blending to the projected root. */
	FTransform Step(const FTransform& SourceRoot, const FTransform& Pelvis, bool bReleased);

private:
	FImpactRootTrackerConfig Config;
	FVector PelvisForwardAxis = FVector::ForwardVector;
	FVector MeshForward = FVector::RightVector;
	float GroundHeight = 0.0f;
	float SmoothedYaw = 0.0f;
	bool bHaveYaw = false;
	int32 ReleasedFrames = 0;

	/** Yaw (degrees) of the pelvis's flattened forward, or the last good yaw when too vertical. */
	float PelvisYaw(const FTransform& Pelvis, bool& bOutValid) const;
};
