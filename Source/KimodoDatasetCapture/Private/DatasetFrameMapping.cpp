// Copyright Animotive. All Rights Reserved.

#include "DatasetFrameMapping.h"

#include "DatasetInputSample.h"

namespace
{
	/**
	 * True when a rate can actually be used for conversion.
	 *
	 * FFrameRate::IsValid() checks only Denominator > 0, so a rate of 0/1 passes it and then
	 * check()-crashes inside FFrameRate::TransformTime, which asserts Numerator != 0. The
	 * name promises more than the function delivers, so this predicate states the real
	 * requirement rather than trusting it.
	 */
	FORCEINLINE bool IsUsableRate(FFrameRate Rate)
	{
		return Rate.Numerator > 0 && Rate.Denominator > 0;
	}
}

FFrameNumber FDatasetFrameMapping::SampleFrameForTime(FFrameTime SequenceTime)
{
	// FloorToFrame, not RoundToFrame. A sample read part-way through a frame belongs to the
	// frame that was on screen when the hardware was read; rounding it forward would place
	// the input on a frame that had not happened yet.
	//
	// FFrameTime's sub-frame part is always in [0, 1), so flooring is well behaved for
	// negative times too: -5 and 0.5 floors to -5, not to -6 or to -4.
	return SequenceTime.FloorToFrame();
}

FFrameNumber FDatasetFrameMapping::DisplayToTick(FFrameTime DisplayTime, FFrameRate DisplayRate, FFrameRate TickResolution)
{
	if (!IsUsableRate(DisplayRate) || !IsUsableRate(TickResolution))
	{
		return FFrameNumber(0);
	}

	// The engine's own conversion, rather than hand-rolled multiplication. It carries the
	// sub-frame part through the rational arithmetic exactly, which a float multiply would
	// not for rates like 30000/1001.
	return FFrameRate::TransformTime(DisplayTime, DisplayRate, TickResolution).FloorToFrame();
}

FFrameNumber FDatasetFrameMapping::TicksPerDisplayFrame(FFrameRate DisplayRate, FFrameRate TickResolution)
{
	if (!IsUsableRate(DisplayRate) || !IsUsableRate(TickResolution))
	{
		// Zero rather than a fallback guess. A garbage step would key every sample of a take
		// onto the same frame, which is a silently corrupt dataset; zero forces the caller to
		// notice.
		return FFrameNumber(0);
	}

	// Measured as the distance between two consecutive display frames, so the result is
	// consistent with DisplayToTick by construction rather than by a parallel formula.
	const FFrameNumber FirstFrame = DisplayToTick(FFrameTime(0), DisplayRate, TickResolution);
	const FFrameNumber SecondFrame = DisplayToTick(FFrameTime(1), DisplayRate, TickResolution);

	return SecondFrame - FirstFrame;
}

int32 FDatasetFrameMapping::SampleCountForDuration(double MaxDurationSeconds, FFrameRate DisplayRate)
{
	if (MaxDurationSeconds <= 0.0 || !IsUsableRate(DisplayRate))
	{
		return 0;
	}

	// Ceil, because a buffer one sample short would reallocate during capture, and the
	// per-frame path is required not to allocate.
	const double SampleCount = MaxDurationSeconds * DisplayRate.AsDecimal();

	return static_cast<int32>(FMath::CeilToDouble(SampleCount));
}

bool FDatasetFrameMapping::AssignFrameNumbers(TArrayView<FDatasetInputSample> Samples, FFrameNumber StartFrame, FFrameNumber FrameStep)
{
	if (FrameStep.Value <= 0)
	{
		// A zero step keys every sample onto one frame; a negative step runs the take
		// backwards. Both leave the samples untouched, so a caller that ignores the return
		// value is left with the frames it had rather than a corrupted run.
		return false;
	}

	FFrameNumber Frame = StartFrame;
	for (FDatasetInputSample& Sample : Samples)
	{
		Sample.FrameNumber = Frame.Value;
		Frame += FrameStep;
	}

	return true;
}

bool FDatasetFrameMapping::AssignFrameNumbersForRate(
	TArrayView<FDatasetInputSample> Samples,
	int32 StartDisplayFrame,
	FFrameRate DisplayRate,
	FFrameRate TickResolution)
{
	if (!IsUsableRate(DisplayRate) || !IsUsableRate(TickResolution))
	{
		return false;
	}

	// Each sample's frame is converted from its OWN display frame, independently. Adding a
	// per-frame step instead would accumulate the rounding error that a non-integer tick
	// interval leaves behind: at 30000/1001 on 24000/1 a frame is 800.8 ticks, and stepping
	// by 800 loses about 0.6 of a frame over ten seconds, which is enough to put a jump on
	// the wrong pose in a training clip.
	for (int32 Index = 0; Index < Samples.Num(); ++Index)
	{
		const FFrameTime DisplayTime(StartDisplayFrame + Index);
		Samples[Index].FrameNumber = DisplayToTick(DisplayTime, DisplayRate, TickResolution).Value;
	}

	return true;
}
