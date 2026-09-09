// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameRate.h"
#include "Misc/FrameTime.h"

struct FDatasetInputSample;

/**
 * Converts sequence time into the frame numbers samples are keyed at.
 *
 * Everything here exists so that no other file has to do this arithmetic. The frame a sample
 * lands on comes from the take's own clock, converted once, here. Anywhere else in the plugin
 * that computed a frame from a tick count or an elapsed duration would be a second time base,
 * which is exactly what the MovieScene design exists to eliminate.
 */
class KIMODODATASETCAPTURE_API FDatasetFrameMapping
{
public:
	/**
	 * The frame a sample taken at SequenceTime belongs to.
	 *
	 * FLOORS, and never rounds. Rounding would place a sample taken part-way through a frame
	 * onto the NEXT frame, which has not happened yet: the input would appear to precede the
	 * animation it caused. Flooring keeps a sample on the frame that was actually being
	 * displayed when the hardware was read.
	 */
	static FFrameNumber SampleFrameForTime(FFrameTime SequenceTime);

	/** Converts a time on DisplayRate into a frame number on TickResolution. */
	static FFrameNumber DisplayToTick(FFrameTime DisplayTime, FFrameRate DisplayRate, FFrameRate TickResolution);

	/**
	 * Ticks between two consecutive display frames.
	 *
	 * Zero when either rate is invalid, so a caller cannot silently step by a garbage
	 * interval and write every sample onto the same frame.
	 */
	static FFrameNumber TicksPerDisplayFrame(FFrameRate DisplayRate, FFrameRate TickResolution);

	/**
	 * Number of samples a capture of MaxDurationSeconds will produce at DisplayRate.
	 *
	 * Used to reserve the capture buffer once, before recording starts, so the per-frame path
	 * never allocates. Rounds UP, because a buffer one sample short would reallocate at the
	 * worst possible moment.
	 */
	static int32 SampleCountForDuration(double MaxDurationSeconds, FFrameRate DisplayRate);

	/**
	 * Assigns contiguous frame numbers to a run of samples, starting at StartFrame and
	 * stepping by FrameStep.
	 *
	 * Returns false, changing nothing, when FrameStep is not positive: a zero step would key
	 * every sample onto one frame and a negative step would run the take backwards.
	 */
	static bool AssignFrameNumbers(TArrayView<FDatasetInputSample> Samples, FFrameNumber StartFrame, FFrameNumber FrameStep);
};
