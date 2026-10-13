// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "ImpactGrid.h"
#include "ImpactParams.h"
#include "ImpactSchedule.h"
#include "ImpactStrengthProfile.h"

#include "ImpactBatchDefinition.generated.h"

class UAnimSequence;
class UImpactRegionTable;
class UImpactSourceMotionTable;
class UImpactorCatalog;
class UPhysicsAsset;
class USkeletalMesh;
class USkeleton;
struct FAssetData;

/**
 * One batch of synthetic impact clips, described once and run unattended. Every clip in the
 * batch is a pure function of (Seed, clip index) and this asset.
 */
UCLASS(BlueprintType)
class KIMODOIMPACTCAPTURE_API UImpactBatchDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// --- Source animations ----------------------------------------------------------------------

	/** Animations always included (still filtered by skeleton). */
	UPROPERTY(EditAnywhere, Category = "Source")
	TArray<TSoftObjectPtr<UAnimSequence>> ExplicitAnimations;

	/** Content folders searched recursively for animation sequences, e.g. /Game/Characters/UEFN_Mannequin/Animations/Run. */
	UPROPERTY(EditAnywhere, Category = "Source", meta = (ContentDir))
	TArray<FDirectoryPath> AnimationFolders;

	/** Only sequences on this skeleton are used: clips bake onto it. */
	UPROPERTY(EditAnywhere, Category = "Source")
	TSoftObjectPtr<USkeleton> RequiredSkeleton;

	/** When not empty, an asset name must contain at least one of these (case-insensitive). */
	UPROPERTY(EditAnywhere, Category = "Source")
	TArray<FString> NameIncludes;

	/** An asset name containing any of these is skipped (case-insensitive). */
	UPROPERTY(EditAnywhere, Category = "Source")
	TArray<FString> NameExcludes;

	/**
	 * A source whose name contains any of these plays once and holds its last frame rather than
	 * looping (case-insensitive). A jump wrapped back to its take-off mid-reaction.
	 */
	UPROPERTY(EditAnywhere, Category = "Source")
	TArray<FString> PlayOnceNames = { TEXT("Jump") };

	/** How each source's motion is worded in captions; unlisted sources are worded from their names. */
	UPROPERTY(EditAnywhere, Category = "Source")
	TSoftObjectPtr<UImpactSourceMotionTable> MotionTable;

	// --- Character -------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Character")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	UPROPERTY(EditAnywhere, Category = "Character")
	TSoftObjectPtr<UPhysicsAsset> PhysicsAsset;

	UPROPERTY(EditAnywhere, Category = "Character")
	TSoftObjectPtr<UImpactRegionTable> RegionTable;

	UPROPERTY(EditAnywhere, Category = "Character")
	TSoftObjectPtr<UImpactorCatalog> Catalog;

	/** The struck person's mass, for the momentum an impactor transfers. */
	UPROPERTY(EditAnywhere, Category = "Character", meta = (ClampMin = "1"))
	float CharacterMassKg = 75.0f;

	// --- Sampling --------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Sampling")
	FImpactDistributions Impact;

	UPROPERTY(EditAnywhere, Category = "Sampling")
	FImpactDeliveryDistributions Delivery;

	UPROPERTY(EditAnywhere, Category = "Sampling")
	FImpactStrengthDistribution Strength;

	/**
	 * Every combination instead of a random draw: each source x each impactor x each region it can
	 * strike x the grid's directions, strengths and phases. ClipCount is then the grid's size.
	 */
	UPROPERTY(EditAnywhere, Category = "Sampling")
	bool bFullGrid = false;

	UPROPERTY(EditAnywhere, Category = "Sampling", meta = (EditCondition = "bFullGrid"))
	FImpactGridSettings Grid;

	// --- Timing ----------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "1"))
	int32 FrameRate = 60;

	/** Frames of exact source motion before the first hit. */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "0"))
	int32 PreRollFrames = 30;

	/** Ceiling on frames after the first hit; reaching it is recorded, not silent. */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "1"))
	int32 MaxReactionFrames = 240;

	/** Frames a settled or recovered character must hold before the clip ends. */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "1"))
	int32 SettleFrames = 20;

	// --- Run ---------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Run")
	int32 Seed = 1;

	UPROPERTY(EditAnywhere, Category = "Run", meta = (ClampMin = "1"))
	int32 ClipCount = 1000;

	/** Content path the baked clips are written under, one folder per batch. */
	UPROPERTY(EditAnywhere, Category = "Run")
	FString OutputRoot = TEXT("/Game/KimodoImpactDataset");

	/** Invalid clips are always recorded in the manifest with their reason; discarded ones are not baked. */
	UPROPERTY(EditAnywhere, Category = "Run")
	bool bDiscardInvalid = false;

	/**
	 * True when the batch can run. Checks settings only; asset contents are validated by their
	 * own types when loaded. OutReason says what to fix.
	 */
	bool ValidateSettings(FText& OutReason) const;
};

/** Turns a batch's source settings into the ordered list of animations it draws from. */
struct KIMODOIMPACTCAPTURE_API FImpactSourceResolver
{
	/**
	 * Pure filter over candidate assets: animation sequences only, on the required skeleton,
	 * passing the name filters; de-duplicated and sorted by object path so clip N always draws
	 * from the same list.
	 */
	static TArray<FSoftObjectPath> Filter(const UImpactBatchDefinition& Batch, TConstArrayView<FAssetData> Candidates);

	/**
	 * Queries the asset registry for the batch's folders and explicit animations, then filters.
	 * False with a stated reason when nothing qualifies.
	 */
	static bool Resolve(const UImpactBatchDefinition& Batch, TArray<FSoftObjectPath>& OutAnimations, FText& OutReason);
};
