// Copyright Animotive. All Rights Reserved.

#include "ImpactBalancePlanner.h"
#include "Misc/AutomationTest.h"

namespace ImpactBalanceTests
{
	// Left foot slightly ahead, right foot slightly behind, so "farther foot" is never a tie.
	const FVector LeftFoot(5, -11, 9.3);
	const FVector RightFoot(-5, 11, 9.3);
	const FVector StandingCom(0, 0, 96);

	FImpactBalanceConfig Unsmoothed()
	{
		FImpactBalanceConfig Config;
		Config.ExcessSmoothing = 1.0f; // geometry tests see the raw excess
		return Config;
	}

	/** Pushed backwards (-X) at Speed, standing source. */
	FImpactBalanceInput Pushed(float Speed, const FVector& Com = StandingCom)
	{
		FImpactBalanceInput Input;
		Input.Com = Com;
		Input.ComVelocity = FVector(-Speed, 0, 0);
		Input.FootPosition[0] = LeftFoot;
		Input.FootPosition[1] = RightFoot;
		return Input;
	}

	float Lean(float Speed, float ComHeight = 96.0f)
	{
		return Speed * FMath::Sqrt(ComHeight / 980.0f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactBalanceCapturePointTest,
	"KimodoDatasetCapture.Impact.Balance.CapturePoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactBalanceCapturePointTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBalanceTests;

	// Formula: CoM + v x sqrt(h / g).
	const FVector CP = FImpactBalancePlanner::CapturePoint(StandingCom, FVector(-100, 50, 0), 980.0f);
	TestEqual(TEXT("Capture point X"), float(CP.X), -Lean(100.0f), 1e-3f);
	TestEqual(TEXT("Capture point Y"), float(CP.Y), Lean(50.0f), 1e-3f);
	TestTrue(TEXT("At rest the capture point is under the CoM"), FImpactBalancePlanner::CapturePoint(StandingCom, FVector::ZeroVector, 980.0f).Equals(StandingCom, 1e-4));

	// Low-pass: a one-frame spike moves the capture point by only the smoothing share.
	FImpactBalancePlanner Smoothed{ FImpactBalanceConfig() };
	const FImpactBalanceOutput First = Smoothed.Step(Pushed(500.0f));
	TestEqual(TEXT("Smoothed excess on the first frame is 0.3 of the spike"), float(First.CapturePoint.X), -0.3f * Lean(500.0f), 0.01f);
	const FImpactBalanceOutput Second = Smoothed.Step(Pushed(0.0f));
	TestEqual(TEXT("And decays the frame after"), float(Second.CapturePoint.X), -0.7f * 0.3f * Lean(500.0f), 0.01f);

	// Inside the trigger: feet pinned where they stood, nothing steps.
	FImpactBalancePlanner Calm(Unsmoothed());
	FImpactBalanceOutput Out;
	for (int32 Index = 0; Index < 30; ++Index)
	{
		Out = Calm.Step(Pushed(30.0f)); // lean ~9 cm, under the 15 cm trigger
	}
	TestTrue(TEXT("Feet pinned"), Out.Feet[0].bPinned && Out.Feet[1].bPinned);
	TestTrue(TEXT("Left foot held where it stood"), Out.Feet[0].Position.Equals(LeftFoot, 1e-3));
	TestTrue(TEXT("Right foot held where it stood"), Out.Feet[1].Position.Equals(RightFoot, 1e-3));
	TestEqual(TEXT("No step inside the trigger"), Out.Steps, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactBalanceSteppingTest,
	"KimodoDatasetCapture.Impact.Balance.Stepping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactBalanceSteppingTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBalanceTests;
	const FImpactBalanceConfig Config = Unsmoothed();
	FImpactBalancePlanner Planner(Config);

	// A push the feet cannot hold: the left foot (farther from the capture point) steps.
	FImpactBalanceOutput Out = Planner.Step(Pushed(100.0f));
	TestTrue(TEXT("Left foot starts swinging"), Out.Feet[0].bSwing);
	TestFalse(TEXT("Right foot stays planted"), Out.Feet[1].bSwing);

	// Landing: at the capture point, offset sideways by half the stance width, at ground height.
	const FVector Landing(-Lean(100.0f), -Config.StanceHalfWidthCm, LeftFoot.Z);
	for (int32 Index = 1; Index < Config.SwingFrames; ++Index)
	{
		Out = Planner.Step(Pushed(100.0f));
		TestTrue(TEXT("Swing foot lifts above the ground mid-swing"), Index != Config.SwingFrames / 2 || Out.Feet[0].Position.Z > LeftFoot.Z + 5.0);
	}
	TestEqual(TEXT("One step landed"), Out.Steps, 1);
	TestTrue(TEXT("Lands at the capture point with stance width"), Out.Feet[0].Position.Equals(Landing, 0.01));
	TestFalse(TEXT("Swing finished"), Out.Feet[0].bSwing);

	// Minimum stance: a bigger push right after landing waits MinStanceFrames before stepping.
	int32 WaitedFrames = 0;
	for (; WaitedFrames < 40; ++WaitedFrames)
	{
		Out = Planner.Step(Pushed(260.0f));
		if (Out.Feet[0].bSwing || Out.Feet[1].bSwing)
		{
			break;
		}
	}
	TestEqual(TEXT("Next step waits out the minimum stance"), WaitedFrames + 1, Config.MinStanceFrames);

	// Alternation: the second step uses the other foot.
	TestTrue(TEXT("Second step uses the right foot"), Out.Feet[1].bSwing && !Out.Feet[0].bSwing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactBalanceFallTest,
	"KimodoDatasetCapture.Impact.Balance.Falls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactBalanceFallTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBalanceTests;

	// Reach clamp: a capture point far out of reach is stepped toward as far as a leg goes.
	{
		const FImpactBalanceConfig Config = Unsmoothed();
		FImpactBalancePlanner Planner(Config);
		FImpactBalanceOutput Out;
		for (int32 Index = 0; Index < Config.SwingFrames; ++Index)
		{
			Out = Planner.Step(Pushed(1000.0f));
		}
		TestEqual(TEXT("First step landed"), Out.Steps, 1);
		TestEqual(TEXT("Step clamped to a leg's reach"), float(FVector::Dist2D(Out.Feet[0].Position, LeftFoot)), Config.MaxStepCm, 0.01f);

		// Repeated short steps: balance lost after more than MaxUncaughtSteps.
		bool bFell = false;
		int32 FellAtSteps = -1;
		for (int32 Index = 0; Index < 400 && !bFell; ++Index)
		{
			Out = Planner.Step(Pushed(1000.0f));
			bFell = Out.bFell;
			FellAtSteps = Out.Steps;
		}
		TestTrue(TEXT("Repeated short steps end in a fall"), bFell);
		TestEqual(TEXT("Falls when the next short step would exceed the limit"), FellAtSteps, Config.MaxUncaughtSteps);
	}

	// Step budget: reachable steps that never settle still end in a fall.
	{
		FImpactBalanceConfig Config = Unsmoothed();
		Config.MaxSteps = 1;
		FImpactBalancePlanner Planner(Config);
		FImpactBalanceOutput Out;
		for (int32 Index = 0; Index < 60 && !Out.bFell; ++Index)
		{
			Out = Planner.Step(Pushed(Index < Config.SwingFrames ? 100.0f : 200.0f));
		}
		TestTrue(TEXT("Exceeding the step budget is a fall"), Out.bFell);
		TestEqual(TEXT("Only the budgeted step was taken"), Out.Steps, 1);
	}

	// Low centre of mass: going down, at once, reported once.
	{
		FImpactBalancePlanner Planner(Unsmoothed());
		const FImpactBalanceOutput Down = Planner.Step(Pushed(0.0f, FVector(0, 0, 50)));
		TestTrue(TEXT("Low CoM is a fall"), Down.bFell);
		TestTrue(TEXT("Fall reported on its frame"), Down.bJustFell);
		const FImpactBalanceOutput After = Planner.Step(Pushed(0.0f, FVector(0, 0, 40)));
		TestTrue(TEXT("Still fallen"), After.bFell);
		TestFalse(TEXT("Fall reported only once"), After.bJustFell);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactBalanceSettleTest,
	"KimodoDatasetCapture.Impact.Balance.SettleAndFooting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactBalanceSettleTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBalanceTests;
	const FImpactBalanceConfig Config = Unsmoothed();

	// Calm: after SettleHoldFrames calm frames the feet blend back over SettleBlendFrames.
	{
		FImpactBalancePlanner Planner(Config);
		FImpactBalanceOutput Out;
		for (int32 Index = 0; Index < Config.SettleHoldFrames; ++Index)
		{
			Out = Planner.Step(Pushed(0.0f));
		}
		TestTrue(TEXT("Settling begins after the hold"), Out.bSettling);
		TestEqual(TEXT("Blend starts at zero"), Out.Feet[0].SettleAlpha, 0.0f, 1e-4f);
		for (int32 Index = 0; Index < Config.SettleBlendFrames / 2; ++Index)
		{
			Out = Planner.Step(Pushed(0.0f));
		}
		TestEqual(TEXT("Halfway through the blend"), Out.Feet[0].SettleAlpha, 0.5f, 0.01f);
		for (int32 Index = 0; Index < Config.SettleBlendFrames; ++Index)
		{
			Out = Planner.Step(Pushed(0.0f));
		}
		TestEqual(TEXT("Blend completes"), Out.Feet[1].SettleAlpha, 1.0f, 1e-4f);
		TestEqual(TEXT("Settled without stepping"), Out.Steps, 0);
	}

	// Footing: after a step, the target frame ends centred over the new feet.
	{
		FImpactBalancePlanner Planner(Config);
		FImpactBalanceOutput Out;
		for (int32 Index = 0; Index < Config.SwingFrames; ++Index)
		{
			Out = Planner.Step(Pushed(100.0f));
		}
		// The body that stepped back now stands over its new feet, at rest.
		const FVector NewFooting = (Out.Feet[0].Position + Out.Feet[1].Position) * 0.5;
		const FVector RestedCom(NewFooting.X, NewFooting.Y, StandingCom.Z);
		for (int32 Index = 0; Index < 300; ++Index)
		{
			Out = Planner.Step(Pushed(0.0f, RestedCom));
		}
		const FVector Before = (LeftFoot + RightFoot) * 0.5;
		const FVector After = (Out.Feet[0].Position + Out.Feet[1].Position) * 0.5;
		const FVector Expected(After.X - Before.X, After.Y - Before.Y, 0.0);
		TestEqual(TEXT("A step was taken"), Out.Steps, 1);
		TestTrue(TEXT("Target frame ends over the new footing"), Out.Offset.Equals(Expected, 0.5));
		TestEqual(TEXT("Target frame never moves vertically"), float(Out.Offset.Z), 0.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactBalanceMovingTest,
	"KimodoDatasetCapture.Impact.Balance.MovingSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactBalanceMovingTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBalanceTests;

	// A run: travelling +X; the hit adds sideways momentum and removes forward momentum.
	FImpactBalanceInput Input;
	Input.bMovingSource = true;
	Input.Com = StandingCom;
	Input.SourceVelocity = FVector(500, 0, 0);
	Input.ComVelocity = FVector(400, 60, 0);
	Input.FootPosition[0] = LeftFoot;
	Input.FootPosition[1] = RightFoot;

	FImpactBalancePlanner Planner(Unsmoothed());
	FImpactBalanceOutput Out;
	for (int32 Index = 0; Index < 30; ++Index)
	{
		Out = Planner.Step(Input);
	}
	TestFalse(TEXT("A run's feet are not pinned by the standing planner"), Out.Feet[0].bPinned || Out.Feet[1].bPinned);
	TestEqual(TEXT("No standing steps on a run"), Out.Steps, 0);
	// Only the sideways component is carried; deceleration along travel belongs to the time warp.
	TestEqual(TEXT("No offset along the direction of travel"), float(Out.Offset.X), 0.0f, 1e-3f);
	TestEqual(TEXT("Sideways momentum carried"), float(Out.Offset.Y), 60.0f * Input.DeltaSeconds * 30, 0.01f);

	// Deterministic.
	FImpactBalancePlanner A(Unsmoothed()), B(Unsmoothed());
	FImpactBalanceOutput OutA, OutB;
	for (int32 Index = 0; Index < 50; ++Index)
	{
		OutA = A.Step(Pushed(150.0f));
		OutB = B.Step(Pushed(150.0f));
	}
	TestTrue(TEXT("Same inputs, same plan"), OutA.Feet[0].Position.Equals(OutB.Feet[0].Position, 0.0) && OutA.Offset.Equals(OutB.Offset, 0.0) && OutA.Steps == OutB.Steps);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactBalanceFallHeightTest,
	"KimodoDatasetCapture.Impact.Balance.FallHeightFollowsSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactBalanceFallHeightTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBalanceTests;
	auto FallsAt = [](float ComHeight, float SourcePelvisHeight)
	{
		FImpactBalancePlanner Planner(Unsmoothed());
		FImpactBalanceInput Input = Pushed(0.0f, FVector(0, 0, ComHeight));
		Input.SourcePelvisHeightCm = SourcePelvisHeight;
		return Planner.Step(Input).bJustFell;
	};
	// Standing: the centre of mass at 50 cm is going down, as before.
	TestTrue(TEXT("Standing source, centre of mass at 50 cm: falling"), FallsAt(50.0f, 95.0f));
	TestFalse(TEXT("Standing source, centre of mass at 70 cm: not falling"), FallsAt(70.0f, 95.0f));
	// A crouch carries its centre of mass low already.
	TestFalse(TEXT("Crouching source (pelvis 60 cm), centre of mass at 50 cm: not falling"), FallsAt(50.0f, 60.0f));
	TestTrue(TEXT("Crouching source, centre of mass at 30 cm: falling"), FallsAt(30.0f, 60.0f));
	// A jump in the air carries it high: the same fraction holds.
	TestTrue(TEXT("Source in the air (pelvis 150 cm), centre of mass at 70 cm: falling"), FallsAt(70.0f, 150.0f));
	// No source height given: the fixed height applies.
	TestTrue(TEXT("Unknown source height, 50 cm: falling"), FallsAt(50.0f, 0.0f));
	TestFalse(TEXT("Unknown source height, 60 cm: not falling"), FallsAt(60.0f, 0.0f));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactBalanceTripGateTest,
	"KimodoDatasetCapture.Impact.Balance.TripNeedsAHitThatStopsTheLeg",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactBalanceTripGateTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBalanceTests;
	const FImpactBalanceConfig Config = Unsmoothed();
	// A jog at 375 cm/s: half the stride's speed is 187.5 cm/s.
	auto Trips = [&Config](int32 LegSide, float StruckDeltaV)
	{
		FImpactBalanceInput Input;
		Input.bMovingSource = true;
		Input.Com = StandingCom;
		Input.SourceVelocity = FVector(375, 0, 0);
		Input.ComVelocity = Input.SourceVelocity;
		Input.FootPosition[0] = LeftFoot;
		Input.FootPosition[1] = RightFoot;
		Input.StruckLegSide = LegSide;
		Input.StruckLegDeltaVCmPerSecond = StruckDeltaV;
		FImpactBalancePlanner Planner(Config);
		return Planner.Step(Input).bTrip;
	};
	TestFalse(TEXT("A thrown stone's tap on the shin (20 cm/s) does not trip a jogger"), Trips(0, 20.0f));
	TestFalse(TEXT("Just under half the stride's speed does not trip"), Trips(1, 187.0f));
	TestTrue(TEXT("Half the stride's speed trips (bounds count)"), Trips(1, 187.5f));
	TestTrue(TEXT("A kick that stops the leg (400 cm/s) trips"), Trips(0, 400.0f));
	TestFalse(TEXT("A hit that struck no leg never trips"), Trips(INDEX_NONE, 1000.0f));
	return true;
}
