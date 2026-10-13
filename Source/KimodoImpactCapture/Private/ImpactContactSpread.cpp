// Copyright Animotive. All Rights Reserved.

#include "ImpactContactSpread.h"

#include "ImpactRandom.h"

FVector FImpactSurfaceContact::AimOffset(const FVector& Direction, float RadiusCm, float AimFraction, FImpactRandom& Random)
{
	// Uniform over the disc: radius by square root, angle uniform.
	const double DiscRadius = FMath::Max(0.0f, RadiusCm * AimFraction);
	const double R = DiscRadius * FMath::Sqrt(Random.NextUnit());
	const double Angle = Random.Range(0.0, 2.0 * UE_DOUBLE_PI);
	FVector Tangent, Bitangent;
	Direction.GetSafeNormal().FindBestAxisVectors(Tangent, Bitangent);
	return Tangent * (R * FMath::Cos(Angle)) + Bitangent * (R * FMath::Sin(Angle));
}

FVector FImpactSurfaceContact::Contact(const FVector& Center, float RadiusCm, const FVector& Direction, const FVector& Offset)
{
	const FVector Travel = Direction.GetSafeNormal();
	// Only the part of the aim across the line of travel matters; clamp it to graze the surface.
	FVector Across = Offset - Travel * FVector::DotProduct(Offset, Travel);
	const double Radius = FMath::Max(0.0f, RadiusCm);
	if (Across.Size() > Radius)
	{
		Across = Across.GetSafeNormal() * Radius;
	}
	// The line meets the sphere first on the side facing the impactor.
	const double Depth = FMath::Sqrt(FMath::Max(0.0, Radius * Radius - Across.SizeSquared()));
	return Center + Across - Travel * Depth;
}

FVector FImpactSurfaceContact::Torque(const FVector& Point, const FVector& CenterOfMass, const FVector& Impulse)
{
	return FVector::CrossProduct(Point - CenterOfMass, Impulse);
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
			// A surface pushes what it touches together: by mass, relative to the struck body, so a
			// light hand beside a struck pelvis moves with it rather than being flung off.
			const float StruckMass = Bodies[StruckIndex].MassKg;
			if (Body.MassKg > 0.0f && StruckMass > 0.0f)
			{
				Weight *= Body.MassKg / StruckMass;
			}
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
