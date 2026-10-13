// Copyright Animotive. All Rights Reserved.

#include "ImpactDistributionReport.h"

#include "ImpactBatchManifest.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactDistributionReport)

FImpactDistributionReport FImpactDistributionReport::Build(const FImpactBatchManifest& Manifest)
{
	return FImpactDistributionReport();
}

FString FImpactDistributionReport::ToJson() const
{
	return FString();
}

bool FImpactDistributionReport::Save(const FString& Filename, FText& OutReason) const
{
	return false;
}

FString FImpactDistributionReport::ToText() const
{
	return FString();
}
