// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"

/** One generated animation and the sentences that describe it. */
struct KIMODODATASETCAPTURE_API FDatasetDescriptionEntry
{
	/** Unique name of the animation; the key in the file. */
	FString Name;
	/** The animation's file, relative to the descriptions file. */
	FString File;
	/** What generated it ("impact", "climb", ...). */
	FString Kind;
	/** Full sentences describing the motion, each naming its subject ("A person ..."). */
	TArray<FString> Descriptions;

	bool operator==(const FDatasetDescriptionEntry& Other) const;
};

/**
 * The one descriptions.json every synthetic generator in this repo lists its animations in
 * (user, 2026-10-15): an object keyed by animation name, each value
 * { "file": ..., "kind": ..., "descriptions": [...] }, keys written in name order. Generators
 * merge into an existing file, so animations from different generators share one index.
 */
class KIMODODATASETCAPTURE_API FDatasetDescriptions
{
public:
	static const TCHAR* DefaultFilename;

	/** Adds or replaces the entry of the same name. False with a reason when it is incomplete. */
	bool Add(const FDatasetDescriptionEntry& Entry, FText& OutReason);

	/** Removes every entry whose file is under Prefix (a generator replacing its own output); returns how many. */
	int32 RemoveFilesUnder(const FString& Prefix);

	int32 Num() const;
	const FDatasetDescriptionEntry* Find(const FString& Name) const;

	bool ToJson(FString& OutJson) const;

	/** Merges a file's entries in, replacing those of the same name. False with a reason when it is not in the format. */
	bool FromJson(const FString& Json, FText& OutReason);

	/** Writes the file whole, through a temporary file, so a reader never sees half of it. */
	bool SaveToFile(const FString& FilePath, FText& OutReason) const;

	/** Merges a file's entries in. False with a reason when it cannot be read or parsed. */
	bool LoadFromFile(const FString& FilePath, FText& OutReason);

private:
	TMap<FString, FDatasetDescriptionEntry> Entries;
};
