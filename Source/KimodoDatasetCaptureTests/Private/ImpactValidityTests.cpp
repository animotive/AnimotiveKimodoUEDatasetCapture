// Copyright Animotive. All Rights Reserved.

#include "ImpactValidity.h"
#include "Misc/AutomationTest.h"

namespace ImpactValidityTests
{
	TArray<FImpactBodySample> Healthy()
	{
		TArray<FImpactBodySample> Bodies;
		for (const TCHAR* Name : { TEXT("pelvis"), TEXT("spine_03"), TEXT("foot_l") })
		{
			FImpactBodySample Body;
			Body.Name = Name;
			Body.Location = FVector(0, 0, Body.Name == FName(TEXT("foot_l")) ? 9.0 : 100.0);
			Body.LinearVelocity = FVector(300, 0, 0);
			Body.AngularVelocityDegPerSecond = FVector(0, 0, 200);
			Body.JointGapCm = 2.0f;
			Bodies.Add(Body);
		}
		return Bodies;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactValidityTest,
	"KimodoDatasetCapture.Impact.Validity.Frames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactValidityTest::RunTest(const FString& Parameters)
{
	using namespace ImpactValidityTests;
	const FImpactValidityThresholds Thresholds;
	FText Reason;
	TestTrue(TEXT("A healthy frame is valid"), FImpactValidity::Check(Healthy(), 0.0f, Thresholds, Reason));

	auto Rejects = [this, &Thresholds](const TCHAR* What, TArray<FImpactBodySample> Bodies, const TCHAR* MustMention)
	{
		FText Why;
		TestFalse(*FString::Printf(TEXT("%s is invalid"), What), FImpactValidity::Check(Bodies, 0.0f, Thresholds, Why));
		TestTrue(*FString::Printf(TEXT("%s reason names %s"), What, MustMention), Why.ToString().Contains(MustMention));
	};

	{ auto B = Healthy(); B[1].Location.X = NAN; Rejects(TEXT("NaN position"), B, TEXT("spine_03")); }
	{ auto B = Healthy(); B[0].Rotation = FQuat(INFINITY, 0, 0, 1); Rejects(TEXT("Infinite rotation"), B, TEXT("pelvis")); }
	{ auto B = Healthy(); B[2].LinearVelocity.Z = NAN; Rejects(TEXT("NaN velocity"), B, TEXT("foot_l")); }
	{ auto B = Healthy(); B[1].LinearVelocity = FVector(Thresholds.MaxBodySpeedCmPerSecond + 1.0f, 0, 0); Rejects(TEXT("Exploding speed"), B, TEXT("spine_03")); }
	{ auto B = Healthy(); B[1].AngularVelocityDegPerSecond = FVector(0, Thresholds.MaxAngularSpeedDegPerSecond + 1.0f, 0); Rejects(TEXT("Exploding spin"), B, TEXT("spine_03")); }
	{ auto B = Healthy(); B[2].Location.Z = -(Thresholds.MaxGroundPenetrationCm + 1.0f); Rejects(TEXT("Sunk through the floor"), B, TEXT("foot_l")); }
	{ auto B = Healthy(); B[0].JointGapCm = Thresholds.MaxJointGapCm + 1.0f; Rejects(TEXT("Torn joint"), B, TEXT("pelvis")); }
	{ auto B = Healthy(); B[1].JointLimitExcessDeg = Thresholds.MaxJointLimitExcessDeg + 1.0f; Rejects(TEXT("Joint bent past its limit"), B, TEXT("spine_03")); }
	{ auto B = Healthy(); B[2].bHingeJoint = true; B[2].JointLimitExcessDeg = Thresholds.MaxHingeLimitExcessDeg + 1.0f; Rejects(TEXT("Hinge folded the wrong way"), B, TEXT("foot_l")); }
	{
		auto B = Healthy();
		B[1].JointLimitExcessDeg = Thresholds.MaxHingeLimitExcessDeg + 1.0f;
		TestTrue(TEXT("A ball joint may overshoot further than a hinge"), FImpactValidity::Check(B, 0.0f, Thresholds, Reason));
	}

	// Limits are inclusive: exactly on a limit is still valid.
	{
		auto B = Healthy();
		B[0].LinearVelocity = FVector(Thresholds.MaxBodySpeedCmPerSecond, 0, 0);
		B[1].AngularVelocityDegPerSecond = FVector(Thresholds.MaxAngularSpeedDegPerSecond, 0, 0);
		B[2].Location.Z = -Thresholds.MaxGroundPenetrationCm;
		B[2].JointGapCm = Thresholds.MaxJointGapCm;
		B[1].JointLimitExcessDeg = Thresholds.MaxJointLimitExcessDeg;
		B[0].bHingeJoint = true;
		B[0].JointLimitExcessDeg = Thresholds.MaxHingeLimitExcessDeg;
		TestTrue(TEXT("Values exactly on every limit are valid"), FImpactValidity::Check(B, 0.0f, Thresholds, Reason));
	}

	// Penetration is measured from the actual ground height, not zero.
	{
		auto B = Healthy();
		for (FImpactBodySample& Body : B)
		{
			Body.Location.Z += 500.0;
		}
		B[2].Location.Z = 500.0 - 10.0;
		TestTrue(TEXT("Ground height is respected"), FImpactValidity::Check(B, 500.0f, Thresholds, Reason));
	}

	TestFalse(TEXT("A frame with no bodies is invalid"), FImpactValidity::Check({}, 0.0f, Thresholds, Reason));
	TestFalse(TEXT("No-bodies rejection states a reason"), Reason.IsEmpty());
	return true;
}
