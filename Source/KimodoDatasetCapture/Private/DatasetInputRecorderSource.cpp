// Copyright Animotive. All Rights Reserved.

#include "DatasetInputRecorderSource.h"

#include "DatasetFrameMapping.h"
#include "DatasetInputSampler.h"
#include "Engine/World.h"
#include "GameFramework/PlayerInput.h"
#include "KimodoDatasetCaptureLog.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneDatasetInputSection.h"
#include "MovieSceneDatasetInputTrack.h"

#define LOCTEXT_NAMESPACE "DatasetInputRecorderSource"

namespace
{
	/**
	 * The world a capture reads from.
	 *
	 * GamePreview is Play In Editor, which is where every capture happens; Game covers a
	 * standalone session. The editor world is deliberately excluded: it has no player input
	 * to read, and recording it would produce a take full of neutral samples that looks like
	 * a player standing perfectly still.
	 */
	UWorld* FindCaptureWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				if (Context.World())
				{
					return Context.World();
				}
			}
		}

		return nullptr;
	}
}

UDatasetInputRecorderSource::UDatasetInputRecorderSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TrackTint = FColor(90, 140, 200);
}

TArray<UTakeRecorderSource*> UDatasetInputRecorderSource::PreRecording(
	ULevelSequence* InSequence,
	FMovieSceneSequenceID InSequenceID,
	ULevelSequence* InRootSequence,
	FManifestSerializer* InManifestSerializer)
{
	InputSection = nullptr;
	bIsCaptureValid = true;
	InvalidReason = FText::GetEmpty();

	UMovieScene* MovieScene = InSequence ? InSequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		Invalidate(LOCTEXT("NoSequence", "This take has no sequence to record into. Try starting the recording again."));
		return TArray<UTakeRecorderSource*>();
	}

	// The track goes on the sequence itself rather than on an object binding: the input
	// belongs to the take, not to one actor, and a take with no bindings must still carry it.
	UMovieSceneDatasetInputTrack* Track = MovieScene->AddTrack<UMovieSceneDatasetInputTrack>();
	if (!Track)
	{
		Invalidate(LOCTEXT("NoTrack", "The input track could not be added to this take. Try starting the recording again."));
		return TArray<UTakeRecorderSource*>();
	}

	InputSection = Track->GetOrCreateInputSection();

	return TArray<UTakeRecorderSource*>();
}

void UDatasetInputRecorderSource::StartRecording(
	const FTimecode& InSectionStartTimecode,
	const FFrameNumber& InSectionFirstFrame,
	ULevelSequence* InSequence)
{
	Buffer.Reset();
	bIsRecording = false;

	if (!bIsCaptureValid)
	{
		// PreRecording already failed. Starting anyway would produce a take whose input
		// section exists but was never written, which reads downstream as a clip that
		// recorded nothing rather than as a failure.
		return;
	}

	// Resolved once, at record start, so a take always describes the character it started on
	// even if the player possesses another mid-take.
	const EDatasetPawnResolution Resolution = FDatasetPawnResolver::ResolveFromWorld(FindCaptureWorld(), Target);
	if (FDatasetPawnResolver::IsFailure(Resolution))
	{
		Invalidate(FDatasetPawnResolver::DescribeFailure(Resolution));
		return;
	}

	const UMovieScene* MovieScene = InSequence ? InSequence->GetMovieScene() : nullptr;
	const FFrameRate DisplayRate = MovieScene ? MovieScene->GetDisplayRate() : FFrameRate(30, 1);

	// Sized from the operator's duration and the take's own display rate, once, here. The
	// buffer refuses samples when full, which is what ends the take, so this single number is
	// both the memory bound and the duration limit.
	const int32 MaxSamples = FDatasetFrameMapping::SampleCountForDuration(MaxDurationSeconds, DisplayRate);
	if (MaxSamples <= 0)
	{
		Invalidate(LOCTEXT("BadDuration", "The maximum clip length is not usable. Set a clip length above zero and try again."));
		return;
	}

	Buffer.Reserve(MaxSamples);
	bIsRecording = true;

	UE_LOG(
		LogKimodoDatasetCapture,
		Verbose,
		TEXT("Dataset input capture started: up to %d samples at %s, first frame %d."),
		MaxSamples,
		*DisplayRate.ToPrettyText().ToString(),
		InSectionFirstFrame.Value);
}

