// Copyright Animotive. All Rights Reserved.

#include "ImpactShard.h"

#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "ImpactShard"

TArray<FImpactShardRange> FImpactSharding::Split(int32 ClipCount, int32 Shards)
{
	TArray<FImpactShardRange> Ranges;
	if (ClipCount <= 0)
	{
		return Ranges;
	}
	const int32 Count = FMath::Clamp(Shards, 1, ClipCount);
	const int32 Base = ClipCount / Count;
	const int32 Extra = ClipCount % Count;
	int32 Start = 0;
	for (int32 Shard = 0; Shard < Count; ++Shard)
	{
		// The first Extra shards take one more clip each.
		const int32 Size = Base + (Shard < Extra ? 1 : 0);
		Ranges.Add({ Start, Size });
		Start += Size;
	}
	return Ranges;
}

bool FImpactSharding::Resolve(int32 ClipCount, int32 Start, int32 Count, FImpactShardRange& OutRange, FText& OutReason)
{
	if (Start < 0 || Start >= ClipCount)
	{
		OutReason = FText::Format(LOCTEXT("StartOutside", "-Start={0} is outside the batch, which has clips 0 to {1}."),
			FText::AsNumber(Start), FText::AsNumber(ClipCount - 1));
		return false;
	}
	const int32 Remaining = ClipCount - Start;
	OutRange = { Start, Count < 0 ? Remaining : FMath::Min(Count, Remaining) };
	return true;
}

FString FImpactSharding::ManifestFilename(const FString& BatchFolder, const FImpactShardRange& Range)
{
	return BatchFolder / FString::Printf(TEXT("Manifest_%d-%d.jsonl"), Range.Start, Range.End());
}

#undef LOCTEXT_NAMESPACE
