// Copyright Animotive. All Rights Reserved.

#include "ImpactSidecarBuilder.h"

#include "ImpactClipSimulator.h"

FDatasetSidecar FImpactSidecarBuilder::Build(const FImpactClipSetup& Setup, const FImpactClipRecording& Recording, const FImpactSidecarContext& Context)
{
	return FDatasetSidecar();
}

FImpactCaptionInput FImpactSidecarBuilder::CaptionInput(const FImpactClipSetup& Setup, const FImpactClipRecording& Recording, const FImpactSourceMotion& Motion)
{
	return FImpactCaptionInput();
}
