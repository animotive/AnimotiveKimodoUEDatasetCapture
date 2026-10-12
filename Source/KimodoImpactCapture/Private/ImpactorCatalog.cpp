// Copyright Animotive. All Rights Reserved.

#include "ImpactorCatalog.h"

#include "ImpactRandom.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactorCatalog)

// Red: stubs returning neutral values so the tests compile and fail.

float FImpactorSample::ImpulseKgCmPerSecond() const
{
	return 0.0f;
}

TArray<FImpactorArchetype> UImpactorCatalog::MakeStarterArchetypes()
{
	return {};
}

bool UImpactorCatalog::Validate(FText& OutReason) const
{
	return true;
}

const FImpactorArchetype* UImpactorCatalog::SampleArchetype(FImpactRandom& Random) const
{
	return nullptr;
}

FImpactorSample UImpactorCatalog::SampleInstance(const FImpactorArchetype& Archetype, FImpactRandom& Random)
{
	return {};
}
