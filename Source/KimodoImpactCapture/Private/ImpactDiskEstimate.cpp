// Copyright Animotive. All Rights Reserved.

#include "ImpactDiskEstimate.h"

#include "HAL/PlatformMisc.h"
#include "ImpactBatchManifest.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "ImpactDiskEstimate"

namespace
{
	FText Gigabytes(int64 Bytes)
	{
		FNumberFormattingOptions Options;
		Options.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
		return FText::Format(LOCTEXT("GB", "{0} GB"), FText::AsNumber(double(Bytes) / (1024.0 * 1024.0 * 1024.0), &Options));
	}
}

int64 FImpactDiskEstimate::BytesPerClip(int32 Bones, int32 MaxFrames)
{
	return int64(FMath::Max(0, Bones)) * int64(FMath::Max(0, MaxFrames)) * BytesPerBoneFrame;
}

int64 FImpactDiskEstimate::PerClip(const FImpactBatchManifest& Manifest, int32 Bones, int32 MaxFrames)
{
	int64 Bytes = 0;
	int32 Kept = 0;
	for (const FImpactManifestEntry& Entry : Manifest.GetEntries())
	{
		// A discarded clip took no disk and says nothing about how big a clip is.
		if (Entry.Bytes > 0)
		{
			Bytes += Entry.Bytes;
			++Kept;
		}
	}
	return Kept > 0 ? Bytes / Kept : BytesPerClip(Bones, MaxFrames);
}

int64 FImpactDiskEstimate::Estimate(int32 PendingClips, int64 PerClipBytes)
{
	return int64(FMath::Max(0, PendingClips)) * PerClipBytes;
}

bool FImpactDiskEstimate::ShouldWarn(int64 EstimateBytes, int64 FreeBytes, FText& OutWarning)
{
	if (FreeBytes < 0 || EstimateBytes <= FreeBytes)
	{
		return false;
	}
	OutWarning = FText::Format(LOCTEXT("WontFit", "The batch needs about {0} but the drive has {1} free. Free space, lower the clip count, or run it in shards on other drives."),
		Gigabytes(EstimateBytes), Gigabytes(FreeBytes));
	return true;
}

int64 FImpactDiskEstimate::FreeBytes(const FString& Path)
{
	uint64 Total = 0;
	uint64 Free = 0;
	return FPlatformMisc::GetDiskTotalAndFreeSpace(FPaths::ConvertRelativePathToFull(Path), Total, Free) ? int64(Free) : -1;
}

#undef LOCTEXT_NAMESPACE
