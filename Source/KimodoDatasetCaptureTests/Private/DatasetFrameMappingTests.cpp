// Copyright Animotive. All Rights Reserved.

#include "DatasetFrameMapping.h"
#include "DatasetInputSample.h"
#include "Misc/AutomationTest.h"
#include "MovieSceneDatasetInputSection.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetFrameMappingFloorTest,
	"KimodoDatasetCapture.FrameMapping.FloorsNeverRounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetFrameMappingFloorTest::RunTest(const FString& Parameters)
{
	// Exactly on a frame boundary stays on that frame.
	TestEqual(TEXT("Whole frame maps to itself"), FDatasetFrameMapping::SampleFrameForTime(FFrameTime(100)).Value, 100);
	TestEqual(TEXT("Frame zero maps to zero"), FDatasetFrameMapping::SampleFrameForTime(FFrameTime(0)).Value, 0);

	// Part-way through a frame floors DOWN. Rounding up would place a sample on a frame that
	// has not happened yet, making the input appear to precede the animation it caused.
	TestEqual(
		TEXT("Just after a frame boundary floors down"),
		FDatasetFrameMapping::SampleFrameForTime(FFrameTime(100, 0.01f)).Value,
		100);
	TestEqual(
		TEXT("Mid-frame floors down"),
		FDatasetFrameMapping::SampleFrameForTime(FFrameTime(100, 0.5f)).Value,
		100);
	TestEqual(
		TEXT("Just before the next boundary still floors down"),
		FDatasetFrameMapping::SampleFrameForTime(FFrameTime(100, 0.99f)).Value,
		100);

	// Negative sequence times are legal and must floor in the same direction, not toward zero.
	TestEqual(
		TEXT("Negative whole frame maps to itself"),
		FDatasetFrameMapping::SampleFrameForTime(FFrameTime(-5)).Value,
		-5);
	TestEqual(
		TEXT("Negative sub-frame floors away from zero"),
		FDatasetFrameMapping::SampleFrameForTime(FFrameTime(-5, 0.5f)).Value,
		-5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetFrameMappingRateConversionTest,
	"KimodoDatasetCapture.FrameMapping.RateConversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetFrameMappingRateConversionTest::RunTest(const FString& Parameters)
{
	// The engine's default authoring setup: 30fps display on a 24000-tick resolution.
	const FFrameRate Display30(30, 1);
	const FFrameRate Tick24000(24000, 1);

	TestEqual(
		TEXT("30fps on 24000 ticks is 800 ticks per frame"),
		FDatasetFrameMapping::TicksPerDisplayFrame(Display30, Tick24000).Value,
		800);
	TestEqual(
		TEXT("Display frame 0 is tick 0"),
		FDatasetFrameMapping::DisplayToTick(FFrameTime(0), Display30, Tick24000).Value,
		0);
	TestEqual(
		TEXT("Display frame 1 is tick 800"),
		FDatasetFrameMapping::DisplayToTick(FFrameTime(1), Display30, Tick24000).Value,
		800);
	TestEqual(
		TEXT("Display frame 300 is tick 240000"),
		FDatasetFrameMapping::DisplayToTick(FFrameTime(300), Display30, Tick24000).Value,
		240000);

	// 60fps, the likely capture rate for gameplay.
	const FFrameRate Display60(60, 1);
	TestEqual(
		TEXT("60fps on 24000 ticks is 400 ticks per frame"),
		FDatasetFrameMapping::TicksPerDisplayFrame(Display60, Tick24000).Value,
		400);

	// NTSC 29.97 is the case that matters, and it does NOT divide evenly: on a 24000-tick
	// resolution a display frame is 24000 * 1001 / 30000 = 800.8 ticks. TicksPerDisplayFrame
	// floors that to 800, which is why it must never be used to place samples.
	const FFrameRate Display2997(30000, 1001);
	TestEqual(
		TEXT("29.97 on 24000 ticks floors to 800 ticks per frame"),
		FDatasetFrameMapping::TicksPerDisplayFrame(Display2997, Tick24000).Value,
		800);

	// The exact conversion keeps the missing 0.8 tick per frame. Display frame 300 is
	// 300 * 800.8 = 240240 ticks, not 300 * 800 = 240000. That 240-tick gap is the drift a
	// fixed step would introduce, and it is the reason AssignFrameNumbersForRate exists.
	TestEqual(
		TEXT("29.97 display frame 300 converts exactly, not by stepping"),
		FDatasetFrameMapping::DisplayToTick(FFrameTime(300), Display2997, Tick24000).Value,
		240240);

	// An invalid rate must yield zero rather than a garbage interval that would key every
	// sample onto the same frame.
	TestEqual(
		TEXT("Zero denominator display rate yields no step"),
		FDatasetFrameMapping::TicksPerDisplayFrame(FFrameRate(30, 0), Tick24000).Value,
		0);
	TestEqual(
		TEXT("Zero numerator display rate yields no step"),
		FDatasetFrameMapping::TicksPerDisplayFrame(FFrameRate(0, 1), Tick24000).Value,
		0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetFrameMappingBufferSizeTest,
	"KimodoDatasetCapture.FrameMapping.SampleCountForDuration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetFrameMappingBufferSizeTest::RunTest(const FString& Parameters)
{
	// The product's stated capture length: ten seconds at 60fps.
	TestEqual(
		TEXT("Ten seconds at 60fps is 600 samples"),
		FDatasetFrameMapping::SampleCountForDuration(10.0, FFrameRate(60, 1)),
		600);
	TestEqual(
		TEXT("Ten seconds at 30fps is 300 samples"),
		FDatasetFrameMapping::SampleCountForDuration(10.0, FFrameRate(30, 1)),
		300);

	// Rounds UP: a buffer one sample short would reallocate during capture, which is the one
	// thing the per-frame path must never do.
	TestEqual(
		TEXT("A fractional sample count rounds up"),
		FDatasetFrameMapping::SampleCountForDuration(0.5, FFrameRate(30000, 1001)),
		15);

	TestEqual(TEXT("Zero duration needs no samples"), FDatasetFrameMapping::SampleCountForDuration(0.0, FFrameRate(60, 1)), 0);
	TestEqual(
		TEXT("Negative duration needs no samples"),
		FDatasetFrameMapping::SampleCountForDuration(-1.0, FFrameRate(60, 1)),
		0);
	TestEqual(
		TEXT("Invalid rate needs no samples"),
		FDatasetFrameMapping::SampleCountForDuration(10.0, FFrameRate(60, 0)),
		0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetFrameMappingAssignmentTest,
	"KimodoDatasetCapture.FrameMapping.ContiguousAssignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetFrameMappingAssignmentTest::RunTest(const FString& Parameters)
{
	// Four samples starting at frame 200, stepping one frame at a time: frames N..N+k with no
	// off-by-one at either end, which is the assertion this task exists for.
	{
		TArray<FDatasetInputSample> Samples;
		Samples.SetNum(4);

		TestTrue(
			TEXT("Frames assigned"),
			FDatasetFrameMapping::AssignFrameNumbers(Samples, FFrameNumber(200), FFrameNumber(1)));

		TestEqual(TEXT("First sample on frame 200"), Samples[0].FrameNumber, 200);
		TestEqual(TEXT("Second sample on frame 201"), Samples[1].FrameNumber, 201);
		TestEqual(TEXT("Third sample on frame 202"), Samples[2].FrameNumber, 202);
		TestEqual(TEXT("Last sample on frame 203"), Samples[3].FrameNumber, 203);

		// Written into a section, the keys must land on exactly those frames and nowhere else.
		UMovieSceneDatasetInputSection* Section = NewObject<UMovieSceneDatasetInputSection>(GetTransientPackage());
		if (Section)
		{
			Section->WriteSamples(Samples);
			TestEqual(TEXT("Section holds four samples"), Section->GetSampleCount(), 4);

			FDatasetInputSample Found;
			TestTrue(TEXT("Frame 200 keyed"), Section->TryReadSample(FFrameNumber(200), Found));
			TestTrue(TEXT("Frame 203 keyed"), Section->TryReadSample(FFrameNumber(203), Found));
			TestFalse(TEXT("Frame 199 not keyed"), Section->TryReadSample(FFrameNumber(199), Found));
			TestFalse(TEXT("Frame 204 not keyed"), Section->TryReadSample(FFrameNumber(204), Found));
		}
	}

	// A tick-resolution step: 60fps samples on a 24000-tick sequence step by 400.
	{
		TArray<FDatasetInputSample> Samples;
		Samples.SetNum(3);

		const FFrameNumber Step = FDatasetFrameMapping::TicksPerDisplayFrame(FFrameRate(60, 1), FFrameRate(24000, 1));
		TestTrue(TEXT("Frames assigned on tick resolution"), FDatasetFrameMapping::AssignFrameNumbers(Samples, FFrameNumber(0), Step));

		TestEqual(TEXT("First tick frame"), Samples[0].FrameNumber, 0);
		TestEqual(TEXT("Second tick frame"), Samples[1].FrameNumber, 400);
		TestEqual(TEXT("Third tick frame"), Samples[2].FrameNumber, 800);
	}

	// A zero step would key every sample onto one frame; a negative step would run the take
	// backwards. Both are rejected, and must change nothing.
	{
		TArray<FDatasetInputSample> Samples;
		Samples.SetNum(2);
		Samples[0].FrameNumber = 7;
		Samples[1].FrameNumber = 9;

		TestFalse(
			TEXT("Zero step rejected"),
			FDatasetFrameMapping::AssignFrameNumbers(Samples, FFrameNumber(100), FFrameNumber(0)));
		TestFalse(
			TEXT("Negative step rejected"),
			FDatasetFrameMapping::AssignFrameNumbers(Samples, FFrameNumber(100), FFrameNumber(-1)));
		TestEqual(TEXT("First sample untouched by rejection"), Samples[0].FrameNumber, 7);
		TestEqual(TEXT("Second sample untouched by rejection"), Samples[1].FrameNumber, 9);
	}

	// An empty run is not an error: a capture that recorded nothing assigns nothing.
	{
		TArray<FDatasetInputSample> Empty;
		TestTrue(
			TEXT("Empty run succeeds"),
			FDatasetFrameMapping::AssignFrameNumbers(Empty, FFrameNumber(0), FFrameNumber(1)));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetFrameMappingNoDriftTest,
	"KimodoDatasetCapture.FrameMapping.NoDriftAcrossATake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetFrameMappingNoDriftTest::RunTest(const FString& Parameters)
{
	// Ten seconds of NTSC 29.97 on a 24000-tick sequence: 300 samples, at 800.8 ticks per
	// frame. This is the case a fixed-step assignment gets wrong.
	const FFrameRate Display2997(30000, 1001);
	const FFrameRate Tick24000(24000, 1);
	const int32 SampleCount = FDatasetFrameMapping::SampleCountForDuration(10.0, Display2997);

	TestEqual(TEXT("Ten seconds of 29.97 is 300 samples"), SampleCount, 300);

	TArray<FDatasetInputSample> Exact;
	Exact.SetNum(SampleCount);
	TestTrue(
		TEXT("Rate-based assignment succeeds"),
		FDatasetFrameMapping::AssignFrameNumbersForRate(Exact, 0, Display2997, Tick24000));

	TArray<FDatasetInputSample> Stepped;
	Stepped.SetNum(SampleCount);
	TestTrue(
		TEXT("Fixed-step assignment succeeds"),
		FDatasetFrameMapping::AssignFrameNumbers(
			Stepped, FFrameNumber(0), FDatasetFrameMapping::TicksPerDisplayFrame(Display2997, Tick24000)));

	// Both agree at the start, by construction.
	TestEqual(TEXT("Both start at frame 0"), Exact[0].FrameNumber, 0);
	TestEqual(TEXT("Stepped also starts at frame 0"), Stepped[0].FrameNumber, 0);

	// Every exact frame must equal an independent conversion of its display frame. This is
	// the property that makes the assignment drift-free: each frame stands on its own.
	bool bAllExact = true;
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const int32 Expected = FDatasetFrameMapping::DisplayToTick(FFrameTime(Index), Display2997, Tick24000).Value;
		if (Exact[Index].FrameNumber != Expected)
		{
			AddError(FString::Printf(
				TEXT("Sample %d is at tick frame %d but its display frame converts to %d."),
				Index, Exact[Index].FrameNumber, Expected));
			bAllExact = false;
			break;
		}
	}
	TestTrue(TEXT("Every sample sits on its exactly converted frame"), bAllExact);

	// And the fixed step really does drift, so this test is testing something. The last
	// sample should be behind by very nearly 0.8 ticks per frame.
	const int32 Drift = Exact.Last().FrameNumber - Stepped.Last().FrameNumber;
	TestTrue(TEXT("Fixed stepping drifts behind over a take"), Drift > 0);
	TestEqual(TEXT("Drift over 300 frames of 29.97 is 239 ticks"), Drift, 239);

	// Frames must still be strictly increasing, or the sidecar would reject them.
	bool bStrictlyIncreasing = true;
	for (int32 Index = 1; Index < SampleCount; ++Index)
	{
		if (Exact[Index].FrameNumber <= Exact[Index - 1].FrameNumber)
		{
			AddError(FString::Printf(TEXT("Sample %d does not advance past sample %d."), Index, Index - 1));
			bStrictlyIncreasing = false;
			break;
		}
	}
	TestTrue(TEXT("Exactly converted frames strictly increase"), bStrictlyIncreasing);

	return true;
}
