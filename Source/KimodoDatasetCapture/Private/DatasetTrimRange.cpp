// Copyright Animotive. All Rights Reserved.

#include "DatasetTrimRange.h"

#include "DatasetSidecar.h"

// RED PHASE: neutral bodies so the tests fail at runtime rather than failing to compile.

FDatasetTrimRange FDatasetTrimRange::FullRange(int32 FirstFrame, int32 LastFrame)
{
	return FDatasetTrimRange();
}

FDatasetTrimRange FDatasetTrimRange::FromDuration(int32 FirstFrame, int32 DurationFrames)
{
	return FDatasetTrimRange();
}

FDatasetTrimRange FDatasetTrimRange::FromLabels(const FDatasetSidecarLabels& Labels)
{
	return FDatasetTrimRange();
}

void FDatasetTrimRange::ApplyToLabels(FDatasetSidecarLabels& OutLabels) const
{
}

bool FDatasetTrimRange::IsValid() const
{
	return false;
}

int32 FDatasetTrimRange::NumFrames() const
{
	return 0;
}

bool FDatasetTrimRange::ContainsFrame(int32 FrameNumber) const
{
	return false;
}

FDatasetTrimRange FDatasetTrimRange::ClampedTo(const FDatasetTrimRange& Bounds) const
{
	return FDatasetTrimRange();
}

bool FDatasetTrimRange::operator==(const FDatasetTrimRange& Other) const
{
	return false;
}
