// Copyright Animotive. All Rights Reserved.

#include "DatasetSidecar.h"

// RED PHASE: neutral bodies so the tests fail at runtime rather than failing to compile.
// Implemented in the Green task that follows.

bool FDatasetSidecarIdentity::operator==(const FDatasetSidecarIdentity& Other) const
{
	return false;
}

bool FDatasetSidecarInputStream::operator==(const FDatasetSidecarInputStream& Other) const
{
	return false;
}

bool FDatasetSidecarSkeleton::operator==(const FDatasetSidecarSkeleton& Other) const
{
	return false;
}

bool FDatasetSidecarLabels::operator==(const FDatasetSidecarLabels& Other) const
{
	return false;
}

int32 FDatasetSidecar::GetSampleCount() const
{
	return 0;
}

bool FDatasetSidecar::AddSample(const FDatasetInputSample& Sample)
{
	return false;
}

bool FDatasetSidecar::TryGetSampleAtFrame(int32 FrameNumber, FDatasetInputSample& OutSample) const
{
	return false;
}

TOptional<int32> FDatasetSidecar::GetFirstFrame() const
{
	return TOptional<int32>();
}

TOptional<int32> FDatasetSidecar::GetLastFrame() const
{
	return TOptional<int32>();
}

bool FDatasetSidecar::HasLayer(EDatasetCaptureLayer Layer) const
{
	return false;
}

void FDatasetSidecar::AddLayer(EDatasetCaptureLayer Layer)
{
}

bool FDatasetSidecar::operator==(const FDatasetSidecar& Other) const
{
	return false;
}
