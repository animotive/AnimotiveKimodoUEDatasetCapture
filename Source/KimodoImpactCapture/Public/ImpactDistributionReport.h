// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ImpactDistributionReport.generated.h"

class FImpactBatchManifest;

/**
 * What a batch actually produced, counted from its manifest, so imbalance is visible before
 * training. Category counts are over valid clips, the ones the dataset keeps; invalid clips are
 * counted by reason.
 */
USTRUCT()
struct KIMODOIMPACTCAPTURE_API FImpactDistributionReport
{
	GENERATED_BODY()

	/** Every clip the manifest records, valid or not. */
	UPROPERTY()
	int32 Total = 0;

	UPROPERTY()
	int32 Valid = 0;

	UPROPERTY()
	int32 Invalid = 0;

	UPROPERTY()
	TMap<FString, int32> Regions;

	UPROPERTY()
	TMap<FString, int32> Intensities;

	UPROPERTY()
	TMap<FString, int32> DeliveryModes;

	UPROPERTY()
	TMap<FString, int32> Outcomes;

	UPROPERTY()
	TMap<FString, int32> SourceAnimations;

	UPROPERTY()
	TMap<FString, int32> Impactors;

	UPROPERTY()
	TMap<FString, int32> InvalidReasons;

	/** Mean frames per valid clip. */
	UPROPERTY()
	float MeanFrames = 0.0f;

	/** Bytes on disk over every clip that was kept. */
	UPROPERTY()
	int64 Bytes = 0;

	static FImpactDistributionReport Build(const FImpactBatchManifest& Manifest);

	/** Pretty JSON of every count. */
	FString ToJson() const;

	/** Writes ToJson() to Filename. */
	bool Save(const FString& Filename, FText& OutReason) const;

	/** A readable summary for the log: each category with counts and shares, largest first. */
	FString ToText() const;
};
