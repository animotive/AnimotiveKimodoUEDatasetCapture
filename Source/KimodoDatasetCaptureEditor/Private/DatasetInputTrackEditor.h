// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneTrackEditor.h"
#include "Templates/SubclassOf.h"

/**
 * Makes the dataset input track visible and scrubbable in Sequencer.
 *
 * Deliberately minimal. The section's channel proxy already tells Sequencer how to draw and
 * key every channel, so this editor only has to exist and claim the track type. Anything more
 * here would be UI for editing input, and input is recorded rather than authored: an operator
 * hand-keying a stick value would be fabricating data the hardware never reported.
 */
class FDatasetInputTrackEditor : public FMovieSceneTrackEditor
{
public:
	explicit FDatasetInputTrackEditor(TSharedRef<ISequencer> InSequencer);

	virtual ~FDatasetInputTrackEditor() override = default;

	/** Called by Sequencer for each opened sequence. */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

	//~ Begin ISequencerTrackEditor
	virtual FText GetDisplayName() const override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(
		UMovieSceneSection& SectionObject,
		UMovieSceneTrack& Track,
		FGuid ObjectBinding) override;
	//~ End ISequencerTrackEditor
};
