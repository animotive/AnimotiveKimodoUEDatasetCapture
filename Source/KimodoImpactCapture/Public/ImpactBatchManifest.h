// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ImpactBatchManifest.generated.h"

/** One finished clip as the batch manifest records it: enough to resume, de-duplicate and report. */
USTRUCT()
struct KIMODOIMPACTCAPTURE_API FImpactManifestEntry
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ClipIndex = INDEX_NONE;

	UPROPERTY()
	FString ClipId;

	/** Object path of the baked clip; empty when an invalid clip was discarded. */
	UPROPERTY()
	FString AnimSequencePath;

	UPROPERTY()
	FString SourceAnimation;

	UPROPERTY()
	FString ImpactorId;

	UPROPERTY()
	FString Region;

	UPROPERTY()
	FString Intensity;

	UPROPERTY()
	FString DeliveryMode;

	UPROPERTY()
	FString Outcome;

	UPROPERTY()
	bool bValid = true;

	UPROPERTY()
	FString InvalidReason;

	UPROPERTY()
	int32 Frames = 0;

	/** Bytes the clip took on disk (asset and sidecar), for the disk estimate. */
	UPROPERTY()
	int64 Bytes = 0;
};

/**
 * The record of which clips of a batch are finished. An append-only log, one JSON entry per line,
 * written and flushed after each clip, so a crash loses at most the clip in flight. A clip whose
 * files exist but whose entry does not was interrupted and is made again.
 */
class KIMODOIMPACTCAPTURE_API FImpactBatchManifest
{
public:
	/**
	 * Reads the log at Filename and makes it the file Append writes to. A missing file is an empty
	 * manifest. A last line cut short by a crash is dropped, and the file is rewritten without it
	 * (to a temporary file, then moved over) so the next entry starts on a line of its own. Lines
	 * that do not parse, and repeats of a clip index already read, are dropped and counted.
	 */
	bool Load(const FString& Filename, FText& OutReason);

	/** Records Entry in memory and appends it to the file. On disk before this returns. */
	bool Append(const FImpactManifestEntry& Entry, FText& OutReason);

	bool Contains(int32 ClipIndex) const;
	const FImpactManifestEntry* Find(int32 ClipIndex) const;
	const TArray<FImpactManifestEntry>& GetEntries() const { return Entries; }
	int32 Num() const { return Entries.Num(); }

	/** Clip indices in [Start, Start + Count) that have no entry yet, ascending. */
	TArray<int32> Pending(int32 Start, int32 Count) const;

	/** Lines dropped by the last Load: cut short, unreadable, or a repeated index. */
	int32 GetDroppedLines() const { return DroppedLines; }

	const FString& GetFilename() const { return Filename; }

	/**
	 * Adds Other's entries for clips this manifest does not have, in memory only. Returns how many
	 * of Other's entries were repeats and skipped.
	 */
	int32 Merge(const FImpactBatchManifest& Other);

	/**
	 * Every manifest file in Folder (every shard's), merged into one. Out is a read-only view: it
	 * has no file to append to. Repeats across shards count once.
	 */
	static bool LoadAll(const FString& Folder, FImpactBatchManifest& Out, FText& OutReason);

	/** Writes every entry to Filename as one manifest, through a temporary file. */
	bool SaveAs(const FString& Filename, FText& OutReason) const;

	/** Manifest files are named Manifest*.jsonl so LoadAll finds every shard's. */
	static const TCHAR* FilePattern;

	/** One entry as a single line of JSON, without the line break. */
	static FString ToLine(const FImpactManifestEntry& Entry);
	static bool FromLine(const FString& Line, FImpactManifestEntry& OutEntry);

private:
	FString Filename;
	TArray<FImpactManifestEntry> Entries;
	TMap<int32, int32> IndexToEntry;
	int32 DroppedLines = 0;
};
