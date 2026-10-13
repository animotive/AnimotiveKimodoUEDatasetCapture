// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasetSidecar.h"
#include "ImpactCaption.h"

struct FImpactClipRecording;
struct FImpactClipSetup;

/** What a clip's sidecar needs beyond its setup and recording. */
struct KIMODOIMPACTCAPTURE_API FImpactSidecarContext
{
	/** Deterministic clip id ("imp_" + hash of the clip's parameters). */
	FString ClipId;
	/** Object path of the baked animation. */
	FString AnimSequencePath;
	/** How the source animation's motion is worded. */
	FImpactSourceMotion Motion;
};

/**
 * Fills a v3 synthetic-impact sidecar from one clip: identity, skeleton, caption and tags,
 * validity, and the impact block with every hit. The capture module's schema knows nothing of
 * impact types; this is where they become its plain strings and numbers.
 */
struct KIMODOIMPACTCAPTURE_API FImpactSidecarBuilder
{
	static FDatasetSidecar Build(const FImpactClipSetup& Setup, const FImpactClipRecording& Recording, const FImpactSidecarContext& Context);

	/** The caption generator's input for a clip. */
	static FImpactCaptionInput CaptionInput(const FImpactClipSetup& Setup, const FImpactClipRecording& Recording, const FImpactSourceMotion& Motion);
};
