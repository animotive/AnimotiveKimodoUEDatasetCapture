// Copyright Animotive. All Rights Reserved.

#include "ImpactRootTracker.h"
#include "Misc/AutomationTest.h"

namespace ImpactRootTrackerTests
{
	// A rig whose pelvis is rolled relative to the component, as real rigs are: its local +Z
	// happens to point along the mesh's forward (+Y).
	const FQuat PelvisReferenceRotation = FQuat::FindBetweenNormals(FVector::UpVector, FVector::RightVector);
	const FTransform ReferencePelvis(PelvisReferenceRotation, FVector(0, 0, 95));
	const FVector MeshForward = FVector::RightVector;

	FImpactRootTracker Make(float Smoothing = 1.0f)
	{
		FImpactRootTrackerConfig Config;
		Config.YawSmoothing = Smoothing;
		return FImpactRootTracker(ReferencePelvis, MeshForward, 0.0f, Config);
	}

	/** The reference pelvis, turned about vertical by YawDegrees and moved to Location. */
	FTransform TurnedPelvis(float YawDegrees, const FVector& Location)
	{
		return FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDegrees)) * PelvisReferenceRotation, Location);
	}

	float YawOf(const FTransform& Root)
	{
		// Root faces the mesh's forward (+Y) at zero; measure its forward's heading.
		const FVector Forward = Root.GetRotation().RotateVector(MeshForward);
		return FMath::UnwindDegrees(FMath::RadiansToDegrees(FMath::Atan2(float(Forward.Y), float(Forward.X))) - 90.0f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactRootProjectTest,
	"KimodoDatasetCapture.Impact.RootTracker.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactRootProjectTest::RunTest(const FString& Parameters)
{
	using namespace ImpactRootTrackerTests;
	FImpactRootTracker Tracker = Make();

	TestTrue(TEXT("Forward axis derived from the reference pose"), Tracker.GetPelvisForwardAxis().Equals(FVector::UpVector, 1e-4));

	// Position: under the pelvis, on the ground.
	const FTransform Root = Tracker.Project(TurnedPelvis(0.0f, FVector(30, 40, 85)));
	TestTrue(TEXT("Root under the pelvis at ground height"), Root.GetLocation().Equals(FVector(30, 40, 0), 1e-3));
	TestEqual(TEXT("Unturned pelvis faces the mesh forward"), YawOf(Root), 0.0f, 0.01f);

	// Facing follows the pelvis's turn.
	TestEqual(TEXT("Turned pelvis turns the root"), YawOf(Tracker.Project(TurnedPelvis(90.0f, FVector(0, 0, 40)))), 90.0f, 0.01f);
	TestEqual(TEXT("Root stays upright"), float(Tracker.Project(TurnedPelvis(37.0f, FVector::ZeroVector)).GetRotation().GetUpVector().Z), 1.0f, 1e-4f);

	// Lying with the pelvis's forward pointing at the sky: no facing from it; the last one is kept.
	FImpactRootTracker Lying = Make();
	Lying.Step(FTransform::Identity, TurnedPelvis(45.0f, FVector(0, 0, 20)), true);
	const FTransform Vertical(FQuat::FindBetweenNormals(FVector::UpVector, FVector::UpVector), FVector(0, 0, 15));
	FTransform Kept;
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Kept = Lying.Step(FTransform::Identity, Vertical, true);
	}
	TestEqual(TEXT("A vertical pelvis keeps the last facing"), YawOf(Kept), 45.0f, 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactRootBlendTest,
	"KimodoDatasetCapture.Impact.RootTracker.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactRootBlendTest::RunTest(const FString& Parameters)
{
	using namespace ImpactRootTrackerTests;
	FImpactRootTrackerConfig Config;
	Config.YawSmoothing = 1.0f;
	FImpactRootTracker Tracker(ReferencePelvis, MeshForward, 0.0f, Config);

	const FTransform SourceRoot(FQuat::Identity, FVector(0, 300, 0));
	const FTransform Pelvis = TurnedPelvis(0.0f, FVector(120, 0, 30));

	// Held: exactly the source root, however far the pelvis has gone.
	TestTrue(TEXT("Before release the root is the source root"), Tracker.Step(SourceRoot, Pelvis, false).Equals(SourceRoot, 1e-4));

	// Released: starts at the source root and arrives under the pelvis after BlendFrames.
	const FTransform First = Tracker.Step(SourceRoot, Pelvis, true);
	TestTrue(TEXT("Release starts close to the source root"), FVector::Dist(First.GetLocation(), SourceRoot.GetLocation()) < FVector::Dist(SourceRoot.GetLocation(), FVector(120, 0, 0)) / Config.BlendFrames * 1.5);
	FTransform Last = First;
	for (int32 Frame = 1; Frame < Config.BlendFrames; ++Frame)
	{
		Last = Tracker.Step(SourceRoot, Pelvis, true);
	}
	TestTrue(TEXT("Arrives under the pelvis"), Last.GetLocation().Equals(FVector(120, 0, 0), 0.01));

	// Facing takes the short way round across +-180.
	FImpactRootTrackerConfig Smooth;
	Smooth.YawSmoothing = 0.5f;
	Smooth.BlendFrames = 1; // measure smoothing alone, not the release blend from the source root
	FImpactRootTracker Wrap(ReferencePelvis, MeshForward, 0.0f, Smooth);
	Wrap.Step(FTransform::Identity, TurnedPelvis(170.0f, FVector::ZeroVector), true);
	const float Next = YawOf(Wrap.Step(FTransform::Identity, TurnedPelvis(-170.0f, FVector::ZeroVector), true));
	TestTrue(TEXT("Smoothing crosses 180 the short way"), FMath::Abs(FMath::UnwindDegrees(Next - 180.0f)) < 15.0f);
	return true;
}
