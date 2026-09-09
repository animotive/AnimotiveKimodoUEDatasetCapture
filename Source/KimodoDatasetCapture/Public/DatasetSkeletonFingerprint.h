// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FReferenceSkeleton;

/** One bone as the fingerprint sees it: a name and the index of its parent. */
struct KIMODODATASETCAPTURE_API FDatasetBoneEntry
{
	FName Name;

	/** Index of the parent bone, or INDEX_NONE for a root. */
	int32 ParentIndex = INDEX_NONE;

	FDatasetBoneEntry() = default;

	FDatasetBoneEntry(FName InName, int32 InParentIndex)
		: Name(InName)
		, ParentIndex(InParentIndex)
	{
	}
};

/**
 * Stable hash of a skeleton's bone hierarchy.
 *
 * A downstream pipeline needs to know whether two clips were recorded on the same rig
 * without opening Unreal, and an asset path alone cannot answer that: a skeleton can keep
 * its path while its hierarchy changes underneath. The fingerprint covers the ordered bone
 * names and their parent indices, which is exactly what makes two recordings comparable.
 *
 * The hash is written into sidecar JSON, so its algorithm is part of the schema. Changing
 * it invalidates every previously captured fingerprint and requires a schema version bump.
 */
class KIMODODATASETCAPTURE_API FDatasetSkeletonFingerprint
{
public:
	/**
	 * Fingerprints an ordered bone list as 16 lowercase hex characters.
	 *
	 * Bone names are hashed case-insensitively, matching FName's own comparison semantics:
	 * two skeletons differing only in the case of a bone name are the same skeleton to the
	 * engine, so they must be the same skeleton to the dataset.
	 *
	 * Order is significant. Two skeletons with the same bones in a different order produce
	 * different fingerprints, because bone order determines how a pose array is interpreted.
	 */
	static FString Compute(TArrayView<const FDatasetBoneEntry> Bones);

	/** Fingerprints an engine reference skeleton. */
	static FString Compute(const FReferenceSkeleton& ReferenceSkeleton);

	/** Extracts the bone list this fingerprint is computed over. */
	static TArray<FDatasetBoneEntry> ExtractBones(const FReferenceSkeleton& ReferenceSkeleton);
};
