// Copyright Animotive. All Rights Reserved.

#include "MovieSceneDatasetInputTrack.h"

#include "MovieSceneDatasetInputSection.h"

#define LOCTEXT_NAMESPACE "MovieSceneDatasetInputTrack"

// RED PHASE: neutral bodies so the tests fail at runtime rather than failing to compile.

UMovieSceneDatasetInputTrack::UMovieSceneDatasetInputTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UMovieSceneDatasetInputSection* UMovieSceneDatasetInputTrack::GetOrCreateInputSection()
{
	return nullptr;
}

UMovieSceneDatasetInputSection* UMovieSceneDatasetInputTrack::GetInputSection() const
{
	return nullptr;
}

bool UMovieSceneDatasetInputTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return false;
}

UMovieSceneSection* UMovieSceneDatasetInputTrack::CreateNewSection()
{
	return nullptr;
}

void UMovieSceneDatasetInputTrack::AddSection(UMovieSceneSection& Section)
{
}

void UMovieSceneDatasetInputTrack::RemoveSection(UMovieSceneSection& Section)
{
}

void UMovieSceneDatasetInputTrack::RemoveSectionAt(int32 SectionIndex)
{
}

bool UMovieSceneDatasetInputTrack::HasSection(const UMovieSceneSection& Section) const
{
	return false;
}

const TArray<UMovieSceneSection*>& UMovieSceneDatasetInputTrack::GetAllSections() const
{
	return Sections;
}

bool UMovieSceneDatasetInputTrack::IsEmpty() const
{
	return true;
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneDatasetInputTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Dataset Input");
}
#endif

#undef LOCTEXT_NAMESPACE
