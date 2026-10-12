// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ImpactOutcome.generated.h"

/**
 * What the hit did to the character. Serialised by name in sidecars and captions: enumerators may
 * be added but never renamed or reordered.
 */
UENUM()
enum class EImpactOutcome : uint8
{
	/** Absorbed in place: no step, no real dip. */
	Flinch,
	/** Knocked off balance and recovered: steps, or a dip, but stays up. */
	Stagger,
	/** Lost balance after trying to keep it and went down. */
	Knockdown,
	/** Overwhelmed at once: went fully limp with no attempt to balance. */
	Ragdoll,
};

/** One simulated frame, from the hit onward, as the classifier sees it. */
struct KIMODOIMPACTCAPTURE_API FImpactFrameState
{
	float PelvisHeightCm = 0.0f;
	/** The source animation's pelvis height on the same frame: what "standing" means here. */
	float StandingPelvisHeightCm = 0.0f;
	/** The capture point lay outside the support the feet provide. */
	bool bOutsideSupport = false;
	/** Recovery steps taken so far. */
	int32 Steps = 0;
	/** Every control released: the character is a ragdoll from this frame. */
	bool bControlsReleased = false;
	/** A leg hit broke a moving character's stride. */
	bool bTrip = false;
};

USTRUCT()
struct KIMODOIMPACTCAPTURE_API FImpactOutcomeThresholds
{
	GENERATED_BODY()

	/** Pelvis at or above this fraction of standing height all reaction long counts as no dip. */
	UPROPERTY(EditAnywhere, Category = "Outcome")
	float FlinchPelvisFraction = 0.85f;

	/** Pelvis below this fraction of standing height means the character went down. */
	UPROPERTY(EditAnywhere, Category = "Outcome")
	float KnockdownPelvisFraction = 0.6f;

	/** Controls released within this many frames of the hit is a ragdoll, not a failed recovery. */
	UPROPERTY(EditAnywhere, Category = "Outcome")
	int32 ImmediateReleaseFrames = 3;
};

struct KIMODOIMPACTCAPTURE_API FImpactOutcomeResult
{
	bool bValid = false;
	EImpactOutcome Outcome = EImpactOutcome::Flinch;
	/** Frame (from the hit) at which the outcome was decided. */
	int32 OutcomeFrame = 0;
	int32 Steps = 0;
	float MinPelvisFraction = 1.0f;
	bool bTrip = false;
};

/** Classifies a reaction from its per-frame trace. Pure; same trace, same result. */
struct KIMODOIMPACTCAPTURE_API FImpactOutcomeClassifier
{
	/** Invalid when the trace is empty or a frame has no standing height. */
	static FImpactOutcomeResult Classify(TConstArrayView<FImpactFrameState> Frames, const FImpactOutcomeThresholds& Thresholds);
};
