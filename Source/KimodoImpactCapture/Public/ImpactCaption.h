// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ImpactOutcome.h"
#include "ImpactParams.h"

#include "ImpactCaption.generated.h"

struct FImpactorArchetype;

/** How a source animation's motion is worded: "running", "standing idle"; and its tag, "run". */
USTRUCT(BlueprintType)
struct KIMODOIMPACTCAPTURE_API FImpactSourceMotion
{
	GENERATED_BODY()

	/** Participle phrase used in captions: "running", "walking", "standing idle". */
	UPROPERTY(EditAnywhere, Category = "Caption")
	FString Participle;

	/** Short motion type used as a tag: "run", "walk", "idle". */
	UPROPERTY(EditAnywhere, Category = "Caption")
	FString Type;

	/** True when the motion travels (a run or walk); the caption may then say it keeps going. */
	UPROPERTY(EditAnywhere, Category = "Caption")
	bool bTravels = false;
};

USTRUCT(BlueprintType)
struct KIMODOIMPACTCAPTURE_API FImpactSourceMotionEntry
{
	GENERATED_BODY()

	/** Source animation asset name, e.g. M_Neutral_Run_Loop_F. */
	UPROPERTY(EditAnywhere, Category = "Caption")
	FName Animation;

	UPROPERTY(EditAnywhere, Category = "Caption")
	FImpactSourceMotion Motion;
};

/**
 * How each source animation's motion is worded in captions. Data, so a project can describe its
 * own animations; an animation missing from the table is worded from its name.
 */
UCLASS(BlueprintType)
class KIMODOIMPACTCAPTURE_API UImpactSourceMotionTable : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Caption")
	TArray<FImpactSourceMotionEntry> Entries;
};

/** Everything a caption is written from. */
struct KIMODOIMPACTCAPTURE_API FImpactCaptionInput
{
	const FImpactorArchetype* Impactor = nullptr;
	FImpactSourceMotion Motion;
	EImpactBodyRegion Region = EImpactBodyRegion::Chest;
	/** Direction the impactor travels, character space (X forward, Y right, Z up). */
	FVector DirectionLocal = FVector::BackwardVector;
	EImpactIntensity Intensity = EImpactIntensity::Medium;
	EImpactOutcome Outcome = EImpactOutcome::Flinch;
	bool bTrip = false;
	int32 Steps = 0;
};

/**
 * The human-language description and tags of a clip, for training: subject, source motion,
 * impactor with its verb, region, direction and outcome, e.g. "A person running is shot in the
 * left foot by a small bullet from the front and trips, falling forwards." Several phrasings per
 * slot, chosen deterministically from the clip's caption stream, so captions are not one fixed
 * sentence shape.
 */
struct KIMODOIMPACTCAPTURE_API FImpactCaption
{
	/** The table's wording for the animation, or else one derived from its name. */
	static FImpactSourceMotion ResolveMotion(const UImpactSourceMotionTable* Table, FName Animation);

	/** Wording derived from an animation's name: Run -> "running", Stand or Idle -> "standing idle". */
	static FImpactSourceMotion MotionFromName(FName Animation);

	/** "the left foot", "the head". */
	static FString RegionPhrase(EImpactBodyRegion Region);

	/** The side the impactor came from: "from the front", "from behind", "from the left", "from above". */
	static FString DirectionPhrase(const FVector& DirectionLocal);

	/** The caption for a clip. Same input and clip, same caption. */
	static FString Describe(const FImpactCaptionInput& Input, int32 BatchSeed, int32 ClipIndex);

	/**
	 * Count different phrasings of the clip (fewer only when the wording runs out), the first its
	 * Describe caption: the descriptions index's "descriptions".
	 */
	static TArray<FString> Descriptions(const FImpactCaptionInput& Input, int32 BatchSeed, int32 ClipIndex, int32 Count = 3);

	/** impact, impactor id, region, outcome, intensity and motion type. */
	static TArray<FString> Tags(const FImpactCaptionInput& Input);
};
