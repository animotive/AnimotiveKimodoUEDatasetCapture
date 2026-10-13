// Copyright Animotive. All Rights Reserved.

#include "ImpactJointLimits.h"
#include "Misc/AutomationTest.h"

namespace ImpactJointLimitTests
{
	FQuat About(const FVector& Axis, float Degrees)
	{
		return FQuat(Axis, FMath::DegreesToRadians(Degrees));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactJointAnglesTest,
	"KimodoDatasetCapture.Impact.JointLimits.Angles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactJointAnglesTest::RunTest(const FString& Parameters)
{
	using namespace ImpactJointLimitTests;

	const FImpactJointAngles Rest = FImpactJointAngles::FromRelative(FQuat::Identity);
	TestTrue(TEXT("Identity has no angles"), FMath::IsNearlyZero(Rest.TwistDeg) && FMath::IsNearlyZero(Rest.Swing1Deg) && FMath::IsNearlyZero(Rest.Swing2Deg));

	const FImpactJointAngles Twisted = FImpactJointAngles::FromRelative(About(FVector::XAxisVector, 30.0f));
	TestEqual(TEXT("Rotation about X is twist"), Twisted.TwistDeg, 30.0f, 0.01f);
	TestEqual(TEXT("Pure twist has no swing1"), Twisted.Swing1Deg, 0.0f, 0.01f);
	TestEqual(TEXT("Pure twist has no swing2"), Twisted.Swing2Deg, 0.0f, 0.01f);

	const FImpactJointAngles Bent = FImpactJointAngles::FromRelative(About(FVector::ZAxisVector, 40.0f));
	TestEqual(TEXT("Rotation about Z is swing1"), Bent.Swing1Deg, 40.0f, 0.01f);
	TestEqual(TEXT("Swing1 has no twist"), Bent.TwistDeg, 0.0f, 0.01f);

	const FImpactJointAngles Back = FImpactJointAngles::FromRelative(About(FVector::ZAxisVector, -62.0f));
	TestEqual(TEXT("Negative swing1 keeps its sign"), Back.Swing1Deg, -62.0f, 0.01f);

	const FImpactJointAngles Tilted = FImpactJointAngles::FromRelative(About(FVector::YAxisVector, 25.0f));
	TestEqual(TEXT("Rotation about Y is swing2"), Tilted.Swing2Deg, 25.0f, 0.01f);
	TestEqual(TEXT("Swing2 has no swing1"), Tilted.Swing1Deg, 0.0f, 0.01f);

	// Swing applied after twist: both recovered.
	const FImpactJointAngles Both = FImpactJointAngles::FromRelative(About(FVector::ZAxisVector, 30.0f) * About(FVector::XAxisVector, 20.0f));
	TestEqual(TEXT("Twist recovered under swing"), Both.TwistDeg, 20.0f, 0.01f);
	TestEqual(TEXT("Swing recovered with twist"), Both.Swing1Deg, 30.0f, 0.01f);

	// Sign of the quaternion does not matter.
	const FQuat Q = About(FVector::ZAxisVector, 50.0f) * About(FVector::XAxisVector, -15.0f);
	const FImpactJointAngles Positive = FImpactJointAngles::FromRelative(Q);
	const FImpactJointAngles Negative = FImpactJointAngles::FromRelative(FQuat(-Q.X, -Q.Y, -Q.Z, -Q.W));
	TestEqual(TEXT("q and -q give the same twist"), Negative.TwistDeg, Positive.TwistDeg, 0.01f);
	TestEqual(TEXT("q and -q give the same swing1"), Negative.Swing1Deg, Positive.Swing1Deg, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactJointRangeTest,
	"KimodoDatasetCapture.Impact.JointLimits.Range",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactJointRangeTest::RunTest(const FString& Parameters)
{
	using namespace ImpactJointLimitTests;
	// The UEFN hip: limited 80 either side of its frame, standing at 45. A ragdoll's hip bends
	// 90 one way and 15 the other from standing.
	FImpactJointRange Hip;
	Hip.MinDeg = -90.0f;
	Hip.MaxDeg = 15.0f;
	float Centre = 0.0f, Half = 0.0f;
	TestTrue(TEXT("The hip range fits the asset"), Hip.Fit(80.0f, 45.0f, Centre, Half));
	TestEqual(TEXT("Centre of -45..60"), Centre, 7.5f, 1e-4f);
	TestEqual(TEXT("Half-width of -45..60"), Half, 52.5f, 1e-4f);

	// Never wider than the asset.
	FImpactJointRange Wide;
	Wide.MinDeg = -150.0f;
	Wide.MaxDeg = 50.0f;
	TestTrue(TEXT("A wide range fits"), Wide.Fit(80.0f, 45.0f, Centre, Half));
	TestEqual(TEXT("Clipped to the asset: centre"), Centre, 0.0f, 1e-4f);
	TestEqual(TEXT("Clipped to the asset: half-width"), Half, 80.0f, 1e-4f);

	// No overlap: nothing to fit.
	FImpactJointRange Outside;
	Outside.MinDeg = 50.0f;
	Outside.MaxDeg = 60.0f;
	TestFalse(TEXT("A range wholly outside the asset does not fit"), Outside.Fit(80.0f, 45.0f, Centre, Half));

	// Turning the parent frame moves the measured swing1 by the centre.
	const FQuat Parent = About(FVector::YAxisVector, 20.0f) * About(FVector::XAxisVector, 35.0f);
	const FQuat Child = Parent * About(FVector::ZAxisVector, 40.0f);
	const FQuat Turned = FImpactJointRange::TurnSwing1(Parent, 7.5f);
	const FImpactJointAngles Before = FImpactJointAngles::FromRelative(Parent.Inverse() * Child);
	const FImpactJointAngles After = FImpactJointAngles::FromRelative(Turned.Inverse() * Child);
	TestEqual(TEXT("Swing1 before turning"), Before.Swing1Deg, 40.0f, 0.01f);
	TestEqual(TEXT("Swing1 after turning is measured from the centre"), After.Swing1Deg, 32.5f, 0.01f);
	TestEqual(TEXT("Turning leaves twist alone"), After.TwistDeg, 0.0f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactJointLimitExcessTest,
	"KimodoDatasetCapture.Impact.JointLimits.Excess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactJointLimitExcessTest::RunTest(const FString& Parameters)
{
	// The UEFN knee: a 70 degree hinge, the other swing and twist locked.
	FImpactJointLimit Knee;
	Knee.Swing1Deg = 70.0f;
	Knee.Swing2Deg = 0.0f;
	Knee.TwistDeg = 0.0f;

	auto Angles = [](float Twist, float Swing1, float Swing2)
	{
		FImpactJointAngles A;
		A.TwistDeg = Twist;
		A.Swing1Deg = Swing1;
		A.Swing2Deg = Swing2;
		return A;
	};

	TestEqual(TEXT("Within the hinge range"), Knee.Excess(Angles(0, 60, 0)), 0.0f);
	TestEqual(TEXT("Exactly on the limit"), Knee.Excess(Angles(0, -70, 0)), 0.0f);
	TestEqual(TEXT("Past the hinge, positive side"), Knee.Excess(Angles(0, 80, 0)), 10.0f, 1e-4f);
	TestEqual(TEXT("Past the hinge, negative side"), Knee.Excess(Angles(0, -85, 0)), 15.0f, 1e-4f);
	TestEqual(TEXT("Sideways bend on a locked axis"), Knee.Excess(Angles(0, 0, -12)), 12.0f, 1e-4f);
	TestEqual(TEXT("Twist on a locked axis"), Knee.Excess(Angles(7, 0, 0)), 7.0f, 1e-4f);
	TestEqual(TEXT("The worst axis wins"), Knee.Excess(Angles(5, 75, -20)), 20.0f, 1e-4f);

	// Both swings limited: the limit is an ellipse, so two swings each within their own limit can
	// still be past it together.
	FImpactJointLimit Shoulder;
	Shoulder.Swing1Deg = 70.0f;
	Shoulder.Swing2Deg = 60.0f;
	Shoulder.TwistDeg = 30.0f;
	TestEqual(TEXT("Inside the ellipse"), Shoulder.Excess(Angles(0, 40, 30)), 0.0f);
	TestEqual(TEXT("On the ellipse's swing1 axis"), Shoulder.Excess(Angles(0, -70, 0)), 0.0f, 1e-3f);
	TestEqual(TEXT("Past the ellipse together"), Shoulder.Excess(Angles(0, 60, 50)), 12.75f, 0.05f);
	TestEqual(TEXT("Past the ellipse on one axis"), Shoulder.Excess(Angles(0, 0, -65)), 5.0f, 1e-3f);

	TestTrue(TEXT("The knee is a hinge"), Knee.IsHinge());
	TestFalse(TEXT("The shoulder is not a hinge"), Shoulder.IsHinge());
	FImpactJointLimit Clavicle;
	Clavicle.Swing1Deg = 15.0f;
	Clavicle.Swing2Deg = 20.0f;
	Clavicle.TwistDeg = 0.0f;
	TestFalse(TEXT("Two limited swings with locked twist is not a hinge"), Clavicle.IsHinge());

	const FImpactJointLimit Free;
	TestEqual(TEXT("A free joint never exceeds"), Free.Excess(Angles(179, -179, 179)), 0.0f);
	TestFalse(TEXT("A free joint is not a hinge"), Free.IsHinge());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactJointLimitExcessBeyondTest,
	"KimodoDatasetCapture.Impact.JointLimits.ExcessBeyondSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactJointLimitExcessBeyondTest::RunTest(const FString& Parameters)
{
	auto Angles = [](float Twist, float Swing1, float Swing2)
	{
		FImpactJointAngles A;
		A.TwistDeg = Twist;
		A.Swing1Deg = Swing1;
		A.Swing2Deg = Swing2;
		return A;
	};

	// A source inside the limit changes nothing.
	FImpactJointLimit Knee;
	Knee.TwistDeg = 0.0f;
	Knee.Swing1Deg = 70.0f;
	Knee.Swing2Deg = 0.0f;
	const FImpactJointAngles Inside = Angles(0, 40, 0);
	for (const FImpactJointAngles& Sim : { Angles(0, 60, 0), Angles(0, 80, 0), Angles(0, 0, -12), Angles(5, 75, -20) })
	{
		TestEqual(TEXT("A source within the limit: the plain excess"), Knee.ExcessBeyond(Sim, Inside), Knee.Excess(Sim), 1e-4f);
	}

	// A Kimodo elbow authored 29 degrees past the hinge: holding it is not a fold.
	const FImpactJointAngles Authored = Angles(0, 99, 0);
	TestEqual(TEXT("At the source's own angle: no excess"), Knee.ExcessBeyond(Angles(0, 99, 0), Authored), 0.0f, 1e-4f);
	TestEqual(TEXT("Short of the source's angle: no excess"), Knee.ExcessBeyond(Angles(0, 85, 0), Authored), 0.0f, 1e-4f);
	TestEqual(TEXT("Past the source's angle: only the physics' part"), Knee.ExcessBeyond(Angles(0, 109, 0), Authored), 10.0f, 1e-4f);
	TestEqual(TEXT("The other side keeps the asset limit"), Knee.ExcessBeyond(Angles(0, -80, 0), Authored), 10.0f, 1e-4f);
	TestEqual(TEXT("A sideways fold on a locked axis is still caught"), Knee.ExcessBeyond(Angles(0, 99, 25), Authored), 25.0f, 1e-4f);
	TestEqual(TEXT("A twist on a locked axis is still caught"), Knee.ExcessBeyond(Angles(9, 99, 0), Authored), 9.0f, 1e-4f);

	// An elliptical cone: an upper arm authored outside it widens the whole cone to reach it.
	FImpactJointLimit Shoulder;
	Shoulder.TwistDeg = 30.0f;
	Shoulder.Swing1Deg = 40.0f;
	Shoulder.Swing2Deg = 30.0f;
	const FImpactJointAngles Hanging = Angles(0, 0, 45);   // 15 past a 30 swing2: the cone scales by 1.5
	TestEqual(TEXT("Cone at the source: no excess"), Shoulder.ExcessBeyond(Angles(0, 0, 45), Hanging), 0.0f, 1e-3f);
	TestEqual(TEXT("Cone past the source"), Shoulder.ExcessBeyond(Angles(0, 0, 55), Hanging), 10.0f, 1e-3f);
	TestEqual(TEXT("Cone scaled on the other swing too"), Shoulder.ExcessBeyond(Angles(0, 60, 0), Hanging), 0.0f, 1e-3f);
	TestEqual(TEXT("and past it"), Shoulder.ExcessBeyond(Angles(0, 70, 0), Hanging), 10.0f, 1e-3f);
	TestEqual(TEXT("The plain cone would call the source a fold"), Shoulder.Excess(Hanging), 15.0f, 1e-3f);

	// Twist past its limit in the source.
	const FImpactJointAngles Twisted = Angles(50, 0, 0);
	TestEqual(TEXT("Twist at the source's"), Shoulder.ExcessBeyond(Angles(50, 0, 0), Twisted), 0.0f, 1e-4f);
	TestEqual(TEXT("Twist past the source's"), Shoulder.ExcessBeyond(Angles(60, 0, 0), Twisted), 10.0f, 1e-4f);
	TestEqual(TEXT("Twist the other way keeps the asset limit"), Shoulder.ExcessBeyond(Angles(-50, 0, 0), Twisted), 20.0f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactJointLimitExcessBeyondAnyTest,
	"KimodoDatasetCapture.Impact.JointLimits.ExcessBeyondRecentTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactJointLimitExcessBeyondAnyTest::RunTest(const FString& Parameters)
{
	auto Angles = [](float Twist, float Swing1, float Swing2)
	{
		FImpactJointAngles A;
		A.TwistDeg = Twist;
		A.Swing1Deg = Swing1;
		A.Swing2Deg = Swing2;
		return A;
	};
	FImpactJointLimit Elbow;
	Elbow.TwistDeg = 0.0f;
	Elbow.Swing1Deg = 70.0f;
	Elbow.Swing2Deg = 0.0f;

	// A sprinting arm swings back from 99 degrees; the held elbow trails, still at 97, while this
	// frame's target is already 80.
	const TArray<FImpactJointAngles> Recent = { Angles(0, 99, 0), Angles(0, 92, 0), Angles(0, 80, 0) };
	const FImpactJointAngles Trailing = Angles(0, 97, 0);
	TestEqual(TEXT("Against this frame's target alone, the lag reads as a fold"), Elbow.ExcessBeyond(Trailing, Recent.Last()), 17.0f, 1e-4f);
	TestEqual(TEXT("Against the recent targets, it does not"), Elbow.ExcessBeyondAny(Trailing, Recent), 0.0f, 1e-4f);
	TestEqual(TEXT("Past every recent target: the part beyond the nearest"), Elbow.ExcessBeyondAny(Angles(0, 105, 0), Recent), 6.0f, 1e-4f);
	TestEqual(TEXT("A sideways fold is still caught"), Elbow.ExcessBeyondAny(Angles(0, 95, 22), Recent), 22.0f, 1e-4f);
	TestEqual(TEXT("A twist is still caught"), Elbow.ExcessBeyondAny(Angles(12, 95, 0), Recent), 12.0f, 1e-4f);
	TestEqual(TEXT("No targets: the plain excess"), Elbow.ExcessBeyondAny(Angles(0, 80, 0), TConstArrayView<FImpactJointAngles>()), 10.0f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactJointLimitOpenedTest,
	"KimodoDatasetCapture.Impact.JointLimits.OpenedToHoldSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactJointLimitOpenedTest::RunTest(const FString& Parameters)
{
	auto Angles = [](float Twist, float Swing1, float Swing2)
	{
		FImpactJointAngles A;
		A.TwistDeg = Twist;
		A.Swing1Deg = Swing1;
		A.Swing2Deg = Swing2;
		return A;
	};
	FImpactJointLimit Elbow;
	Elbow.TwistDeg = 0.0f;
	Elbow.Swing1Deg = 70.0f;
	Elbow.Swing2Deg = 0.0f;

	// A Kimodo forearm: rolls 41 degrees, bends 27 sideways, flexes within the hinge.
	const TArray<FImpactJointAngles> Kimodo = { Angles(-22, 6, -26), Angles(41, -48, 10), Angles(-10, -60, 27) };
	const FImpactJointLimit Opened = Elbow.OpenedToHold(Kimodo, 5.0f);
	TestEqual(TEXT("Twist opens to the source's largest roll plus the margin"), Opened.TwistDeg, 46.0f, 1e-3f);
	// With both swings limited the swing limit is an elliptical cone (as in Chaos), so holding a
	// 60-degree flex bent 27 sideways scales the hinge axis a little too, and no further.
	TestTrue(TEXT("The hinge is never narrowed"), Opened.Swing1Deg >= 70.0f);
	TestTrue(TEXT("The hinge opens only as far as the cone needs"), Opened.Swing1Deg < 90.0f);
	TestTrue(TEXT("Sideways is at least the source's bend plus the margin"), Opened.Swing2Deg >= 32.0f - 1e-3f);
	for (const FImpactJointAngles& Source : Kimodo)
	{
		TestEqual(TEXT("Every source angle is inside the opened limit"), Opened.Excess(Source), 0.0f, 1e-3f);
	}
	TestTrue(TEXT("A real fold past the opened limit still shows"), Opened.Excess(Angles(0, -95, 0)) > 5.0f);

	// A source that stays inside changes nothing (Epic's elbows).
	const TArray<FImpactJointAngles> Epic = { Angles(0, -40, 0), Angles(0, 10, 0) };
	const FImpactJointLimit Same = Elbow.OpenedToHold(Epic, 5.0f);
	TestEqual(TEXT("Inside: twist unchanged"), Same.TwistDeg, 0.0f);
	TestEqual(TEXT("Inside: swing1 unchanged"), Same.Swing1Deg, 70.0f);
	TestEqual(TEXT("Inside: swing2 unchanged"), Same.Swing2Deg, 0.0f);
	TestEqual(TEXT("No source: unchanged"), Elbow.OpenedToHold(TConstArrayView<FImpactJointAngles>(), 5.0f).TwistDeg, 0.0f);

	// A free axis stays free.
	FImpactJointLimit Free;
	Free.TwistDeg = 180.0f;
	Free.Swing1Deg = 40.0f;
	Free.Swing2Deg = 40.0f;
	TestEqual(TEXT("A free twist stays free"), Free.OpenedToHold(Kimodo, 5.0f).TwistDeg, 180.0f);

	// A cone: every source angle inside afterwards, even one off both axes.
	FImpactJointLimit Shoulder;
	Shoulder.TwistDeg = 30.0f;
	Shoulder.Swing1Deg = 40.0f;
	Shoulder.Swing2Deg = 30.0f;
	const TArray<FImpactJointAngles> Arms = { Angles(0, 38, 28), Angles(0, 0, 45), Angles(10, 30, 0) };
	const FImpactJointLimit Cone = Shoulder.OpenedToHold(Arms, 0.0f);
	for (const FImpactJointAngles& Arm : Arms)
	{
		TestEqual(TEXT("Every arm angle is inside the opened cone"), Cone.Excess(Arm), 0.0f, 1e-3f);
	}
	TestTrue(TEXT("The cone is still a cone"), Cone.Swing1Deg < 180.0f && Cone.Swing2Deg < 180.0f);
	TestEqual(TEXT("The twist the arms stay within is unchanged"), Cone.TwistDeg, 30.0f);
	return true;
}
