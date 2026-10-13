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

	const FImpactJointLimit Free;
	TestEqual(TEXT("A free joint never exceeds"), Free.Excess(Angles(179, -179, 179)), 0.0f);
	return true;
}
