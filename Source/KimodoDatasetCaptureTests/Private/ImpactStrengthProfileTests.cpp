// Copyright Animotive. All Rights Reserved.

#include "ImpactRandom.h"
#include "ImpactStrengthProfile.h"
#include "Misc/AutomationTest.h"

namespace ImpactStrengthTests
{
	FImpactBodyStrengthQuery Upper(int32 Distance, bool bMoving = false, bool bPelvis = false)
	{
		FImpactBodyStrengthQuery Query;
		Query.ContactDistance = Distance;
		Query.bMovingSource = bMoving;
		Query.bPelvis = bPelvis;
		return Query;
	}

	FImpactBodyStrengthQuery Leg(int32 Distance, bool bMoving = false)
	{
		FImpactBodyStrengthQuery Query = Upper(Distance, bMoving);
		Query.bLeg = true;
		return Query;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactStrengthSampleTest,
	"KimodoDatasetCapture.Impact.Strength.Sampling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactStrengthSampleTest::RunTest(const FString& Parameters)
{
	const FImpactStrengthDistribution Distribution;
	bool bInRange = true;
	for (int32 Clip = 0; Clip < 500; ++Clip)
	{
		FImpactRandom Random(1, Clip, EImpactRandomStream::StrengthProfile);
		const FImpactStrengthProfile Profile = FImpactStrengthProfile::Sample(Distribution, Random);
		bInRange &= Profile.WorldStrength >= Distribution.WorldStrength.Min && Profile.WorldStrength <= Distribution.WorldStrength.Max;
		const float Ratio = Profile.ParentStrength / Profile.WorldStrength;
		bInRange &= Ratio >= Distribution.ParentToWorldRatio.Min - 1e-4f && Ratio <= Distribution.ParentToWorldRatio.Max + 1e-4f;
	}
	TestTrue(TEXT("Sampled strengths within the distribution"), bInRange);

	FImpactRandom A(1, 7, EImpactRandomStream::StrengthProfile), B(1, 7, EImpactRandomStream::StrengthProfile);
	TestEqual(TEXT("Same clip, same profile"), FImpactStrengthProfile::Sample(Distribution, A).WorldStrength, FImpactStrengthProfile::Sample(Distribution, B).WorldStrength);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactStrengthShapeTest,
	"KimodoDatasetCapture.Impact.Strength.Shape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactStrengthShapeTest::RunTest(const FString& Parameters)
{
	using namespace ImpactStrengthTests;
	const FImpactStrengthProfile Profile;

	// Pre-roll: full strength everywhere.
	const FImpactBodyStrength Pre = Profile.Evaluate(Upper(0), -5);
	TestTrue(TEXT("Pre-roll is full strength"), Pre.World == 1.0f && Pre.Joint == 1.0f);

	// Hit frame: the upper body lets go; standing legs keep their hold.
	const FImpactBodyStrength Hit = Profile.Evaluate(Upper(0), 0);
	TestEqual(TEXT("Struck upper body world at impact"), Hit.World, Profile.ImpactUpperWorld, 1e-4f);
	TestEqual(TEXT("Struck upper body joint at impact"), Hit.Joint, Profile.ImpactUpperJoint, 1e-4f);
	TestEqual(TEXT("Far upper body also lets go at impact"), Profile.Evaluate(Upper(4), 0).World, Profile.ImpactUpperWorld, 1e-4f);
	TestEqual(TEXT("Standing leg keeps world hold"), Profile.Evaluate(Leg(3), 0).World, 1.0f, 1e-4f);

	// Moving source: the pelvis keeps some hold and the legs go with it.
	TestEqual(TEXT("Moving pelvis world at impact"), Profile.Evaluate(Upper(2, true, true), 0).World, Profile.ImpactMovingPelvisWorld, 1e-4f);
	TestEqual(TEXT("Moving leg world at impact"), Profile.Evaluate(Leg(3, true), 0).World, Profile.ImpactMovingLegWorld, 1e-4f);
	TestEqual(TEXT("Standing pelvis behaves as upper body"), Profile.Evaluate(Upper(2, false, true), 0).World, Profile.ImpactUpperWorld, 1e-4f);

	// After the impact phase: release is local to the contact.
	const int32 After = Profile.ImpactFrames + Profile.ImpactBlendFrames;
	const float AtContact = Profile.Evaluate(Upper(0), After).World;
	const float OneLink = Profile.Evaluate(Upper(1), After).World;
	const float FarAway = Profile.Evaluate(Upper(4), After).World;
	TestTrue(TEXT("Contact holds less than its neighbour"), AtContact < OneLink);
	TestTrue(TEXT("Neighbour holds less than a distant body"), OneLink < FarAway);
	TestTrue(TEXT("Contact joint is released"), Profile.Evaluate(Upper(0), After).Joint < 1.0f);
	TestTrue(TEXT("Two links away the joint is still released"), Profile.Evaluate(Upper(2), After).Joint < 1.0f);
	TestEqual(TEXT("Three links away the joint is not released"), Profile.Evaluate(Upper(3), After).Joint,
		Profile.Evaluate(Upper(3), Profile.RecoverFrames * 10).Joint, 1e-4f);

	// Recovery: everything back to full strength.
	for (const FImpactBodyStrengthQuery& Query : { Upper(0), Upper(3), Leg(2), Leg(2, true), Upper(1, true, true) })
	{
		const FImpactBodyStrength Late = Profile.Evaluate(Query, FMath::Max(Profile.RecoverFrames, After));
		TestEqual(TEXT("Recovered world strength is full"), Late.World, 1.0f, 1e-4f);
		TestEqual(TEXT("Recovered joint strength is full"), Late.Joint, 1.0f, 1e-4f);
	}

	// Ragdoll: nothing holds.
	FImpactBodyStrengthQuery Released = Upper(0);
	Released.bReleased = true;
	for (const int32 Frame : { -3, 0, 15, 500 })
	{
		const FImpactBodyStrength Limp = Profile.Evaluate(Released, Frame);
		TestTrue(*FString::Printf(TEXT("Released body has no strength at frame %d"), Frame), Limp.World == 0.0f && Limp.Joint == 0.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactStrengthSmoothTest,
	"KimodoDatasetCapture.Impact.Strength.Smooth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactStrengthSmoothTest::RunTest(const FString& Parameters)
{
	using namespace ImpactStrengthTests;
	const FImpactStrengthProfile Profile;

	// After the hit frame itself, no multiplier jumps between consecutive frames. A one-frame
	// strength switch was the cause of the spike's leg jitter.
	float Largest = 0.0f;
	for (const FImpactBodyStrengthQuery& Query : { Upper(0), Upper(1), Upper(2), Upper(5), Leg(3), Leg(3, true), Upper(2, true, true) })
	{
		for (int32 Frame = 1; Frame < 200; ++Frame)
		{
			const FImpactBodyStrength Previous = Profile.Evaluate(Query, Frame - 1);
			const FImpactBodyStrength Current = Profile.Evaluate(Query, Frame);
			Largest = FMath::Max(Largest, FMath::Abs(Current.World - Previous.World));
			Largest = FMath::Max(Largest, FMath::Abs(Current.Joint - Previous.Joint));
		}
	}
	TestTrue(*FString::Printf(TEXT("Largest frame-to-frame change %.3f is at most 0.1"), Largest), Largest <= 0.1f);

	// Every multiplier stays within 0..1.
	bool bBounded = true;
	for (int32 Frame = -2; Frame < 200; ++Frame)
	{
		for (int32 Distance = 0; Distance < 6; ++Distance)
		{
			const FImpactBodyStrength S = Profile.Evaluate(Upper(Distance), Frame);
			bBounded &= S.World >= 0.0f && S.World <= 1.0f && S.Joint >= 0.0f && S.Joint <= 1.0f;
		}
	}
	TestTrue(TEXT("Multipliers stay within 0..1"), bBounded);
	return true;
}
