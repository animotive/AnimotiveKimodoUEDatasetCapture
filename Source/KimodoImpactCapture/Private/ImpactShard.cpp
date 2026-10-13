// Copyright Animotive. All Rights Reserved.

#include "ImpactShard.h"

TArray<FImpactShardRange> FImpactSharding::Split(int32 ClipCount, int32 Shards)
{
	return {};
}

bool FImpactSharding::Resolve(int32 ClipCount, int32 Start, int32 Count, FImpactShardRange& OutRange, FText& OutReason)
{
	return false;
}

FString FImpactSharding::ManifestFilename(const FString& BatchFolder, const FImpactShardRange& Range)
{
	return FString();
}
