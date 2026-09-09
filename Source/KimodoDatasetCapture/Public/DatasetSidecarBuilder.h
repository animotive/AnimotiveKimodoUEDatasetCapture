// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasetSidecar.h"
#include "Misc/FrameRate.h"

class UMovieSceneDatasetInputSection;

/**
 * Builds a sidecar FROM a recorded input section.
 *
 * The direction matters and is the whole point of this type. The section, living inside the
 * take's own LevelSequence on the take's own frame clock, is the single source of truth. The
 * sidecar is derived from it at save time. Nothing accumulates a sidecar alongside the
 * section during capture, because two records of the same thing can disagree, and the one
 * that disagrees silently is the one shipped to training.
 */
class KIMODODATASETCAPTURE_API FDatasetSidecarBuilder
{
public:
	/** Everything the sidecar needs that the section itself cannot know. */
	struct FBuildContext
	{
		FString TakeName;
		FString LevelSequencePath;
		FString AnimSequencePath;

		/** The take's rates, carried through as rationals rather than collapsed to floats. */
		FFrameRate DisplayRate;
		FFrameRate TickResolution;

		/** ISO 8601 UTC. Provenance only, never used for alignment. */
		FString CaptureStartedUtc;

		/** Rig the motion was recorded on, fingerprint included. */
		FDatasetSidecarSkeleton Skeleton;

		/** Optional layers beyond raw input that this capture actually recorded. */
		TArray<EDatasetCaptureLayer> OptionalLayers;
	};

	/**
	 * Derives a complete sidecar from Section plus Context.
	 *
	 * Fails, with an operator-readable reason, when the section holds no samples or when its
	 * samples cannot form an ordered stream. A take that recorded nothing is not a clip with
	 * an empty stream: it is a failed capture, and it must say so rather than produce a
	 * plausible-looking sidecar describing no input.
	 *
	 * Trim defaults to the full recorded range. The layer list always contains RawInput,
	 * because a section with samples in it is by definition a raw input recording.
	 */
	static bool BuildFromSection(
		const UMovieSceneDatasetInputSection& Section,
		const FBuildContext& Context,
		FDatasetSidecar& OutSidecar,
		FString& OutFailureReason);
};
