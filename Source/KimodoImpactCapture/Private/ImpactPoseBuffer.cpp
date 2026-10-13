// Copyright Animotive. All Rights Reserved.

#include "ImpactPoseBuffer.h"

#define LOCTEXT_NAMESPACE "ImpactPoseBuffer"

void FImpactPoseBuffer::Reserve(int32 MaxFrames, int32 InBoneCount)
{
	MaxFrameCount = FMath::Max(0, MaxFrames);
	BoneCount = FMath::Max(0, InBoneCount);
	Transforms.Reset();
	Transforms.SetNumUninitialized(MaxFrameCount * BoneCount);
	LastKnownData = Transforms.GetData();
	ReallocationCount = 0;
	FrameCount = 0;
	bOverflowed = false;
}

TArrayView<FTransform> FImpactPoseBuffer::AddFrame()
{
	if (IsFull())
	{
		bOverflowed = true;
		return TArrayView<FTransform>();
	}
	// The block is sized once by Reserve; frames are views into it, so nothing here can allocate.
	if (Transforms.GetData() != LastKnownData)
	{
		++ReallocationCount;
		LastKnownData = Transforms.GetData();
	}
	TArrayView<FTransform> Frame(Transforms.GetData() + FrameCount * BoneCount, BoneCount);
	++FrameCount;
	return Frame;
}

int32 FImpactPoseBuffer::Num() const { return FrameCount; }
int32 FImpactPoseBuffer::Capacity() const { return MaxFrameCount; }
int32 FImpactPoseBuffer::GetBoneCount() const { return BoneCount; }
bool FImpactPoseBuffer::IsFull() const { return FrameCount >= MaxFrameCount; }
bool FImpactPoseBuffer::HasOverflowed() const { return bOverflowed; }

FText FImpactPoseBuffer::GetOverflowReason() const
{
	return bOverflowed
		? FText::Format(LOCTEXT("Overflow", "The clip ran past the {0} frames reserved for it; its poses were not all recorded."), FText::AsNumber(MaxFrameCount))
		: FText();
}

TArrayView<const FTransform> FImpactPoseBuffer::GetFrame(int32 Frame) const
{
	if (Frame < 0 || Frame >= FrameCount)
	{
		return TArrayView<const FTransform>();
	}
	return TArrayView<const FTransform>(Transforms.GetData() + Frame * BoneCount, BoneCount);
}

void FImpactPoseBuffer::Reset()
{
	FrameCount = 0;
	bOverflowed = false;
}

int32 FImpactPoseBuffer::GetReallocationCount() const { return ReallocationCount; }

#undef LOCTEXT_NAMESPACE
