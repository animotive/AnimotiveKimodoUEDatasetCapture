// Copyright Animotive. All Rights Reserved.

#include "Channels/MovieSceneChannelData.h"
#include "DatasetInputSample.h"
#include "Misc/AutomationTest.h"
#include "MovieSceneDatasetInputSection.h"
#include "MovieSceneDatasetInputTrack.h"
#include "UObject/Package.h"

namespace
{
	/** A transient section, so these tests need no asset and no loaded level. */
	UMovieSceneDatasetInputSection* NewTransientSection()
	{
		return NewObject<UMovieSceneDatasetInputSection>(GetTransientPackage());
	}

	FDatasetInputSample MakeSample(int32 FrameNumber, float LeftStickY, EDatasetGamepadButton Buttons)
	{
		FDatasetInputSample Sample;
		Sample.FrameNumber = FrameNumber;
		Sample.LeftStickY = LeftStickY;
		Sample.Buttons = static_cast<uint32>(Buttons);
		return Sample;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMovieSceneDatasetInputSectionWriteReadTest,
	"KimodoDatasetCapture.InputSection.WriteRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMovieSceneDatasetInputSectionWriteReadTest::RunTest(const FString& Parameters)
{
	UMovieSceneDatasetInputSection* Section = NewTransientSection();
	if (!Section)
	{
		AddError(TEXT("Failed to construct a dataset input section."));
		return false;
	}

	TestEqual(TEXT("New section has no samples"), Section->GetSampleCount(), 0);

	// Every field set to a distinct value, so a channel wired to the wrong member is caught
	// rather than hidden by two channels sharing a value.
	FDatasetInputSample Written;
	Written.FrameNumber = 100;
	Written.LeftStickX = 0.125f;
	Written.LeftStickY = -0.25f;
	Written.RightStickX = 0.375f;
	Written.RightStickY = -0.5f;
	Written.LeftTrigger = 0.625f;
	Written.RightTrigger = 0.75f;
	Written.SetHeld(EDatasetGamepadButton::FaceBottom, true);
	Written.SetHeld(EDatasetGamepadButton::ShoulderLeft, true);

	Section->WriteSample(Written);
	TestEqual(TEXT("One sample recorded"), Section->GetSampleCount(), 1);

	FDatasetInputSample ReadBack;
	TestTrue(TEXT("Sample read back at its own frame"), Section->TryReadSample(FFrameNumber(100), ReadBack));

	TestEqual(TEXT("Frame number preserved"), ReadBack.FrameNumber, 100);
	TestEqual(TEXT("LeftStickX preserved"), ReadBack.LeftStickX, 0.125f);
	TestEqual(TEXT("LeftStickY preserved"), ReadBack.LeftStickY, -0.25f);
	TestEqual(TEXT("RightStickX preserved"), ReadBack.RightStickX, 0.375f);
	TestEqual(TEXT("RightStickY preserved"), ReadBack.RightStickY, -0.5f);
	TestEqual(TEXT("LeftTrigger preserved"), ReadBack.LeftTrigger, 0.625f);
	TestEqual(TEXT("RightTrigger preserved"), ReadBack.RightTrigger, 0.75f);
	TestEqual(TEXT("Button bitmask preserved"), ReadBack.Buttons, Written.Buttons);
	TestTrue(TEXT("Whole sample round-trips through channels"), ReadBack == Written);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMovieSceneDatasetInputSectionKeyTimesTest,
	"KimodoDatasetCapture.InputSection.KeyTimesLandOnWrittenFrames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMovieSceneDatasetInputSectionKeyTimesTest::RunTest(const FString& Parameters)
{
	UMovieSceneDatasetInputSection* Section = NewTransientSection();
	if (!Section)
	{
		AddError(TEXT("Failed to construct a dataset input section."));
		return false;
	}

	// A take starting at a non-zero frame, which is the normal case and the one where an
	// index-for-frame mistake would go unnoticed if frames started at zero.
	const TArray<FDatasetInputSample> Samples
	{
		MakeSample(200, 0.0f, EDatasetGamepadButton::None),
		MakeSample(201, 0.5f, EDatasetGamepadButton::None),
		MakeSample(202, 1.0f, EDatasetGamepadButton::FaceBottom),
		MakeSample(203, 1.0f, EDatasetGamepadButton::FaceBottom),
	};

	Section->WriteSamples(Samples);
	TestEqual(TEXT("All four samples recorded"), Section->GetSampleCount(), 4);

	// Key times are inspected directly, rather than inferred from evaluation, because this is
	// the assertion the whole track design rests on: a key must sit ON the frame it was
	// sampled at, not near it.
	const TArrayView<const FFrameNumber> ButtonTimes = Section->Buttons.GetData().GetTimes();
	TestEqual(TEXT("Button channel has four keys"), ButtonTimes.Num(), 4);
	if (ButtonTimes.Num() == 4)
	{
		TestEqual(TEXT("First key on frame 200"), ButtonTimes[0].Value, 200);
		TestEqual(TEXT("Second key on frame 201"), ButtonTimes[1].Value, 201);
		TestEqual(TEXT("Third key on frame 202"), ButtonTimes[2].Value, 202);
		TestEqual(TEXT("Fourth key on frame 203"), ButtonTimes[3].Value, 203);
	}

	const TArrayView<const FFrameNumber> AxisTimes = Section->LeftStickY.GetData().GetTimes();
	TestEqual(TEXT("Axis channel has four keys"), AxisTimes.Num(), 4);
	if (AxisTimes.Num() == 4 && ButtonTimes.Num() == 4)
	{
		// Axis and button keys must share frames exactly, or a sample would be split across
		// two frames and the input stream would no longer be one coherent per-frame record.
		TestEqual(TEXT("Axis and button keys share the first frame"), AxisTimes[0].Value, ButtonTimes[0].Value);
		TestEqual(TEXT("Axis and button keys share the last frame"), AxisTimes[3].Value, ButtonTimes[3].Value);
	}

	// No off-by-one at either end: both boundary frames read back, and the frames just
	// outside the range do not.
	FDatasetInputSample Edge;
	TestTrue(TEXT("First frame reads back"), Section->TryReadSample(FFrameNumber(200), Edge));
	TestEqual(TEXT("First frame carries its own value"), Edge.LeftStickY, 0.0f);
	TestTrue(TEXT("Last frame reads back"), Section->TryReadSample(FFrameNumber(203), Edge));
	TestEqual(TEXT("Last frame carries its own value"), Edge.LeftStickY, 1.0f);
	TestFalse(TEXT("Frame before the first is absent"), Section->TryReadSample(FFrameNumber(199), Edge));
	TestFalse(TEXT("Frame after the last is absent"), Section->TryReadSample(FFrameNumber(204), Edge));

	// Frame 0 is not a recorded frame here. A section that answered for it would be keyed on
	// array index rather than frame number.
	TestFalse(TEXT("Frame zero is absent"), Section->TryReadSample(FFrameNumber(0), Edge));

	// Every sample survives in order.
	const TArray<FDatasetInputSample> All = Section->ReadAllSamples();
	TestEqual(TEXT("ReadAllSamples returns every sample"), All.Num(), 4);
	if (All.Num() == 4)
	{
		TestEqual(TEXT("Samples come back in frame order"), All[0].FrameNumber, 200);
		TestEqual(TEXT("Last sample is last"), All[3].FrameNumber, 203);
		TestTrue(TEXT("Read-all agrees with what was written"), All[2] == Samples[2]);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMovieSceneDatasetInputSectionNoInterpolationTest,
	"KimodoDatasetCapture.InputSection.NoInvention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMovieSceneDatasetInputSectionNoInterpolationTest::RunTest(const FString& Parameters)
{
	UMovieSceneDatasetInputSection* Section = NewTransientSection();
	if (!Section)
	{
		AddError(TEXT("Failed to construct a dataset input section."));
		return false;
	}

	// A deliberate gap: frames 100 and 110 sampled, nothing between. This happens when a
	// frame is dropped mid-capture.
	const TArray<FDatasetInputSample> Samples
	{
		MakeSample(100, 0.0f, EDatasetGamepadButton::None),
		MakeSample(110, 1.0f, EDatasetGamepadButton::FaceBottom),
	};
	Section->WriteSamples(Samples);

	TestEqual(TEXT("Two samples recorded"), Section->GetSampleCount(), 2);

	// The channels would happily evaluate frame 105 to the held value of the previous key.
	// Reading a sample there must still fail: an unsampled frame is absent, and a plausible
	// invented sample in a training set is worse than a gap, because nothing downstream can
	// tell it was invented.
	FDatasetInputSample Invented;
	TestFalse(TEXT("Unsampled frame inside a gap is absent"), Section->TryReadSample(FFrameNumber(105), Invented));
	TestFalse(TEXT("Frame adjacent to a key is still absent"), Section->TryReadSample(FFrameNumber(101), Invented));

	// Negative frames are legal in MovieScene and must behave identically.
	UMovieSceneDatasetInputSection* NegativeSection = NewTransientSection();
	if (NegativeSection)
	{
		NegativeSection->WriteSample(MakeSample(-5, 0.5f, EDatasetGamepadButton::FaceTop));
		FDatasetInputSample Negative;
		TestTrue(TEXT("Negative frame reads back"), NegativeSection->TryReadSample(FFrameNumber(-5), Negative));
		TestEqual(TEXT("Negative frame number preserved"), Negative.FrameNumber, -5);
		TestFalse(TEXT("Neighbouring negative frame is absent"), NegativeSection->TryReadSample(FFrameNumber(-4), Negative));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMovieSceneDatasetInputSectionRangeTest,
	"KimodoDatasetCapture.InputSection.SectionRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMovieSceneDatasetInputSectionRangeTest::RunTest(const FString& Parameters)
{
	UMovieSceneDatasetInputSection* Section = NewTransientSection();
	if (!Section)
	{
		AddError(TEXT("Failed to construct a dataset input section."));
		return false;
	}

	const TArray<FDatasetInputSample> Samples
	{
		MakeSample(200, 0.0f, EDatasetGamepadButton::None),
		MakeSample(203, 1.0f, EDatasetGamepadButton::None),
	};
	Section->WriteSamples(Samples);

	// A section whose range does not cover its keys draws wrong in Sequencer and can be
	// evaluated out of existence, so the range must follow the data.
	const TRange<FFrameNumber> Range = Section->GetRange();
	TestFalse(TEXT("Range is not empty"), Range.IsEmpty());
	TestTrue(TEXT("Range has a lower bound"), Range.HasLowerBound());
	TestTrue(TEXT("Range has an upper bound"), Range.HasUpperBound());
	TestTrue(TEXT("Range starts at or before the first sample"), Range.GetLowerBoundValue().Value <= 200);
	TestTrue(TEXT("Range covers the last sample"), Range.Contains(FFrameNumber(203)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMovieSceneDatasetInputTrackTest,
	"KimodoDatasetCapture.InputTrack.SectionManagement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMovieSceneDatasetInputTrackTest::RunTest(const FString& Parameters)
{
	UMovieSceneDatasetInputTrack* Track = NewObject<UMovieSceneDatasetInputTrack>(GetTransientPackage());
	if (!Track)
	{
		AddError(TEXT("Failed to construct a dataset input track."));
		return false;
	}

	TestTrue(TEXT("New track is empty"), Track->IsEmpty());
	TestEqual(TEXT("New track has no sections"), Track->GetAllSections().Num(), 0);
	TestTrue(TEXT("Track has no input section yet"), Track->GetInputSection() == nullptr);

	TestTrue(TEXT("Track supports its own section type"), Track->SupportsType(UMovieSceneDatasetInputSection::StaticClass()));
	TestFalse(TEXT("Track rejects a foreign section type"), Track->SupportsType(UMovieSceneSection::StaticClass()));

	UMovieSceneDatasetInputSection* Created = Track->GetOrCreateInputSection();
	TestTrue(TEXT("Input section created"), Created != nullptr);
	TestFalse(TEXT("Track is no longer empty"), Track->IsEmpty());
	TestEqual(TEXT("Track holds one section"), Track->GetAllSections().Num(), 1);

	// One take, one section. Asking again must return the same section rather than adding a
	// second one, or a take could end up with its input split across two sections.
	TestTrue(TEXT("Second call returns the same section"), Track->GetOrCreateInputSection() == Created);
	TestEqual(TEXT("Still one section"), Track->GetAllSections().Num(), 1);
	TestTrue(TEXT("GetInputSection finds it"), Track->GetInputSection() == Created);

	if (Created)
	{
		TestTrue(TEXT("Track reports it has the section"), Track->HasSection(*Created));

		Track->RemoveSection(*Created);
		TestTrue(TEXT("Track is empty after removal"), Track->IsEmpty());
		TestTrue(TEXT("Removed section is gone"), Track->GetInputSection() == nullptr);
	}

	// CreateNewSection must produce the right type: Sequencer calls it, not our code.
	UMovieSceneSection* NewSection = Track->CreateNewSection();
	TestTrue(TEXT("CreateNewSection returns a section"), NewSection != nullptr);
	TestTrue(
		TEXT("CreateNewSection returns a dataset input section"),
		NewSection != nullptr && NewSection->IsA<UMovieSceneDatasetInputSection>());

	return true;
}
