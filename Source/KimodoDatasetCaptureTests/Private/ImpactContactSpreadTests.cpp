// Copyright Animotive. All Rights Reserved.

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
