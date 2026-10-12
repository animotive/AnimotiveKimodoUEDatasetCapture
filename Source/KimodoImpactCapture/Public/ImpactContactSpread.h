// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FImpactRandom;

/**
 * Where an impactor meets the struck body. An impactor travels along a direction, aimed at a point
 * off the body's centre; it lands where that line meets the body's surface on the side facing it.
 * An off-centre contact twists the body as well as pushing it.
 */
struct KIMODOIMPACTCAPTURE_API FImpactSurfaceContact
{
	/**
	 * Random aim offset perpendicular to Direction, uniform over a disc of AimFraction x Radius.
	 * Deterministic for a given random stream.
	 */
	static FVector AimOffset(const FVector& Direction, float RadiusCm, float AimFraction, FImpactRandom& Random);

	/**
	 * Contact point on a sphere (Center, RadiusCm) for an impactor travelling along Direction
	 * aimed at Center + Offset. An offset beyond the radius is clamped to graze the surface.
	 */
	static FVector Contact(const FVector& Center, float RadiusCm, const FVector& Direction, const FVector& Offset);

	/** Torque an impulse applied at Point produces about CenterOfMass. */
	static FVector Torque(const FVector& Point, const FVector& CenterOfMass, const FVector& Impulse);
};

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
