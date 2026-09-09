// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasetPawnResolver.h"
#include "DatasetTakeRecording.h"
#include "TakeRecorderSource.h"

#include "DatasetInputRecorderSource.generated.h"

class UMovieSceneDatasetInputSection;
class UMovieSceneDatasetInputTrack;
class ULevelSequence;

/**
 * Take Recorder source that records the player's raw gamepad input.
 *
 * Take Recorder owns the lifecycle: it decides when recording starts and stops, and it hands
 * this source the sequence's own time on every tick. That is the whole reason the plugin is
 * built as a source rather than as an independent recorder. The frame a sample lands on comes
 * from the sequence clock Take Recorder is already using for the animation, so input and
 * animation cannot desync through pause, time dilation, or a frame hitch. There is no second
 * time base to reconcile, because there is no second time base.
 *
 * This source never starts or stops a recording itself. It reacts.
 */
UCLASS(Category = "Dataset Capture", meta = (TakeRecorderDisplayName = "Dataset Input"))
class UDatasetInputRecorderSource : public UTakeRecorderSource
{
	GENERATED_BODY()

public:
	UDatasetInputRecorderSource(const FObjectInitializer& ObjectInitializer);

	// Deliberately no duration property. The operator decides when a take stops by pressing
	// stop in Take Recorder; a maximum length would be a knob they should not have to think
	// about. The buffer's internal safety ceiling is not a user-facing setting.

	//~ Begin UTakeRecorderSource
	KIMODODATASETCAPTURE_API virtual TArray<UTakeRecorderSource*> PreRecording(
		ULevelSequence* InSequence,
		FMovieSceneSequenceID InSequenceID,
		ULevelSequence* InRootSequence,
		FManifestSerializer* InManifestSerializer) override;

	KIMODODATASETCAPTURE_API virtual void StartRecording(
		const FTimecode& InSectionStartTimecode,
		const FFrameNumber& InSectionFirstFrame,
		ULevelSequence* InSequence) override;

	KIMODODATASETCAPTURE_API virtual void TickRecording(const FQualifiedFrameTime& CurrentSequenceTime) override;

	KIMODODATASETCAPTURE_API virtual void StopRecording(ULevelSequence* InSequence) override;

	KIMODODATASETCAPTURE_API virtual FText GetDisplayTextImpl() const override;
	KIMODODATASETCAPTURE_API virtual FText GetCategoryTextImpl() const override;

	/** Subscenes are not supported: input belongs in the same sequence as the animation. */
	virtual bool SupportsSubscenes() const override { return false; }
	//~ End UTakeRecorderSource

	/** True while a take is being recorded. */
	KIMODODATASETCAPTURE_API bool IsRecording() const;

	/** Samples recorded so far in the current or just-finished take. */
	KIMODODATASETCAPTURE_API int32 GetRecordedSampleCount() const;

	/** False when the take failed. The reason is operator-readable. */
	KIMODODATASETCAPTURE_API bool IsCaptureValid() const;

	KIMODODATASETCAPTURE_API FText GetInvalidReason() const;

	/** The section this take recorded into, or null before PreRecording. */
	KIMODODATASETCAPTURE_API UMovieSceneDatasetInputSection* GetInputSection() const;

private:
	/** Marks the take failed with an operator-readable reason and stops sampling. */
	void Invalidate(const FText& Reason);

	/** The section created in PreRecording and written in StopRecording. */
	UPROPERTY(Transient)
	TObjectPtr<UMovieSceneDatasetInputSection> InputSection;

	/**
	 * The take's state machine, and the only place take decisions are made.
	 *
	 * This source is a shell that forwards Take Recorder's callbacks. Keeping a second copy of
	 * the recording state here would mean two state machines able to disagree about whether a
	 * take is valid, and the disagreement would surface as a written clip that the panel
	 * believed had failed.
	 */
	FDatasetTakeRecording Recording;

	/** Pawn, mesh and player input resolved at record start. */
	FDatasetPawnTarget Target;

	/** Set when the source itself fails before a take can begin, e.g. no sequence. */
	bool bShellFailed = false;

	FText ShellFailureReason;
};
