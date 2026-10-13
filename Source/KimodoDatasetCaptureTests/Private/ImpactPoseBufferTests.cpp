// Copyright Animotive. All Rights Reserved.

#include "ImpactPoseBuffer.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactPoseBufferTest,
	"KimodoDatasetCapture.Impact.PoseBuffer.Reserve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactPoseBufferTest::RunTest(const FString& Parameters)
{
	// A full-length clip on the UEFN mannequin: the 240-frame reaction ceiling plus pre-roll.
	const int32 MaxFrames = 300;
	const int32 Bones = 92;
	FImpactPoseBuffer Buffer;
	Buffer.Reserve(MaxFrames, Bones);
	TestEqual(TEXT("Capacity is the reserved frame count"), Buffer.Capacity(), MaxFrames);
	TestEqual(TEXT("Bone count is kept"), Buffer.GetBoneCount(), Bones);
	TestEqual(TEXT("Starts empty"), Buffer.Num(), 0);

	// A whole clip fills without moving its storage.
	bool bEveryFrameSized = true;
	for (int32 Frame = 0; Frame < MaxFrames; ++Frame)
	{
		TArrayView<FTransform> Pose = Buffer.AddFrame();
		bEveryFrameSized &= Pose.Num() == Bones;
		for (int32 Bone = 0; Bone < Pose.Num(); ++Bone)
		{
			Pose[Bone] = FTransform(FVector(Frame, Bone, 0.0));
		}
	}
	TestTrue(TEXT("Every frame has one transform per bone"), bEveryFrameSized);
	TestEqual(TEXT("Every frame recorded"), Buffer.Num(), MaxFrames);
	TestTrue(TEXT("Full at capacity"), Buffer.IsFull());
	TestEqual(TEXT("A full clip never reallocates"), Buffer.GetReallocationCount(), 0);
	TestFalse(TEXT("Filling exactly to capacity is not an overflow"), Buffer.HasOverflowed());

	// Frames read back as written.
	const TArrayView<const FTransform> First = Buffer.GetFrame(0);
	const TArrayView<const FTransform> Last = Buffer.GetFrame(MaxFrames - 1);
	TestTrue(TEXT("First frame reads back"), First.Num() == Bones && First[5].GetLocation().Equals(FVector(0, 5, 0)));
	TestTrue(TEXT("Last frame reads back"), Last.Num() == Bones && Last[Bones - 1].GetLocation().Equals(FVector(MaxFrames - 1, Bones - 1, 0)));

	// Beyond the reserve: refused, recorded, explained; nothing already recorded changes.
	TestEqual(TEXT("A frame past capacity is refused"), Buffer.AddFrame().Num(), 0);
	TestTrue(TEXT("The overflow is recorded"), Buffer.HasOverflowed());
	TestTrue(TEXT("The reason names the capacity"), Buffer.GetOverflowReason().ToString().Contains(TEXT("300")));
	TestEqual(TEXT("Refusal adds no frame"), Buffer.Num(), MaxFrames);
	TestEqual(TEXT("Refusal does not reallocate"), Buffer.GetReallocationCount(), 0);

	// Reset keeps the reserve for the next clip.
	Buffer.Reset();
	TestEqual(TEXT("Reset empties"), Buffer.Num(), 0);
	TestFalse(TEXT("Reset clears the overflow"), Buffer.HasOverflowed());
	TestEqual(TEXT("Reset keeps capacity"), Buffer.Capacity(), MaxFrames);
	Buffer.AddFrame();
	TestEqual(TEXT("Reuse after reset does not reallocate"), Buffer.GetReallocationCount(), 0);

	// Out of range reads are empty, not a crash.
	TestEqual(TEXT("Reading past the end is empty"), Buffer.GetFrame(5).Num(), 0);
	TestEqual(TEXT("Reading a negative frame is empty"), Buffer.GetFrame(-1).Num(), 0);

	// A degenerate reserve holds nothing and refuses at once.
	FImpactPoseBuffer Empty;
	Empty.Reserve(0, Bones);
	TestEqual(TEXT("A zero-frame reserve refuses"), Empty.AddFrame().Num(), 0);
	TestTrue(TEXT("and records the overflow"), Empty.HasOverflowed());
	return true;
}
