// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasetCaptureBuffer.h"
#include "DatasetInputSample.h"
#include "Misc/FrameRate.h"
#include "Misc/QualifiedFrameTime.h"

class UMovieSceneDatasetInputSection;

/** What a tick did. */
enum class EDatasetTakeTickResult : uint8
{
	/** The sample was recorded. */
	Sampled,

	/**
	 * The sequence clock had not advanced past the last sample, so nothing was recorded.
	 * Take Recorder can tick more than once within a frame; that is normal, not a fault.
	 */
	DuplicateFrame,

	/** The configured duration was reached. Recording has ended, and the take is valid. */
	Full,

	/** Not recording: either not begun, already finished, or invalidated. */
	NotRecording,
};

/**
 * One take's recording, with no engine lifecycle attached.
 *
 * The Take Recorder source is a shell that forwards engine callbacks here. Everything that
 * decides anything - which frame a sample lands on, when a take ends, whether a take is
 * still valid, what gets written - lives in this class, so it can all be exercised headlessly
 * with scripted sequence times instead of requiring a running game and a controller in
 * someone's hands.
 */
class KIMODODATASETCAPTURE_API FDatasetTakeRecording
{
public:
	struct FConfig
	{
		/** The take's display rate. Used only to size the buffer. */
		FFrameRate DisplayRate = FFrameRate(60, 1);

		/** Longest clip to record. Reaching it ends the take, valid. */
		double MaxDurationSeconds = 10.0;
	};

	/**
	 * Begins a take, reserving the buffer from the config.
	 *
	 * False, with an operator-readable reason, when the config cannot produce a recordable
	 * take. Any previous take's state is cleared either way, so a failed Begin cannot leave
	 * the previous take's samples in place to be written as if they were this one's.
	 */
	bool Begin(const FConfig& Config, FText& OutFailureReason);

	/**
	 * Records one sample at the frame derived from SequenceTime.
	 *
	 * The frame comes from the SEQUENCE time, never from Input.FrameNumber, which is
	 * overwritten. That is the alignment guarantee: the caller cannot accidentally supply a
	 * frame from some other clock, because its value is discarded.
	 */
	EDatasetTakeTickResult Tick(const FQualifiedFrameTime& SequenceTime, const FDatasetInputSample& Input);

	/** Marks the take failed. The first reason is kept; recording stops immediately. */
	void Invalidate(const FText& Reason);

	/**
	 * Writes the recorded samples into Section and ends the take.
	 *
	 * False, writing NOTHING to Section, when the take is invalid or recorded no samples. A
	 * partially written section would look like a real clip to everything downstream, which
	 * is the outcome product-guidelines.md forbids.
	 */
	bool Finish(UMovieSceneDatasetInputSection& Section, FText& OutFailureReason);

	bool IsRecording() const;

	/** False once the take has failed. */
	bool IsValid() const;

	FText GetInvalidReason() const;

	int32 GetSampleCount() const;

	/** Samples the take can hold, from the duration and rate given to Begin. */
	int32 GetCapacity() const;

	TArrayView<const FDatasetInputSample> GetSamples() const;

private:
	FDatasetCaptureBuffer Buffer;

	bool bIsRecording = false;

	bool bIsValid = true;

	FText InvalidReason;
};
