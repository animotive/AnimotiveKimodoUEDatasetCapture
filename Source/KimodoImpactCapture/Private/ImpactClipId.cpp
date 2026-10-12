// Copyright Animotive. All Rights Reserved.

#include "ImpactClipId.h"

#include "ImpactParams.h"
#include "ImpactSchedule.h"

// Red: stubs returning neutral values so the tests compile and fail.

uint64 FImpactClipId::Fnv1a64(TConstArrayView<uint8> Bytes)
{
	return 0;
}

FString FImpactClipId::CanonicalString(FName SourceAnimation, const FImpactParams& Params, const FImpactSchedule& Schedule)
{
	return FString();
}

FString FImpactClipId::Make(FName SourceAnimation, const FImpactParams& Params, const FImpactSchedule& Schedule)
{
	return FString();
}
