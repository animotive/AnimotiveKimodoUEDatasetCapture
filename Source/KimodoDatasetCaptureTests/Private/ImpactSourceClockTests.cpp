// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "ImpactSourceClock.h"
#include "Misc/AutomationTest.h"

namespace ImpactSourceClockTests
{
	const FImpactSourceClockConfig Config;
	// A 2.5 s run loop travelling 12.5 m along +Y, as the GASP forward run does.
	FImpactSourceClock Run() { return FImpactSourceClock(2.5, FVector(0, 1250, 0), Config); }
	FImpactSourceClock Idle() { return FImpactSourceClock(10.0, FVector::ZeroVector, Config); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactSourceClockClassifyTest,
	"KimodoDatasetCapture.Impact.SourceClock.Classification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactSourceClockClassifyTest::RunTest(const FString& Parameters)
{
	using namespace ImpactSourceClockTests;
	TestTrue(TEXT("A run is a moving source"), Run().IsMoving());
	TestEqual(TEXT("Run's nominal speed"), Run().NominalSpeedCmPerSecond(), 500.0f, 1e-3f);
	TestFalse(TEXT("An idle is a standing source"), Idle().IsMoving());

	// Exactly at the threshold is standing; just above is moving.
	TestFalse(TEXT("At the threshold is standing"), FImpactSourceClock(1.0, FVector(Config.MovingSpeedThresholdCmPerSecond, 0, 0), Config).IsMoving());
	TestTrue(TEXT("Above the threshold is moving"), FImpactSourceClock(1.0, FVector(Config.MovingSpeedThresholdCmPerSecond + 1.0f, 0, 0), Config).IsMoving());

	// Decided once: slowing the run to a crawl never makes it standing.
	FImpactSourceClock Slowed = Run();
	Slowed.ApplyHit(0, 1000.0f);
	TestTrue(TEXT("Maximum slowdown applied"), Slowed.GetDrop() == Config.MaxDrop);
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		Slowed.Advance(Frame, 1.0f / 60.0f);
	}
	TestTrue(TEXT("A slowed run is still a moving source"), Slowed.IsMoving());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactSourceClockWarpTest,
	"KimodoDatasetCapture.Impact.SourceClock.TimeWarp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactSourceClockWarpTest::RunTest(const FString& Parameters)
{
	using namespace ImpactSourceClockTests;

	// Drop proportional to the speed lost along travel: 91 of 516 -> 0.44 at gain 2.5 (spike run 30k).
	FImpactSourceClock A = FImpactSourceClock(2.5, FVector(0, 1290, 0), Config);
	A.ApplyHit(30, 91.0f);
	TestEqual(TEXT("Drop proportional to lost speed"), A.GetDrop(), 91.0f / 516.0f * Config.DropGain, 1e-3f);

	FImpactSourceClock Clamped = Run();
	Clamped.ApplyHit(30, 10000.0f);
	TestEqual(TEXT("Drop clamped"), Clamped.GetDrop(), Config.MaxDrop);
	FImpactSourceClock Sped = Run();
	Sped.ApplyHit(30, -200.0f);
	TestEqual(TEXT("A hit from behind never speeds playback up"), Sped.GetDrop(), 0.0f);
	FImpactSourceClock Still = Idle();
	Still.ApplyHit(30, 200.0f);
	TestEqual(TEXT("Standing sources are not warped"), Still.GetDrop(), 0.0f);

	// Timing: full speed until the drop applies, then recovering over RecoverFrames.
	FImpactSourceClock Clock = Run();
	Clock.ApplyHit(30, 200.0f);
	const int32 Start = 30 + Config.MeasureDelayFrames;
	TestEqual(TEXT("Full speed before the hit"), Clock.RateAt(10), 1.0f);
	TestEqual(TEXT("Full speed until the drop applies"), Clock.RateAt(Start), 1.0f);
	TestEqual(TEXT("Dropped just after"), Clock.RateAt(Start + 1), 1.0f - Clock.GetDrop(), 0.01f);
	TestEqual(TEXT("Recovered after RecoverFrames"), Clock.RateAt(Start + Config.RecoverFrames), 1.0f, 1e-4f);
	bool bMonotonic = true;
	for (int32 Frame = Start + 2; Frame <= Start + Config.RecoverFrames; ++Frame)
	{
		bMonotonic &= Clock.RateAt(Frame) >= Clock.RateAt(Frame - 1) - 1e-6f;
	}
	TestTrue(TEXT("Recovery never slows down again"), bMonotonic);

	// Advance integrates the rate: an unwarped clock is plain time.
	FImpactSourceClock Plain = Run();
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Plain.Advance(Frame, 1.0f / 60.0f);
	}
	TestEqual(TEXT("Unwarped clock advances one second in 60 frames"), Plain.GetTime(), 1.0, 1e-6);
	FImpactSourceClock Warped = Run();
	Warped.ApplyHit(10, 200.0f);
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Warped.Advance(Frame, 1.0f / 60.0f);
	}
	TestTrue(TEXT("Warped clock falls behind"), Warped.GetTime() < Plain.GetTime() - 0.05);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactSourceClockLoopTest,
	"KimodoDatasetCapture.Impact.SourceClock.Loops",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactSourceClockLoopTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Loop 0 at the start"), FImpactSourceClock::LoopIndex(0.0, 2.5), 0);
	TestEqual(TEXT("Loop 0 just before the end"), FImpactSourceClock::LoopIndex(2.49, 2.5), 0);
	TestEqual(TEXT("Loop 1 exactly at the end"), FImpactSourceClock::LoopIndex(2.5, 2.5), 1);
	TestEqual(TEXT("Loop 2 later"), FImpactSourceClock::LoopIndex(6.0, 2.5), 2);
	TestEqual(TEXT("Local time wraps"), FImpactSourceClock::LoopLocalTime(6.0, 2.5), 1.0, 1e-9);
	TestEqual(TEXT("Local time at a loop boundary is zero"), FImpactSourceClock::LoopLocalTime(5.0, 2.5), 0.0, 1e-9);
	TestEqual(TEXT("Negative time is loop 0"), FImpactSourceClock::LoopIndex(-1.0, 2.5), 0);

