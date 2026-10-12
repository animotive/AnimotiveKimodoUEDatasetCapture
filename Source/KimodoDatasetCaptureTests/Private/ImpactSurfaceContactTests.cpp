// Copyright Animotive. All Rights Reserved.

#include "ImpactContactSpread.h"
#include "ImpactRandom.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactSurfaceContactTest,
	"KimodoDatasetCapture.Impact.SurfaceContact.Geometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactSurfaceContactTest::RunTest(const FString& Parameters)
{
	const FVector Center(0, 0, 120);
	constexpr float Radius = 12.0f;
	// Travelling backwards into the character's front (from +X toward -X).
	const FVector Direction(-1, 0, 0);

	// Dead centre: the front of the sphere, and a pure push.
	const FVector Centred = FImpactSurfaceContact::Contact(Center, Radius, Direction, FVector::ZeroVector);
	TestTrue(TEXT("Centred hit lands on the front of the body"), Centred.Equals(Center - Direction * Radius, 1e-3));
	TestTrue(TEXT("Centred hit has no torque"), FImpactSurfaceContact::Torque(Centred, Center, Direction * 1000.0).IsNearlyZero(1e-3));

	// Off-centre: still on the surface, still the facing side, and it twists.
	const FVector Offset(0, 6, 3);
	const FVector OffCentre = FImpactSurfaceContact::Contact(Center, Radius, Direction, Offset);
	TestEqual(TEXT("Off-centre contact lies on the surface"), float(FVector::Dist(OffCentre, Center)), Radius, 1e-3f);
	TestTrue(TEXT("Off-centre contact faces the impactor"), FVector::DotProduct(OffCentre - Center, Direction) < 0.0);
	TestTrue(TEXT("Off-centre contact keeps the aim offset across the direction"),
		FMath::IsNearlyEqual(OffCentre.Y, Center.Y + Offset.Y, 1e-3) && FMath::IsNearlyEqual(OffCentre.Z, Center.Z + Offset.Z, 1e-3));
	const FVector Impulse = Direction * 1000.0;
	const FVector Twist = FImpactSurfaceContact::Torque(OffCentre, Center, Impulse);
	TestTrue(TEXT("Off-centre hit twists the body"), Twist.Size() > 1.0);
	TestEqual(TEXT("Torque is perpendicular to the impulse"), float(FVector::DotProduct(Twist.GetSafeNormal(), Impulse.GetSafeNormal())), 0.0f, 1e-4f);
	// Contact on the +Y side pushed along -X: r x J = (r.x, 6, 0) x (-1000, 0, 0) = (0, 0, 6000).
	const FVector SideTwist = FImpactSurfaceContact::Torque(FImpactSurfaceContact::Contact(Center, Radius, Direction, FVector(0, 6, 0)), Center, Impulse);
	TestEqual(TEXT("Side hit twists about the vertical axis"), float(SideTwist.Z), 6000.0f, 0.5f);
	TestTrue(TEXT("Side hit has no other torque"), FMath::IsNearlyZero(SideTwist.X, 1e-3) && FMath::IsNearlyZero(SideTwist.Y, 1e-3));

	// An aim beyond the radius grazes rather than missing.
	const FVector Graze = FImpactSurfaceContact::Contact(Center, Radius, Direction, FVector(0, 40, 0));
	TestEqual(TEXT("Over-wide aim still touches the surface"), float(FVector::Dist(Graze, Center)), Radius, 1e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactSurfaceAimTest,
	"KimodoDatasetCapture.Impact.SurfaceContact.Aim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactSurfaceAimTest::RunTest(const FString& Parameters)
{
	const FVector Direction = FVector(-1, 0.3, -0.2).GetSafeNormal();
	constexpr float Radius = 10.0f;
	constexpr float Fraction = 0.8f;

	FImpactRandom Random(3, 4, EImpactRandomStream::Direction);
	bool bPerpendicular = true, bWithin = true;
	double Spread = 0.0;
	for (int32 Index = 0; Index < 2000; ++Index)
	{
		const FVector Offset = FImpactSurfaceContact::AimOffset(Direction, Radius, Fraction, Random);
		bPerpendicular &= FMath::Abs(FVector::DotProduct(Offset, Direction)) < 1e-3;
		bWithin &= Offset.Size() <= Radius * Fraction + 1e-3;
		Spread = FMath::Max(Spread, Offset.Size());
	}
	TestTrue(TEXT("Aim offsets lie across the direction of travel"), bPerpendicular);
	TestTrue(TEXT("Aim offsets stay within the aim disc"), bWithin);
	TestTrue(TEXT("Aim offsets use the whole disc"), Spread > Radius * Fraction * 0.9);

	FImpactRandom A(3, 4, EImpactRandomStream::Direction), B(3, 4, EImpactRandomStream::Direction);
	TestTrue(TEXT("Deterministic"), FImpactSurfaceContact::AimOffset(Direction, Radius, Fraction, A).Equals(FImpactSurfaceContact::AimOffset(Direction, Radius, Fraction, B), 0.0));

	// The surface contact feeds the spread: shares still sum exactly.
	const TArray<FImpactBodyProxy> Bodies = { { TEXT("spine_03"), FVector(0, 0, 120), 12.0f }, { TEXT("spine_01"), FVector(0, 0, 100), 10.0f } };
	const FVector Contact = FImpactSurfaceContact::Contact(Bodies[0].Center, Bodies[0].RadiusCm, FVector(-1, 0, 0), FVector(0, 4, -6));
	float Total = 0.0f;
	for (const FImpactBodyShare& Share : FImpactContactSpread::Distribute(Bodies, 0, Contact, 20.0f, 5000.0f))
	{
		Total += Share.ImpulseKgCmPerSecond;
	}
	TestEqual(TEXT("Spread from a surface contact conserves the impulse"), Total, 5000.0f, 0.5f);
	return true;
}
