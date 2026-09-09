// Copyright Animotive. All Rights Reserved.

#include "DatasetTakeRecording.h"

#include "DatasetFrameMapping.h"
#include "KimodoDatasetCaptureLog.h"
#include "MovieSceneDatasetInputSection.h"

#define LOCTEXT_NAMESPACE "DatasetTakeRecording"

bool FDatasetTakeRecording::Begin(const FConfig& Config, FText& OutFailureReason)
{
	OutFailureReason = FText::GetEmpty();

	// Cleared before validating, so a failed Begin can never leave the previous take's
	// samples in place to be written out as if they belonged to this one.
	Buffer.Reset();
	bIsRecording = false;
	bIsValid = true;
	InvalidReason = FText::GetEmpty();

	const int32 MaxSamples = FDatasetFrameMapping::SampleCountForDuration(Config.SafetyCeilingSeconds, Config.DisplayRate);
	if (MaxSamples <= 0)
	{
		OutFailureReason = LOCTEXT(
			"UnusableConfig",
			"This capture cannot record anything. The take does not have a usable frame rate.");
		bIsValid = false;
		InvalidReason = OutFailureReason;
		return false;
	}

	// The single allocation of the take.
	Buffer.Reserve(MaxSamples);
	CeilingSeconds = Config.SafetyCeilingSeconds;
	bIsRecording = true;

	return true;
}

EDatasetTakeTickResult FDatasetTakeRecording::Tick(const FQualifiedFrameTime& SequenceTime, const FDatasetInputSample& Input)
{
	if (!bIsRecording)
	{
		return EDatasetTakeTickResult::NotRecording;
	}

	FDatasetInputSample Sample = Input;

	// THE alignment guarantee. The frame comes from the sequence clock Take Recorder is
	// already using for the animation, and Input.FrameNumber is OVERWRITTEN rather than
	// trusted, so no caller can key a sample from some other clock even by accident.
	Sample.FrameNumber = FDatasetFrameMapping::SampleFrameForTime(SequenceTime.Time).Value;

	if (Buffer.Add(Sample))
	{
		return EDatasetTakeTickResult::Sampled;
	}

	// Add refused, for one of exactly two reasons.
	if (Buffer.IsFull())
	{
		// The safety ceiling. A take is stopped by the operator, so arriving here means a
		// recording ran for five minutes without being stopped. Its input now covers less
		// time than its animation, which makes it unusable, so it fails loudly rather than
		// being silently truncated into a clip that looks complete.
		Invalidate(FText::Format(
			LOCTEXT(
				"CeilingExceeded",
				"This recording ran past {0} minutes and was stopped to protect memory. Its input is incomplete, so please record a shorter clip."),
			FText::AsNumber(FMath::RoundToInt(CeilingSeconds / 60.0), &FNumberFormattingOptions::DefaultNoGrouping())));

		UE_LOG(
			LogKimodoDatasetCapture,
			Error,
			TEXT("Take exceeded the %d sample safety ceiling and was invalidated."),
			Buffer.Capacity());

		return EDatasetTakeTickResult::CeilingExceeded;
	}

	// The frame did not advance, which happens when Take Recorder ticks twice inside one
	// frame. Normal, and not a fault: the take stays valid and recording.
	return EDatasetTakeTickResult::DuplicateFrame;
}

void FDatasetTakeRecording::Invalidate(const FText& Reason)
{
	// First reason wins: a later, vaguer message would displace the specific one the operator
	// needs in order to act.
	if (bIsValid)
	{
		bIsValid = false;
		InvalidReason = Reason;
	}

	bIsRecording = false;
}

bool FDatasetTakeRecording::Finish(UMovieSceneDatasetInputSection& Section, FText& OutFailureReason)
{
	bIsRecording = false;

	if (!bIsValid)
	{
		// Nothing is written. A partially written section would read downstream as a real
		// clip, which is the outcome product-guidelines.md forbids: the operator can retake
		// in ten seconds, but a plausible bad clip costs a fine-tune run.
		OutFailureReason = InvalidReason;
		return false;
	}

	if (Buffer.Num() == 0)
	{
		OutFailureReason = LOCTEXT("NoSamples", "No input was recorded during this take. Please record it again.");
		bIsValid = false;
		InvalidReason = OutFailureReason;
		return false;
	}

	OutFailureReason = FText::GetEmpty();

	// Written once, at the end, from samples already keyed to the sequence's own frames.
	Section.WriteSamples(Buffer.GetSamples());

	return true;
}

bool FDatasetTakeRecording::IsRecording() const
{
	return bIsRecording;
}

bool FDatasetTakeRecording::IsValid() const
{
	return bIsValid;
}

FText FDatasetTakeRecording::GetInvalidReason() const
{
	return InvalidReason;
}

int32 FDatasetTakeRecording::GetSampleCount() const
{
	return Buffer.Num();
}

int32 FDatasetTakeRecording::GetCapacity() const
{
	return Buffer.Capacity();
}

TArrayView<const FDatasetInputSample> FDatasetTakeRecording::GetSamples() const
{
	return Buffer.GetSamples();
}

#undef LOCTEXT_NAMESPACE