	// Root motion carried: each loop re-based on where the last ended.
	const FTransform Delta(FQuat(FVector::UpVector, FMath::DegreesToRadians(10.0)), FVector(0, 1250, 0));
	TestTrue(TEXT("No loops, no offset"), FImpactSourceClock::LoopOffset(Delta, 0).Equals(FTransform::Identity, 1e-6));
	TestTrue(TEXT("One loop is the delta"), FImpactSourceClock::LoopOffset(Delta, 1).Equals(Delta, 1e-6));
	TestTrue(TEXT("Two loops compose"), FImpactSourceClock::LoopOffset(Delta, 2).Equals(Delta * Delta, 1e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactPoseSamplerTest,
	"KimodoDatasetCapture.Impact.SourceClock.PoseSamplerGASP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactPoseSamplerTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin"));
	UAnimSequence* Run = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F.M_Neutral_Run_Loop_F"));
	UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop.M_Neutral_Stand_Idle_Loop"));
	if (!Mesh || !Run || !Idle)
	{
		AddWarning(TEXT("GameAnimationSample content not found; pose sampler check skipped."));
		return true;
	}

	const FImpactPoseSampler RunSampler(*Mesh, *Run);
	const FImpactPoseSampler IdleSampler(*Mesh, *Idle);
	TArray<FTransform> Pose;
	RunSampler.Sample(0.0, Pose);
	if (!TestEqual(TEXT("One transform per mesh bone"), Pose.Num(), Mesh->GetRefSkeleton().GetNum()))
	{
		// Indexing a wrong-sized pose would crash the editor and take the rest of the suite with it.
		return true;
	}

