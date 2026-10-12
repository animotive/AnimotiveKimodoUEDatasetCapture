// Copyright Animotive. All Rights Reserved.

#include "ImpactRegionTable.h"

#include "PhysicsEngine/PhysicsAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactRegionTable)

// Red: stubs returning neutral values so the tests compile and fail.

TArray<FImpactRegionBodies> UImpactRegionTable::MakeUEFNMannequinEntries()
{
	return {};
}

bool UImpactRegionTable::Validate(const UPhysicsAsset& PhysicsAsset, FText& OutReason) const
{
	return true;
}

TArray<FName> UImpactRegionTable::BodiesFor(EImpactBodyRegion Region) const
{
	return {};
}
