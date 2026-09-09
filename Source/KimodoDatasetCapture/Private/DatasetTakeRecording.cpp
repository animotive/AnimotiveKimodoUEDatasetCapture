// Copyright Animotive. All Rights Reserved.

#include "DatasetTakeRecording.h"

#include "DatasetFrameMapping.h"
#include "MovieSceneDatasetInputSection.h"

// RED PHASE: neutral bodies so the tests fail at runtime rather than failing to compile.

bool FDatasetTakeRecording::Begin(const FConfig& Config, FText& OutFailureReason)
{
	return false;
}

EDatasetTakeTickResult FDatasetTakeRecording::Tick(const FQualifiedFrameTime& SequenceTime, const FDatasetInputSample& Input)
{
	return EDatasetTakeTickResult::NotRecording;
}

void FDatasetTakeRecording::Invalidate(const FText& Reason)
{
}

bool FDatasetTakeRecording::Finish(UMovieSceneDatasetInputSection& Section, FText& OutFailureReason)
{
	return false;
}

bool FDatasetTakeRecording::IsRecording() const
{
	return false;
}

bool FDatasetTakeRecording::IsValid() const
{
	return false;
}

FText FDatasetTakeRecording::GetInvalidReason() const
{
	return FText::GetEmpty();
}

int32 FDatasetTakeRecording::GetSampleCount() const
{
	return 0;
}

int32 FDatasetTakeRecording::GetCapacity() const
{
	return 0;
}

TArrayView<const FDatasetInputSample> FDatasetTakeRecording::GetSamples() const
{
	return TArrayView<const FDatasetInputSample>();
}
