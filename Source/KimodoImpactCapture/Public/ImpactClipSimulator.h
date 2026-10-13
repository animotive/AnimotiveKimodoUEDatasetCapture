// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImpactBalancePlanner.h"
#include "ImpactClipWindow.h"
#include "ImpactOutcome.h"
#include "ImpactParams.h"
#include "ImpactRootTracker.h"
#include "ImpactSchedule.h"
#include "ImpactSourceClock.h"
#include "ImpactStrengthProfile.h"
#include "ImpactValidity.h"

class FImpactSimWorld;
class UAnimSequence;
class UImpactRegionTable;
class UPhysicsAsset;
class USkeletalMesh;

/** Everything one clip's simulation needs, all sampled or resolved beforehand. */
struct KIMODOIMPACTCAPTURE_API FImpactClipSetup
{
	USkeletalMesh* Mesh = nullptr;
	UPhysicsAsset* PhysicsAsset = nullptr;
	UAnimSequence* Source = nullptr;
	const UImpactRegionTable* RegionTable = nullptr;

	FImpactParams Params;
	FImpactSchedule Schedule;
	FImpactStrengthProfile Strength;

	int32 BatchSeed = 0;
	int32 ClipIndex = 0;

	int32 FrameRate = 60;
	int32 PreRollFrames = 30;
	int32 MaxReactionFrames = 240;
	int32 SettleFrames = 20;
	/** Below this speed (relative to the source while held; absolute once released) the body is quiet. */
	float SettleSpeedCmPerSecond = 15.0f;
	/** The same for a moving source: a simulated runner never matches its source within 15 cm/s. */
	float SettleSpeedMovingCmPerSecond = 80.0f;
	/**
	 * Smallest radius the reaction-gain part of an impulse is spread over. A bullet's physical push
	 * is tiny; the reaction to being shot is an upper-body flinch, not a 45 m/s shove on one light
	 * body. At 25 cm a head shot still spun the neck at 5,900 deg/s; 40 cm reaches the neck, upper
	 * spine and clavicles.
	 */
	float ReactionSpreadRadiusCm = 40.0f;

	/** Direction the mesh faces in component space (+Y for the UEFN mannequin). */
	FVector MeshForward = FVector(0.0, 1.0, 0.0);

	FImpactBalanceConfig Balance;
	FImpactFallRules FallRules;
	FImpactSourceClockConfig Clock;
	FImpactOutcomeThresholds Outcome;
	FImpactValidityThresholds Validity;
	FImpactRootTrackerConfig Root;
};

/** One simulated frame, raw: Phase 4 turns these into bones. */
struct KIMODOIMPACTCAPTURE_API FImpactRecordedFrame
{
	/** Every body's world transform, in AImpactReactionCharacter::GetBodies() order. */
	TArray<FTransform> Bodies;
	/** Source pose in component space (root motion included, planner offset not). */
	TArray<FTransform> Source;
	/** Planner's horizontal displacement of the target frame. */
	FVector Offset = FVector::ZeroVector;
	/** Root bone for the baked clip, component space. */
	FTransform Root = FTransform::Identity;
	bool bReleased = false;
	/** Feet placed by the planner this frame, and where (world). */
	bool bFootPlaced[2] = { false, false };
	FVector FootTarget[2] = { FVector::ZeroVector, FVector::ZeroVector };
};

struct KIMODOIMPACTCAPTURE_API FImpactClipRecording
{
	/** Spawn transform; component space of every recorded pose. */
	FTransform Reference = FTransform::Identity;
	TArray<FName> BodyBones;
	TArray<int32> BodyBoneIndices;
	TArray<FImpactRecordedFrame> Frames;

	/** First frame of the reaction (the first hit). */
	int32 ImpactFrame = 0;
	bool bMovingSource = false;
	bool bOverwhelmed = false;
	EImpactClipEnd End = EImpactClipEnd::None;
	FText InvalidReason;
	FImpactOutcomeResult Outcome;
	/** Worst joint-limit excess over the clip, and the body it was on: how close the clip came to folding. */
	float MaxJointLimitExcessDeg = 0.0f;
	FName MaxJointLimitExcessBody;
};

/**
 * Runs one clip end to end in a shared world: kinematic pre-roll on the exact source motion, then
 * the hit, the reaction and its end. Assembles the tested pieces (clip window, source clock,
 * strength profile, balance planner, fall rules, surface contact and spread, root tracker,
 * validity, outcome classifier) around the reaction character.
 */
struct KIMODOIMPACTCAPTURE_API FImpactClipSimulator
{
	/** False with a reason when the clip could not be set up. A clip that runs but breaks returns true with End = Invalidated. */
	static bool Run(FImpactSimWorld& World, const FImpactClipSetup& Setup, FImpactClipRecording& OutRecording, FText& OutReason);
};
