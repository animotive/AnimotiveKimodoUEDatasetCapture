// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FDatasetSidecarLabels;

/**
 * An inclusive range of frames, in frame space.
 *
 * Everything here is integer frame arithmetic. No seconds, no floats: a trim expressed in
 * seconds has to be converted back to frames somewhere, and that conversion is where
 * off-by-one errors and rounding enter a dataset.
 *
 * Both ends are INCLUSIVE. A range of 100..102 is three frames. Half-open ranges are the
 * more usual convention in code, but an operator dragging a trim handle is pointing at a
 * frame they can see, and the last frame they want is the last frame they get.
 */
struct KIMODODATASETCAPTURE_API FDatasetTrimRange
{
	int32 InFrame = 0;

	int32 OutFrame = 0;

	FDatasetTrimRange() = default;

	FDatasetTrimRange(int32 InInFrame, int32 InOutFrame)
		: InFrame(InInFrame)
		, OutFrame(InOutFrame)
	{
	}

	/** The full inclusive range between two recorded frames. */
	static FDatasetTrimRange FullRange(int32 FirstFrame, int32 LastFrame);

	/**
	 * The range starting at FirstFrame and covering DurationFrames frames.
	 *
	 * A duration of zero yields an INVALID range rather than a one-frame range, because a
	 * capture that recorded nothing must not present itself as having recorded a frame.
	 */
	static FDatasetTrimRange FromDuration(int32 FirstFrame, int32 DurationFrames);

	/** Reads the trim range out of a sidecar's label block. */
	static FDatasetTrimRange FromLabels(const FDatasetSidecarLabels& Labels);

	/** Writes this range into a sidecar's label block. */
	void ApplyToLabels(FDatasetSidecarLabels& OutLabels) const;

	/** True when the range covers at least one frame. */
	bool IsValid() const;

	/** Number of frames covered, inclusive. Zero when the range is invalid. */
	int32 NumFrames() const;

	/** True when FrameNumber falls inside the range, both ends included. */
	bool ContainsFrame(int32 FrameNumber) const;

	/**
	 * This range restricted to Bounds.
	 *
	 * A range extending past either end is pulled back to it. A range sharing no frame with
	 * Bounds yields an invalid range rather than a nearest-fit guess: the caller asked for
	 * frames that do not exist, and inventing a plausible answer would hide that.
	 */
	FDatasetTrimRange ClampedTo(const FDatasetTrimRange& Bounds) const;

	bool operator==(const FDatasetTrimRange& Other) const;
	bool operator!=(const FDatasetTrimRange& Other) const { return !(*this == Other); }
};
