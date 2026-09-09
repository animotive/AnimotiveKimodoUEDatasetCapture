// Copyright Animotive. All Rights Reserved.

#include "DatasetSidecarBuilder.h"

#include "DatasetTrimRange.h"
#include "KimodoDatasetCaptureLog.h"
#include "MovieSceneDatasetInputSection.h"

#define LOCTEXT_NAMESPACE "DatasetSidecarBuilder"

bool FDatasetSidecarBuilder::BuildFromSection(
	const UMovieSceneDatasetInputSection& Section,
	const FBuildContext& Context,
	FDatasetSidecar& OutSidecar,
	FText& OutFailureReason)
{
	OutFailureReason = FText::GetEmpty();

	const TArray<FDatasetInputSample> Samples = Section.ReadAllSamples();
	if (Samples.IsEmpty())
	{
		// Fail-loud, per product-guidelines.md. A take that recorded nothing is a failed
		// capture, not a clip with an empty stream, and a sidecar describing no input would
		// look perfectly well formed to everything downstream.
		OutFailureReason = LOCTEXT("NoInput", "This take recorded no input. Nothing was captured, so there is no clip to label.");
		UE_LOG(LogKimodoDatasetCapture, Error, TEXT("Cannot build a sidecar for take '%s': the input section is empty."), *Context.TakeName);
		return false;
	}

	FDatasetSidecar Sidecar;

	Sidecar.Identity.TakeName = Context.TakeName;
	Sidecar.Identity.LevelSequencePath = Context.LevelSequencePath;
	Sidecar.Identity.AnimSequencePath = Context.AnimSequencePath;
	Sidecar.Identity.CaptureStartedUtc = Context.CaptureStartedUtc;

	// Rates are copied as the rationals they are. Collapsing them to a float here would make
	// 30000/1001 unrecoverable downstream, and a dataset that cannot state its own frame rate
	// exactly cannot be aligned back to its source.
	Sidecar.Identity.FrameRateNumerator = Context.DisplayRate.Numerator;
	Sidecar.Identity.FrameRateDenominator = Context.DisplayRate.Denominator;
	Sidecar.Identity.TickResolutionNumerator = Context.TickResolution.Numerator;
	Sidecar.Identity.TickResolutionDenominator = Context.TickResolution.Denominator;

	Sidecar.Skeleton = Context.Skeleton;

	// A section with samples in it is by definition a raw input recording, so that layer is
	// asserted here rather than trusted from the caller.
	Sidecar.AddLayer(EDatasetCaptureLayer::RawInput);
	for (const EDatasetCaptureLayer Layer : Context.OptionalLayers)
	{
		Sidecar.AddLayer(Layer);
	}

	for (const FDatasetInputSample& Sample : Samples)
	{
		if (!Sidecar.AddSample(Sample))
		{
			// AddSample only refuses a frame that does not advance. Reaching here means the
			// section's keys are out of order or duplicated, which would misalign every
			// frame lookup after the offending sample, so the whole build fails rather than
			// producing a stream with a hole in it.
			OutFailureReason = FText::Format(
				LOCTEXT("OutOfOrder", "This take's input is out of order at frame {0}, so it cannot be saved. Please record it again."),
				FText::AsNumber(Sample.FrameNumber, &FNumberFormattingOptions::DefaultNoGrouping()));
			UE_LOG(
				LogKimodoDatasetCapture,
				Error,
				TEXT("Take '%s' has a non-advancing input frame at %d; refusing to build a sidecar."),
				*Context.TakeName,
				Sample.FrameNumber);
			return false;
		}
	}

	// Duration counts recorded samples, not the span between first and last frame. With a
	// dropped frame those differ, and the honest number is how many samples exist.
	Sidecar.Identity.DurationFrames = Sidecar.GetSampleCount();

	// Trim starts as the full recorded range, in the section's own frame numbers. The
	// operator tightens it later; until then the clip covers everything that was captured.
	const FDatasetTrimRange FullRange = FDatasetTrimRange::FullRange(
		Sidecar.GetFirstFrame().Get(0),
		Sidecar.GetLastFrame().Get(0));
	FullRange.ApplyToLabels(Sidecar.Labels);

	// Caption is deliberately left empty. Capture reports whether the clip is valid; the
	// operator supplies the label, and an auto-generated caption would be a guess presented
	// as a fact to a training run.
	OutSidecar = MoveTemp(Sidecar);
	return true;
}

#undef LOCTEXT_NAMESPACE
