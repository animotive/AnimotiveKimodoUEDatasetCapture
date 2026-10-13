// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Balance and stepping parameters; defaults are the values approved by eye in the Phase 1 spike. */
struct KIMODOIMPACTCAPTURE_API FImpactBalanceConfig
{
	int32 SwingFrames = 15;
	float SwingLiftCm = 15.0f;
	/** A step is needed when the capture point is farther than this from the middle of the feet. */
	float StepTriggerCm = 15.0f;
	/** Longest step a leg can take. */
	float MaxStepCm = 70.0f;
	/** Half the distance between the feet, kept when landing. */
	float StanceHalfWidthCm = 11.0f;
	/** A landed foot stays planted at least this long before the next step. */
	int32 MinStanceFrames = 8;
	/** Steps allowed before balance counts as lost. */
	int32 MaxSteps = 5;
	/** Consecutive steps that fall short of the capture point before balance counts as lost. */
	int32 MaxUncaughtSteps = 3;
	/** Centre of mass below this height means the character is going down. */
	float FallComHeightCm = 55.0f;
	/** Calm frames needed before the feet start blending back onto the source pose. */
	int32 SettleHoldFrames = 20;
	/** Frames over which the feet blend back onto the source pose once settled. */
	int32 SettleBlendFrames = 20;
	/** Excess speed below this counts as calm. */
	float CalmExcessSpeedCmPerSecond = 20.0f;
	/** Low-pass factor on excess velocity per frame (1 = no smoothing). */
	float ExcessSmoothing = 0.3f;
	/** Share of excess momentum the target frame carries on a standing source. */
	float StandingOffsetFollow = 0.8f;
	/** Per-frame pull of the target frame toward the new footing once the character has stepped. */
	float FootingPull = 0.08f;
	float GravityCmPerSecondSquared = 980.0f;
	/** Frames a tripped foot stays caught where the hit stopped it. */
	int32 TripHoldFrames = 12;
	/** A moving character falls when its excess momentum carries the capture point beyond this. */
	float MovingFallReachCm = 70.0f;
};

/** Thresholds above which a hit overwhelms the character outright: full ragdoll, no recovery. */
struct KIMODOIMPACTCAPTURE_API FImpactFallRules
{
	/** Whole-body speed change, cm/s. 5.5 m/s: a car or a tackle, not a rifle round. */
	float OverwhelmBodyDeltaVCmPerSecond = 550.0f;
	/** Struck-body speed change, cm/s: a violent blow to one light body part. */
	float OverwhelmStruckBodyDeltaVCmPerSecond = 9000.0f;

	/**
	 * True when the reaction impulse overwhelms the whole body, or the struck part's speed change
	 * from its share of the spread push (FImpactContactSpread::StruckDeltaV) does (bounds count).
	 * Not the impulse over the bare struck-part mass: a hand is half a kilogram, and a light punch
	 * read as violent.
	 */
	bool IsOverwhelming(float ReactionImpulseKgCmPerSecond, float BodyMassKg, float StruckDeltaVCmPerSecond) const;
};

/** One frame of measured state, after the hit. */
struct KIMODOIMPACTCAPTURE_API FImpactBalanceInput
{
	float DeltaSeconds = 1.0f / 60.0f;
	/** Simulated centre of mass and its velocity, world space. */
	FVector Com = FVector::ZeroVector;
	FVector ComVelocity = FVector::ZeroVector;
	/** Velocity the source animation itself carries on this frame. */
	FVector SourceVelocity = FVector::ZeroVector;
	/** Simulated foot positions this frame (0 left, 1 right); used to pin the feet on the first frame. */
	FVector FootPosition[2] = { FVector::ZeroVector, FVector::ZeroVector };
	/** The source travels (a run), as classified once per clip. */
	bool bMovingSource = false;
	/** Leg the hit struck (0 left, 1 right), or INDEX_NONE when it struck no leg. */
	int32 StruckLegSide = INDEX_NONE;
};

struct KIMODOIMPACTCAPTURE_API FImpactFootTarget
{
	/** The planner owns this foot: it is placed kinematically at Position. */
	bool bPinned = false;
	bool bSwing = false;
	FVector Position = FVector::ZeroVector;
	/** 0: at Position. 1: fully back on the source pose (the caller blends between them). */
	float SettleAlpha = 0.0f;
};

struct KIMODOIMPACTCAPTURE_API FImpactBalanceOutput
{
	/** Horizontal displacement of the whole target frame. */
	FVector Offset = FVector::ZeroVector;
	FImpactFootTarget Feet[2];
	FVector CapturePoint = FVector::ZeroVector;
	int32 Steps = 0;
	/** Balance is lost; release every control. */
	bool bFell = false;
	/** True only on the frame balance was lost. */
	bool bJustFell = false;
	bool bSettling = false;
	/** A leg hit broke a moving character's stride. */
	bool bTrip = false;
};

/**
 * Capture-point stepping for a struck character, as pure logic: it reads measured centre of mass
 * and feet and decides where the feet go and how far the character is carried. No physics here,
 * so every rule is tested in isolation. See tech-stack.md for why each rule exists.
 */
class KIMODOIMPACTCAPTURE_API FImpactBalancePlanner
{
public:
	explicit FImpactBalancePlanner(const FImpactBalanceConfig& InConfig);

	/** Consumes one frame after the hit. After a fall, keeps reporting the fall and changes nothing. */
	FImpactBalanceOutput Step(const FImpactBalanceInput& Input);

	/** Overwhelming hit: release every control from the next frame, with no attempt to balance. */
	void ReleaseImmediately();

	/** Capture point for a centre of mass and horizontal excess velocity. */
	static FVector CapturePoint(const FVector& Com, const FVector& ExcessVelocity, float GravityCmPerSecondSquared);

private:
	struct FFoot
	{
		bool bPinned = false;
		FVector Planted = FVector::ZeroVector;
		bool bSwing = false;
		FVector SwingFrom = FVector::ZeroVector;
		FVector SwingTo = FVector::ZeroVector;
		int32 SwingStart = 0;
		FVector Current = FVector::ZeroVector;
	};

	FImpactBalanceConfig Config;
	FFoot Feet[2];
	FVector Offset = FVector::ZeroVector;
	FVector SmoothedExcess = FVector::ZeroVector;
	FVector FootingAtHit = FVector::ZeroVector;
	FVector LastCapturePoint = FVector::ZeroVector;
	int32 Frame = 0;
	int32 Steps = 0;
	int32 UncaughtSteps = 0;
	int32 LastSwingSide = INDEX_NONE;
	int32 LastLandFrame = INDEX_NONE;
	int32 CalmFrames = 0;
	int32 SettleStart = INDEX_NONE;
	bool bFell = false;
	bool bPendingJustFell = false;
	bool bTrip = false;
	int32 TripSide = INDEX_NONE;
	FVector TripFoot = FVector::ZeroVector;
};
