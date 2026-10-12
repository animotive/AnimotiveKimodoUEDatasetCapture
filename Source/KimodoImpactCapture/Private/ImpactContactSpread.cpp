// Copyright Animotive. All Rights Reserved.

#include "ImpactContactSpread.h"

// Red: stub returning a neutral value so the tests compile and fail.

TArray<FImpactBodyShare> FImpactContactSpread::Distribute(
	TConstArrayView<FImpactBodyProxy> Bodies, int32 StruckIndex, const FVector& ContactPoint,
	float ContactRadiusCm, float TotalImpulseKgCmPerSecond)
{
	return {};
}
