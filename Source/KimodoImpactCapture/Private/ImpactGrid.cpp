// Copyright Animotive. All Rights Reserved.

#include "ImpactGrid.h"

#include "ImpactBatchDefinition.h"
#include "ImpactorCatalog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactGrid)

bool FImpactGridCell::operator==(const FImpactGridCell& Other) const
{
	return Source == Other.Source && Archetype == Other.Archetype && Region == Other.Region && Azimuth == Other.Azimuth
		&& Elevation == Other.Elevation && Strength == Other.Strength && Phase == Other.Phase;
}

bool FImpactGrid::Build(const UImpactBatchDefinition& Batch, const UImpactorCatalog& Catalog, int32 NumSources, FText& OutReason)
{
	return false;
}

int32 FImpactGrid::Num() const
{
	return 0;
}

FImpactGridCell FImpactGrid::Cell(int32 Index) const
{
	return FImpactGridCell();
}

FImpactParams FImpactGrid::Params(const FImpactGridCell& Cell, const UImpactBatchDefinition& Batch, const UImpactorCatalog& Catalog)
{
	return FImpactParams();
}
