// Copyright Animotive. All Rights Reserved.

#include "ImpactSwingArc.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactSwingArcTest,
	"KimodoDatasetCapture.Impact.SwingArc.Shape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactSwingArcTest::RunTest(const FString& Parameters)
{
	const FVector From(0, 0, 10);
	const FVector To(-40, 12, 14);
	constexpr float Lift = 15.0f;

	TestTrue(TEXT("Starts exactly at From"), FImpactSwingArc::Evaluate(From, To, 0.0f, Lift).Equals(From, 1e-4));
	TestTrue(TEXT("Lands exactly at To"), FImpactSwingArc::Evaluate(From, To, 1.0f, Lift).Equals(To, 1e-4));
	TestTrue(TEXT("Phase below zero clamps to From"), FImpactSwingArc::Evaluate(From, To, -0.5f, Lift).Equals(From, 1e-4));
	TestTrue(TEXT("Phase above one clamps to To"), FImpactSwingArc::Evaluate(From, To, 1.5f, Lift).Equals(To, 1e-4));

	// Peak lift at mid-swing, above the blended ground height.
	const FVector Mid = FImpactSwingArc::Evaluate(From, To, 0.5f, Lift);
	TestEqual(TEXT("Mid-swing height is the blended ends plus the lift"), float(Mid.Z), 12.0f + Lift, 1e-3f);
	float Highest = -1e9f;
	float HighestPhase = 0.0f;
	for (int32 Step = 0; Step <= 100; ++Step)
	{
		const float Phase = Step / 100.0f;
		const float Actual = float(FImpactSwingArc::Evaluate(From, To, Phase, Lift).Z);
		if (Actual > Highest)
		{
			Highest = Actual;
			HighestPhase = Phase;
		}
	}
	TestEqual(TEXT("Highest point is at mid-swing"), HighestPhase, 0.5f, 0.02f);

	// Horizontal progress toward To never goes backwards.
	const FVector Direction = (To - From).GetSafeNormal2D();
	double Previous = -1.0;
	bool bMonotonic = true;
	for (int32 Step = 0; Step <= 100; ++Step)
	{
		const FVector P = FImpactSwingArc::Evaluate(From, To, Step / 100.0f, Lift);
		const double Progress = FVector::DotProduct(FVector(P.X - From.X, P.Y - From.Y, 0.0), Direction);
		bMonotonic &= Progress >= Previous - 1e-6;
		Previous = Progress;
	}
	TestTrue(TEXT("Horizontal progress is monotonic"), bMonotonic);

	// Leaves and lands with almost no horizontal speed: first and last 1% of the swing move far
	// less than 1% of the distance.
	const double Distance = FVector::Dist2D(From, To);
	TestTrue(TEXT("Smooth start"), FVector::Dist2D(FImpactSwingArc::Evaluate(From, To, 0.01f, Lift), From) < 0.01 * Distance * 0.1);
	TestTrue(TEXT("Smooth landing"), FVector::Dist2D(FImpactSwingArc::Evaluate(From, To, 0.99f, Lift), To) < 0.01 * Distance * 0.1);

	// A zero-length step only lifts and sets down.
	const FVector InPlace = FImpactSwingArc::Evaluate(From, From, 0.5f, Lift);
	TestTrue(TEXT("Step in place stays over the spot"), FVector::Dist2D(InPlace, From) < 1e-4);
	TestEqual(TEXT("Step in place lifts"), float(InPlace.Z), float(From.Z) + Lift, 1e-3f);
	return true;
}
