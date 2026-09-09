// Copyright Animotive. All Rights Reserved.

#include "DatasetCaptureBuffer.h"

void FDatasetCaptureBuffer::Reserve(int32 MaxSamples)
{
	MaxSampleCount = FMath::Max(0, MaxSamples);

	Samples.Reset();
	Samples.Reserve(MaxSampleCount);

	// Baseline for the move detector, taken after the only allocation this class intends to
	// make. Everything after this point should leave the address alone.
	LastKnownData = Samples.GetData();
	ReallocationCount = 0;
}

bool FDatasetCaptureBuffer::Add(const FDatasetInputSample& Sample)
{
	if (IsFull())
	{
		// Refuse rather than grow. Growing here would allocate on a game frame, and the
		// recorder reads this refusal as "the take is done", so accommodating the extra
		// sample would also silently extend the take past its configured maximum.
		return false;
	}

	if (Samples.Num() > 0 && Sample.FrameNumber <= Samples.Last().FrameNumber)
	{
		// Same invariant the sidecar enforces, applied where samples enter, while the caller
		// can still report it as a capture fault.
		return false;
	}

	Samples.Add(Sample);

	// The address is compared rather than trusting Max(): if TArray ever moved the storage
	// for a reason other than growth, this still catches it, because what matters is whether
	// an allocation happened, not why.
	const FDatasetInputSample* CurrentData = Samples.GetData();
	if (CurrentData != LastKnownData)
	{
		++ReallocationCount;
		LastKnownData = CurrentData;
	}

	return true;
}

int32 FDatasetCaptureBuffer::Num() const
{
	return Samples.Num();
}

int32 FDatasetCaptureBuffer::Capacity() const
{
	// The reservation, not TArray's slack. TArray may over-allocate, and a buffer that
	// accepted more than the operator's configured duration because of allocator slack would
	// produce takes of inconsistent length.
	return MaxSampleCount;
}

bool FDatasetCaptureBuffer::IsFull() const
{
	return Samples.Num() >= MaxSampleCount;
}

void FDatasetCaptureBuffer::Reset()
{
	// Reset, not Empty: the capacity is kept so the next take in a session needs no
	// allocation. Fifty takes should allocate once.
	Samples.Reset();
	LastKnownData = Samples.GetData();
}

TArrayView<const FDatasetInputSample> FDatasetCaptureBuffer::GetSamples() const
{
	return TArrayView<const FDatasetInputSample>(Samples);
}

int32 FDatasetCaptureBuffer::GetReallocationCount() const
{
	return ReallocationCount;
}
