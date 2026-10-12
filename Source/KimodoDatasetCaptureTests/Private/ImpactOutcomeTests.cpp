// Copyright Animotive. All Rights Reserved.

#include "ImpactOutcome.h"
#include "Misc/AutomationTest.h"

namespace ImpactOutcomeTests
{
	constexpr float Standing = 100.0f;

	/** N quiet frames at standing height. */
	TArray<FImpactFrameState> Quiet(int32 Count = 60)
	{
		TArray<FImpactFrameState> Frames;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FImpactFrameState Frame;
			Frame.PelvisHeightCm = Standing;
			Frame.StandingPelvisHeightCm = Standing;
			Frames.Add(Frame);
		}
		return Frames;
	}

	FImpactOutcomeResult Classify(const TArray<FImpactFrameState>& Frames)
	{
		return FImpactOutcomeClassifier::Classify(Frames, FImpactOutcomeThresholds());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactOutcomeClassesTest,
	"KimodoDatasetCapture.Impact.Outcome.Classes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactOutcomeClassesTest::RunTest(const FString& Parameters)
{
	using namespace ImpactOutcomeTests;

	// Flinch: nothing happens beyond a small give.
	TArray<FImpactFrameState> Flinch = Quiet();
	Flinch[10].PelvisHeightCm = 90.0f;
	const FImpactOutcomeResult A = Classify(Flinch);
	TestTrue(TEXT("Flinch trace is valid"), A.bValid);
	TestEqual(TEXT("Small give is a flinch"), A.Outcome, EImpactOutcome::Flinch);
	TestEqual(TEXT("Flinch records the lowest pelvis fraction"), A.MinPelvisFraction, 0.9f, 1e-4f);

	// Stagger by stepping, decided on the first frame with a step.
	TArray<FImpactFrameState> Stepped = Quiet();
	for (int32 Index = 12; Index < Stepped.Num(); ++Index)
	{
		Stepped[Index].Steps = Index < 30 ? 1 : 2;
	}
	const FImpactOutcomeResult B = Classify(Stepped);
	TestEqual(TEXT("Stepping is a stagger"), B.Outcome, EImpactOutcome::Stagger);
	TestEqual(TEXT("Stagger decided at the first step"), B.OutcomeFrame, 12);
	TestEqual(TEXT("Stagger records total steps"), B.Steps, 2);

	// Stagger without a step: capture point left the support.
	TArray<FImpactFrameState> Outside = Quiet();
	Outside[7].bOutsideSupport = true;
	const FImpactOutcomeResult C = Classify(Outside);
	TestEqual(TEXT("Leaving support is a stagger"), C.Outcome, EImpactOutcome::Stagger);
	TestEqual(TEXT("Decided when support was left"), C.OutcomeFrame, 7);

	// Knockdown by going down while still trying.
	TArray<FImpactFrameState> Down = Quiet();
	Down[5].Steps = 1;
	for (int32 Index = 20; Index < Down.Num(); ++Index)
	{
		Down[Index].PelvisHeightCm = 30.0f;
	}
	const FImpactOutcomeResult D = Classify(Down);
	TestEqual(TEXT("Going down is a knockdown"), D.Outcome, EImpactOutcome::Knockdown);
	TestEqual(TEXT("Knockdown decided when the pelvis went down"), D.OutcomeFrame, 20);

	// Knockdown by a late release: balance lost after trying.
	TArray<FImpactFrameState> LateRelease = Quiet();
	for (int32 Index = 15; Index < LateRelease.Num(); ++Index)
	{
		LateRelease[Index].bControlsReleased = true;
	}
	const FImpactOutcomeResult E = Classify(LateRelease);
	TestEqual(TEXT("A late release is a knockdown"), E.Outcome, EImpactOutcome::Knockdown);
	TestEqual(TEXT("Decided at the release"), E.OutcomeFrame, 15);

	// Ragdoll: released straight away.
	TArray<FImpactFrameState> Limp = Quiet();
	for (int32 Index = 1; Index < Limp.Num(); ++Index)
	{
		Limp[Index].bControlsReleased = true;
		Limp[Index].PelvisHeightCm = 20.0f;
	}
	const FImpactOutcomeResult F = Classify(Limp);
	TestEqual(TEXT("Immediate release is a ragdoll"), F.Outcome, EImpactOutcome::Ragdoll);
	TestEqual(TEXT("Ragdoll decided at the release"), F.OutcomeFrame, 1);

	// Trip is carried, whatever the class.
	TArray<FImpactFrameState> Tripped = LateRelease;
	Tripped[4].bTrip = true;
	TestTrue(TEXT("Trip recorded"), Classify(Tripped).bTrip);
	TestFalse(TEXT("No trip when none occurred"), E.bTrip);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactOutcomeBoundsTest,
	"KimodoDatasetCapture.Impact.Outcome.Bounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactOutcomeBoundsTest::RunTest(const FString& Parameters)
{
	using namespace ImpactOutcomeTests;
	const FImpactOutcomeThresholds Thresholds;

	auto WithDip = [](float Fraction)
	{
		TArray<FImpactFrameState> Frames = Quiet();
		Frames[10].PelvisHeightCm = Standing * Fraction;
		return Frames;
	};
	TestEqual(TEXT("Exactly the flinch bound is a flinch"), Classify(WithDip(Thresholds.FlinchPelvisFraction)).Outcome, EImpactOutcome::Flinch);
	TestEqual(TEXT("Just below the flinch bound is a stagger"), Classify(WithDip(Thresholds.FlinchPelvisFraction - 0.001f)).Outcome, EImpactOutcome::Stagger);
	TestEqual(TEXT("Exactly the knockdown bound is a stagger"), Classify(WithDip(Thresholds.KnockdownPelvisFraction)).Outcome, EImpactOutcome::Stagger);
	TestEqual(TEXT("Just below the knockdown bound is a knockdown"), Classify(WithDip(Thresholds.KnockdownPelvisFraction - 0.001f)).Outcome, EImpactOutcome::Knockdown);

	auto ReleasedAt = [](int32 Frame)
	{
		TArray<FImpactFrameState> Frames = Quiet();
		for (int32 Index = Frame; Index < Frames.Num(); ++Index)
		{
			Frames[Index].bControlsReleased = true;
		}
		return Frames;
	};
	TestEqual(TEXT("Release on the last immediate frame is a ragdoll"), Classify(ReleasedAt(Thresholds.ImmediateReleaseFrames)).Outcome, EImpactOutcome::Ragdoll);
	TestEqual(TEXT("Release one frame later is a knockdown"), Classify(ReleasedAt(Thresholds.ImmediateReleaseFrames + 1)).Outcome, EImpactOutcome::Knockdown);

	// Standing height varies with the source (a crouch is lower); fractions use each frame's own.
	TArray<FImpactFrameState> Crouch = Quiet();
	for (FImpactFrameState& Frame : Crouch)
	{
		Frame.StandingPelvisHeightCm = 60.0f;
		Frame.PelvisHeightCm = 58.0f;
	}
	TestEqual(TEXT("A low source is judged against its own height"), Classify(Crouch).Outcome, EImpactOutcome::Flinch);

	// Invalid traces.
	TestFalse(TEXT("Empty trace is invalid"), FImpactOutcomeClassifier::Classify({}, Thresholds).bValid);
	TArray<FImpactFrameState> NoStanding = Quiet();
	NoStanding[3].StandingPelvisHeightCm = 0.0f;
	TestFalse(TEXT("A frame without standing height is invalid"), Classify(NoStanding).bValid);

	// Deterministic.
	const TArray<FImpactFrameState> Same = WithDip(0.7f);
	const FImpactOutcomeResult First = Classify(Same);
	const FImpactOutcomeResult Second = Classify(Same);
	TestTrue(TEXT("Same trace, same result"), First.Outcome == Second.Outcome && First.OutcomeFrame == Second.OutcomeFrame);
	return true;
}
