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
	 * Ticks between the first two display frames, FLOORED.
	 *
	 * Zero when either rate is unusable, so a caller cannot silently step by a garbage
	 * interval and write every sample onto the same frame.
	 *
	 * WARNING. This is a whole number, and for many real frame rates the true interval is
	 * not. At 30000/1001 on a 24000/1 tick resolution a display frame is 800.8 ticks, so this
	 * returns 800 and stepping by it loses 0.8 ticks per frame: about 480 ticks, or 0.6 of a
	 * frame, across a ten-second capture. Use it for display and diagnostics. To place
	 * samples, use AssignFrameNumbersForRate, which converts each frame exactly and cannot
	 * accumulate error.
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

	/**
	 * Assigns frame numbers by converting each sample's display frame to ticks exactly.
	 *
	 * This is the function capture should use. Sample i is placed at the tick frame for
	 * display frame (StartDisplayFrame + i), computed independently, so no rounding error can
	 * accumulate across a take.
	 *
	 * The fixed-step alternative above is only correct when a display frame is a whole number
	 * of ticks. It is not for NTSC-family rates: 30000/1001 on 24000/1 is 800.8 ticks per
	 * frame, and stepping by 800 drifts roughly 0.6 of a frame over ten seconds, which is
	 * enough to put a jump on the wrong pose in a training clip.
	 *
	 * Returns false, changing nothing, when either rate is unusable.
	 */
	static bool AssignFrameNumbersForRate(
		TArrayView<FDatasetInputSample> Samples,
		int32 StartDisplayFrame,
		FFrameRate DisplayRate,
		FFrameRate TickResolution);
};
