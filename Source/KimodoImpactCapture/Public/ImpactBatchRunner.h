// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "ImpactBatchDefinition.h"
#include "ImpactBatchManifest.h"
#include "ImpactCaption.h"
#include "ImpactDistributionReport.h"
#include "ImpactGrid.h"
#include "ImpactRegionTable.h"
#include "ImpactShard.h"
#include "ImpactorCatalog.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/StrongObjectPtr.h"

class FImpactPoseBuffer;
class FImpactSimWorld;
struct FImpactClipRecording;
struct FImpactClipSetup;

/**
 * Everything a batch's clips are drawn from, loaded once and held strongly: simulation ticks can
 * collect unreferenced objects mid-run (the Phase 3 review lost its catalog that way).
 */
struct KIMODOIMPACTCAPTURE_API FImpactBatchAssets
{
	TStrongObjectPtr<USkeletalMesh> Mesh;
	TStrongObjectPtr<UPhysicsAsset> PhysicsAsset;
	TStrongObjectPtr<UImpactRegionTable> RegionTable;
	TStrongObjectPtr<UImpactorCatalog> Catalog;
	/** Optional; source motion is worded from names without it. */
	TStrongObjectPtr<UImpactSourceMotionTable> MotionTable;
	/** The batch's resolved sources, in FImpactSourceResolver order. */
	TArray<TStrongObjectPtr<UAnimSequence>> Sources;
	/** Per source: plays once and holds its last frame (UImpactBatchDefinition::PlayOnceNames). */
	TArray<bool> SourcePlaysOnce;
	/** The full grid's layout, when the batch is a full grid. */
	FImpactGrid Grid;

	/** Clips in the batch: the grid's size for a full grid, else ClipCount. */
	int32 ClipCount(const UImpactBatchDefinition& Batch) const;

	/** Resolves and loads the batch's assets. False with a reason when any cannot be loaded. */
	static bool Load(const UImpactBatchDefinition& Batch, FImpactBatchAssets& Out, FText& OutReason);

	/** True when Name matches one of the batch's play-once names. */
	static bool PlaysOnce(const UImpactBatchDefinition& Batch, const FString& Name);
};

/** One clip's setup and identity, a pure function of the batch, its assets and the clip index. */
struct KIMODOIMPACTCAPTURE_API FImpactClipFactory
{
	/**
	 * Draws the clip's source (from the source-animation stream), its impact, schedule and
	 * strength profile, and copies the batch's timing. False with a reason when the batch's
	 * distributions cannot be sampled.
	 */
	static bool Make(const UImpactBatchDefinition& Batch, const FImpactBatchAssets& Assets, int32 ClipIndex,
		FImpactClipSetup& OutSetup, FString& OutClipId, FText& OutReason);
};

/** Turns a recording into component-space bone poses for the bake: bodies rebuilt down the chain, then leg IK. */
struct KIMODOIMPACTCAPTURE_API FImpactClipPoser
{
	static void Build(const FReferenceSkeleton& Ref, const FImpactClipRecording& Recording, const FVector& MeshForward, FImpactPoseBuffer& OutPoses);
};

struct KIMODOIMPACTCAPTURE_API FImpactBatchRunConfig
{
	/** -Start / -Count; a negative Count runs to the end of the batch. */
	int32 Start = 0;
	int32 Count = -1;
	/** Collect garbage after this many clips, so a long batch's memory stays flat. */
	int32 CollectGarbageEveryClips = 25;
};

/** What a run has done so far, for logs and the panel's live counters. */
struct KIMODOIMPACTCAPTURE_API FImpactBatchRunStats
{
	/** Clips in this run's range already finished before it started. */
	int32 Skipped = 0;
	int32 Done = 0;
	int32 Invalid = 0;
	int32 Pending = 0;
	TMap<FString, int32> Outcomes;
};

/**
 * Runs a batch's pending clips one at a time: set up, simulate, bake, write the sidecar, record
 * in the manifest. The headless commandlet and the editor panel both drive it.
 */
class KIMODOIMPACTCAPTURE_API FImpactBatchRunner
{
public:
	FImpactBatchRunner();
	~FImpactBatchRunner();

	/**
	 * Validates and loads the batch, opens this range's manifest, and works out which clips are
	 * left. False with a reason when it cannot run. A disk shortfall is not a failure: it is in
	 * GetDiskWarning() for the caller to decide on.
	 */
	bool Begin(UImpactBatchDefinition& Batch, const FImpactBatchRunConfig& Config, FText& OutReason);

	/** Runs the next pending clip. False when there was none, or with a reason when the run must stop. */
	bool Step(FText& OutReason);

	bool IsDone() const;

	/** Writes the distribution report over every shard's manifest. */
	bool Finish(FText& OutReason);

	const FImpactBatchRunStats& GetStats() const { return Stats; }
	const FText& GetDiskWarning() const { return DiskWarning; }
	const FImpactBatchManifest& GetManifest() const { return Manifest; }
	const FImpactDistributionReport& GetReport() const { return Report; }

	/** The batch's folder on disk: OutputRoot/BatchName, where clips, sidecars, manifests and the report live. */
	const FString& GetBatchFolder() const { return BatchFolder; }

	/** Long package name of the batch's output folder (OutputRoot/BatchName). */
	static FString BatchPackagePath(const UImpactBatchDefinition& Batch);

	/** The distribution report's file in a batch folder. */
	static FString ReportFilename(const FString& BatchFolder);

private:
	TStrongObjectPtr<UImpactBatchDefinition> Batch;
	FImpactBatchAssets Assets;
	FImpactBatchRunConfig Config;
	FImpactShardRange Range;
	FImpactBatchManifest Manifest;
	FImpactDistributionReport Report;
	TArray<int32> Pending;
	int32 NextPending = 0;
	int32 SinceGarbage = 0;
	FImpactBatchRunStats Stats;
	FText DiskWarning;
	FString BatchFolder;
	TUniquePtr<FImpactSimWorld> World;
};
