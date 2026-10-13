// Copyright Animotive. All Rights Reserved.

#include "ImpactBalancePlanner.h"
#include "ImpactContactSpread.h"
#include "Misc/AutomationTest.h"

namespace ImpactContactSpreadTests
{
	/** A short vertical stack: abdomen, chest, neck, head, 20 cm apart, plus a distant foot. */
	TArray<FImpactBodyProxy> Torso()
	{
		return {
			{ TEXT("spine_01"), FVector(0, 0, 100), 10.0f },
			{ TEXT("spine_03"), FVector(0, 0, 120), 10.0f },
			{ TEXT("neck_01"), FVector(0, 0, 140), 6.0f },
			{ TEXT("head"), FVector(0, 0, 160), 10.0f },
			{ TEXT("foot_l"), FVector(0, 0, 10), 6.0f },
		};
	}

	float Sum(const TArray<FImpactBodyShare>& Shares)
	{
		float Total = 0.0f;
		for (const FImpactBodyShare& Share : Shares)
		{
			Total += Share.ImpulseKgCmPerSecond;
		}
		return Total;
	}

	const FImpactBodyShare* Find(const TArray<FImpactBodyShare>& Shares, const TCHAR* Name)
	{
		return Shares.FindByPredicate([Name](const FImpactBodyShare& S) { return S.Name == FName(Name); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactContactSpreadTest,
	"KimodoDatasetCapture.Impact.ContactSpread.Distribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactContactSpreadTest::RunTest(const FString& Parameters)
{
	using namespace ImpactContactSpreadTests;
	const TArray<FImpactBodyProxy> Bodies = Torso();
	// Struck at the front surface of the chest.
	const FVector Contact(10, 0, 120);

	// Point contact: one body, the full impulse.
	const TArray<FImpactBodyShare> Point = FImpactContactSpread::Distribute(Bodies, 1, Contact, 0.0f, 10000.0f);
	TestEqual(TEXT("Point contact hits exactly one body"), Point.Num(), 1);
	TestTrue(TEXT("Point contact hits the struck body"), Point.Num() == 1 && Point[0].Name == FName(TEXT("spine_03")));
	TestEqual(TEXT("Point contact carries the full impulse"), Sum(Point), 10000.0f, 0.01f);

	// A small radius that reaches no neighbour still loads only the struck body.
	TestEqual(TEXT("Small radius reaching no neighbour hits one body"), FImpactContactSpread::Distribute(Bodies, 1, Contact, 3.0f, 10000.0f).Num(), 1);

	// A wide contact reaches the neighbours but not the far-away foot.
	const TArray<FImpactBodyShare> Wide = FImpactContactSpread::Distribute(Bodies, 1, Contact, 30.0f, 10000.0f);
	TestTrue(TEXT("Wide contact includes the struck body"), Find(Wide, TEXT("spine_03")) != nullptr);
	TestTrue(TEXT("Wide contact includes a neighbour"), Find(Wide, TEXT("spine_01")) != nullptr);
	TestNull(TEXT("Wide contact excludes a distant body"), Find(Wide, TEXT("foot_l")));
	TestEqual(TEXT("Shares sum to the impulse"), Sum(Wide), 10000.0f, 0.5f);

	// The struck body takes the largest share; nearer bodies take more than farther ones.
	const FImpactBodyShare* Chest = Find(Wide, TEXT("spine_03"));
	const FImpactBodyShare* Abdomen = Find(Wide, TEXT("spine_01"));
	const FImpactBodyShare* Head = Find(Wide, TEXT("head"));
	if (Chest && Abdomen)
	{
		TestTrue(TEXT("Struck body takes the largest share"), Chest->ImpulseKgCmPerSecond > Abdomen->ImpulseKgCmPerSecond);
	}
	if (Abdomen && Head)
	{
		TestTrue(TEXT("Nearer body takes more than a farther one"), Abdomen->ImpulseKgCmPerSecond > Head->ImpulseKgCmPerSecond);
	}
	bool bAllPositive = true;
	for (const FImpactBodyShare& Share : Wide)
	{
		bAllPositive &= Share.ImpulseKgCmPerSecond > 0.0f;
	}
	TestTrue(TEXT("Every included body takes a positive share"), bAllPositive);

	// Apply points: on each body's surface, toward the contact.
	if (Abdomen)
	{
		const FVector Expected = Bodies[0].Center + (Contact - Bodies[0].Center).GetSafeNormal() * Bodies[0].RadiusCm;
		TestTrue(TEXT("Neighbour's apply point is its surface point nearest the contact"), Abdomen->ApplyPoint.Equals(Expected, 0.01));
	}
	if (Chest)
	{
		TestTrue(TEXT("Struck body's apply point is the contact point"), Chest->ApplyPoint.Equals(Contact, 0.01));
	}

	// A very wide contact (a car) loads the whole torso and still conserves the impulse.
	const TArray<FImpactBodyShare> Huge = FImpactContactSpread::Distribute(Bodies, 1, Contact, 500.0f, 64000.0f);
	TestEqual(TEXT("Huge contact reaches every body"), Huge.Num(), Bodies.Num());
	TestEqual(TEXT("Huge contact conserves the impulse"), Sum(Huge), 64000.0f, 2.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactContactSpreadMassTest,
	"KimodoDatasetCapture.Impact.ContactSpread.ByMass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactContactSpreadMassTest::RunTest(const FString& Parameters)
{
	using namespace ImpactContactSpreadTests;
	// A pelvis struck from the side, a light hand hanging beside it and a heavy thigh below, both
	// the same distance from the contact. A wide surface pushes what it touches together, so each
	// neighbour takes impulse in proportion to its mass: the hand is not flung faster than the body.
	const TArray<FImpactBodyProxy> Bodies = {
		{ TEXT("pelvis"), FVector(0, 0, 100), 12.0f, 12.0f },
		{ TEXT("hand_l"), FVector(-30, 0, 100), 5.0f, 0.6f },
		{ TEXT("thigh_l"), FVector(-12, 0, 82), 5.0f, 9.0f },
	};
	const FVector Contact(-12, 0, 100);
	const TArray<FImpactBodyShare> Shares = FImpactContactSpread::Distribute(Bodies, 0, Contact, 60.0f, 60000.0f);
	const FImpactBodyShare* Pelvis = Find(Shares, TEXT("pelvis"));
	const FImpactBodyShare* Hand = Find(Shares, TEXT("hand_l"));
	const FImpactBodyShare* Thigh = Find(Shares, TEXT("thigh_l"));
	if (!TestTrue(TEXT("All three bodies share the impact"), Pelvis && Hand && Thigh))
	{
		return false;
	}
	TestEqual(TEXT("Shares sum to the impulse"), Sum(Shares), 60000.0f, 1.0f);
	TestEqual(TEXT("Equidistant neighbours share in proportion to mass"),
		Thigh->ImpulseKgCmPerSecond / Hand->ImpulseKgCmPerSecond, 9.0f / 0.6f, 0.01f);

	// The velocity each body is given never exceeds the struck body's.
	const float PelvisDv = Pelvis->ImpulseKgCmPerSecond / 12.0f;
	TestTrue(TEXT("The hand is not flung faster than the struck body"), Hand->ImpulseKgCmPerSecond / 0.6f <= PelvisDv + 1e-3f);
	TestTrue(TEXT("The thigh is not flung faster than the struck body"), Thigh->ImpulseKgCmPerSecond / 9.0f <= PelvisDv + 1e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactContactSpreadInvalidTest,
	"KimodoDatasetCapture.Impact.ContactSpread.InvalidInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactContactSpreadInvalidTest::RunTest(const FString& Parameters)
{
	using namespace ImpactContactSpreadTests;
	const TArray<FImpactBodyProxy> Bodies = Torso();
	const FVector Contact(10, 0, 120);
	TestEqual(TEXT("Invalid struck index gives no shares"), FImpactContactSpread::Distribute(Bodies, 99, Contact, 10.0f, 1000.0f).Num(), 0);
	TestEqual(TEXT("Negative struck index gives no shares"), FImpactContactSpread::Distribute(Bodies, -1, Contact, 10.0f, 1000.0f).Num(), 0);
	TestEqual(TEXT("Zero impulse gives no shares"), FImpactContactSpread::Distribute(Bodies, 1, Contact, 10.0f, 0.0f).Num(), 0);
	TestEqual(TEXT("Negative impulse gives no shares"), FImpactContactSpread::Distribute(Bodies, 1, Contact, 10.0f, -5.0f).Num(), 0);
	TestEqual(TEXT("No bodies gives no shares"), FImpactContactSpread::Distribute({}, 0, Contact, 10.0f, 1000.0f).Num(), 0);
	// A negative radius is treated as a point contact, not an error that drops the hit.
	TestEqual(TEXT("Negative radius behaves as a point contact"), FImpactContactSpread::Distribute(Bodies, 1, Contact, -4.0f, 1000.0f).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactContactDeltaVTest,
	"KimodoDatasetCapture.Impact.ContactSpread.ContactDeltaV",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactContactDeltaVTest::RunTest(const FString& Parameters)
{
	// A right arm, mannequin masses: hand, forearm, upper arm (out of a 40 cm reach), a chest far off.
	const TArray<FImpactBodyProxy> Arm = {
		{ TEXT("hand_r"), FVector(0, 0, 0), 4.0f, 0.5f },
		{ TEXT("lowerarm_r"), FVector(0, 20, 0), 5.0f, 1.2f },
		{ TEXT("upperarm_r"), FVector(0, 45, 0), 6.0f, 2.0f },
		{ TEXT("spine_05"), FVector(0, 90, 0), 15.0f, 12.0f },
	};
	const FVector Contact(0, -4, 0);
	const float LightPunch = 8500.0f;

	// The joints hold the hand to the forearm: a blow to the hand moves both.
	TestEqual(TEXT("Spread over 40 cm, the hand and forearm move together"),
		FImpactContactSpread::ContactDeltaV(Arm, 0, Contact, 40.0f, LightPunch), LightPunch / (0.5f + 1.2f), 1e-1f);
	TestEqual(TEXT("A point contact moves the hand alone"),
		FImpactContactSpread::ContactDeltaV(Arm, 0, Contact, 1.0f, LightPunch), LightPunch / 0.5f, 1e-1f);

	const FImpactFallRules Rules;
	TestFalse(TEXT("A light punch to the hand does not overwhelm"),
		Rules.IsOverwhelming(LightPunch, 75.0f, FImpactContactSpread::ContactDeltaV(Arm, 0, Contact, 40.0f, LightPunch)));
	TestTrue(TEXT("A violent blow on the hand alone still would"),
		Rules.IsOverwhelming(LightPunch, 75.0f, FImpactContactSpread::ContactDeltaV(Arm, 0, Contact, 1.0f, LightPunch)));

	TestEqual(TEXT("Invalid struck index: none"), FImpactContactSpread::ContactDeltaV(Arm, 9, Contact, 40.0f, LightPunch), 0.0f);
	TestEqual(TEXT("No impulse: none"), FImpactContactSpread::ContactDeltaV(Arm, 0, Contact, 40.0f, 0.0f), 0.0f);
	TArray<FImpactBodyProxy> Massless = Arm;
	for (FImpactBodyProxy& Body : Massless)
	{
		Body.MassKg = 0.0f;
	}
	TestEqual(TEXT("Unknown masses: none"), FImpactContactSpread::ContactDeltaV(Massless, 0, Contact, 40.0f, LightPunch), 0.0f);
	return true;
}
