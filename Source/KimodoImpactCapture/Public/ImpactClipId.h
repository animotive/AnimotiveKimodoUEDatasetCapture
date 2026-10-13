// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FImpactParams;
struct FImpactSchedule;

/**
 * Deterministic clip identity. The id is a hash of every parameter that shapes the clip, so the
 * same parameters always give the same id and any change gives a new one. Used for output paths,
 * manifest entries and sidecars; path-safe by construction ("imp_" + 16 lowercase hex digits).
 */
struct KIMODOIMPACTCAPTURE_API FImpactClipId
{
	/** 64-bit FNV-1a over raw bytes. Fully specified, so identical on every platform. */
	static uint64 Fnv1a64(TConstArrayView<uint8> Bytes);

	/**
	 * Every parameter, in a fixed order, floats as exact bit patterns so no rounding can make two
	 * different clips collide. Prefixed with a format version.
	 */
	static FString CanonicalString(FName SourceAnimation, const FImpactParams& Params, const FImpactSchedule& Schedule);

	/** "imp_" + 16 lowercase hex digits of the hash of the canonical string. */
	static FString Make(FName SourceAnimation, const FImpactParams& Params, const FImpactSchedule& Schedule);
};

/**
 * The human-readable name a clip's asset and sidecar files carry: IMP_<index>_<Source>_<Impactor>_
 * <Region>_<Outcome>, e.g. IMP_000007_Sprint_SmallBullet_FootLeft_Knockdown. The index makes it unique
 * and sorts a batch in clip order; FImpactClipId stays the clip's identity in sidecars and manifests.
 */
struct KIMODOIMPACTCAPTURE_API FImpactClipName
{
	/** Name for clip Index. Parts are made path-safe; empty parts are left out. */
	static FString Make(int32 Index, const FString& SourceLabel, const FString& Impactor, const FString& Region, const FString& Outcome);

	/** Every name of clip Index starts with this, whatever its other parts. */
	static FString IndexPrefix(int32 Index);

	/**
	 * A short label from a source animation's asset name: retarget suffix, M_ prefix, the Kimodo
	 * and Loop/F/B tokens and seed suffixes (S1, L4, C1) dropped, the rest joined in PascalCase.
	 * M_Kimodo_Crouch_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin -> CrouchIdle.
	 */
	static FString SourceLabel(const FString& AssetName);
};

