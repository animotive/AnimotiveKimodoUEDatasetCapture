// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Interval.h"

#include "ImpactStrengthProfile.generated.h"

struct FImpactRandom;

/** Per-clip spread of Physics Control strengths, around the values the spike was approved at. */
USTRUCT()
struct KIMODOIMPACTCAPTURE_API FImpactStrengthDistribution
{
	GENERATED_BODY()

	/** World-space control strength. The approved spike ran at 8. */
	UPROPERTY(EditAnywhere, Category = "Strength")
	FFloatInterval WorldStrength = FFloatInterval(6.0f, 10.0f);

	/** Parent-space (joint) strength as a multiple of world strength. The approved spike ran at 2. */
	UPROPERTY(EditAnywhere, Category = "Strength")
	FFloatInterval ParentToWorldRatio = FFloatInterval(1.6f, 2.4f);

	/**
	 * Joint tone a ragdoll keeps at the moment of release, as a multiple of joint strength. Around
	 * the "Loose" look approved in the Phase 3 review; 0.3-0.5 read as too rigid.
	 */
	UPROPERTY(EditAnywhere, Category = "Strength")
	FFloatInterval ReleasedTone = FFloatInterval(0.1f, 0.18f);

	/** Tone the ragdoll fades to, as a multiple of its tone at release. */
	UPROPERTY(EditAnywhere, Category = "Strength")
	FFloatInterval ReleasedToneEndRatio = FFloatInterval(0.2f, 0.4f);
};

/** What the profile needs to know about one body. */
struct KIMODOIMPACTCAPTURE_API FImpactBodyStrengthQuery
{
	/** Links along the body tree from the struck body; 0 is the struck body. */
	int32 ContactDistance = 0;
	bool bLeg = false;
	bool bPelvis = false;
	/** The source animation travels (a run), as classified once per clip. */
	bool bMovingSource = false;
	/** The character is a ragdoll: no world hold, joints keep only a fading tone. */
	bool bReleased = false;
	/** Spine, neck or head: braced harder than the limbs while falling. */
	bool bCore = false;
	/** Frames since the character became a ragdoll; 0 on the release frame. */
	int32 FramesSinceRelease = 0;
};

/** Multipliers on a body's base world-space and joint (parent-space) strengths. */
struct KIMODOIMPACTCAPTURE_API FImpactBodyStrength
{
	float World = 1.0f;
	float Joint = 1.0f;
	/** Multiplier on the joint control's damping ratio. */
	float JointDamping = 1.0f;
};

/**
 * How hard each body holds the animation through a reaction, frame by frame. Encodes the values
 * approved by eye in the Phase 1 spike (see tech-stack.md): local release around the contact, an
 * impact phase in which the upper body lets go, legs that travel with the hips on a moving
 * source, and a smooth return to full strength. Standing feet belong to the balance planner.
 */
struct KIMODOIMPACTCAPTURE_API FImpactStrengthProfile
{
	float WorldStrength = 8.0f;
	float ParentStrength = 16.0f;

	/** Frames after the hit during which the upper body nearly lets go. */
	int32 ImpactFrames = 10;
	/** Frames over which the impact phase blends into the release profile. */
	int32 ImpactBlendFrames = 20;
	/** Frames over which local release returns to full strength. */
	int32 RecoverFrames = 60;

	float ImpactUpperWorld = 0.02f;
	float ImpactUpperJoint = 0.15f;
	/** On a moving source the pelvis keeps some hold, or it tears away from the legs. */
	float ImpactMovingPelvisWorld = 0.3f;
	/** On a moving source the legs go with the hips through the impact phase. */
	float ImpactMovingLegWorld = 0.2f;

	/**
	 * A ragdoll is not limp: joints keep some tone toward the pose they had at release, fading over
	 * ReleasedToneFadeFrames. Limp joints under soft limits folded limbs unnaturally in the Phase 3
	 * foot-shot trip; the approved look is a light tone (0.4 read as too rigid).
	 */
	float ReleasedToneStart = 0.14f;
	float ReleasedToneEnd = 0.042f;
	int32 ReleasedToneFadeFrames = 45;
	/**
	 * Falling people brace the trunk and neck. With the limbs' tone the head flopped at its limits
	 * through most of the Phase 3 trip; 2x read as too rigid.
	 */
	float ReleasedCoreToneScale = 1.25f;
	/**
	 * Heavily overdamped, so a lightly toned limb settles rather than whipping. Damping scales with
	 * tone: at the approved light tone, less than 4.5x let the car hit fold an elbow backwards.
	 */
	float ReleasedJointDamping = 4.5f;

	static FImpactStrengthProfile Sample(const FImpactStrengthDistribution& Distribution, FImpactRandom& Random);

	/**
	 * Multipliers for one body. FramesSinceHit below zero is pre-roll (full strength); zero is the
	 * hit frame.
	 */
	FImpactBodyStrength Evaluate(const FImpactBodyStrengthQuery& Body, int32 FramesSinceHit) const;
};
