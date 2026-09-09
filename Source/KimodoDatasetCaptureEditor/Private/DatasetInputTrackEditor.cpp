// Copyright Animotive. All Rights Reserved.

#include "DatasetInputTrackEditor.h"

#include "ISequencerSection.h"
#include "MovieSceneDatasetInputTrack.h"
#include "MovieSceneSection.h"
#include "Sections/MovieSceneSubSection.h"

#define LOCTEXT_NAMESPACE "DatasetInputTrackEditor"

FDatasetInputTrackEditor::FDatasetInputTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
}

TSharedRef<ISequencerTrackEditor> FDatasetInputTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShared<FDatasetInputTrackEditor>(OwningSequencer);
}

FText FDatasetInputTrackEditor::GetDisplayName() const
{
	return LOCTEXT("TrackEditorName", "Dataset Input");
}

bool FDatasetInputTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return Type == UMovieSceneDatasetInputTrack::StaticClass();
}

bool FDatasetInputTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	// Any sequence. The track is added by the capture source rather than by a user, so there
	// is nothing to restrict here, and refusing a sequence type would only make a recorded
	// take unopenable in some contexts.
	return true;
}

TSharedRef<ISequencerSection> FDatasetInputTrackEditor::MakeSectionInterface(
	UMovieSceneSection& SectionObject,
	UMovieSceneTrack& Track,
	FGuid ObjectBinding)
{
	// The base section interface draws the channels straight from the section's channel
	// proxy, which is already populated with names and sort order. No custom section widget
	// is needed, and adding one would be UI for authoring data that is meant to be recorded.
	return MakeShared<FSequencerSection>(SectionObject);
}

#undef LOCTEXT_NAMESPACE
