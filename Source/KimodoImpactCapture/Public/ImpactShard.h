// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** A contiguous run of clip indices, [Start, Start + Count). */
struct KIMODOIMPACTCAPTURE_API FImpactShardRange
{
	int32 Start = 0;
	int32 Count = 0;

	int32 End() const { return Start + Count; }
	bool operator==(const FImpactShardRange& Other) const { return Start == Other.Start && Count == Other.Count; }
};

/**
 * Splitting a batch across processes with -Start/-Count. Each process writes its own manifest file,
 * so no two processes ever append to the same file; resuming reads them all.
 */
struct KIMODOIMPACTCAPTURE_API FImpactSharding
{
	/**
	 * Shards contiguous ranges covering [0, ClipCount) with no gap or overlap, sizes differing by at
	 * most one. Never an empty range: fewer clips than shards gives one range per clip.
	 */
	static TArray<FImpactShardRange> Split(int32 ClipCount, int32 Shards);

	/**
	 * The range a process runs from its -Start and -Count. A negative Count means to the end of the
	 * batch; a Count past the end stops at it. False with a reason when Start is outside the batch.
	 */
	static bool Resolve(int32 ClipCount, int32 Start, int32 Count, FImpactShardRange& OutRange, FText& OutReason);

	/** The manifest file one range's process appends to, inside the batch's folder. */
	static FString ManifestFilename(const FString& BatchFolder, const FImpactShardRange& Range);
};
