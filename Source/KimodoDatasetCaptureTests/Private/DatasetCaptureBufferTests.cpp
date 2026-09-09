// Copyright Animotive. All Rights Reserved.

#include "DatasetCaptureBuffer.h"
#include "DatasetFrameMapping.h"
#include "Misc/AutomationTest.h"

namespace
{
	FDatasetInputSample SampleAt(int32 FrameNumber)
	{
		FDatasetInputSample Sample;
		Sample.FrameNumber = FrameNumber;
		return Sample;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetCaptureBufferReservationTest,
	"KimodoDatasetCapture.CaptureBuffer.ReservesOnceAndNeverGrows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetCaptureBufferReservationTest::RunTest(const FString& Parameters)
{
	// The real capture size: ten seconds at 60fps, taken from the same helper the recorder
	// will use, so this test breaks if that arithmetic ever changes.
	const int32 MaxSamples = FDatasetFrameMapping::SampleCountForDuration(10.0, FFrameRate(60, 1));
	TestEqual(TEXT("A ten second take at 60fps is 600 samples"), MaxSamples, 600);

	FDatasetCaptureBuffer Buffer;
	Buffer.Reserve(MaxSamples);

	TestEqual(TEXT("Capacity reserved"), Buffer.Capacity(), MaxSamples);
	TestEqual(TEXT("Buffer starts empty"), Buffer.Num(), 0);
	TestFalse(TEXT("Empty buffer is not full"), Buffer.IsFull());
	TestEqual(TEXT("No reallocation from reserving"), Buffer.GetReallocationCount(), 0);

	// A whole take's worth of samples.
	for (int32 Index = 0; Index < MaxSamples; ++Index)
	{
		if (!Buffer.Add(SampleAt(Index)))
		{
			AddError(FString::Printf(TEXT("Sample %d was refused before the buffer was full."), Index));
			break;
		}
	}

	TestEqual(TEXT("Every sample stored"), Buffer.Num(), MaxSamples);
	TestTrue(TEXT("Buffer is now full"), Buffer.IsFull());

	// The assertion this class exists for. A comment claiming "no allocation in the hot path"
	// is unverifiable; a reallocation counter is not.
	TestEqual(TEXT("A full take allocated nothing"), Buffer.GetReallocationCount(), 0);

	// Full means full: the buffer refuses rather than growing, because growing is the
	// allocation this is all avoiding. That refusal is also what stops the take.
	TestFalse(TEXT("Sample past capacity is refused"), Buffer.Add(SampleAt(MaxSamples)));
	TestEqual(TEXT("Refused sample was not stored"), Buffer.Num(), MaxSamples);
	TestEqual(TEXT("Refusal did not allocate"), Buffer.GetReallocationCount(), 0);

	// Reset keeps the capacity, so a session of fifty takes allocates once rather than fifty
	// times.
	Buffer.Reset();
	TestEqual(TEXT("Reset empties the buffer"), Buffer.Num(), 0);
	TestEqual(TEXT("Reset keeps the capacity"), Buffer.Capacity(), MaxSamples);
	TestFalse(TEXT("Reset buffer is not full"), Buffer.IsFull());

	for (int32 Index = 0; Index < MaxSamples; ++Index)
	{
		Buffer.Add(SampleAt(Index * 2));
	}
	TestEqual(TEXT("Second take fills again"), Buffer.Num(), MaxSamples);
	TestEqual(TEXT("Second take allocated nothing either"), Buffer.GetReallocationCount(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetCaptureBufferOrderTest,
	"KimodoDatasetCapture.CaptureBuffer.RejectsNonAdvancingFrames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetCaptureBufferOrderTest::RunTest(const FString& Parameters)
{
	FDatasetCaptureBuffer Buffer;
	Buffer.Reserve(8);

	TestTrue(TEXT("Frame 100 accepted"), Buffer.Add(SampleAt(100)));
	TestTrue(TEXT("Frame 101 accepted"), Buffer.Add(SampleAt(101)));

	// The same invariant the sidecar enforces, applied where the samples enter. Enforcing it
	// twice is deliberate: this is the layer that can still report a capture fault, and the
	// sidecar is the layer that must never ship a broken stream.
	TestFalse(TEXT("Repeated frame refused"), Buffer.Add(SampleAt(101)));
	TestFalse(TEXT("Earlier frame refused"), Buffer.Add(SampleAt(100)));
	TestEqual(TEXT("Refusals stored nothing"), Buffer.Num(), 2);

	// A gap is fine: a dropped frame is still in order.
	TestTrue(TEXT("Frame after a gap accepted"), Buffer.Add(SampleAt(110)));
	TestEqual(TEXT("Gap sample stored"), Buffer.Num(), 3);

	// Negative frames are legal in MovieScene, so a take can start before zero.
	FDatasetCaptureBuffer NegativeBuffer;
	NegativeBuffer.Reserve(4);
	TestTrue(TEXT("Negative frame accepted"), NegativeBuffer.Add(SampleAt(-10)));
	TestTrue(TEXT("Advancing negative frame accepted"), NegativeBuffer.Add(SampleAt(-9)));
	TestFalse(TEXT("Non-advancing negative frame refused"), NegativeBuffer.Add(SampleAt(-9)));

	// The view must expose exactly what was accepted, in order.
	const TArrayView<const FDatasetInputSample> Samples = Buffer.GetSamples();
	TestEqual(TEXT("View has three samples"), Samples.Num(), 3);
	if (Samples.Num() == 3)
	{
		TestEqual(TEXT("First is frame 100"), Samples[0].FrameNumber, 100);
		TestEqual(TEXT("Second is frame 101"), Samples[1].FrameNumber, 101);
		TestEqual(TEXT("Third is frame 110"), Samples[2].FrameNumber, 110);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetCaptureBufferUnreservedTest,
	"KimodoDatasetCapture.CaptureBuffer.UnreservedBufferAcceptsNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetCaptureBufferUnreservedTest::RunTest(const FString& Parameters)
{
	// A buffer nobody reserved has zero capacity, so it is already full and accepts nothing.
	// This makes "forgot to reserve" a loud, immediate refusal rather than a capture that
	// quietly allocates on its first frame.
	FDatasetCaptureBuffer Buffer;

	TestEqual(TEXT("Unreserved capacity is zero"), Buffer.Capacity(), 0);
	TestTrue(TEXT("Unreserved buffer is full"), Buffer.IsFull());
	TestFalse(TEXT("Unreserved buffer refuses samples"), Buffer.Add(SampleAt(0)));
	TestEqual(TEXT("Nothing was stored"), Buffer.Num(), 0);
	TestEqual(TEXT("Nothing was allocated"), Buffer.GetReallocationCount(), 0);

	// Reserving zero or a negative count is the same as not reserving.
	Buffer.Reserve(0);
	TestTrue(TEXT("Zero reservation is full"), Buffer.IsFull());
	Buffer.Reserve(-5);
	TestEqual(TEXT("Negative reservation clamps to zero"), Buffer.Capacity(), 0);
	TestFalse(TEXT("Still refuses samples"), Buffer.Add(SampleAt(0)));

	return true;
}
