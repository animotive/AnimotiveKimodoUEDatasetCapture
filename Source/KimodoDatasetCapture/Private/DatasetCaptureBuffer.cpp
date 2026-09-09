// Copyright Animotive. All Rights Reserved.

#include "DatasetCaptureBuffer.h"

// RED PHASE: neutral bodies so the tests fail at runtime rather than failing to compile.

void FDatasetCaptureBuffer::Reserve(int32 MaxSamples)
{
}

bool FDatasetCaptureBuffer::Add(const FDatasetInputSample& Sample)
{
	return false;
}

int32 FDatasetCaptureBuffer::Num() const
{
	return 0;
}

int32 FDatasetCaptureBuffer::Capacity() const
{
	return 0;
}

bool FDatasetCaptureBuffer::IsFull() const
{
	return false;
}

void FDatasetCaptureBuffer::Reset()
{
}

TArrayView<const FDatasetInputSample> FDatasetCaptureBuffer::GetSamples() const
{
	return TArrayView<const FDatasetInputSample>();
}

int32 FDatasetCaptureBuffer::GetReallocationCount() const
{
	return 0;
}
