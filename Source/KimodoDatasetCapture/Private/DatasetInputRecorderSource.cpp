// Copyright Animotive. All Rights Reserved.

#include "DatasetInputRecorderSource.h"

#include "DatasetInputSampler.h"
#include "Engine/World.h"
#include "GameFramework/PlayerInput.h"
#include "KimodoDatasetCaptureLog.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneDatasetInputSection.h"
#include "MovieSceneDatasetInputTrack.h"
#include "TakeRecorderActorSource.h"
#include "TakeRecorderSources.h"

#define LOCTEXT_NAMESPACE "DatasetInputRecorderSource"

namespace
{
	/**
	 * The world a capture reads from.
	 *
	 * PIE is where every capture happens; Game covers a standalone session. The editor world
	 * is deliberately excluded: it has no player input to read, and recording it would
	 * produce a take full of neutral samples that looks exactly like a player standing
	 * perfectly still.
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
	bShellFailed = false;
	ShellFailureReason = FText::GetEmpty();

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

	// Pull in the animation. Take Recorder produces animation tracks from ACTOR sources, so a
	// take with only this source in it would contain input channels and no motion. Returning
	// an actor source here is the API's own extension point for exactly this, and is how the
	// engine's player source works.
	return EnsureAnimationSourceForPawn();
}

TArray<UTakeRecorderSource*> UDatasetInputRecorderSource::EnsureAnimationSourceForPawn()
{
	TArray<UTakeRecorderSource*> NewSources;

	APawn* Pawn = nullptr;
	if (UWorld* World = FindCaptureWorld())
	{
		if (const APlayerController* Controller = World->GetFirstPlayerController())
		{
			Pawn = Controller->GetPawn();
		}
	}

	if (!Pawn)
	{
		// Not invalidated here. StartRecording resolves the pawn properly and reports the
		// specific reason (nothing playing, no pawn, no skeletal mesh); duplicating that
		// judgement now would risk two different messages for one problem.
		return NewSources;
	}

	UTakeRecorderSources* Sources = GetTypedOuter<UTakeRecorderSources>();
	if (!Sources)
	{
		UE_LOG(
			LogKimodoDatasetCapture,
			Warning,
			TEXT("Dataset input source has no sources list, so no animation source was added. The take will contain input but no motion."));
		return NewSources;
	}

	// If the operator already added a source for this pawn - by hand, or via the engine's
	// Player source - adding another would record the same animation twice.
	for (UTakeRecorderSource* Source : Sources->GetSources())
	{
		const UTakeRecorderActorSource* ActorSource = Cast<UTakeRecorderActorSource>(Source);
		if (ActorSource && ActorSource->Target.Get() == Pawn)
		{
			return NewSources;
		}
	}

	UTakeRecorderActorSource* ActorSource = NewObject<UTakeRecorderActorSource>(
		Sources, UTakeRecorderActorSource::StaticClass(), NAME_None, RF_Transactional);
	ActorSource->Target = Pawn;

	// The actor source builds its property map from a PropertyChangedEvent rather than on
	// PreRecording, so one has to be sent explicitly after setting Target. Without this the
	// source records nothing. Copied from UTakeRecorderPlayerSource, which does the same and
	// explains why.
	FPropertyChangedEvent PropertyChangedEvent(
		UTakeRecorderActorSource::StaticClass()->FindPropertyByName(
			GET_MEMBER_NAME_CHECKED(UTakeRecorderActorSource, Target)),
		EPropertyChangeType::ValueSet);
	ActorSource->PostEditChangeProperty(PropertyChangedEvent);

	AddedAnimationSource = ActorSource;
	NewSources.Add(ActorSource);

	UE_LOG(
		LogKimodoDatasetCapture,
		Verbose,
		TEXT("Added an animation source for '%s' so the take carries motion alongside its input."),
		*Pawn->GetName());

	return NewSources;
}

TArray<UTakeRecorderSource*> UDatasetInputRecorderSource::PostRecording(
	ULevelSequence* InSequence,
	ULevelSequence* InRootSequence,
	const bool bCancelled)
{
	TArray<UTakeRecorderSource*> SourcesToRemove;

	// Only the source this one added is removed, and only if it still exists. A source the
	// operator added themselves is theirs to keep.
	if (AddedAnimationSource.IsValid())
	{
		SourcesToRemove.Add(AddedAnimationSource.Get());
	}

	AddedAnimationSource = nullptr;

	return SourcesToRemove;
}

void UDatasetInputRecorderSource::StartRecording(
	const FTimecode& InSectionStartTimecode,
	const FFrameNumber& InSectionFirstFrame,
	ULevelSequence* InSequence)
{
	if (bShellFailed)
	{
		// PreRecording already failed. Beginning a take anyway would produce a take whose
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

	// SafetyCeilingSeconds is left at its default. The operator stops the take; nothing here
	// imposes a length.
	FDatasetTakeRecording::FConfig Config;
	Config.DisplayRate = MovieScene ? MovieScene->GetDisplayRate() : FFrameRate(30, 1);

	FText BeginFailure;
	if (!Recording.Begin(Config, BeginFailure))
	{
		Invalidate(BeginFailure);
		return;
	}

	UE_LOG(
		LogKimodoDatasetCapture,
		Verbose,
		TEXT("Dataset input capture started at %s, first frame %d; buffer holds up to %d samples."),
		*Config.DisplayRate.ToPrettyText().ToString(),
		InSectionFirstFrame.Value,
		Recording.GetCapacity());
}

void UDatasetInputRecorderSource::TickRecording(const FQualifiedFrameTime& CurrentSequenceTime)
{
	if (!Recording.IsRecording())
	{
		return;
	}

	// Rechecked every tick. PIE can end and an actor can be destroyed mid-take, and a take
	// that lost its subject is INVALID rather than merely shorter: the remaining samples
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

	// The sample's frame is assigned inside FDatasetTakeRecording from the sequence time
	// below, not here. This shell never computes a frame, which is what keeps a second time
	// base from creeping in.
	const FDatasetInputSample Input = FDatasetInputSampler::SampleFromPlayerInput(0, *PlayerInput);

	// The result needs no handling here: Sampled and DuplicateFrame are both normal, and
	// CeilingExceeded has already invalidated the take and logged the reason.
	Recording.Tick(CurrentSequenceTime, Input);
}

void UDatasetInputRecorderSource::StopRecording(ULevelSequence* InSequence)
{
	if (bShellFailed)
	{
		UE_LOG(
			LogKimodoDatasetCapture,
			Error,
			TEXT("Dataset input capture failed and was not written: %s"),
			*ShellFailureReason.ToString());
		return;
	}

	if (!InputSection)
	{
		Invalidate(LOCTEXT("NoSection", "This take has nowhere to store its input. Please record it again."));
		UE_LOG(LogKimodoDatasetCapture, Error, TEXT("Dataset input capture failed and was not written: no section."));
		return;
	}

	FText FinishFailure;
	if (!Recording.Finish(*InputSection, FinishFailure))
	{
		// Finish already wrote nothing. The reason comes from the take's own state machine, so
		// the panel and the log agree by construction rather than by being kept in step.
		UE_LOG(
			LogKimodoDatasetCapture,
			Error,
			TEXT("Dataset input capture failed and was not written: %s"),
			*FinishFailure.ToString());
		return;
	}

	UE_LOG(
		LogKimodoDatasetCapture,
		Verbose,
		TEXT("Dataset input capture wrote %d samples to the input section."),
		Recording.GetSampleCount());
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
	return Recording.IsRecording();
}

int32 UDatasetInputRecorderSource::GetRecordedSampleCount() const
{
	return Recording.GetSampleCount();
}

bool UDatasetInputRecorderSource::IsCaptureValid() const
{
	// Either failure counts: the shell can fail before a take exists (no sequence), and the
	// take can fail once it does (pawn lost). A caller asking "is this capture good" must not
	// have to know which layer failed.
	return !bShellFailed && Recording.IsValid();
}

FText UDatasetInputRecorderSource::GetInvalidReason() const
{
	if (bShellFailed)
	{
		return ShellFailureReason;
	}

	return Recording.GetInvalidReason();
}

UMovieSceneDatasetInputSection* UDatasetInputRecorderSource::GetInputSection() const
{
	return InputSection;
}

void UDatasetInputRecorderSource::Invalidate(const FText& Reason)
{
	// Recorded in both places deliberately. The take's state machine owns validity once a
	// take has begun; the shell flag covers failures that happen before Begin, when there is
	// no take to invalidate yet.
	if (!bShellFailed)
	{
		bShellFailed = true;
		ShellFailureReason = Reason;
	}

	Recording.Invalidate(Reason);
}

#undef LOCTEXT_NAMESPACE
