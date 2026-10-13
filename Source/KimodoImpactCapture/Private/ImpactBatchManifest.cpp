// Copyright Animotive. All Rights Reserved.

#include "ImpactBatchManifest.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactBatchManifest)

bool FImpactBatchManifest::Load(const FString& InFilename, FText& OutReason)
{
	Filename = InFilename;
	return true;
}

bool FImpactBatchManifest::Append(const FImpactManifestEntry& Entry, FText& OutReason)
{
	return true;
}

bool FImpactBatchManifest::Contains(int32 ClipIndex) const
{
	return false;
}

const FImpactManifestEntry* FImpactBatchManifest::Find(int32 ClipIndex) const
{
	return nullptr;
}

TArray<int32> FImpactBatchManifest::Pending(int32 Start, int32 Count) const
{
	return {};
}

FString FImpactBatchManifest::ToLine(const FImpactManifestEntry& Entry)
{
	return FString();
}

bool FImpactBatchManifest::FromLine(const FString& Line, FImpactManifestEntry& OutEntry)
{
	return false;
}
