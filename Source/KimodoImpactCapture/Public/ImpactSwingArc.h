// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Path of a stepping foot. */
struct KIMODOIMPACTCAPTURE_API FImpactSwingArc
{
	/**
	 * Foot position at Phase (clamped to 0..1) of a swing from From to To. Horizontal progress
	 * eases in and out, so the foot leaves and lands with no horizontal speed; height blends the
	 * two ends and adds LiftCm x sin(pi x Phase), peaking at mid-swing.
	 */
	static FVector Evaluate(const FVector& From, const FVector& To, float Phase, float LiftCm);
};
