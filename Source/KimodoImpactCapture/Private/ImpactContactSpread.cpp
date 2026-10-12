// Copyright Animotive. All Rights Reserved.

#include "ImpactContactSpread.h"

#include "ImpactRandom.h"

// Red: stubs returning neutral values so the tests compile and fail.

FVector FImpactSurfaceContact::AimOffset(const FVector& Direction, float RadiusCm, float AimFraction, FImpactRandom& Random)
{
	return FVector(1, 1, 1) * RadiusCm * 10.0f;
}

FVector FImpactSurfaceContact::Contact(const FVector& Center, float RadiusCm, const FVector& Direction, const FVector& Offset)
{
	return FVector::ZeroVector;
}

FVector FImpactSurfaceContact::Torque(const FVector& Point, const FVector& CenterOfMass, const FVector& Impulse)
{
	return FVector::ZeroVector;
}

TArray<FImpactBodyShare> FImpactContactSpread::Distribute(
	TConstArrayView<FImpactBodyProxy> Bodies, int32 StruckIndex, const FVector& ContactPoint,
	float ContactRadiusCm, float TotalImpulseKgCmPerSecond)
{
	TArray<FImpactBodyShare> Shares;
	if (!Bodies.IsValidIndex(StruckIndex) || !(TotalImpulseKgCmPerSecond > 0.0f))
	{
		return Shares;
	}
	const float Radius = FMath::Max(0.0f, ContactRadiusCm);

	TArray<float> Weights;
	float TotalWeight = 0.0f;
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
	{
		const FImpactBodyProxy& Body = Bodies[Index];
		const FVector ToContact = ContactPoint - Body.Center;
		const float SurfaceDistance = FMath::Max(0.0f, float(ToContact.Size()) - Body.RadiusCm);

		float Weight = 0.0f;
		if (Index == StruckIndex)
		{
			// The struck body is where the impactor landed: full weight, at the contact itself.
			Weight = 1.0f;
		}
		else if (Radius > 0.0f && SurfaceDistance < Radius)
		{
			Weight = FMath::Square(1.0f - SurfaceDistance / Radius);
		}
		if (Weight <= 0.0f)
		{
			continue;
		}

		FImpactBodyShare Share;
		Share.Name = Body.Name;
		Share.ApplyPoint = (Index == StruckIndex || SurfaceDistance <= 0.0f)
			? ContactPoint
			: Body.Center + ToContact.GetSafeNormal() * Body.RadiusCm;
		Shares.Add(Share);
		Weights.Add(Weight);
		TotalWeight += Weight;
	}

	// Normalise; the last share takes the remainder so the spread conserves the impulse exactly.
	float Assigned = 0.0f;
	for (int32 Index = 0; Index < Shares.Num(); ++Index)
	{
		Shares[Index].ImpulseKgCmPerSecond = Index + 1 < Shares.Num()
			? TotalImpulseKgCmPerSecond * Weights[Index] / TotalWeight
			: TotalImpulseKgCmPerSecond - Assigned;
		Assigned += Shares[Index].ImpulseKgCmPerSecond;
	}
	return Shares;
}
