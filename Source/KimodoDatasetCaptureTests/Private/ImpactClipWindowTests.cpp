// Copyright Animotive. All Rights Reserved.

#include "ImpactClipWindow.h"
#include "Misc/AutomationTest.h"

namespace ImpactClipWindowTests
{
	constexpr float Moving = 200.0f;
	constexpr float Still = 1.0f;

	FImpactClipWindowConfig Config(int32 PreRoll = 10, int32 MaxReaction = 100, int32 Settle = 5, int32 LastHit = 0)
	{
		FImpactClipWindowConfig C;
		C.PreRollFrames = PreRoll;
		C.MaxReactionFrames = MaxReaction;
		C.SettleFrames = Settle;
		C.LastHitOffsetFrames = LastHit;
		return C;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactClipWindowPhasesTest,
	"KimodoDatasetCapture.Impact.ClipWindow.Phases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactClipWindowPhasesTest::RunTest(const FString& Parameters)
{
	using namespace ImpactClipWindowTests;
	FImpactClipWindow Window(Config(10, 100, 5));

	// Pre-roll: exactly PreRollFrames frames, then the hit.
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		TestEqual(*FString::Printf(TEXT("Frame %d runs in pre-roll"), Frame), Window.PhaseOfNextFrame(), EImpactClipPhase::PreRoll);
		TestFalse(*FString::Printf(TEXT("Frame %d is not the hit"), Frame), Window.IsNextFrameFirstHit());
		Window.Advance(Still);
	}
	TestTrue(TEXT("Frame 10 is the first hit"), Window.IsNextFrameFirstHit());
	TestEqual(TEXT("The hit frame runs in reaction"), Window.PhaseOfNextFrame(), EImpactClipPhase::Reaction);
	Window.Advance(Moving);
	TestFalse(TEXT("Only one first-hit frame"), Window.IsNextFrameFirstHit());

	// Moving: stays in reaction.
	for (int32 Index = 0; Index < 20; ++Index)
	{
		Window.Advance(Moving);
	}
	TestEqual(TEXT("Moving body stays in reaction"), Window.GetPhase(), EImpactClipPhase::Reaction);

	// Quiet: settle counts, and ends after exactly SettleFrames quiet frames.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Window.Advance(Still);
		TestEqual(TEXT("Counting quiet frames"), Window.GetPhase(), EImpactClipPhase::Settle);
	}
	Window.Advance(Still);
	TestEqual(TEXT("Done after the fifth quiet frame"), Window.GetPhase(), EImpactClipPhase::Done);
	TestEqual(TEXT("Ended by settling"), Window.GetEnd(), EImpactClipEnd::Settled);
	TestEqual(TEXT("Clip length: 10 pre-roll + 1 hit + 20 moving + 5 quiet"), Window.GetFramesConsumed(), 36);

	// Done is final.
	Window.Advance(Moving);
	TestEqual(TEXT("Advancing after Done changes nothing"), Window.GetFramesConsumed(), 36);
	TestEqual(TEXT("Still settled"), Window.GetEnd(), EImpactClipEnd::Settled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactClipWindowSettleTest,
	"KimodoDatasetCapture.Impact.ClipWindow.Settle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactClipWindowSettleTest::RunTest(const FString& Parameters)
{
	using namespace ImpactClipWindowTests;

	// A single moving frame resets the count.
	FImpactClipWindow Reset(Config(0, 100, 5));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Reset.Advance(Still);
	}
	Reset.Advance(Moving);
	TestEqual(TEXT("Motion resets the settle count"), Reset.GetPhase(), EImpactClipPhase::Reaction);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Reset.Advance(Still);
	}
	TestNotEqual(TEXT("Four quiet frames after a reset are not enough"), Reset.GetPhase(), EImpactClipPhase::Done);
	Reset.Advance(Still);
	TestEqual(TEXT("Five consecutive quiet frames settle"), Reset.GetEnd(), EImpactClipEnd::Settled);

