// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * A game world to simulate impact clips in, one per clip. Each rule here was learned the hard way
 * (tech-stack.md):
 *  - one world per clip: clips sharing a world inherited each other's physics state. The Phase 1
 *    spike's crash on recreating worlds (the Water world subsystem) no longer occurs;
 *  - a bare world has no game mode, so BeginPlay is dispatched by hand or components never tick;
 *  - every simulated step is its own engine frame (GFrameCounter), or tick functions run once and stop;
 *  - teardown is EndPlay, then tear down, then destroy, then collect garbage.
 */
class KIMODOIMPACTCAPTURE_API FImpactSimWorld
{
public:
	/** Creates the world and a floor whose top surface is at GroundHeight. */
	explicit FImpactSimWorld(float InGroundHeight = 0.0f);
	~FImpactSimWorld();

	FImpactSimWorld(const FImpactSimWorld&) = delete;
	FImpactSimWorld& operator=(const FImpactSimWorld&) = delete;

	UWorld* GetWorld() const { return World; }
	float GetGroundHeight() const { return GroundHeight; }

	/**
	 * Simulates one fixed step of DeltaSeconds as Substeps engine frames of DeltaSeconds / Substeps
	 * each (at least one).
	 */
	void Step(float DeltaSeconds, int32 Substeps = 1);

private:
	UWorld* World = nullptr;
	float GroundHeight = 0.0f;
};
