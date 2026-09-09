// Copyright Animotive. All Rights Reserved.

#include "DatasetFrameMapping.h"
#include "DatasetTakeRecording.h"
#include "Misc/AutomationTest.h"
#include "MovieSceneDatasetInputSection.h"
#include "UObject/Package.h"

namespace
{
	const FFrameRate TickResolution(24000, 1);

	FQualifiedFrameTime SequenceTimeForDisplayFrame(int32 Index, FFrameRate DisplayRate)
	{
		const FFrameNumber TickFrame = FDatasetFrameMapping::DisplayToTick(FFrameTime(Index), DisplayRate, TickResolution);

		return FQualifiedFrameTime(FFrameTime(TickFrame), TickResolution);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSafetyCeilingNotAFeatureTest,
	"KimodoDatasetCapture.SafetyCeiling.DoesNotLimitAnOrdinaryTake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSafetyCeilingNotAFeatureTest::RunTest(const FString& Parameters)
{
	// The operator decides when a take stops. The ceiling must be far enough away that it
	// never interferes with a real session, so the default is asserted here: five minutes at
	// 60fps is 18,000 samples, thirty times the product's typical clip.
	const FFrameRate DisplayRate(60, 1);

	FDatasetTakeRecording Recording;
	FDatasetTakeRecording::FConfig DefaultConfig;
	DefaultConfig.DisplayRate = DisplayRate;

	FText FailureReason;
	TestTrue(TEXT("Take begins with the default ceiling"), Recording.Begin(DefaultConfig, FailureReason));
	TestEqual(TEXT("Default ceiling is five minutes of frames"), Recording.GetCapacity(), 18000);

	// A normal ten-second take runs to the operator's stop with the ceiling nowhere in sight.
	for (int32 DisplayFrame = 0; DisplayFrame < 600; ++DisplayFrame)
	{
		const EDatasetTakeTickResult Result = Recording.Tick(SequenceTimeForDisplayFrame(DisplayFrame, DisplayRate), FDatasetInputSample());
		if (Result != EDatasetTakeTickResult::Sampled)
		{
			AddError(FString::Printf(TEXT("Frame %d was not sampled during an ordinary take."), DisplayFrame));
			break;
		}
	}

	TestEqual(TEXT("Ten seconds recorded"), Recording.GetSampleCount(), 600);

	// Still recording: nothing stopped it, because nothing is supposed to. The operator has
	// simply not pressed stop yet.
	TestTrue(TEXT("Take is still recording after ten seconds"), Recording.IsRecording());
	TestTrue(TEXT("Take is valid"), Recording.IsValid());

	// A much longer take is equally fine, as long as it stays under the ceiling. Thirty
	// seconds, well past the product's typical clip, must not be treated specially.
	for (int32 DisplayFrame = 600; DisplayFrame < 1800; ++DisplayFrame)
	{
		Recording.Tick(SequenceTimeForDisplayFrame(DisplayFrame, DisplayRate), FDatasetInputSample());
	}
	TestEqual(TEXT("Thirty seconds recorded"), Recording.GetSampleCount(), 1800);
	TestTrue(TEXT("A thirty second take is still recording"), Recording.IsRecording());
	TestTrue(TEXT("A thirty second take is still valid"), Recording.IsValid());

	// The operator stops it, and it writes normally.
	UMovieSceneDatasetInputSection* Section = NewObject<UMovieSceneDatasetInputSection>(GetTransientPackage());
	if (Section)
	{
		TestTrue(TEXT("An operator-stopped take writes"), Recording.Finish(*Section, FailureReason));
		TestEqual(TEXT("Every sample reached the section"), Section->GetSampleCount(), 1800);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSafetyCeilingFailsLoudlyTest,
	"KimodoDatasetCapture.SafetyCeiling.ExceedingItFailsLoudly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSafetyCeilingFailsLoudlyTest::RunTest(const FString& Parameters)
{
	// A deliberately tiny ceiling, so the breach can be reached in a test. Production uses
	// five minutes; the behaviour at the boundary is what matters, not the number.
	const FFrameRate DisplayRate(60, 1);

	FDatasetTakeRecording Recording;
	FDatasetTakeRecording::FConfig Config;
	Config.DisplayRate = DisplayRate;
	Config.SafetyCeilingSeconds = 1.0;

	FText FailureReason;
	TestTrue(TEXT("Take begins"), Recording.Begin(Config, FailureReason));
	TestEqual(TEXT("Ceiling is sixty samples"), Recording.GetCapacity(), 60);

	// Exactly at the ceiling: sixty samples all record, and the take is still valid. Reaching
	// the ceiling is not the failure; exceeding it is.
	for (int32 DisplayFrame = 0; DisplayFrame < 60; ++DisplayFrame)
	{
		const EDatasetTakeTickResult Result = Recording.Tick(SequenceTimeForDisplayFrame(DisplayFrame, DisplayRate), FDatasetInputSample());
		if (Result != EDatasetTakeTickResult::Sampled)
		{
			AddError(FString::Printf(TEXT("Frame %d was refused at or below the ceiling."), DisplayFrame));
			break;
		}
	}

	TestEqual(TEXT("Sixty samples recorded"), Recording.GetSampleCount(), 60);
	TestTrue(TEXT("A take exactly at the ceiling is still valid"), Recording.IsValid());
	TestTrue(TEXT("A take exactly at the ceiling is still recording"), Recording.IsRecording());

	// The sixty-first sample breaches it. The Error log is required, so it is declared, which
	// makes the diagnostic itself an assertion.
	AddExpectedErrorPlain(TEXT("exceeded the 60 sample safety ceiling"), EAutomationExpectedErrorFlags::Contains, 1);

	const EDatasetTakeTickResult Breach = Recording.Tick(SequenceTimeForDisplayFrame(60, DisplayRate), FDatasetInputSample());
	TestEqual(
		TEXT("Breaching the ceiling reports it"),
		static_cast<int32>(Breach),
		static_cast<int32>(EDatasetTakeTickResult::CeilingExceeded));

	// A breach is a FAILURE, not a truncation. The take's input now covers less time than its
	// animation, so a silently shortened clip would be a misaligned clip that looks complete.
	TestFalse(TEXT("Breaching the ceiling invalidates the take"), Recording.IsValid());
	TestFalse(TEXT("Breaching the ceiling stops recording"), Recording.IsRecording());
	TestFalse(TEXT("The breach states a reason"), Recording.GetInvalidReason().IsEmpty());

	// The reason is for an operator: it says what happened and what to do, and names the
	// duration in minutes rather than in samples or frames.
	const FString Reason = Recording.GetInvalidReason().ToString();
	TestTrue(TEXT("Reason mentions the length"), Reason.Contains(TEXT("minute")));
	TestFalse(TEXT("Reason avoids engine jargon"), Reason.Contains(TEXT("Buffer")));
	TestTrue(TEXT("Reason tells the operator what to do"), Reason.Contains(TEXT("shorter")));

	// And nothing is written, so a breached take cannot become a clip.
	UMovieSceneDatasetInputSection* Section = NewObject<UMovieSceneDatasetInputSection>(GetTransientPackage());
	if (Section)
	{
		TestFalse(TEXT("A breached take does not write"), Recording.Finish(*Section, FailureReason));
		TestEqual(TEXT("Nothing was written"), Section->GetSampleCount(), 0);
	}

	// The ceiling scales with the rate, since it is a duration: one second at 30fps is thirty
	// samples, not sixty.
	FDatasetTakeRecording HalfRate;
	FDatasetTakeRecording::FConfig HalfRateConfig;
	HalfRateConfig.DisplayRate = FFrameRate(30, 1);
	HalfRateConfig.SafetyCeilingSeconds = 1.0;
	TestTrue(TEXT("Thirty fps take begins"), HalfRate.Begin(HalfRateConfig, FailureReason));
	TestEqual(TEXT("One second at 30fps is thirty samples"), HalfRate.GetCapacity(), 30);

	return true;
}
