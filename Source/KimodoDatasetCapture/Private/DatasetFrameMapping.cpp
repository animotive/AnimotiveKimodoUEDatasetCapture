// Copyright Animotive. All Rights Reserved.

#include "DatasetFrameMapping.h"

#include "DatasetInputSample.h"

// RED PHASE: neutral bodies so the tests fail at runtime rather than failing to compile.

FFrameNumber FDatasetFrameMapping::SampleFrameForTime(FFrameTime SequenceTime)
{
	return FFrameNumber(0);
}

FFrameNumber FDatasetFrameMapping::DisplayToTick(FFrameTime DisplayTime, FFrameRate DisplayRate, FFrameRate TickResolution)
{
	return FFrameNumber(0);
}

FFrameNumber FDatasetFrameMapping::TicksPerDisplayFrame(FFrameRate DisplayRate, FFrameRate TickResolution)
{
	return FFrameNumber(0);
}

int32 FDatasetFrameMapping::SampleCountForDuration(double MaxDurationSeconds, FFrameRate DisplayRate)
{
	return 0;
}

bool FDatasetFrameMapping::AssignFrameNumbers(TArrayView<FDatasetInputSample> Samples, FFrameNumber StartFrame, FFrameNumber FrameStep)
{
	return false;
}
