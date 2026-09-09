// Copyright Animotive. All Rights Reserved.

#include "DatasetInputSample.h"
#include "Misc/AutomationTest.h"

namespace
{
	/** A sample with one button held, for transition tests. */
	FDatasetInputSample MakeSampleWithButtons(int32 FrameNumber, EDatasetGamepadButton Buttons)
	{
		FDatasetInputSample Sample;
		Sample.FrameNumber = FrameNumber;
		Sample.Buttons = static_cast<uint32>(Buttons);
		return Sample;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetInputSampleButtonStateTest,
	"KimodoDatasetCapture.InputSample.ButtonState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetInputSampleButtonStateTest::RunTest(const FString& Parameters)
{
	FDatasetInputSample Sample;

	TestFalse(TEXT("Default sample holds no button"), Sample.IsHeld(EDatasetGamepadButton::FaceBottom));

	Sample.SetHeld(EDatasetGamepadButton::FaceBottom, true);
	TestTrue(TEXT("FaceBottom held after set"), Sample.IsHeld(EDatasetGamepadButton::FaceBottom));
	TestFalse(TEXT("Setting one button does not set another"), Sample.IsHeld(EDatasetGamepadButton::FaceLeft));

	Sample.SetHeld(EDatasetGamepadButton::FaceLeft, true);
	TestTrue(TEXT("FaceBottom still held after setting FaceLeft"), Sample.IsHeld(EDatasetGamepadButton::FaceBottom));
	TestTrue(TEXT("FaceLeft held"), Sample.IsHeld(EDatasetGamepadButton::FaceLeft));

	Sample.SetHeld(EDatasetGamepadButton::FaceBottom, false);
	TestFalse(TEXT("FaceBottom cleared"), Sample.IsHeld(EDatasetGamepadButton::FaceBottom));
	TestTrue(TEXT("Clearing one button does not clear another"), Sample.IsHeld(EDatasetGamepadButton::FaceLeft));

	// Highest declared bit, to catch a mask built with a too-narrow type.
	Sample.SetHeld(EDatasetGamepadButton::SpecialRight, true);
	TestTrue(TEXT("Highest declared button bit round-trips"), Sample.IsHeld(EDatasetGamepadButton::SpecialRight));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetInputSampleAxisStorageTest,
	"KimodoDatasetCapture.InputSample.AxisStorage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetInputSampleAxisStorageTest::RunTest(const FString& Parameters)
{
	FDatasetInputSample Sample;

	TestEqual(TEXT("Default frame number is zero"), Sample.FrameNumber, 0);
	TestEqual(TEXT("Default left stick X is zero"), Sample.LeftStickX, 0.0f);
	TestEqual(TEXT("Default right trigger is zero"), Sample.RightTrigger, 0.0f);

	Sample.FrameNumber = 42;
	Sample.LeftStickX = -1.0f;
	Sample.LeftStickY = 1.0f;
	Sample.RightStickX = 0.25f;
	Sample.RightStickY = -0.75f;
	Sample.LeftTrigger = 0.0f;
	Sample.RightTrigger = 1.0f;

	TestEqual(TEXT("Frame number stored"), Sample.FrameNumber, 42);
	TestEqual(TEXT("Left stick X at range minimum"), Sample.LeftStickX, -1.0f);
	TestEqual(TEXT("Left stick Y at range maximum"), Sample.LeftStickY, 1.0f);
	TestEqual(TEXT("Right stick X stored"), Sample.RightStickX, 0.25f);
	TestEqual(TEXT("Right stick Y stored"), Sample.RightStickY, -0.75f);
	TestEqual(TEXT("Right trigger at range maximum"), Sample.RightTrigger, 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetInputSampleNeutralTest,
	"KimodoDatasetCapture.InputSample.Neutral",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetInputSampleNeutralTest::RunTest(const FString& Parameters)
{
	FDatasetInputSample Sample;
	TestTrue(TEXT("Default sample is neutral"), Sample.IsNeutral());

	Sample.SetHeld(EDatasetGamepadButton::ShoulderRight, true);
	TestFalse(TEXT("Held button is not neutral"), Sample.IsNeutral());

	Sample = FDatasetInputSample();
	Sample.LeftStickY = 0.5f;
	TestFalse(TEXT("Deflected stick is not neutral"), Sample.IsNeutral());

	Sample = FDatasetInputSample();
	Sample.LeftTrigger = 0.5f;
	TestFalse(TEXT("Pulled trigger is not neutral"), Sample.IsNeutral());

	// Frame number is bookkeeping, not input, so it must not affect neutrality.
	Sample = FDatasetInputSample();
	Sample.FrameNumber = 600;
	TestTrue(TEXT("Frame number alone stays neutral"), Sample.IsNeutral());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetInputSampleTransitionTest,
	"KimodoDatasetCapture.InputSample.Transitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetInputSampleTransitionTest::RunTest(const FString& Parameters)
{
	const FDatasetInputSample None = MakeSampleWithButtons(0, EDatasetGamepadButton::None);
	const FDatasetInputSample JumpHeld = MakeSampleWithButtons(1, EDatasetGamepadButton::FaceBottom);

	// Up -> down is a press on the frame it first appears.
	TestEqual(
		TEXT("Press detected from none to held"),
		static_cast<uint32>(FDatasetInputSample::GetPressedSince(None, JumpHeld)),
		static_cast<uint32>(EDatasetGamepadButton::FaceBottom));
	TestTrue(
		TEXT("WasPressed agrees for that button"),
		FDatasetInputSample::WasPressed(None, JumpHeld, EDatasetGamepadButton::FaceBottom));
	TestFalse(
		TEXT("WasPressed false for an untouched button"),
		FDatasetInputSample::WasPressed(None, JumpHeld, EDatasetGamepadButton::FaceTop));

	// Held across two frames is not a new press. This is the case that produces phantom
	// double-jumps in a dataset if transitions are derived incorrectly.
	const FDatasetInputSample JumpStillHeld = MakeSampleWithButtons(2, EDatasetGamepadButton::FaceBottom);
	TestEqual(
		TEXT("Continuously held button is not a new press"),
		static_cast<uint32>(FDatasetInputSample::GetPressedSince(JumpHeld, JumpStillHeld)),
		static_cast<uint32>(EDatasetGamepadButton::None));

	// Down -> up is a release.
	TestEqual(
		TEXT("Release detected from held to none"),
		static_cast<uint32>(FDatasetInputSample::GetReleasedSince(JumpStillHeld, None)),
		static_cast<uint32>(EDatasetGamepadButton::FaceBottom));
	TestTrue(
		TEXT("WasReleased agrees for that button"),
		FDatasetInputSample::WasReleased(JumpStillHeld, None, EDatasetGamepadButton::FaceBottom));
	TestFalse(
		TEXT("Release is not reported as a press"),
		FDatasetInputSample::WasPressed(JumpStillHeld, None, EDatasetGamepadButton::FaceBottom));

	// Simultaneous press and release across one frame boundary.
	const FDatasetInputSample JumpAndInteract = MakeSampleWithButtons(
		3, EDatasetGamepadButton::FaceBottom | EDatasetGamepadButton::FaceLeft);
	const FDatasetInputSample InteractAndSprint = MakeSampleWithButtons(
		4, EDatasetGamepadButton::FaceLeft | EDatasetGamepadButton::ShoulderLeft);

	TestEqual(
		TEXT("Only the newly held button is a press"),
		static_cast<uint32>(FDatasetInputSample::GetPressedSince(JumpAndInteract, InteractAndSprint)),
		static_cast<uint32>(EDatasetGamepadButton::ShoulderLeft));
	TestEqual(
		TEXT("Only the no-longer-held button is a release"),
		static_cast<uint32>(FDatasetInputSample::GetReleasedSince(JumpAndInteract, InteractAndSprint)),
		static_cast<uint32>(EDatasetGamepadButton::FaceBottom));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetInputSampleEqualityTest,
	"KimodoDatasetCapture.InputSample.Equality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetInputSampleEqualityTest::RunTest(const FString& Parameters)
{
	FDatasetInputSample A;
	A.FrameNumber = 7;
	A.LeftStickX = 0.5f;
	A.SetHeld(EDatasetGamepadButton::DPadUp, true);

	FDatasetInputSample B = A;
	TestTrue(TEXT("Copy equals original"), A == B);
	TestFalse(TEXT("Copy is not unequal to original"), A != B);

	// Every field must participate, or a round-trip test could pass while losing data.
	B = A;
	B.FrameNumber = 8;
	TestTrue(TEXT("Differing frame number compares unequal"), A != B);

	B = A;
	B.LeftStickX = 0.5001f;
	TestTrue(TEXT("Differing axis compares unequal"), A != B);

	B = A;
	B.SetHeld(EDatasetGamepadButton::DPadDown, true);
	TestTrue(TEXT("Differing buttons compare unequal"), A != B);

	return true;
}
