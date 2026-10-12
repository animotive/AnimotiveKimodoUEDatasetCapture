// Copyright Animotive. All Rights Reserved.

#include "ImpactClipWindow.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactClipWindow)

// Red: stubs returning neutral values so the tests compile and fail.

bool FImpactClipWindowConfig::Validate(FText& OutReason) const
{
	return true;
}

FImpactClipWindow::FImpactClipWindow(const FImpactClipWindowConfig& InConfig)
	: Config(InConfig)
{
}

EImpactClipPhase FImpactClipWindow::Advance(float BodySpeedCmPerSecond)
{
	return Phase;
}

void FImpactClipWindow::Invalidate(const FText& Reason)
{
}

EImpactClipPhase FImpactClipWindow::PhaseOfNextFrame() const
{
	return Phase;
}

bool FImpactClipWindow::IsNextFrameFirstHit() const
{
	return false;
}
