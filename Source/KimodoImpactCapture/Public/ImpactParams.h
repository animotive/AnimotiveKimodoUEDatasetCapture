// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImpactorCatalog.h"
#include "Math/Interval.h"

#include "ImpactParams.generated.h"

class UImpactorCatalog;

/** Descriptive strength of a hit, derived from its reaction impulse; used in captions and reports. */
UENUM()
enum class EImpactIntensity : uint8
{
	Tap,
	Light,
	Medium,
	Heavy,
	Knockdown,
	Extreme,
};

USTRUCT()
struct KIMODOIMPACTCAPTURE_API FImpactRegionWeight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Impact")
	EImpactBodyRegion Region = EImpactBodyRegion::Chest;

	UPROPERTY(EditAnywhere, Category = "Impact", meta = (ClampMin = "0"))
	float Weight = 1.0f;
};

/**
 * Lower bound of each intensity bucket above Tap, as reaction impulse in kg x cm/s. A value
 * exactly on a bound belongs to the higher bucket. Defaults are anchored on the Phase 1 spike:
 * 15k produced one recovery step, 30k two, 50k two large ones.
 */
USTRUCT()
struct KIMODOIMPACTCAPTURE_API FImpactIntensityThresholds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Impact")
	float Light = 5000.0f;

	UPROPERTY(EditAnywhere, Category = "Impact")
	float Medium = 15000.0f;

	UPROPERTY(EditAnywhere, Category = "Impact")
	float Heavy = 30000.0f;

	UPROPERTY(EditAnywhere, Category = "Impact")
	float Knockdown = 50000.0f;

	UPROPERTY(EditAnywhere, Category = "Impact")
	float Extreme = 80000.0f;

	/** Bucket for a reaction impulse. Values on a bound go up. */
	EImpactIntensity Classify(float ReactionImpulseKgCmPerSecond) const;
};

/** The distributions a batch draws impact parameters from. */
USTRUCT()
struct KIMODOIMPACTCAPTURE_API FImpactDistributions
{
	GENERATED_BODY()

	/** Regions that may be hit, with relative weights. */
	UPROPERTY(EditAnywhere, Category = "Impact")
	TArray<FImpactRegionWeight> Regions;

	/**
	 * Elevation of the direction the impactor travels, in degrees, in character space:
	 * 0 is horizontal, positive is travelling upwards. Sampled area-uniformly.
	 */
	UPROPERTY(EditAnywhere, Category = "Impact")
	FFloatInterval ElevationDegrees = FFloatInterval(-90.0f, 90.0f);

	/**
	 * Azimuth of the direction the impactor travels, in degrees, in character space (X forward,
	 * Y right): 180 travels backwards into the character's front, i.e. a hit from the front.
	 */
	UPROPERTY(EditAnywhere, Category = "Impact")
	FFloatInterval AzimuthDegrees = FFloatInterval(0.0f, 360.0f);

	/** Normalised phase of the source animation at which the first hit lands. */
	UPROPERTY(EditAnywhere, Category = "Impact")
	FFloatInterval SourcePhase = FFloatInterval(0.0f, 1.0f);

	/** Coefficient of restitution for momentum transfer; 0 is perfectly plastic. */
	UPROPERTY(EditAnywhere, Category = "Impact", meta = (ClampMin = "0", ClampMax = "1"))
	float Restitution = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Impact")
	FImpactIntensityThresholds Intensity;
};

/** Every sampled parameter of one clip's primary impact. Recorded in full in the sidecar. */
struct KIMODOIMPACTCAPTURE_API FImpactParams
{
	FImpactorSample Impactor;

	/** Momentum actually transferred to a body of the character's mass, kg x cm/s. */
	float TransferredImpulseKgCmPerSecond = 0.0f;

	/** The archetype's reaction gain at sampling time. */
	float ReactionGain = 1.0f;

	/** Impulse the simulation applies: transferred x reaction gain, kg x cm/s. */
	float ReactionImpulseKgCmPerSecond = 0.0f;

	EImpactBodyRegion Region = EImpactBodyRegion::Chest;

	/** Unit direction the impactor travels, character space (X forward, Y right, Z up). */
	FVector DirectionLocal = FVector::BackwardVector;

	float SourcePhase = 0.0f;

	EImpactIntensity Intensity = EImpactIntensity::Tap;
};

/** Draws FImpactParams for one clip. Pure: same inputs, same parameters. */
struct KIMODOIMPACTCAPTURE_API FImpactParamsSampler
{
	/**
	 * Momentum transferred to a body of BodyMassKg by an impactor, in kg x cm/s:
	 * reduced mass x speed x (1 + restitution). A bullet transfers about its own momentum; a car
	 * transfers about the body's mass times its speed, not its own enormous momentum.
	 */
	static float TransferredImpulse(float ImpactorMassKg, float SpeedMetresPerSecond, float BodyMassKg, float Restitution);

	/**
	 * Samples one clip's primary impact. False with a stated reason when the catalog or the
	 * distributions cannot be sampled.
	 */
	static bool Sample(
		const UImpactorCatalog& Catalog, const FImpactDistributions& Distributions, float CharacterMassKg,
		int32 BatchSeed, int32 ClipIndex, FImpactParams& OutParams, FText& OutReason);
};