void UDatasetInputRecorderSource::TickRecording(const FQualifiedFrameTime& CurrentSequenceTime)
{
	if (!bIsRecording)
	{
		return;
	}

	// The pawn is rechecked every tick. PIE can end and an actor can be destroyed mid-take,
	// and a take that lost its subject is invalid rather than shorter: the remaining samples
	// would describe input with no motion to pair it with.
	if (!Target.IsStillValid())
	{
		Invalidate(LOCTEXT("PawnLost", "The character being recorded disappeared during the take. Please record it again."));
		return;
	}

	const UPlayerInput* PlayerInput = Target.PlayerInput.Get();
	if (!PlayerInput)
	{
		Invalidate(LOCTEXT("InputLost", "The player's input could not be read during the take. Please record it again."));
		return;
	}

	// THE alignment guarantee, in one line: the frame comes from the sequence time Take
	// Recorder handed us, floored to a frame. Nothing here counts ticks, accumulates elapsed
	// seconds, or consults a wall clock, so there is no second time base able to drift from
	// the animation being recorded alongside this.
	const FFrameNumber SampleFrame = FDatasetFrameMapping::SampleFrameForTime(CurrentSequenceTime.Time);

	const FDatasetInputSample Sample = FDatasetInputSampler::SampleFromPlayerInput(SampleFrame.Value, *PlayerInput);

	if (!Buffer.Add(Sample))
	{
		// Two reasons Add refuses, and both mean this take is done sampling rather than
		// broken: the buffer is full (the configured duration elapsed), or the sequence
		// clock did not advance past the last sample, which happens when Take Recorder ticks
		// twice within one frame. Neither is a failure, so the take stays valid.
		if (Buffer.IsFull())
		{
			bIsRecording = false;
			UE_LOG(
				LogKimodoDatasetCapture,
				Verbose,
				TEXT("Dataset input capture reached its maximum length after %d samples."),
				Buffer.Num());
		}
	}
}

void UDatasetInputRecorderSource::StopRecording(ULevelSequence* InSequence)
{
	bIsRecording = false;

	if (!bIsCaptureValid)
	{
		// Fail loud, and write NOTHING. A partially written section is the one outcome
		// product-guidelines.md rules out: it would look like a real clip downstream.
		UE_LOG(
			LogKimodoDatasetCapture,
			Error,
			TEXT("Dataset input capture failed and was not written: %s"),
			*InvalidReason.ToString());
		return;
	}

	if (!InputSection)
	{
		Invalidate(LOCTEXT("NoSection", "This take has nowhere to store its input. Please record it again."));
		return;
	}

	if (Buffer.Num() == 0)
	{
		Invalidate(LOCTEXT("NoSamples", "No input was recorded during this take. Please record it again."));
		return;
	}

	// Written once, at stop, from the buffer that was filled on the sequence's own frames.
	InputSection->WriteSamples(Buffer.GetSamples());

	UE_LOG(
		LogKimodoDatasetCapture,
		Verbose,
		TEXT("Dataset input capture wrote %d samples to the input section."),
		Buffer.Num());
}

FText UDatasetInputRecorderSource::GetDisplayTextImpl() const
{
	return LOCTEXT("SourceDisplayName", "Dataset Input");
}

FText UDatasetInputRecorderSource::GetCategoryTextImpl() const
{
	return LOCTEXT("SourceCategory", "Dataset Capture");
}

bool UDatasetInputRecorderSource::IsRecording() const
{
	return bIsRecording;
}

int32 UDatasetInputRecorderSource::GetRecordedSampleCount() const
{
	return Buffer.Num();
}

bool UDatasetInputRecorderSource::IsCaptureValid() const
{
	return bIsCaptureValid;
}

FText UDatasetInputRecorderSource::GetInvalidReason() const
{
	return InvalidReason;
}

UMovieSceneDatasetInputSection* UDatasetInputRecorderSource::GetInputSection() const
{
	return InputSection;
}

void UDatasetInputRecorderSource::Invalidate(const FText& Reason)
{
	// First reason wins. A later, vaguer message would replace the specific one the operator
	// actually needs to act on.
	if (bIsCaptureValid)
	{
		bIsCaptureValid = false;
		InvalidReason = Reason;
	}

	bIsRecording = false;
}

#undef LOCTEXT_NAMESPACE
