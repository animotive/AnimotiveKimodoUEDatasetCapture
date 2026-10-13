// Copyright Animotive. All Rights Reserved.

#include "ImpactBalancePlanner.h"
#include "Misc/AutomationTest.h"

namespace ImpactFallTripTests
{
	const FVector LeftFoot(5, -11, 9.3);
	const FVector RightFoot(-5, 11, 9.3);
	const FVector Com(0, 0, 96);

	FImpactBalanceConfig Unsmoothed()
	{
		FImpactBalanceConfig Config;
		Config.ExcessSmoothing = 1.0f;
		return Config;
	}

	/** Running along +X at Speed; the hit removes nothing unless Excess says so. */
	FImpactBalanceInput Running(float Speed, int32 StruckLeg = INDEX_NONE, const FVector& Excess = FVector::ZeroVector)
	{
		FImpactBalanceInput Input;
		Input.bMovingSource = true;
		Input.Com = Com;
		Input.SourceVelocity = FVector(Speed, 0, 0);
		Input.ComVelocity = Input.SourceVelocity + Excess;
		Input.FootPosition[0] = LeftFoot;
		Input.FootPosition[1] = RightFoot;
		Input.StruckLegSide = StruckLeg;
		return Input;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactOverwhelmTest,
	"KimodoDatasetCapture.Impact.FallTrip.Overwhelming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactOverwhelmTest::RunTest(const FString& Parameters)
{
	const FImpactFallRules Rules;
	TestTrue(TEXT("A car (62k on 75 kg) overwhelms"), Rules.IsOverwhelming(62000.0f, 75.0f, 1500.0f));
	TestFalse(TEXT("A rifle round (30k on 75 kg, chest, spread) does not"), Rules.IsOverwhelming(30000.0f, 75.0f, 1500.0f));
	TestTrue(TEXT("Exactly the body threshold overwhelms"), Rules.IsOverwhelming(Rules.OverwhelmBodyDeltaVCmPerSecond * 75.0f, 75.0f, 0.0f));
	TestFalse(TEXT("Just below the body threshold does not"), Rules.IsOverwhelming(Rules.OverwhelmBodyDeltaVCmPerSecond * 75.0f - 1.0f, 75.0f, 0.0f));
	TestTrue(TEXT("A struck part driven at the threshold overwhelms"), Rules.IsOverwhelming(1000.0f, 75.0f, Rules.OverwhelmStruckBodyDeltaVCmPerSecond));
	TestFalse(TEXT("Just below it does not"), Rules.IsOverwhelming(1000.0f, 75.0f, Rules.OverwhelmStruckBodyDeltaVCmPerSecond - 1.0f));
	TestFalse(TEXT("Zero body mass never overwhelms (no divide by zero)"), Rules.IsOverwhelming(1.0e6f, 0.0f, 0.0f));

	// The planner releases straight away: the classifier sees a ragdoll.
	FImpactBalancePlanner Planner(ImpactFallTripTests::Unsmoothed());
	Planner.ReleaseImmediately();
	FImpactBalanceInput Standing;
	Standing.Com = ImpactFallTripTests::Com;
	Standing.FootPosition[0] = ImpactFallTripTests::LeftFoot;
	Standing.FootPosition[1] = ImpactFallTripTests::RightFoot;
	const FImpactBalanceOutput First = Planner.Step(Standing);
	TestTrue(TEXT("Released on the first frame"), First.bFell && First.bJustFell);
	TestFalse(TEXT("No foot is held once released"), First.Feet[0].bPinned || First.Feet[1].bPinned);
	const FImpactBalanceOutput Second = Planner.Step(Standing);
	TestFalse(TEXT("Release reported once"), Second.bJustFell);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactTripTest,
	"KimodoDatasetCapture.Impact.FallTrip.Trip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactTripTest::RunTest(const FString& Parameters)
{
	using namespace ImpactFallTripTests;
	const FImpactBalanceConfig Config = Unsmoothed();

	// Sprinting, shot in the left foot: the foot is caught, the body carries on, and it goes down.
	{
		FImpactBalancePlanner Planner(Config);
		FImpactBalanceOutput Out = Planner.Step(Running(500.0f, 0));
		TestTrue(TEXT("A leg hit on a run is a trip"), Out.bTrip);
		TestTrue(TEXT("The struck foot is caught where it was"), Out.Feet[0].bPinned && Out.Feet[0].Position.Equals(LeftFoot, 1e-3));
		TestFalse(TEXT("The other foot keeps running"), Out.Feet[1].bPinned);
		int32 Frames = 1;
		while (!Out.bFell && Frames < Config.TripHoldFrames)
		{
			Out = Planner.Step(Running(500.0f, 0));
			++Frames;
		}
		TestTrue(TEXT("A sprinter tripped by the foot falls"), Out.bFell);
		TestTrue(TEXT("Falls forward: the capture point is ahead of the caught foot"), Out.CapturePoint.X > LeftFoot.X + Config.MovingFallReachCm);
		TestTrue(TEXT("Trip still recorded after the fall"), Out.bTrip);
	}

	// Walking pace, same hit: the trip is caught and the character carries on.
	{
		FImpactBalancePlanner Planner(Config);
		FImpactBalanceOutput Out;
		for (int32 Index = 0; Index < Config.TripHoldFrames + 5; ++Index)
		{
			Out = Planner.Step(Running(100.0f, 1));
		}
		TestTrue(TEXT("Slow trip recorded"), Out.bTrip);
		TestFalse(TEXT("A slow trip is recovered"), Out.bFell);
		TestFalse(TEXT("The caught foot is released after the hold"), Out.Feet[1].bPinned);
	}

	// A run hit somewhere other than a leg is not a trip.
	{
		FImpactBalancePlanner Planner(Config);
		TestFalse(TEXT("Chest hit on a run is no trip"), Planner.Step(Running(500.0f)).bTrip);
	}

	// A standing character hit in the leg is handled by stepping, not as a trip.
	{
		FImpactBalancePlanner Planner(Config);
		FImpactBalanceInput Standing;
		Standing.Com = Com;
		Standing.FootPosition[0] = LeftFoot;
		Standing.FootPosition[1] = RightFoot;
		Standing.StruckLegSide = 0;
		TestFalse(TEXT("Leg hit while standing is no trip"), Planner.Step(Standing).bTrip);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactMovingFallTest,
	"KimodoDatasetCapture.Impact.FallTrip.MovingBalance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactMovingFallTest::RunTest(const FString& Parameters)
{
	using namespace ImpactFallTripTests;
	const FImpactBalanceConfig Config = Unsmoothed();

	// A heavy side hit on a runner: excess momentum beyond a leg's reach is a fall.
	{
		FImpactBalancePlanner Planner(Config);
		const FImpactBalanceOutput Out = Planner.Step(Running(500.0f, INDEX_NONE, FVector(0, 800, 0)));
		TestTrue(TEXT("Heavy side hit on a run falls"), Out.bFell);
		TestFalse(TEXT("Not a trip"), Out.bTrip);
	}

	// A modest chest hit: the run decelerates and recovers.
	{
		FImpactBalancePlanner Planner(Config);
		FImpactBalanceOutput Out;
		for (int32 Index = 0; Index < 30; ++Index)
		{
			Out = Planner.Step(Running(500.0f, INDEX_NONE, FVector(-90, 0, 0)));
		}
		TestFalse(TEXT("Modest hit on a run recovers"), Out.bFell);
	}

	// Exactly at reach is still caught; just beyond is not.
	{
		const float Lean = FMath::Sqrt(float(Com.Z) / Config.GravityCmPerSecondSquared);
		FImpactBalancePlanner AtReach(Config), Beyond(Config);
		TestFalse(TEXT("At reach is caught"), AtReach.Step(Running(500.0f, INDEX_NONE, FVector(0, Config.MovingFallReachCm / Lean, 0))).bFell);
		TestTrue(TEXT("Beyond reach falls"), Beyond.Step(Running(500.0f, INDEX_NONE, FVector(0, (Config.MovingFallReachCm + 1.0f) / Lean, 0))).bFell);
	}
	return true;
}
