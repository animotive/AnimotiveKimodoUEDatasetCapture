// Copyright Animotive. All Rights Reserved.

#include "ImpactCaption.h"

#include "ImpactorCatalog.h"
#include "ImpactRandom.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactCaption)

FImpactSourceMotion FImpactCaption::ResolveMotion(const UImpactSourceMotionTable* Table, FName Animation)
{
	return FImpactSourceMotion();
}

FImpactSourceMotion FImpactCaption::MotionFromName(FName Animation)
{
	return FImpactSourceMotion();
}

FString FImpactCaption::RegionPhrase(EImpactBodyRegion Region)
{
	return FString();
}

FString FImpactCaption::DirectionPhrase(const FVector& DirectionLocal)
{
	return FString();
}

FString FImpactCaption::Describe(const FImpactCaptionInput& Input, int32 BatchSeed, int32 ClipIndex)
{
	return FString();
}

TArray<FString> FImpactCaption::Tags(const FImpactCaptionInput& Input)
{
	return TArray<FString>();
}
