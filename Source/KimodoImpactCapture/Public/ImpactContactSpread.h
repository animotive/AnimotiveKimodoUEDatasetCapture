// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** A physics body approximated as a sphere, for contact-area tests. */
struct KIMODOIMPACTCAPTURE_API FImpactBodyProxy
{
	FName Name;
	FVector Center = FVector::ZeroVector;
	float RadiusCm = 0.0f;
};

/** One body's part of a spread impact. */
struct KIMODOIMPACTCAPTURE_API FImpactBodyShare
{
	FName Name;
	float ImpulseKgCmPerSecond = 0.0f;
	/** Where on that body the share is applied: its surface point nearest the contact. */
	FVector ApplyPoint = FVector::ZeroVector;
};

/**
 * Spreads one impact across the bodies inside its contact area. A bullet's point contact loads a
 * single body; a car's wide contact loads every body within its radius. The struck body always
 * takes the largest share, and the shares always sum to the impulse exactly.
 */
struct KIMODOIMPACTCAPTURE_API FImpactContactSpread
{
	/**
	 * Empty when StruckIndex is invalid or the impulse is not positive. Otherwise one share per
	 * body whose surface lies within ContactRadiusCm of ContactPoint, the struck body always
	 * included, weighted by (1 - surface distance / radius) squared.
	 */
	static TArray<FImpactBodyShare> Distribute(
		TConstArrayView<FImpactBodyProxy> Bodies, int32 StruckIndex, const FVector& ContactPoint,
		float ContactRadiusCm, float TotalImpulseKgCmPerSecond);
};
