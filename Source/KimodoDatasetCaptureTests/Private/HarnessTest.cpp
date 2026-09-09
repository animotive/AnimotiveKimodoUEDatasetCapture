// Copyright Animotive. All Rights Reserved.

#include "KimodoDatasetCaptureModule.h"
#include "Misc/AutomationTest.h"

/**
 * Proves the automation harness discovers and runs this plugin's tests headlessly.
 *
 * The test name deliberately begins with KimodoDatasetCapture so the filter in the
 * documented headless command matches it:
 *   -ExecCmds="Automation RunTests KimodoDatasetCapture;Quit"
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKimodoDatasetCaptureHarnessTest,
	"KimodoDatasetCapture.Harness.HarnessRuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKimodoDatasetCaptureHarnessTest::RunTest(const FString& Parameters)
{
	// RED PHASE: asserts a deliberately false condition, so a passing report would mean
	// the harness is not actually executing this test. Flipped in the Green task.
	TestTrue(TEXT("Harness is running this test (deliberate red-phase failure)"), false);

	TestTrue(TEXT("Capture module is loaded"), FKimodoDatasetCaptureModule::IsAvailable());

	return true;
}
