// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImpactorCatalog.h"
#include "Math/Interval.h"

#include "ImpactSchedule.generated.h"

struct FImpactParams;

USTRUCT()
struct KIMODOIMPACTCAPTURE_API FImpactDeliveryWeight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Delivery")
	EImpactDeliveryMode Mode = EImpactDeliveryMode::Instant;

	UPROPERTY(EditAnywhere, Category = "Delivery", meta = (ClampMin = "0"))
	float Weight = 1.0f;
};

/** How a batch turns one sampled impact into hits over time. */
USTRUCT()
struct KIMODOIMPACTCAPTURE_API FImpactDeliveryDistributions
{
	GENERATED_BODY()

	/** Weighted delivery modes. Empty: every impactor uses its archetype's default delivery. */
	UPROPERTY(EditAnywhere, Category = "Delivery")
	TArray<FImpactDeliveryWeight> ModeOverride;

	/** Number of hits in a burst. At least two. */
	UPROPERTY(EditAnywhere, Category = "Delivery")
	FInt32Interval BurstHits = FInt32Interval(2, 5);

	/** Frames between consecutive burst hits. At least one. */
	UPROPERTY(EditAnywhere, Category = "Delivery")
	FInt32Interval BurstSpacingFrames = FInt32Interval(2, 8);

	/** Largest angle between a burst hit's direction and the impact's direction, degrees. */
	UPROPERTY(EditAnywhere, Category = "Delivery", meta = (ClampMin = "0", ClampMax = "90"))
	float BurstDirectionJitterDegrees = 10.0f;

	/** Duration of a sustained push in frames. At least one. */
	UPROPERTY(EditAnywhere, Category = "Delivery")
	FInt32Interval SustainedFrames = FInt32Interval(10, 45);

	/** Distance from the struck point at which a projectile is spawned, cm. */
	UPROPERTY(EditAnywhere, Category = "Delivery")
	FFloatInterval ProjectileLaunchDistanceCm = FFloatInterval(150.0f, 400.0f);
};

/** One scripted hit. */
struct KIMODOIMPACTCAPTURE_API FImpactHit
{
	/** Frames after the first hit. The first hit is at zero. */
	int32 FrameOffset = 0;

	/** Zero: an instantaneous impulse. Otherwise the impulse is spread over this many frames. */
	int32 DurationFrames = 0;

	/** Reaction impulse this hit delivers, kg x cm/s. */
	float ImpulseKgCmPerSecond = 0.0f;

	/** Unit direction of travel, character space. */
	FVector DirectionLocal = FVector::BackwardVector;
};

/** A physical object thrown at the character; the collision itself delivers the transfer. */
struct KIMODOIMPACTCAPTURE_API FImpactProjectileSpec
{
	EImpactorShape Shape = EImpactorShape::Sphere;
	float SizeCm = 0.0f;
	float MassKg = 0.0f;
	float SpeedMetresPerSecond = 0.0f;
	float LaunchDistanceCm = 0.0f;

	/**
	 * Reaction impulse beyond what the collision transfers (reaction minus transfer), applied at
	 * the contact. Zero for impactors whose reaction gain is one.
	 */
	float ExtraReactionImpulseKgCmPerSecond = 0.0f;
};

/** Every hit of one clip, in order. */
struct KIMODOIMPACTCAPTURE_API FImpactSchedule
{
	EImpactDeliveryMode Mode = EImpactDeliveryMode::Instant;
	TArray<FImpactHit> Hits;
	TOptional<FImpactProjectileSpec> Projectile;

	/** Sum of the scripted hits' impulses. */
	float TotalScriptedImpulse() const;
};

struct KIMODOIMPACTCAPTURE_API FImpactScheduleSampler
{
	/** Builds the hit schedule for one clip. False with a stated reason when the distributions are invalid. */
	static bool Sample(
		const FImpactParams& Params, const FImpactDeliveryDistributions& Distributions,
		int32 BatchSeed, int32 ClipIndex, FImpactSchedule& OutSchedule, FText& OutReason);
};
