// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImpactBalancePlanner.h"
#include "ImpactClipWindow.h"
#include "ImpactJointLimits.h"
#include "ImpactOutcome.h"
#include "ImpactPoseBuffer.h"
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
	/** The source plays once and holds its last frame (a jump) rather than looping. */
	bool bSourcePlaysOnce = false;
	const UImpactRegionTable* RegionTable = nullptr;

	FImpactParams Params;
	FImpactSchedule Schedule;
	FImpactStrengthProfile Strength;

	int32 BatchSeed = 0;
	int32 ClipIndex = 0;

	int32 FrameRate = 60;

	/**
	 * Physics steps per recorded frame. At 60 Hz (1) the solver cannot always hold a light body
	 * struck hard: forearms tore 45-59 degrees through locked axes at 2,000-3,000 deg/s, and about
	 * 4% of clips are discarded as invalid. 4 (240 Hz) cut that to 0.3% but made standing
	 * characters step far more (crouch idle Flinch 386 -> 18 of 10k cells), cause not yet found.
	 * Kept at 60 Hz for now (user, 2026-10-14); see the plan's follow-ups.
	 */
	int32 SimSubsteps = 1;
	/**
	 * Solver iterations per body. At the project's 8 position iterations limb chains folded past
	 * their limits (1,361 of 13,680 grid clips invalid); 32 held them (about 600).
	 */
	int32 SolverPositionIterations = 0;
	int32 SolverVelocityIterations = 0;
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

	/**
	 * Degrees past the source's own range a joint's opened limit allows. Joints the source takes
	 * past the physics asset's limits are opened to hold it (Kimodo's forearm roll and sideways
	 * bend), so the physics can follow the motion it is given.
	 */
	float SourceLimitMarginDeg = 5.0f;

	/**
	 * A hit at least this strong while the source is in the air (FImpactSourceFlight) releases the
	 * body at once: a person in the air cannot resist it. Weaker hits ride the source to its landing.
	 */
	EImpactIntensity MidAirReleaseIntensity = EImpactIntensity::Heavy;

	/**
	 * The source is in a low stance when its pelvis at the hit is below this share of the reference
	 * (standing) pose's pelvis height: a crouch sits near 0.6, a walk near 0.95.
	 */
	float LowStancePelvisFraction = 0.8f;

	/**
	 * Frames the body is simulated, following the source, before the clip starts recording. The
	 * world hold settles a few centimetres under gravity once bodies stop being kinematic (a
	 * crouch ~6 cm over 6 frames); settled unrecorded, the clip never shows the drop.
	 */
	int32 WarmUpFrames = 20;

	/**
	 * A source foot this far above its reference-pose height is off the ground (a jump): the
	 * planner lets it follow the source and pins it where it lands.
	 */
	float AirborneFootClearanceCm = 5.0f;

	/** A collapsing hit (FImpactorArchetype::Collapses) lets go this many frames after it lands. */
	int32 CollapseDelayFrames = 2;

	/** Direction the mesh faces in component space (+Y for the UEFN mannequin). */
	FVector MeshForward = FVector(0.0, 1.0, 0.0);

	FImpactBalanceConfig Balance;
	FImpactFallRules FallRules;
	FImpactSourceClockConfig Clock;
	FImpactOutcomeThresholds Outcome;
	FImpactValidityThresholds Validity;
	FImpactRootTrackerConfig Root;
	FImpactRagdollRanges Ragdoll;
};

/**
 * One simulated frame's small state. Its poses live in the recording's pose buffers at the same
 * frame index, so recording a frame never allocates.
 */
struct KIMODOIMPACTCAPTURE_API FImpactRecordedFrame
{
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
	/** Every body's world transform per frame, in AImpactReactionCharacter::GetBodies() order. */
	FImpactPoseBuffer BodyPoses;
	/** Source pose per frame in component space (root motion included, planner offset not). */
	FImpactPoseBuffer SourcePoses;

	/** First frame of the reaction (the first hit). */
	int32 ImpactFrame = 0;
	/** The physics body the contact landed on, within the struck region. */
	FName StruckBody;
	/** Character space (X forward, Y right, Z up) to world, for hit directions. */
	FQuat CharacterToWorld = FQuat::Identity;
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
 * Runs one clip end to end in a shared world: an unrecorded warm-up and a pre-roll held to the
 * source motion, then the hit, the reaction and its end. Assembles the tested pieces (clip window, source clock,
 * strength profile, balance planner, fall rules, surface contact and spread, root tracker,
 * validity, outcome classifier) around the reaction character.
 */
struct KIMODOIMPACTCAPTURE_API FImpactClipSimulator
{
	/** False with a reason when the clip could not be set up. A clip that runs but breaks returns true with End = Invalidated. */
	static bool Run(FImpactSimWorld& World, const FImpactClipSetup& Setup, FImpactClipRecording& OutRecording, FText& OutReason);

	/**
	 * Where in its source the clip starts: from the source phase, or so the first hit lands on the
	 * setup's flight moment when the source plays once and leaves the ground.
	 */
	static double SourceStartSeconds(const FImpactClipSetup& Setup);
};
