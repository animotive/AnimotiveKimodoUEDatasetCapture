// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * The one game world a process simulates impact clips in. Each rule here was learned the hard way
 * in the Phase 1 spike (tech-stack.md):
 *  - one world per process: recreating a world per clip crashed the Water world subsystem;
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
