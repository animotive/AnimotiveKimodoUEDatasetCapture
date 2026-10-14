// Copyright Animotive. All Rights Reserved.

#include "Engine/World.h"
#include "ImpactClipSimulator.h"
#include "ImpactSimWorld.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactSimWorldSubstepsTest,
	"KimodoDatasetCapture.Impact.SimWorld.Substeps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactSimWorldSubstepsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("A clip simulates physics at 240 Hz for 60 fps recording"), FImpactClipSetup().SimSubsteps, 4);

	FImpactSimWorld World;
	const double Start = World.GetWorld()->GetTimeSeconds();
	const uint64 FramesBefore = GFrameCounter;
	World.Step(1.0f / 60.0f, 4);
	TestEqual(TEXT("One step advances the world by its whole length"), World.GetWorld()->GetTimeSeconds() - Start, 1.0 / 60.0, 1e-5);
	TestEqual(TEXT("In four engine frames"), GFrameCounter - FramesBefore, uint64(4));

	const uint64 FramesMid = GFrameCounter;
	World.Step(1.0f / 60.0f, 0);
	TestEqual(TEXT("Zero substeps still steps once"), GFrameCounter - FramesMid, uint64(1));
	TestEqual(TEXT("And still advances the whole step"), World.GetWorld()->GetTimeSeconds() - Start, 2.0 / 60.0, 1e-5);
	return true;
}
