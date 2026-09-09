// Copyright Animotive. All Rights Reserved.

#include "DatasetFrameMapping.h"
#include "DatasetInputSample.h"
#include "DatasetSidecarBuilder.h"
#include "DatasetTakeRecording.h"
#include "Misc/AutomationTest.h"
#include "MovieSceneDatasetInputSection.h"
#include "UObject/Package.h"

namespace
{
	/** The take's clocks: 60fps gameplay on the engine's default 24000-tick resolution. */
	const FFrameRate CaptureDisplayRate(60, 1);
	const FFrameRate CaptureTickResolution(24000, 1);

	UMovieSceneDatasetInputSection* NewTransientSection()
	{
		return NewObject<UMovieSceneDatasetInputSection>(GetTransientPackage());
	}

	/** The sequence time at display frame Index, as Take Recorder would supply it. */
	FQualifiedFrameTime SequenceTimeForDisplayFrame(int32 Index)
	{
		const FFrameNumber TickFrame = FDatasetFrameMapping::DisplayToTick(
			FFrameTime(Index), CaptureDisplayRate, CaptureTickResolution);

		return FQualifiedFrameTime(FFrameTime(TickFrame), CaptureTickResolution);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetAlignmentScriptedInputTest,
	"KimodoDatasetCapture.Alignment.ScriptedInputLandsOnItsOwnFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetAlignmentScriptedInputTest::RunTest(const FString& Parameters)
{
	// The claim under test, stated plainly: a button pressed on a known frame must appear on
	// that exact frame in the recorded section, with no neighbouring frame claiming it.
	//
	// The script: forty frames of walking, jump held on display frames 12 through 15 only.
	constexpr int32 FrameCount = 40;
	constexpr int32 JumpFirstFrame = 12;
	constexpr int32 JumpLastFrame = 15;

	FDatasetTakeRecording Recording;
	FDatasetTakeRecording::FConfig Config;
	Config.DisplayRate = CaptureDisplayRate;
	Config.MaxDurationSeconds = 10.0;

	FText FailureReason;
	TestTrue(TEXT("Take begins"), Recording.Begin(Config, FailureReason));
	TestTrue(TEXT("Take is recording"), Recording.IsRecording());
	TestTrue(TEXT("Take is valid"), Recording.IsValid());

	for (int32 DisplayFrame = 0; DisplayFrame < FrameCount; ++DisplayFrame)
	{
		FDatasetInputSample Input;
		Input.LeftStickY = 1.0f;

		// Deliberately a wrong frame number, to prove Tick ignores it and takes the frame
		// from the sequence clock instead. If Tick ever trusted this, every dataset would be
		// keyed on whatever the caller happened to pass.
		Input.FrameNumber = -999;

		const bool bJumpHeld = DisplayFrame >= JumpFirstFrame && DisplayFrame <= JumpLastFrame;
		Input.SetHeld(EDatasetGamepadButton::FaceBottom, bJumpHeld);

		const EDatasetTakeTickResult Result = Recording.Tick(SequenceTimeForDisplayFrame(DisplayFrame), Input);
		if (Result != EDatasetTakeTickResult::Sampled)
		{
			AddError(FString::Printf(TEXT("Display frame %d was not sampled (result %d)."), DisplayFrame, static_cast<int32>(Result)));
			break;
		}
	}

	TestEqual(TEXT("Every scripted frame was sampled"), Recording.GetSampleCount(), FrameCount);

	UMovieSceneDatasetInputSection* Section = NewTransientSection();
	if (!Section)
	{
		AddError(TEXT("Failed to construct a section."));
		return false;
	}

	TestTrue(TEXT("Take finishes and writes"), Recording.Finish(*Section, FailureReason));
	TestEqual(TEXT("Section holds every sample"), Section->GetSampleCount(), FrameCount);

	// Frame by frame: the jump must be held on exactly the scripted frames, and on no other.
	// Checked against the tick frame each display frame converts to, so the assertion is
	// about alignment rather than about array positions lining up.
	bool bAllFramesCorrect = true;
	for (int32 DisplayFrame = 0; DisplayFrame < FrameCount; ++DisplayFrame)
	{
		const FFrameNumber TickFrame = FDatasetFrameMapping::DisplayToTick(
			FFrameTime(DisplayFrame), CaptureDisplayRate, CaptureTickResolution);

		FDatasetInputSample Recorded;
		if (!Section->TryReadSample(TickFrame, Recorded))
		{
			AddError(FString::Printf(
				TEXT("Display frame %d (tick frame %d) is missing from the section."), DisplayFrame, TickFrame.Value));
			bAllFramesCorrect = false;
			break;
		}

		const bool bExpectedJump = DisplayFrame >= JumpFirstFrame && DisplayFrame <= JumpLastFrame;
		if (Recorded.IsHeld(EDatasetGamepadButton::FaceBottom) != bExpectedJump)
		{
			AddError(FString::Printf(
				TEXT("Display frame %d (tick frame %d): jump was %s but should have been %s."),
				DisplayFrame,
				TickFrame.Value,
				Recorded.IsHeld(EDatasetGamepadButton::FaceBottom) ? TEXT("held") : TEXT("not held"),
				bExpectedJump ? TEXT("held") : TEXT("not held")));
			bAllFramesCorrect = false;
			break;
		}
	}
	TestTrue(TEXT("The jump is held on exactly its scripted frames"), bAllFramesCorrect);

	// The press and release must read as transitions on the exact boundary frames. This is
	// the form the dataset's semantics actually depend on: "presses jump" is a transition,
	// not a state.
	const FFrameNumber BeforePress = FDatasetFrameMapping::DisplayToTick(FFrameTime(JumpFirstFrame - 1), CaptureDisplayRate, CaptureTickResolution);
	const FFrameNumber AtPress = FDatasetFrameMapping::DisplayToTick(FFrameTime(JumpFirstFrame), CaptureDisplayRate, CaptureTickResolution);
	const FFrameNumber AtLastHeld = FDatasetFrameMapping::DisplayToTick(FFrameTime(JumpLastFrame), CaptureDisplayRate, CaptureTickResolution);
	const FFrameNumber AfterRelease = FDatasetFrameMapping::DisplayToTick(FFrameTime(JumpLastFrame + 1), CaptureDisplayRate, CaptureTickResolution);

	FDatasetInputSample Before;
	FDatasetInputSample Press;
	FDatasetInputSample LastHeld;
	FDatasetInputSample Released;
	if (Section->TryReadSample(BeforePress, Before)
		&& Section->TryReadSample(AtPress, Press)
		&& Section->TryReadSample(AtLastHeld, LastHeld)
		&& Section->TryReadSample(AfterRelease, Released))
	{
		TestTrue(
			TEXT("Jump reads as pressed on its first frame"),
			FDatasetInputSample::WasPressed(Before, Press, EDatasetGamepadButton::FaceBottom));
		TestTrue(
			TEXT("Jump reads as released on the frame after its last"),
			FDatasetInputSample::WasReleased(LastHeld, Released, EDatasetGamepadButton::FaceBottom));
	}
	else
	{
		AddError(TEXT("Could not read the four frames around the jump."));
	}

	// And the whole thing must survive derivation into a sidecar with the same frames.
	FDatasetSidecarBuilder::FBuildContext Context;
	Context.TakeName = TEXT("AlignmentTake");
	Context.DisplayRate = CaptureDisplayRate;
	Context.TickResolution = CaptureTickResolution;

	FDatasetSidecar Sidecar;
	TestTrue(TEXT("Sidecar derives from the section"), FDatasetSidecarBuilder::BuildFromSection(*Section, Context, Sidecar, FailureReason));

	FDatasetInputSample FromSidecar;
	TestTrue(TEXT("Sidecar carries the jump frame"), Sidecar.TryGetSampleAtFrame(AtPress.Value, FromSidecar));
	TestTrue(TEXT("Sidecar's jump frame still holds jump"), FromSidecar.IsHeld(EDatasetGamepadButton::FaceBottom));
	TestTrue(TEXT("Sidecar has no sample between recorded frames"), !Sidecar.TryGetSampleAtFrame(AtPress.Value + 1, FromSidecar));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetAlignmentDuplicateTickTest,
	"KimodoDatasetCapture.Alignment.DuplicateTickDoesNotDoubleSample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetAlignmentDuplicateTickTest::RunTest(const FString& Parameters)
{
	FDatasetTakeRecording Recording;
	FDatasetTakeRecording::FConfig Config;
	Config.DisplayRate = CaptureDisplayRate;

	FText FailureReason;
	TestTrue(TEXT("Take begins"), Recording.Begin(Config, FailureReason));

	FDatasetInputSample Input;
	Input.SetHeld(EDatasetGamepadButton::FaceBottom, true);

	TestEqual(
		TEXT("First tick samples"),
		static_cast<int32>(Recording.Tick(SequenceTimeForDisplayFrame(0), Input)),
		static_cast<int32>(EDatasetTakeTickResult::Sampled));

	// Take Recorder can tick more than once inside a frame. A second sample on the same frame
	// must be refused rather than recorded, since two samples for one frame would break the
	// strictly-increasing invariant the sidecar depends on.
	TestEqual(
		TEXT("Second tick on the same frame is a duplicate"),
		static_cast<int32>(Recording.Tick(SequenceTimeForDisplayFrame(0), Input)),
		static_cast<int32>(EDatasetTakeTickResult::DuplicateFrame));
	TestEqual(TEXT("Duplicate stored nothing"), Recording.GetSampleCount(), 1);

	// A duplicate is normal, not a fault: the take carries on.
	TestTrue(TEXT("Take is still recording"), Recording.IsRecording());
	TestTrue(TEXT("Take is still valid"), Recording.IsValid());

	// Sub-frame advancement within the same frame is also a duplicate, because it floors to
	// the same frame.
	const FQualifiedFrameTime SubFrame(FFrameTime(FDatasetFrameMapping::DisplayToTick(FFrameTime(0), CaptureDisplayRate, CaptureTickResolution), 0.5f), CaptureTickResolution);
	TestEqual(
		TEXT("Sub-frame advance is a duplicate"),
		static_cast<int32>(Recording.Tick(SubFrame, Input)),
		static_cast<int32>(EDatasetTakeTickResult::DuplicateFrame));

	// The next real frame samples again.
	TestEqual(
		TEXT("Next frame samples"),
		static_cast<int32>(Recording.Tick(SequenceTimeForDisplayFrame(1), Input)),
		static_cast<int32>(EDatasetTakeTickResult::Sampled));
	TestEqual(TEXT("Two samples stored"), Recording.GetSampleCount(), 2);

	return true;
}
