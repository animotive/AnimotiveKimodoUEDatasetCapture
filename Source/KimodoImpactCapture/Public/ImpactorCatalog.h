// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Math/Interval.h"

#include "ImpactorCatalog.generated.h"

struct FImpactRandom;

/**
 * How an impact is delivered. Serialised by name in sidecars: enumerators may be added but
 * never renamed or reordered.
 */
/**
 * Where on the body a hit lands. Serialised by name in sidecars and captions: enumerators may be
 * added but never renamed or reordered. Mapped to physics bodies per skeleton by a region table.
 */
UENUM()
enum class EImpactBodyRegion : uint8
{
	Head,
	Neck,
	Chest,
	Abdomen,
	Pelvis,
	UpperArmLeft,
	UpperArmRight,
	LowerArmLeft,
	LowerArmRight,
	HandLeft,
	HandRight,
	ThighLeft,
	ThighRight,
	ShinLeft,
	ShinRight,
	FootLeft,
	FootRight,
};

UENUM()
enum class EImpactDeliveryMode : uint8
{
	/** One instantaneous impulse. */
	Instant,
	/** Several impulses in quick succession (a shotgun blast, a flurry of punches). */
	Burst,
	/** A force applied over a duration (wind, water, a crowd push). */
	Sustained,
	/** A physical object spawned and thrown so that it really collides. */
	Projectile,
};

/** Shape of the impacting object, for projectile spawning and caption wording. */
UENUM()
enum class EImpactorShape : uint8
{
	Sphere,
	Capsule,
	Box,
	/** No single object: a broad contact area such as a vehicle front or a gust. */
	Area,
};

/**
 * What hit the character. Everything a caption needs to name the impactor, and the physical
 * ranges its impulse is derived from. Impulse is never an independent parameter: it is always
 * mass x speed, so a bullet and a car can never share an impulse by accident.
 */
USTRUCT()
struct KIMODOIMPACTCAPTURE_API FImpactorArchetype
{
	GENERATED_BODY()

	/** Stable identifier, written to sidecars and tags. */
	UPROPERTY(EditAnywhere, Category = "Impactor")
	FName Id;

	/** Noun phrase used in captions, with its article: "a small bullet", "a car". */
	UPROPERTY(EditAnywhere, Category = "Impactor")
	FString NounPhrase;

	/** One plain-language line describing the impactor, written to the sidecar. */
	UPROPERTY(EditAnywhere, Category = "Impactor")
	FString Description;

	/** Verbs a caption may use for this impactor: "shot", "hit", "rammed". */
	UPROPERTY(EditAnywhere, Category = "Impactor")
	TArray<FString> Verbs;

	UPROPERTY(EditAnywhere, Category = "Impactor")
	EImpactorShape Shape = EImpactorShape::Sphere;

	/** Characteristic size of the object in centimetres (diameter, length or width). */
	UPROPERTY(EditAnywhere, Category = "Impactor", meta = (ClampMin = "0"))
	float SizeCm = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Impactor")
	FFloatInterval MassKg = FFloatInterval(1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Impactor")
	FFloatInterval SpeedMetresPerSecond = FFloatInterval(1.0f, 1.0f);

	/** Contact radius on the body, in centimetres: point-like for a bullet, wide for a car. */
	UPROPERTY(EditAnywhere, Category = "Impactor")
	FFloatInterval ContactRadiusCm = FFloatInterval(1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Impactor")
	EImpactDeliveryMode DefaultDelivery = EImpactDeliveryMode::Instant;

	/**
	 * Multiplier from transferred momentum to the reaction impulse the simulation applies. A real
	 * bullet transfers almost no momentum; people react to being shot through pain and injury,
	 * not momentum. Recorded with every clip, so physical and reactive impulse stay separable.
	 */
	UPROPERTY(EditAnywhere, Category = "Impactor", meta = (ClampMin = "0"))
	float ReactionGain = 1.0f;

	/**
	 * How long the impactor stays in contact, in seconds; an instant hit is pushed over this long.
	 * A car's contact lasts about a tenth of a second: delivered in one frame, the joints had to
	 * drag the body to full speed instantly and buckled. Zero is a single frame.
	 */
	UPROPERTY(EditAnywhere, Category = "Impactor", meta = (ClampMin = "0"))
	float ContactSeconds = 0.0f;

	/** Frames an instant hit is pushed over at FrameRate: ContactSeconds rounded up, at least one. */
	int32 ContactFrames(int32 FrameRate) const;

	/**
	 * Elevation of the impactor's direction of travel, in degrees: positive travels upward (arrives
	 * from below). Intersected with the batch's range. A car bumper travels level; drawn over the
	 * whole sphere, one hit a thigh from below and launched the body.
	 */
	UPROPERTY(EditAnywhere, Category = "Impactor")
	FFloatInterval ElevationDegrees = FFloatInterval(-90.0f, 90.0f);

	/**
	 * Body regions this impactor can plausibly strike; empty means any. The batch's region weights
	 * still apply within these. A car reaches the hips and legs, not the neck.
	 */
	UPROPERTY(EditAnywhere, Category = "Impactor")
	TArray<EImpactBodyRegion> Regions;

	/** True when the impactor can strike Region. */
	bool CanStrike(EImpactBodyRegion Region) const;

	/** Relative sampling weight within a catalog. Zero excludes the archetype. */
	UPROPERTY(EditAnywhere, Category = "Impactor", meta = (ClampMin = "0"))
	float Weight = 1.0f;
};

/** One concrete impactor drawn from an archetype. */
struct KIMODOIMPACTCAPTURE_API FImpactorSample
{
	const FImpactorArchetype* Archetype = nullptr;
	float MassKg = 0.0f;
	float SpeedMetresPerSecond = 0.0f;
	float ContactRadiusCm = 0.0f;

	/** Momentum carried by the impactor, in engine units (kg x cm/s). Always mass x speed. */
	float ImpulseKgCmPerSecond() const;
};

/**
 * The set of things that can hit the character. A data asset, so a project can extend it
 * without a rebuild; the plugin supplies a starter set.
 */
UCLASS(BlueprintType)
class KIMODOIMPACTCAPTURE_API UImpactorCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Impactors")
	TArray<FImpactorArchetype> Archetypes;

	/**
	 * Adds every starter archetype whose id the catalog does not already have. Never replaces or
	 * removes an entry, so a project's own impactors and edits survive. Shown as a button on the
	 * asset's Details panel. Returns how many were added.
	 */
	UFUNCTION(CallInEditor, Category = "Impactors")
	int32 FillWithStarterArchetypes();

	/** The plugin's starter archetypes, from a small bullet to a wrecking ball. */
	static TArray<FImpactorArchetype> MakeStarterArchetypes();

	/**
	 * True when the catalog can be sampled. On failure, OutReason states what is wrong in
	 * terms the dataset engineer can act on.
	 */
	bool Validate(FText& OutReason) const;

	/** Weighted choice of archetype. Null when the catalog is empty or every weight is zero. */
	const FImpactorArchetype* SampleArchetype(FImpactRandom& Random) const;

	/** Draws mass, speed and contact radius within the archetype's ranges. */
	static FImpactorSample SampleInstance(const FImpactorArchetype& Archetype, FImpactRandom& Random);
};
