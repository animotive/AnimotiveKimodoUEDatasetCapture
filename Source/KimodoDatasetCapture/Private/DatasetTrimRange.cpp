// Copyright Animotive. All Rights Reserved.

#include "DatasetTrimRange.h"

#include "DatasetSidecar.h"

namespace
{
	/** The canonical invalid range: out-point one before the in-point, so NumFrames is zero. */
	FDatasetTrimRange MakeInvalidRange()
	{
		return FDatasetTrimRange(0, -1);
	}
}

FDatasetTrimRange FDatasetTrimRange::FullRange(int32 FirstFrame, int32 LastFrame)
{
	return FDatasetTrimRange(FirstFrame, LastFrame);
}

FDatasetTrimRange FDatasetTrimRange::FromDuration(int32 FirstFrame, int32 DurationFrames)
{
	if (DurationFrames <= 0)
	{
		// Zero or negative duration is not a range. Returning an explicitly invalid range
		// rather than clamping to one frame keeps "recorded nothing" distinguishable from
		// "recorded a single frame".
		return MakeInvalidRange();
	}

	// Inclusive, so the last frame is FirstFrame + Duration - 1.
	return FDatasetTrimRange(FirstFrame, FirstFrame + DurationFrames - 1);
}

FDatasetTrimRange FDatasetTrimRange::FromLabels(const FDatasetSidecarLabels& Labels)
{
	return FDatasetTrimRange(Labels.TrimInFrame, Labels.TrimOutFrame);
}

void FDatasetTrimRange::ApplyToLabels(FDatasetSidecarLabels& OutLabels) const
{
	// Only the two trim fields are written. Everything else in the label block belongs to
	// the operator and must survive a trim edit untouched.
	OutLabels.TrimInFrame = InFrame;
	OutLabels.TrimOutFrame = OutFrame;
}

bool FDatasetTrimRange::IsValid() const
{
	return InFrame <= OutFrame;
}

int32 FDatasetTrimRange::NumFrames() const
{
	if (!IsValid())
	{
		return 0;
	}

	return OutFrame - InFrame + 1;
}

bool FDatasetTrimRange::ContainsFrame(int32 FrameNumber) const
{
	// An invalid range contains nothing, including the frames it names, which falls out of
	// the inclusive comparison without a special case.
	return FrameNumber >= InFrame && FrameNumber <= OutFrame;
}

FDatasetTrimRange FDatasetTrimRange::ClampedTo(const FDatasetTrimRange& Bounds) const
{
	if (!IsValid() || !Bounds.IsValid())
	{
		return MakeInvalidRange();
	}

	// Disjoint ranges produce an invalid result rather than a nearest-fit guess: the caller
	// asked for frames that were never recorded, and an answer that looks reasonable would
	// conceal that.
	if (OutFrame < Bounds.InFrame || InFrame > Bounds.OutFrame)
	{
		return MakeInvalidRange();
	}

	return FDatasetTrimRange(
		FMath::Max(InFrame, Bounds.InFrame),
		FMath::Min(OutFrame, Bounds.OutFrame));
}

bool FDatasetTrimRange::operator==(const FDatasetTrimRange& Other) const
{
	return InFrame == Other.InFrame && OutFrame == Other.OutFrame;
}
