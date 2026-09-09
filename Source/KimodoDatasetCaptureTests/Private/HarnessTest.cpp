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
	// This test was first committed asserting false, and observed failing headlessly, so
	// that a later pass proves the harness executes it rather than merely reporting success
	// for a suite it never found.
	TestTrue(TEXT("Harness is running this test"), true);

	// The tests module links against the capture module, so the capture module must be
	// loaded by the time tests run. If this fails, module loading phases are wrong.
	TestTrue(TEXT("Capture module is loaded"), FKimodoDatasetCaptureModule::IsAvailable());

	return true;
}
