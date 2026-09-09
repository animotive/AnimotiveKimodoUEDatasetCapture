// Copyright Animotive. All Rights Reserved.

#include "DatasetSidecarBuilder.h"

#include "MovieSceneDatasetInputSection.h"

// RED PHASE: neutral body so the tests fail at runtime rather than failing to compile.

bool FDatasetSidecarBuilder::BuildFromSection(
	const UMovieSceneDatasetInputSection& Section,
	const FBuildContext& Context,
	FDatasetSidecar& OutSidecar,
	FString& OutFailureReason)
{
	return false;
}
