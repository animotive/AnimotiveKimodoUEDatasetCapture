// Copyright Animotive. All Rights Reserved.

#include "ImpactPoseBuffer.h"

void FImpactPoseBuffer::Reserve(int32 MaxFrames, int32 InBoneCount)
{
}

TArrayView<FTransform> FImpactPoseBuffer::AddFrame()
{
	return TArrayView<FTransform>();
}

int32 FImpactPoseBuffer::Num() const { return 0; }
int32 FImpactPoseBuffer::Capacity() const { return 0; }
int32 FImpactPoseBuffer::GetBoneCount() const { return 0; }
bool FImpactPoseBuffer::IsFull() const { return false; }
bool FImpactPoseBuffer::HasOverflowed() const { return false; }
FText FImpactPoseBuffer::GetOverflowReason() const { return FText(); }
TArrayView<const FTransform> FImpactPoseBuffer::GetFrame(int32 Frame) const { return TArrayView<const FTransform>(); }
void FImpactPoseBuffer::Reset() {}
int32 FImpactPoseBuffer::GetReallocationCount() const { return 0; }
