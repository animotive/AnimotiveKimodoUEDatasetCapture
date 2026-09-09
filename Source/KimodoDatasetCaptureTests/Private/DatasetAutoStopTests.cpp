// Copyright Animotive. All Rights Reserved.

#include "DatasetFrameMapping.h"
#include "DatasetTakeRecording.h"
#include "Misc/AutomationTest.h"
#include "MovieSceneDatasetInputSection.h"
#include "UObject/Package.h"

namespace
{
	const FFrameRate TickResolution(24000, 1);

	/**
	 * Runs a take to its natural end, ticking once per display frame.
	 *
	 * Returns the number of ticks that reported Sampled or Full, i.e. the ticks that actually
	 * recorded something, plus the frame index at which recording stopped.
	 */
	struct FRunResult
	{
		int32 RecordedTicks = 0;
		int32 StoppedAtFrame = INDEX_NONE;
		int32 TicksAfterStop = 0;
	};

	FRunResult RunTakeToCompletion(FDatasetTakeRecording& Recording, FFrameRate DisplayRate, int32 MaxFramesToTick)
	{
		FRunResult Result;

		for (int32 DisplayFrame = 0; DisplayFrame < MaxFramesToTick; ++DisplayFrame)
		{
			const FFrameNumber TickFrame = FDatasetFrameMapping::DisplayToTick(FFrameTime(DisplayFrame), DisplayRate, TickResolution);
			const FQualifiedFrameTime SequenceTime(FFrameTime(TickFrame), TickResolution);

			const EDatasetTakeTickResult TickResult = Recording.Tick(SequenceTime, FDatasetInputSample());

			if (TickResult == EDatasetTakeTickResult::Sampled || TickResult == EDatasetTakeTickResult::Full)
			{
				++Result.RecordedTicks;
			}

			if (TickResult == EDatasetTakeTickResult::Full && Result.StoppedAtFrame == INDEX_NONE)
			{
				Result.StoppedAtFrame = DisplayFrame;
			}

			if (Result.StoppedAtFrame != INDEX_NONE && DisplayFrame > Result.StoppedAtFrame)
			{
				if (TickResult != EDatasetTakeTickResult::NotRecording)
				{
					++Result.TicksAfterStop;
				}
			}
		}

		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetAutoStopExactLengthTest,
	"KimodoDatasetCapture.AutoStop.StopsAtExactlyTheConfiguredLength",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetAutoStopExactLengthTest::RunTest(const FString& Parameters)
{
	// The product's stated clip: ten seconds at 60fps.
	const FFrameRate DisplayRate(60, 1);

	FDatasetTakeRecording Recording;
	FDatasetTakeRecording::FConfig Config;
	Config.DisplayRate = DisplayRate;
	Config.MaxDurationSeconds = 10.0;

	FText FailureReason;
	TestTrue(TEXT("Take begins"), Recording.Begin(Config, FailureReason));
	TestEqual(TEXT("Capacity is ten seconds of frames"), Recording.GetCapacity(), 600);

	// Ticked well past the limit, so the stop has to come from the take rather than from the
	// caller running out of frames.
	const FRunResult Run = RunTakeToCompletion(Recording, DisplayRate, 900);

	TestEqual(TEXT("Exactly the configured number of samples recorded"), Recording.GetSampleCount(), 600);
	TestEqual(TEXT("Exactly the configured number of ticks recorded"), Run.RecordedTicks, 600);

	// Stopped on the 600th frame, which is index 599: not 598, not 600. An off-by-one here is
	// a clip one frame short or one frame long in every capture the plugin ever makes.
	TestEqual(TEXT("Stopped on the last frame of the configured length"), Run.StoppedAtFrame, 599);

	// Every tick after the stop reports NotRecording. A take that quietly kept accepting
	// samples would exceed the operator's configured length.
	TestEqual(TEXT("No tick after the stop did anything"), Run.TicksAfterStop, 0);
	TestFalse(TEXT("Take is no longer recording"), Recording.IsRecording());

	// Reaching the limit is a NORMAL end, not a failure. Auto-stop is the feature, so a take
	// that hits it must still be valid and writable.
	TestTrue(TEXT("A take that reached its length is still valid"), Recording.IsValid());
	TestTrue(TEXT("A take that reached its length has no failure reason"), Recording.GetInvalidReason().IsEmpty());

	UMovieSceneDatasetInputSection* Section = NewObject<UMovieSceneDatasetInputSection>(GetTransientPackage());
	if (Section)
	{
		TestTrue(TEXT("An auto-stopped take writes normally"), Recording.Finish(*Section, FailureReason));
		TestEqual(TEXT("All 600 samples reached the section"), Section->GetSampleCount(), 600);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetAutoStopOtherLengthsTest,
	"KimodoDatasetCapture.AutoStop.HonoursOtherLengthsAndRates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetAutoStopOtherLengthsTest::RunTest(const FString& Parameters)
{
	// A one-second take at 60fps: 60 samples. Short takes are the case where an off-by-one is
	// proportionally worst.
	{
		FDatasetTakeRecording Recording;
		FDatasetTakeRecording::FConfig Config;
		Config.DisplayRate = FFrameRate(60, 1);
		Config.MaxDurationSeconds = 1.0;

		FText FailureReason;
		TestTrue(TEXT("One second take begins"), Recording.Begin(Config, FailureReason));
		TestEqual(TEXT("One second at 60fps is 60 samples"), Recording.GetCapacity(), 60);

		const FRunResult Run = RunTakeToCompletion(Recording, Config.DisplayRate, 200);
		TestEqual(TEXT("Sixty samples recorded"), Recording.GetSampleCount(), 60);
		TestEqual(TEXT("Stopped on frame 59"), Run.StoppedAtFrame, 59);
		TestEqual(TEXT("Nothing recorded after the stop"), Run.TicksAfterStop, 0);
	}

	// Thirty fps: the same duration, half the samples. The limit is a duration, not a sample
	// count, so it must track the rate.
	{
		FDatasetTakeRecording Recording;
		FDatasetTakeRecording::FConfig Config;
		Config.DisplayRate = FFrameRate(30, 1);
		Config.MaxDurationSeconds = 10.0;

		FText FailureReason;
		TestTrue(TEXT("Thirty fps take begins"), Recording.Begin(Config, FailureReason));
		TestEqual(TEXT("Ten seconds at 30fps is 300 samples"), Recording.GetCapacity(), 300);

		RunTakeToCompletion(Recording, Config.DisplayRate, 500);
		TestEqual(TEXT("Three hundred samples recorded"), Recording.GetSampleCount(), 300);
		TestFalse(TEXT("Take stopped"), Recording.IsRecording());
	}

	// NTSC 29.97 for ten seconds: 300 samples, and the frames are not evenly spaced in ticks.
	// The limit must still land exactly, since the sample count comes from the duration and
	// the frames come from the exact conversion.
	{
		FDatasetTakeRecording Recording;
		FDatasetTakeRecording::FConfig Config;
		Config.DisplayRate = FFrameRate(30000, 1001);
		Config.MaxDurationSeconds = 10.0;

		FText FailureReason;
		TestTrue(TEXT("NTSC take begins"), Recording.Begin(Config, FailureReason));
		TestEqual(TEXT("Ten seconds at 29.97 is 300 samples"), Recording.GetCapacity(), 300);

		const FRunResult Run = RunTakeToCompletion(Recording, Config.DisplayRate, 500);
		TestEqual(TEXT("Three hundred samples recorded at 29.97"), Recording.GetSampleCount(), 300);
		TestEqual(TEXT("Stopped on frame 299"), Run.StoppedAtFrame, 299);

		// The recorded frames must still strictly increase despite the uneven tick spacing,
		// or the sidecar would refuse the stream.
		const TArrayView<const FDatasetInputSample> Samples = Recording.GetSamples();
		bool bStrictlyIncreasing = true;
		for (int32 Index = 1; Index < Samples.Num(); ++Index)
		{
			if (Samples[Index].FrameNumber <= Samples[Index - 1].FrameNumber)
			{
				AddError(FString::Printf(TEXT("Sample %d does not advance past sample %d."), Index, Index - 1));
				bStrictlyIncreasing = false;
				break;
			}
		}
		TestTrue(TEXT("NTSC frames strictly increase"), bStrictlyIncreasing);
	}

	return true;
}
