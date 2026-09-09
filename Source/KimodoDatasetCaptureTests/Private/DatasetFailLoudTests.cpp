// Copyright Animotive. All Rights Reserved.

#include "DatasetInputRecorderSource.h"
#include "DatasetSidecarBuilder.h"
#include "DatasetTakeRecording.h"
#include "Misc/AutomationTest.h"
#include "MovieSceneDatasetInputSection.h"
#include "UObject/Package.h"

namespace
{
	UMovieSceneDatasetInputSection* NewTransientSection()
	{
		return NewObject<UMovieSceneDatasetInputSection>(GetTransientPackage());
	}

	FQualifiedFrameTime TickTimeAtFrame(int32 TickFrame)
	{
		return FQualifiedFrameTime(FFrameTime(FFrameNumber(TickFrame)), FFrameRate(24000, 1));
	}

	FDatasetInputSample WalkingInput()
	{
		FDatasetInputSample Input;
		Input.LeftStickY = 1.0f;

		return Input;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetFailLoudMidTakeTest,
	"KimodoDatasetCapture.FailLoud.InvalidatedTakeWritesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetFailLoudMidTakeTest::RunTest(const FString& Parameters)
{
	FDatasetTakeRecording Recording;
	FDatasetTakeRecording::FConfig Config;
	Config.DisplayRate = FFrameRate(60, 1);
	Config.SafetyCeilingSeconds = 10.0;

	FText FailureReason;
	TestTrue(TEXT("Take begins"), Recording.Begin(Config, FailureReason));

	// Half a second of good samples, then the pawn is destroyed.
	for (int32 Index = 0; Index < 30; ++Index)
	{
		Recording.Tick(TickTimeAtFrame(Index * 400), WalkingInput());
	}
	TestEqual(TEXT("Thirty samples recorded before the failure"), Recording.GetSampleCount(), 30);

	const FText PawnLost = FText::FromString(TEXT("The character being recorded disappeared during the take. Please record it again."));
	Recording.Invalidate(PawnLost);

	TestFalse(TEXT("Invalidated take is not valid"), Recording.IsValid());
	TestFalse(TEXT("Invalidated take stops recording"), Recording.IsRecording());
	TestEqual(TEXT("Reason is the one supplied"), Recording.GetInvalidReason().ToString(), PawnLost.ToString());

	// Ticks after invalidation do nothing at all. A take that kept sampling after losing its
	// subject would accumulate input with no motion to pair it with.
	const EDatasetTakeTickResult AfterFailure = Recording.Tick(TickTimeAtFrame(30 * 400), WalkingInput());
	TestEqual(
		TEXT("Ticking an invalidated take does nothing"),
		static_cast<int32>(AfterFailure),
		static_cast<int32>(EDatasetTakeTickResult::NotRecording));
	TestEqual(TEXT("No further samples recorded"), Recording.GetSampleCount(), 30);

	// THE fail-loud requirement: the thirty good samples are NOT written. A section holding
	// half a take looks downstream exactly like a real short clip, and nothing about it says
	// the character vanished partway through.
	UMovieSceneDatasetInputSection* Section = NewTransientSection();
	if (!Section)
	{
		AddError(TEXT("Failed to construct a section."));
		return false;
	}

	TestFalse(TEXT("Finish fails on an invalidated take"), Recording.Finish(*Section, FailureReason));
	TestEqual(TEXT("Finish reports the original reason"), FailureReason.ToString(), PawnLost.ToString());
	TestEqual(TEXT("NOTHING was written to the section"), Section->GetSampleCount(), 0);

	// And no sidecar can be derived from an untouched section, so a failed take cannot
	// produce a sidecar describing it either.
	FDatasetSidecarBuilder::FBuildContext Context;
	Context.TakeName = TEXT("FailedTake");
	Context.DisplayRate = Config.DisplayRate;
	Context.TickResolution = FFrameRate(24000, 1);

	AddExpectedErrorPlain(TEXT("the input section is empty"), EAutomationExpectedErrorFlags::Contains, 1);

	FDatasetSidecar Sidecar;
	FText BuildFailure;
	TestFalse(
		TEXT("No sidecar can be built from a failed take"),
		FDatasetSidecarBuilder::BuildFromSection(*Section, Context, Sidecar, BuildFailure));
	TestFalse(TEXT("Sidecar failure states a reason"), BuildFailure.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetFailLoudFirstReasonTest,
	"KimodoDatasetCapture.FailLoud.KeepsTheFirstReason",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetFailLoudFirstReasonTest::RunTest(const FString& Parameters)
{
	FDatasetTakeRecording Recording;
	FDatasetTakeRecording::FConfig Config;
	Config.DisplayRate = FFrameRate(60, 1);

	FText FailureReason;
	TestTrue(TEXT("Take begins"), Recording.Begin(Config, FailureReason));
	Recording.Tick(TickTimeAtFrame(0), WalkingInput());

	const FText Specific = FText::FromString(TEXT("The character being recorded disappeared during the take."));
	const FText Vague = FText::FromString(TEXT("Something went wrong."));

	Recording.Invalidate(Specific);
	Recording.Invalidate(Vague);

	// The first, specific reason survives. A later generic message would displace exactly the
	// information the operator needs in order to know what to do differently.
	TestEqual(TEXT("First reason is kept"), Recording.GetInvalidReason().ToString(), Specific.ToString());

	// A new take clears the previous failure, so one bad take does not poison the session.
	TestTrue(TEXT("A new take begins cleanly"), Recording.Begin(Config, FailureReason));
	TestTrue(TEXT("New take is valid"), Recording.IsValid());
	TestTrue(TEXT("New take has no reason"), Recording.GetInvalidReason().IsEmpty());
	TestEqual(TEXT("New take starts empty"), Recording.GetSampleCount(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetFailLoudEmptyTakeTest,
	"KimodoDatasetCapture.FailLoud.EmptyTakeIsAFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetFailLoudEmptyTakeTest::RunTest(const FString& Parameters)
{
	FDatasetTakeRecording Recording;
	FDatasetTakeRecording::FConfig Config;
	Config.DisplayRate = FFrameRate(60, 1);

	FText FailureReason;
	TestTrue(TEXT("Take begins"), Recording.Begin(Config, FailureReason));

	UMovieSceneDatasetInputSection* Section = NewTransientSection();
	if (!Section)
	{
		AddError(TEXT("Failed to construct a section."));
		return false;
	}

	// Stopped before a single frame was sampled. This is a failure, not a zero-length clip:
	// "recorded nothing" must never be mistaken for "recorded stillness", because the second
	// is a legitimate dataset entry and the first is a broken session.
	TestFalse(TEXT("Finishing an empty take fails"), Recording.Finish(*Section, FailureReason));
	TestFalse(TEXT("Empty take states a reason"), FailureReason.IsEmpty());
	TestFalse(TEXT("Empty take is marked invalid"), Recording.IsValid());
	TestEqual(TEXT("Nothing was written"), Section->GetSampleCount(), 0);

	// An unusable configuration fails at Begin, before any sampling, with a stated reason.
	FDatasetTakeRecording::FConfig BadConfig;
	BadConfig.DisplayRate = FFrameRate(60, 1);
	BadConfig.SafetyCeilingSeconds = 0.0;

	FDatasetTakeRecording Unusable;
	FText ConfigFailure;
	TestFalse(TEXT("A zero-length take cannot begin"), Unusable.Begin(BadConfig, ConfigFailure));
	TestFalse(TEXT("Unusable config states a reason"), ConfigFailure.IsEmpty());
	TestFalse(TEXT("Unusable take is not recording"), Unusable.IsRecording());
	TestEqual(
		TEXT("Ticking a take that never began does nothing"),
		static_cast<int32>(Unusable.Tick(TickTimeAtFrame(0), WalkingInput())),
		static_cast<int32>(EDatasetTakeTickResult::NotRecording));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetFailLoudSourceWithoutAWorldTest,
	"KimodoDatasetCapture.FailLoud.SourceFailsLoudlyWithoutAWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetFailLoudSourceWithoutAWorldTest::RunTest(const FString& Parameters)
{
	// Arming with nothing playing is the most common operator mistake, and it is reachable
	// headlessly, so the source's own failure path is asserted here rather than left to
	// manual testing.
	UDatasetInputRecorderSource* Source = NewObject<UDatasetInputRecorderSource>(GetTransientPackage());
	if (!Source)
	{
		AddError(TEXT("Failed to construct a recorder source."));
		return false;
	}

	TestFalse(TEXT("A fresh source is not recording"), Source->IsRecording());
	TestTrue(TEXT("A fresh source is valid"), Source->IsCaptureValid());

	// PreRecording with no sequence must fail loudly rather than proceeding to record into
	// nothing.
	Source->PreRecording(nullptr, FMovieSceneSequenceID(), nullptr, nullptr);
	TestFalse(TEXT("No sequence invalidates the capture"), Source->IsCaptureValid());
	TestFalse(TEXT("Invalid capture states a reason"), Source->GetInvalidReason().IsEmpty());
	TestTrue(TEXT("No section was created"), Source->GetInputSection() == nullptr);

	// Starting anyway must not begin recording.
	Source->StartRecording(FTimecode(), FFrameNumber(0), nullptr);
	TestFalse(TEXT("Recording did not start"), Source->IsRecording());

	// Ticking must not sample.
	Source->TickRecording(FQualifiedFrameTime(FFrameTime(0), FFrameRate(24000, 1)));
	TestEqual(TEXT("No samples were recorded"), Source->GetRecordedSampleCount(), 0);

	// Stopping must report the failure and write nothing. The Error log is required, so it is
	// declared expected rather than suppressed.
	AddExpectedErrorPlain(TEXT("Dataset input capture failed"), EAutomationExpectedErrorFlags::Contains, 1);
	Source->StopRecording(nullptr);
	TestFalse(TEXT("Capture is still invalid after stopping"), Source->IsCaptureValid());

	return true;
}