	// The forward run travels 12.5 m per 2.5 s loop along +Y (Phase 1 spike), the idle stays put.
	TestEqual(TEXT("Run loop length"), RunSampler.GetLength(), 2.5, 0.01);
	TestEqual(TEXT("Run root travel per loop"), float(RunSampler.GetLoopDelta().GetLocation().Size2D()), 1250.0f, 5.0f);
	TestTrue(TEXT("Idle root does not travel"), IdleSampler.GetLoopDelta().GetLocation().Size2D() < 1.0);
	const FImpactSourceClock RunClock(RunSampler.GetLength(), RunSampler.GetLoopDelta().GetLocation(), FImpactSourceClockConfig());
	TestTrue(TEXT("Real run classified moving"), RunClock.IsMoving());

	// Continuity across the loop boundary: the pelvis does not jump back to the start.
	const int32 Pelvis = Mesh->GetRefSkeleton().FindBoneIndex(TEXT("pelvis"));
	TArray<FTransform> Before, After;
	RunSampler.Sample(RunSampler.GetLength() - 1.0 / 120.0, Before);
	RunSampler.Sample(RunSampler.GetLength() + 1.0 / 120.0, After);
	TestTrue(TEXT("Pelvis continuous across the loop"), FVector::Dist(Before[Pelvis].GetLocation(), After[Pelvis].GetLocation()) < 15.0);

	// Two loops carry twice the travel.
	TArray<FTransform> Start, TwoLoops;
	RunSampler.Sample(0.0, Start);
	RunSampler.Sample(2.0 * RunSampler.GetLength(), TwoLoops);
	TestEqual(TEXT("Two loops travel twice as far"), float(FVector::Dist2D(Start[Pelvis].GetLocation(), TwoLoops[Pelvis].GetLocation())), 2500.0f, 10.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactSourceStartWindowTest,
	"KimodoDatasetCapture.Impact.SourceClock.PlayOnceStartWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactSourceStartWindowTest::RunTest(const FString& Parameters)
{
	const double Length = 3.6;   // the Kimodo jump on the spot
	const double LeadIn = 31.0 / 60.0;

	// A looping source starts anywhere in its loop, as before.
	TestEqual(TEXT("Looping: phase 0 starts at 0"), FImpactSourceClock::StartSeconds(0.0, Length, false, LeadIn), 0.0, 1e-9);
	TestEqual(TEXT("Looping: phase 0.5 starts mid-loop"), FImpactSourceClock::StartSeconds(0.5, Length, false, LeadIn), 1.8, 1e-9);

	// A play-once source: the hit lands between its start and half a second before its end.
	for (const double Phase : { 0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0 })
	{
		const double Hit = FImpactSourceClock::StartSeconds(Phase, Length, true, LeadIn) + LeadIn;
		TestTrue(*FString::Printf(TEXT("Play-once, phase %.2f: the hit lands at %.2f s, inside the action"), Phase, Hit),
			Hit >= -1e-9 && Hit <= Length - 0.5 + 1e-9);
	}
	TestEqual(TEXT("Phase 0 hits at the very start"), FImpactSourceClock::StartSeconds(0.0, Length, true, LeadIn) + LeadIn, 0.0, 1e-9);
	TestEqual(TEXT("Phase 1 hits half a second before the end"), FImpactSourceClock::StartSeconds(1.0, Length, true, LeadIn) + LeadIn, Length - 0.5, 1e-9);
	TestTrue(TEXT("Hits spread with the phase"), FImpactSourceClock::StartSeconds(0.75, Length, true, LeadIn) > FImpactSourceClock::StartSeconds(0.25, Length, true, LeadIn));
	TestEqual(TEXT("A source shorter than the tail hits at its start"), FImpactSourceClock::StartSeconds(0.8, 0.3, true, LeadIn) + LeadIn, 0.0, 1e-9);
	return true;
}

