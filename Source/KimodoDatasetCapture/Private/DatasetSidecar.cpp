// Copyright Animotive. All Rights Reserved.

#include "DatasetSidecar.h"

bool FDatasetSidecarIdentity::operator==(const FDatasetSidecarIdentity& Other) const
{
	return TakeName == Other.TakeName
		&& LevelSequencePath == Other.LevelSequencePath
		&& AnimSequencePath == Other.AnimSequencePath
		&& FrameRateNumerator == Other.FrameRateNumerator
		&& FrameRateDenominator == Other.FrameRateDenominator
		&& TickResolutionNumerator == Other.TickResolutionNumerator
		&& TickResolutionDenominator == Other.TickResolutionDenominator
		&& CaptureStartedUtc == Other.CaptureStartedUtc
		&& DurationFrames == Other.DurationFrames;
}

bool FDatasetSidecarInputStream::operator==(const FDatasetSidecarInputStream& Other) const
{
	if (Samples.Num() != Other.Samples.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < Samples.Num(); ++Index)
	{
		if (Samples[Index] != Other.Samples[Index])
		{
			return false;
		}
	}

	return true;
}

bool FDatasetSidecarSkeleton::operator==(const FDatasetSidecarSkeleton& Other) const
{
	return SkeletonPath == Other.SkeletonPath
		&& BoneCount == Other.BoneCount
		&& BoneHierarchyFingerprint == Other.BoneHierarchyFingerprint;
}

bool FDatasetSidecarLabels::operator==(const FDatasetSidecarLabels& Other) const
{
	return Caption == Other.Caption
		&& Tags == Other.Tags
		&& TrimInFrame == Other.TrimInFrame
		&& TrimOutFrame == Other.TrimOutFrame
		&& bIsValid == Other.bIsValid
		&& InvalidReason == Other.InvalidReason;
}

int32 FDatasetSidecar::GetSampleCount() const
{
	return InputStream.Samples.Num();
}

bool FDatasetSidecar::AddSample(const FDatasetInputSample& Sample)
{
	// Strictly increasing frame numbers are the invariant every frame lookup depends on.
	// Rejecting here, rather than sorting or overwriting, keeps the failure at the point
	// where the caller can still report it as a capture fault.
	if (InputStream.Samples.Num() > 0 && Sample.FrameNumber <= InputStream.Samples.Last().FrameNumber)
	{
		return false;
	}

	InputStream.Samples.Add(Sample);
	return true;
}

bool FDatasetSidecar::TryGetSampleAtFrame(int32 FrameNumber, FDatasetInputSample& OutSample) const
{
	// Binary search, valid because AddSample guarantees ascending frame numbers. A capture
	// is hundreds of samples and a review pass queries it per frame, so a linear scan here
	// would be quadratic over a clip.
	int32 Low = 0;
	int32 High = InputStream.Samples.Num() - 1;

	while (Low <= High)
	{
		const int32 Mid = Low + (High - Low) / 2;
		const int32 MidFrame = InputStream.Samples[Mid].FrameNumber;

		if (MidFrame == FrameNumber)
		{
			OutSample = InputStream.Samples[Mid];
			return true;
		}

		if (MidFrame < FrameNumber)
		{
			Low = Mid + 1;
		}
		else
		{
			High = Mid - 1;
		}
	}

	// Deliberately does not interpolate or snap to a neighbour: a frame that was not
	// recorded must read as absent, not as an invented sample.
	return false;
}

TOptional<int32> FDatasetSidecar::GetFirstFrame() const
{
	if (InputStream.Samples.IsEmpty())
	{
		return TOptional<int32>();
	}

	return InputStream.Samples[0].FrameNumber;
}

TOptional<int32> FDatasetSidecar::GetLastFrame() const
{
	if (InputStream.Samples.IsEmpty())
	{
		return TOptional<int32>();
	}

	return InputStream.Samples.Last().FrameNumber;
}

bool FDatasetSidecar::HasLayer(EDatasetCaptureLayer Layer) const
{
	return Layers.Contains(Layer);
}

void FDatasetSidecar::AddLayer(EDatasetCaptureLayer Layer)
{
	Layers.AddUnique(Layer);
}

bool FDatasetSidecar::operator==(const FDatasetSidecar& Other) const
{
	return SchemaVersion == Other.SchemaVersion
		&& Identity == Other.Identity
		&& InputStream == Other.InputStream
		&& Skeleton == Other.Skeleton
		&& Labels == Other.Labels
		&& Layers == Other.Layers;
}

bool FDatasetSidecarImpactHit::operator==(const FDatasetSidecarImpactHit& Other) const
{
	return true;
}

bool FDatasetSidecarImpactControl::operator==(const FDatasetSidecarImpactControl& Other) const
{
	return true;
}

bool FDatasetSidecarImpact::operator==(const FDatasetSidecarImpact& Other) const
{
	return true;
}

bool FDatasetSidecar::IsSyntheticImpact() const
{
	return false;
}
