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
