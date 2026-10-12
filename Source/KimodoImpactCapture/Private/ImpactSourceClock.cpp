// Copyright Animotive. All Rights Reserved.

#include "ImpactSourceClock.h"

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"

// Red: stubs returning neutral values so the tests compile and fail.

FImpactSourceClock::FImpactSourceClock(double LoopLengthSeconds, const FVector& RootTravelPerLoop, const FImpactSourceClockConfig& InConfig)
	: Config(InConfig)
{
}

bool FImpactSourceClock::IsMoving() const { return false; }
float FImpactSourceClock::NominalSpeedCmPerSecond() const { return 0.0f; }
void FImpactSourceClock::ApplyHit(int32 InHitFrame, float LostSpeedAlongTravelCmPerSecond) {}
float FImpactSourceClock::RateAt(int32 Frame) const { return 0.0f; }
double FImpactSourceClock::Advance(int32 Frame, float DeltaSeconds) { return 0.0; }
int32 FImpactSourceClock::LoopIndex(double InTime, double InLoopLength) { return -1; }
double FImpactSourceClock::LoopLocalTime(double InTime, double InLoopLength) { return -1.0; }
FTransform FImpactSourceClock::LoopOffset(const FTransform& InLoopDelta, int32 Loops) { return FTransform(FVector(1, 2, 3)); }

FImpactPoseSampler::FImpactPoseSampler(const USkeletalMesh& Mesh, const UAnimSequence& InSequence) {}
void FImpactPoseSampler::SampleLoop(double InTime, TArray<FTransform>& OutComponentSpace) const { OutComponentSpace.Reset(); }
void FImpactPoseSampler::Sample(double InTime, TArray<FTransform>& OutComponentSpace) const { OutComponentSpace.Reset(); }