	// A quiet body cannot settle before the last scheduled hit has landed.
	FImpactClipWindow Burst(Config(0, 100, 3, 12));
	for (int32 Index = 0; Index < 12; ++Index)
	{
		Burst.Advance(Still);
	}
	TestEqual(TEXT("No settling while hits remain"), Burst.GetPhase(), EImpactClipPhase::Reaction);
	// Frame 12 is the last hit; it and the two after it are the three quiet frames.
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Burst.Advance(Still);
	}
	TestEqual(TEXT("Settles three quiet frames from the last hit"), Burst.GetEnd(), EImpactClipEnd::Settled);
	TestEqual(TEXT("Burst clip length"), Burst.GetFramesConsumed(), 15);

	// The threshold is exclusive: exactly at it counts as moving.
	FImpactClipWindowConfig AtThreshold = Config(0, 100, 2);
	FImpactClipWindow Edge(AtThreshold);
	Edge.Advance(AtThreshold.SettleSpeedCmPerSecond);
	Edge.Advance(AtThreshold.SettleSpeedCmPerSecond);
	TestEqual(TEXT("Speed equal to the threshold is not quiet"), Edge.GetPhase(), EImpactClipPhase::Reaction);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactClipWindowCeilingTest,
	"KimodoDatasetCapture.Impact.ClipWindow.CeilingAndInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactClipWindowCeilingTest::RunTest(const FString& Parameters)
{
	using namespace ImpactClipWindowTests;

	// Ceiling: exactly MaxReactionFrames frames after the hit, and recorded.
	FImpactClipWindow Ceiling(Config(10, 50, 5));
	for (int32 Index = 0; Index < 59; ++Index)
	{
		Ceiling.Advance(Moving);
	}
	TestNotEqual(TEXT("Not done one frame before the ceiling"), Ceiling.GetPhase(), EImpactClipPhase::Done);
	Ceiling.Advance(Moving);
	TestEqual(TEXT("Done at the ceiling"), Ceiling.GetPhase(), EImpactClipPhase::Done);
	TestEqual(TEXT("Ceiling recorded as the end"), Ceiling.GetEnd(), EImpactClipEnd::ReactionCeiling);
	TestEqual(TEXT("Clip length at the ceiling"), Ceiling.GetFramesConsumed(), 60);

	// Invalidation ends the clip with its reason, at any phase.
	FImpactClipWindow Broken(Config(10, 50, 5));
	Broken.Advance(Still);
	Broken.Invalidate(INVTEXT("Physics produced a non-finite transform."));
	TestEqual(TEXT("Invalidated clip is done"), Broken.GetPhase(), EImpactClipPhase::Done);
	TestEqual(TEXT("Invalidated recorded as the end"), Broken.GetEnd(), EImpactClipEnd::Invalidated);
	TestEqual(TEXT("Reason kept"), Broken.GetInvalidReason().ToString(), FString(TEXT("Physics produced a non-finite transform.")));
	Broken.Advance(Still);
	TestEqual(TEXT("No frames consumed after invalidation"), Broken.GetFramesConsumed(), 1);

	// No pre-roll: the very first frame is the hit.
	FImpactClipWindow Immediate(Config(0, 50, 5));
	TestTrue(TEXT("Zero pre-roll hits on the first frame"), Immediate.IsNextFrameFirstHit());

	// Config validation.
	FText Reason;
	TestTrue(TEXT("Sound config validates"), Config().Validate(Reason));
	TestFalse(TEXT("Zero reaction ceiling rejected"), Config(10, 0, 5).Validate(Reason));
	TestFalse(TEXT("Zero settle frames rejected"), Config(10, 50, 0).Validate(Reason));
	TestFalse(TEXT("Negative pre-roll rejected"), Config(-1, 50, 5).Validate(Reason));
	TestFalse(TEXT("Last hit beyond the ceiling rejected"), Config(10, 50, 5, 60).Validate(Reason));
	TestFalse(TEXT("Rejection states a reason"), Reason.IsEmpty());
	return true;
}
