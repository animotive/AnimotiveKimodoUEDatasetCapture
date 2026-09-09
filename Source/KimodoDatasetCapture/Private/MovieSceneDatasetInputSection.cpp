// Copyright Animotive. All Rights Reserved.

#include "MovieSceneDatasetInputSection.h"

#include "Channels/MovieSceneChannelProxy.h"

// RED PHASE: neutral bodies so the tests fail at runtime rather than failing to compile.

UMovieSceneDatasetInputSection::UMovieSceneDatasetInputSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RecreateChannelProxy();
}

void UMovieSceneDatasetInputSection::WriteSample(const FDatasetInputSample& Sample)
{
}

void UMovieSceneDatasetInputSection::WriteSamples(TArrayView<const FDatasetInputSample> Samples)
{
}

bool UMovieSceneDatasetInputSection::TryReadSample(FFrameNumber FrameNumber, FDatasetInputSample& OutSample) const
{
	return false;
}

int32 UMovieSceneDatasetInputSection::GetSampleCount() const
{
	return 0;
}

TArray<FDatasetInputSample> UMovieSceneDatasetInputSection::ReadAllSamples() const
{
	return TArray<FDatasetInputSample>();
}

void UMovieSceneDatasetInputSection::RecreateChannelProxy()
{
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>();
}

void UMovieSceneDatasetInputSection::PostEditImport()
{
	Super::PostEditImport();
	RecreateChannelProxy();
}

void UMovieSceneDatasetInputSection::PostLoad()
{
	Super::PostLoad();
	RecreateChannelProxy();
}
