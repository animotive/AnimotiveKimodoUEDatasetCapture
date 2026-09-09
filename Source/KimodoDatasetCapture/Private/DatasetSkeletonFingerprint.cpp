// Copyright Animotive. All Rights Reserved.

#include "DatasetSkeletonFingerprint.h"

#include "ReferenceSkeleton.h"

namespace
{
	// FNV-1a, 64-bit. Chosen because it is short, dependency-free, and above all FIXED:
	// this hash is written into sidecar JSON, so the algorithm is part of the schema. An
	// engine-provided hash could change between versions and silently invalidate every
	// fingerprint already captured.
	constexpr uint64 FnvOffsetBasis = 0xcbf29ce484222325ull;
	constexpr uint64 FnvPrime = 0x100000001b3ull;

	FORCEINLINE uint64 HashByte(uint64 Hash, uint8 Byte)
	{
		return (Hash ^ static_cast<uint64>(Byte)) * FnvPrime;
	}

	uint64 HashInt32(uint64 Hash, int32 Value)
	{
		// Little-endian byte order, stated explicitly rather than inherited from the host,
		// so a fingerprint computed on one platform matches one computed on another.
		const uint32 Unsigned = static_cast<uint32>(Value);
		for (int32 Shift = 0; Shift < 32; Shift += 8)
		{
			Hash = HashByte(Hash, static_cast<uint8>((Unsigned >> Shift) & 0xFF));
		}

		return Hash;
	}

	uint64 HashBoneName(uint64 Hash, FName Name)
	{
		// Lowercased before hashing, so case differences do not change the fingerprint,
		// matching FName's case-insensitive comparison semantics.
		const FString Lowered = Name.ToString().ToLower();

		// Hashed as UTF-8 so a non-ASCII bone name produces the same bytes regardless of
		// the platform's TCHAR width.
		const FTCHARToUTF8 Utf8(*Lowered);
		const uint8* Bytes = reinterpret_cast<const uint8*>(Utf8.Get());
		for (int32 Index = 0; Index < Utf8.Length(); ++Index)
		{
			Hash = HashByte(Hash, Bytes[Index]);
		}

		// Terminator, so ("ab", "c") and ("a", "bc") cannot collide.
		return HashByte(Hash, 0);
	}
}

FString FDatasetSkeletonFingerprint::Compute(TArrayView<const FDatasetBoneEntry> Bones)
{
	uint64 Hash = FnvOffsetBasis;

	// Bone count participates, so a truncated list cannot collide with a shorter skeleton
	// that happens to hash the same prefix.
	Hash = HashInt32(Hash, Bones.Num());

	for (const FDatasetBoneEntry& Bone : Bones)
	{
		Hash = HashBoneName(Hash, Bone.Name);
		Hash = HashInt32(Hash, Bone.ParentIndex);
	}

	// Lowercase, zero-padded to a fixed 16 characters, so downstream comparison is a plain
	// string equality with no normalisation and no variable-length surprises.
	return FString::Printf(TEXT("%016llx"), Hash);
}

FString FDatasetSkeletonFingerprint::Compute(const FReferenceSkeleton& ReferenceSkeleton)
{
	return Compute(ExtractBones(ReferenceSkeleton));
}

TArray<FDatasetBoneEntry> FDatasetSkeletonFingerprint::ExtractBones(const FReferenceSkeleton& ReferenceSkeleton)
{
	const TArray<FMeshBoneInfo>& BoneInfos = ReferenceSkeleton.GetRefBoneInfo();

	TArray<FDatasetBoneEntry> Bones;
	Bones.Reserve(BoneInfos.Num());

	for (const FMeshBoneInfo& BoneInfo : BoneInfos)
	{
		Bones.Emplace(BoneInfo.Name, BoneInfo.ParentIndex);
	}

	return Bones;
}
