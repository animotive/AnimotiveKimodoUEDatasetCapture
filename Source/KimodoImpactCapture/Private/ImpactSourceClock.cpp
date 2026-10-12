// Copyright Animotive. All Rights Reserved.

#include "ImpactSourceClock.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"

FImpactSourceClock::FImpactSourceClock(double LoopLengthSeconds, const FVector& RootTravelPerLoop, const FImpactSourceClockConfig& InConfig)
	: Config(InConfig)
	, LoopLength(FMath::Max(LoopLengthSeconds, UE_SMALL_NUMBER))
	, RootTravel(RootTravelPerLoop)
{
}

float FImpactSourceClock::NominalSpeedCmPerSecond() const
{
	return float(RootTravel.Size2D() / LoopLength);
}

bool FImpactSourceClock::IsMoving() const
{
	// Decided from the source's own root speed, never from the warped playback.
	return NominalSpeedCmPerSecond() > Config.MovingSpeedThresholdCmPerSecond;
}

void FImpactSourceClock::ApplyHit(int32 InHitFrame, float LostSpeedAlongTravelCmPerSecond)
{
	HitFrame = InHitFrame;
	if (!IsMoving())
	{
		Drop = 0.0f;
		return;
	}
	const float Fraction = LostSpeedAlongTravelCmPerSecond / FMath::Max(NominalSpeedCmPerSecond(), 1.0f);
	Drop = FMath::Clamp(Fraction * Config.DropGain, 0.0f, Config.MaxDrop);
}

float FImpactSourceClock::RateAt(int32 Frame) const
{
	if (HitFrame == INDEX_NONE || Drop <= 0.0f)
	{
		return 1.0f;
	}
	const int32 Start = HitFrame + Config.MeasureDelayFrames;
	if (Frame <= Start)
	{
		return 1.0f;
	}
	const float Recover = FMath::SmoothStep(0.0f, 1.0f, float(Frame - Start) / float(FMath::Max(1, Config.RecoverFrames)));
	return 1.0f - Drop * (1.0f - Recover);
}

double FImpactSourceClock::Advance(int32 Frame, float DeltaSeconds)
{
	Time += double(DeltaSeconds) * RateAt(Frame);
	return Time;
}

int32 FImpactSourceClock::LoopIndex(double InTime, double InLoopLength)
{
	if (InTime <= 0.0 || InLoopLength <= 0.0)
	{
		return 0;
	}
	return FMath::FloorToInt32(InTime / InLoopLength);
}

double FImpactSourceClock::LoopLocalTime(double InTime, double InLoopLength)
{
	if (InLoopLength <= 0.0)
	{
		return 0.0;
	}
	return FMath::Max(0.0, InTime - LoopIndex(InTime, InLoopLength) * InLoopLength);
}

FTransform FImpactSourceClock::LoopOffset(const FTransform& InLoopDelta, int32 Loops)
{
	FTransform Offset = FTransform::Identity;
	for (int32 Loop = 0; Loop < Loops; ++Loop)
	{
		Offset = Offset * InLoopDelta;
	}
	return Offset;
}

FImpactPoseSampler::FImpactPoseSampler(const USkeletalMesh& Mesh, const UAnimSequence& InSequence)
	: MeshRef(&Mesh.GetRefSkeleton())
	, Sequence(&InSequence)
	, Length(InSequence.GetPlayLength())
{
	const FReferenceSkeleton& SkeletonRef = InSequence.GetSkeleton()->GetReferenceSkeleton();
	for (int32 Bone = 0; Bone < MeshRef->GetNum(); ++Bone)
	{
		SkeletonIndex.Add(SkeletonRef.FindBoneIndex(MeshRef->GetBoneName(Bone)));
	}
	TArray<FTransform> Start, End;
	SampleLoop(0.0, Start);
	SampleLoop(Length, End);
	// The next loop's root is this loop's root re-based on where this loop ended.
	LoopDelta = Start[0].Inverse() * End[0];
}

void FImpactPoseSampler::SampleLoop(double InTime, TArray<FTransform>& OutComponentSpace) const
{
	const double Clamped = FMath::Clamp(InTime, 0.0, Length);
	OutComponentSpace.SetNum(MeshRef->GetNum());
	const TArray<FTransform>& RefPose = MeshRef->GetRefBonePose();
	const FAnimExtractContext Context(Clamped);
	for (int32 Bone = 0; Bone < MeshRef->GetNum(); ++Bone)
	{
		FTransform Local = RefPose[Bone];
		if (SkeletonIndex[Bone] != INDEX_NONE)
		{
			Sequence->GetBoneTransform(Local, FSkeletonPoseBoneIndex(SkeletonIndex[Bone]), Context, false);
		}
		const int32 Parent = MeshRef->GetParentIndex(Bone);
		OutComponentSpace[Bone] = Parent == INDEX_NONE ? Local : Local * OutComponentSpace[Parent];
	}
}

void FImpactPoseSampler::Sample(double InTime, TArray<FTransform>& OutComponentSpace) const
{
	const int32 Loops = FImpactSourceClock::LoopIndex(InTime, Length);
	SampleLoop(FImpactSourceClock::LoopLocalTime(InTime, Length), OutComponentSpace);
	const FTransform Offset = FImpactSourceClock::LoopOffset(LoopDelta, Loops);
	for (FTransform& Transform : OutComponentSpace)
	{
		Transform = Transform * Offset;
	}
}
