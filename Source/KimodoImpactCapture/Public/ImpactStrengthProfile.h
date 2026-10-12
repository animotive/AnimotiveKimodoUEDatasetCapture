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
	/** Every control released: the character is a ragdoll. */
	bool bReleased = false;
};

/** Multipliers on a body's base world-space and joint (parent-space) strengths. */
struct KIMODOIMPACTCAPTURE_API FImpactBodyStrength
{
	float World = 1.0f;
	float Joint = 1.0f;
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

	static FImpactStrengthProfile Sample(const FImpactStrengthDistribution& Distribution, FImpactRandom& Random);

	/**
	 * Multipliers for one body. FramesSinceHit below zero is pre-roll (full strength); zero is the
	 * hit frame.
	 */
	FImpactBodyStrength Evaluate(const FImpactBodyStrengthQuery& Body, int32 FramesSinceHit) const;
};
