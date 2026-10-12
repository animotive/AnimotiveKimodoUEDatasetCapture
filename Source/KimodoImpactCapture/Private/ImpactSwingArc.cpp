// Copyright Animotive. All Rights Reserved.

#include "ImpactSwingArc.h"

FVector FImpactSwingArc::Evaluate(const FVector& From, const FVector& To, float Phase, float LiftCm)
{
	const float Clamped = FMath::Clamp(Phase, 0.0f, 1.0f);
	const float Progress = FMath::SmoothStep(0.0f, 1.0f, Clamped);
	FVector Position = FMath::Lerp(From, To, double(Progress));
	Position.Z += LiftCm * FMath::Sin(PI * Clamped);
	return Position;
}
