// Copyright Animotive. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "CoreMinimal.h"
#include "DatasetInputSample.h"
#include "MovieSceneSection.h"

#include "MovieSceneDatasetInputSection.generated.h"

/**
 * Holds one take's raw input as MovieScene channels, inside the take's own LevelSequence.
 *
 * This is the mechanism that makes frame alignment structural rather than something to
 * verify: the samples live on the same sequence, with the same tick resolution and the same
 * frame clock, as the animation Take Recorder captured. There is no second time base to
 * drift, no timestamp to reconcile, and pause or time dilation cannot desync the two.
 *
 * Channel layout is six float channels for the axes plus ONE integer channel carrying the
 * button bitmask. A bool channel per button would read more naturally in Sequencer, but
 * fourteen extra channels is noise, and the bitmask matches FDatasetInputSample exactly,
 * which removes a conversion step (and therefore a class of conversion bug) between the
 * section and the sidecar.
 *
 * Every key is written with CONSTANT interpolation. Interpolating input is meaningless and
 * actively harmful: half a button press does not exist, and a stick value invented between
 * two samples is data the hardware never reported.
 */
UCLASS(MinimalAPI)
class UMovieSceneDatasetInputSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	KIMODODATASETCAPTURE_API UMovieSceneDatasetInputSection(const FObjectInitializer& ObjectInitializer);

	/** Left stick, each axis in [-1, 1]. */
	UPROPERTY()
	FMovieSceneFloatChannel LeftStickX;

	UPROPERTY()
	FMovieSceneFloatChannel LeftStickY;

	/** Right stick, each axis in [-1, 1]. */
	UPROPERTY()
	FMovieSceneFloatChannel RightStickX;

	UPROPERTY()
	FMovieSceneFloatChannel RightStickY;

	/** Analog triggers, each in [0, 1]. */
	UPROPERTY()
	FMovieSceneFloatChannel LeftTrigger;

	UPROPERTY()
	FMovieSceneFloatChannel RightTrigger;

	/** Gamepad buttons held, as a bitmask. Authoritative for sample presence. */
	UPROPERTY()
	FMovieSceneIntegerChannel Buttons;

	/** Keyboard and mouse buttons held, as an EDatasetKeyboardButton bitmask. */
	UPROPERTY()
	FMovieSceneIntegerChannel KeyboardButtons;

	/** Mouse movement per frame, in engine units. */
	UPROPERTY()
	FMovieSceneFloatChannel MouseDeltaX;

	UPROPERTY()
	FMovieSceneFloatChannel MouseDeltaY;

	/**
	 * Writes one sample as a key on every channel at Sample.FrameNumber.
	 *
	 * The frame number comes from the sample, which took it from the sequence's clock. It is
	 * never derived from a tick count here, because a tick count is exactly the parallel time
	 * base this design exists to avoid.
	 */
	KIMODODATASETCAPTURE_API void WriteSample(const FDatasetInputSample& Sample);

	/** Writes samples in order, then expands the section range to cover them. */
	KIMODODATASETCAPTURE_API void WriteSamples(TArrayView<const FDatasetInputSample> Samples);

	/**
	 * Reads the sample recorded at exactly FrameNumber.
	 *
	 * False when no sample was recorded on that frame. It deliberately does not return the
	 * held value from an earlier key, even though the channels would happily evaluate to it:
	 * a frame that was never sampled must read as absent, not as a plausible invention.
	 */
	KIMODODATASETCAPTURE_API bool TryReadSample(FFrameNumber FrameNumber, FDatasetInputSample& OutSample) const;

	/** Number of samples recorded. */
	KIMODODATASETCAPTURE_API int32 GetSampleCount() const;

	/** Every recorded sample, in frame order. */
	KIMODODATASETCAPTURE_API TArray<FDatasetInputSample> ReadAllSamples() const;

	/** Rebuilds the channel proxy. Called on construction and after loading. */
	KIMODODATASETCAPTURE_API void RecreateChannelProxy();

	//~ Begin UObject
	KIMODODATASETCAPTURE_API virtual void PostEditImport() override;
	KIMODODATASETCAPTURE_API virtual void PostLoad() override;
	//~ End UObject
};
