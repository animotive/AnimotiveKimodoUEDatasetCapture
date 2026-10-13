// Copyright Animotive. All Rights Reserved.

#include "DatasetInputSample.h"
#include "DatasetInputSampler.h"
#include "DatasetSidecar.h"
#include "DatasetSidecarSerializer.h"
#include "Misc/AutomationTest.h"
#include "MovieSceneDatasetInputSection.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetKeyboardSampleFieldsTest,
	"KimodoDatasetCapture.KeyboardMouse.SampleFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetKeyboardSampleFieldsTest::RunTest(const FString& Parameters)
{
	FDatasetInputSample Sample;

	// Keyboard bits live in their own mask, so a keyboard key can never be mistaken for a
	// gamepad button by anything reading the dataset.
	TestFalse(TEXT("No key held by default"), Sample.IsKeyHeld(EDatasetKeyboardButton::Space));
	TestEqual(TEXT("Keyboard mask starts empty"), Sample.KeyboardButtons, static_cast<uint32>(0));

	Sample.SetKeyHeld(EDatasetKeyboardButton::Space, true);
	TestTrue(TEXT("Space held after set"), Sample.IsKeyHeld(EDatasetKeyboardButton::Space));

	// The two masks are genuinely independent: holding the space bar must not make the
	// gamepad's bottom face button read as held, even though both mean "jump" in most games.
	TestFalse(TEXT("Space does not set a gamepad bit"), Sample.IsHeld(EDatasetGamepadButton::FaceBottom));

	Sample.SetHeld(EDatasetGamepadButton::FaceBottom, true);
	TestTrue(TEXT("Gamepad bit set independently"), Sample.IsHeld(EDatasetGamepadButton::FaceBottom));
	TestTrue(TEXT("Keyboard bit still set"), Sample.IsKeyHeld(EDatasetKeyboardButton::Space));

	Sample.SetKeyHeld(EDatasetKeyboardButton::Space, false);
	TestFalse(TEXT("Space cleared"), Sample.IsKeyHeld(EDatasetKeyboardButton::Space));
	TestTrue(TEXT("Clearing a key leaves the gamepad bit"), Sample.IsHeld(EDatasetGamepadButton::FaceBottom));

	// Highest declared keyboard bit, to catch a mask built with a too-narrow type.
	Sample.SetKeyHeld(EDatasetKeyboardButton::MouseMiddle, true);
	TestTrue(TEXT("Highest keyboard bit round-trips"), Sample.IsKeyHeld(EDatasetKeyboardButton::MouseMiddle));

	// Mouse delta is stored in its own fields, in the engine's own units. It is NOT scaled
	// into a stick value: a mouse has no rest position and no maximum, so a normalised
	// version would be an invention.
	FDatasetInputSample Mouse;
	Mouse.MouseDeltaX = 12.5f;
	Mouse.MouseDeltaY = -7.25f;
	TestEqual(TEXT("Mouse delta X stored unmodified"), Mouse.MouseDeltaX, 12.5f);
	TestEqual(TEXT("Mouse delta Y stored unmodified"), Mouse.MouseDeltaY, -7.25f);
	TestEqual(TEXT("Mouse delta does not touch the right stick"), Mouse.RightStickX, 0.0f);

	// Neutrality accounts for the new fields, or a keyboard-and-mouse session would look
	// entirely idle.
	FDatasetInputSample Neutral;
	TestTrue(TEXT("Default sample is neutral"), Neutral.IsNeutral());

	Neutral.SetKeyHeld(EDatasetKeyboardButton::KeyW, true);
	TestFalse(TEXT("A held key is not neutral"), Neutral.IsNeutral());

	FDatasetInputSample Moved;
	Moved.MouseDeltaX = 3.0f;
	TestFalse(TEXT("Mouse movement is not neutral"), Moved.IsNeutral());

	// Equality must cover the new fields, or a round-trip could drop a whole device.
	FDatasetInputSample A;
	A.SetKeyHeld(EDatasetKeyboardButton::KeyW, true);
	A.MouseDeltaX = 5.0f;

	FDatasetInputSample B = A;
	TestTrue(TEXT("Copy equals original"), A == B);

	B = A;
	B.SetKeyHeld(EDatasetKeyboardButton::KeyA, true);
	TestTrue(TEXT("Differing keyboard mask compares unequal"), A != B);

	B = A;
	B.MouseDeltaY = 1.0f;
	TestTrue(TEXT("Differing mouse delta compares unequal"), A != B);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetKeyboardTransitionsTest,
	"KimodoDatasetCapture.KeyboardMouse.Transitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetKeyboardTransitionsTest::RunTest(const FString& Parameters)
{
	FDatasetInputSample Idle;

	FDatasetInputSample SpaceDown;
	SpaceDown.SetKeyHeld(EDatasetKeyboardButton::Space, true);

	FDatasetInputSample StillDown;
	StillDown.SetKeyHeld(EDatasetKeyboardButton::Space, true);

	// Keyboard presses are derived the same way gamepad presses are, and for the same reason:
	// a stored transition can disagree with the state it came from.
	TestTrue(
		TEXT("Space reads as pressed"),
		FDatasetInputSample::WasKeyPressed(Idle, SpaceDown, EDatasetKeyboardButton::Space));
	TestFalse(
		TEXT("Space held across two frames is not a new press"),
		FDatasetInputSample::WasKeyPressed(SpaceDown, StillDown, EDatasetKeyboardButton::Space));
	TestTrue(
		TEXT("Space reads as released"),
		FDatasetInputSample::WasKeyReleased(StillDown, Idle, EDatasetKeyboardButton::Space));
	TestFalse(
		TEXT("A release is not a press"),
		FDatasetInputSample::WasKeyPressed(StillDown, Idle, EDatasetKeyboardButton::Space));

	// A gamepad transition query must not see keyboard state, and vice versa.
	FDatasetInputSample PadDown;
	PadDown.SetHeld(EDatasetGamepadButton::FaceBottom, true);
	TestFalse(
		TEXT("A keyboard press is not a gamepad press"),
		FDatasetInputSample::WasPressed(Idle, SpaceDown, EDatasetGamepadButton::FaceBottom));
	TestFalse(
		TEXT("A gamepad press is not a keyboard press"),
		FDatasetInputSample::WasKeyPressed(Idle, PadDown, EDatasetKeyboardButton::Space));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetKeyboardBindingsTest,
	"KimodoDatasetCapture.KeyboardMouse.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetKeyboardBindingsTest::RunTest(const FString& Parameters)
{
	const TArray<FDatasetInputSampler::FKeyboardBinding>& Bindings = FDatasetInputSampler::GetKeyboardBindings();

	TestEqual(TEXT("Eleven keyboard and mouse buttons are bound"), Bindings.Num(), 11);

	TSet<FKey> SeenKeys;
	TSet<uint32> SeenBits;
	for (const FDatasetInputSampler::FKeyboardBinding& Binding : Bindings)
	{
		bool bKeySeen = false;
		SeenKeys.Add(Binding.Key, &bKeySeen);
		if (bKeySeen)
		{
			AddError(FString::Printf(TEXT("Key '%s' is bound more than once."), *Binding.Key.ToString()));
		}

		bool bBitSeen = false;
		SeenBits.Add(static_cast<uint32>(Binding.Button), &bBitSeen);
		if (bBitSeen)
		{
			AddError(FString::Printf(TEXT("Keyboard bit %u is bound more than once."), static_cast<uint32>(Binding.Button)));
		}
	}

	// The bits are named after the KEYS, not after what they might mean. A client's project
	// may bind W to anything, so calling this bit "MoveForward" would assert something the
	// plugin cannot know. Meaning is the semantic layer's job, not the raw layer's.
	auto FindKeyFor = [&Bindings](EDatasetKeyboardButton Button) -> FKey
	{
		const FDatasetInputSampler::FKeyboardBinding* Found = Bindings.FindByPredicate(
			[Button](const FDatasetInputSampler::FKeyboardBinding& Binding)
			{
				return Binding.Button == Button;
			});

		return Found ? Found->Key : FKey();
	};

	TestTrue(TEXT("KeyW is W"), FindKeyFor(EDatasetKeyboardButton::KeyW) == EKeys::W);
	TestTrue(TEXT("KeyA is A"), FindKeyFor(EDatasetKeyboardButton::KeyA) == EKeys::A);
	TestTrue(TEXT("KeyS is S"), FindKeyFor(EDatasetKeyboardButton::KeyS) == EKeys::S);
	TestTrue(TEXT("KeyD is D"), FindKeyFor(EDatasetKeyboardButton::KeyD) == EKeys::D);
	TestTrue(TEXT("Space is the space bar"), FindKeyFor(EDatasetKeyboardButton::Space) == EKeys::SpaceBar);
	TestTrue(TEXT("KeyE is E"), FindKeyFor(EDatasetKeyboardButton::KeyE) == EKeys::E);
	TestTrue(TEXT("LeftShift is left shift"), FindKeyFor(EDatasetKeyboardButton::LeftShift) == EKeys::LeftShift);
	TestTrue(TEXT("LeftControl is left control"), FindKeyFor(EDatasetKeyboardButton::LeftControl) == EKeys::LeftControl);
	TestTrue(TEXT("MouseLeft is the left mouse button"), FindKeyFor(EDatasetKeyboardButton::MouseLeft) == EKeys::LeftMouseButton);
	TestTrue(TEXT("MouseRight is the right mouse button"), FindKeyFor(EDatasetKeyboardButton::MouseRight) == EKeys::RightMouseButton);
	TestTrue(TEXT("MouseMiddle is the middle mouse button"), FindKeyFor(EDatasetKeyboardButton::MouseMiddle) == EKeys::MiddleMouseButton);

	// The two mouse axes, in sample field order.
	const TArray<FKey>& MouseAxes = FDatasetInputSampler::GetMouseAxisKeys();
	TestEqual(TEXT("Two mouse axes are bound"), MouseAxes.Num(), 2);
	if (MouseAxes.Num() == 2)
	{
		TestTrue(TEXT("First mouse axis is X"), MouseAxes[0] == EKeys::MouseX);
		TestTrue(TEXT("Second mouse axis is Y"), MouseAxes[1] == EKeys::MouseY);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetKeyboardSamplingTest,
	"KimodoDatasetCapture.KeyboardMouse.SamplesBothDevices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetKeyboardSamplingTest::RunTest(const FString& Parameters)
{
	// Walking forward on the keyboard, looking with the mouse, gamepad untouched: the exact
	// situation of a keyboard-and-mouse test session.
	TMap<FKey, float> AxisValues;
	AxisValues.Add(EKeys::MouseX, 8.0f);
	AxisValues.Add(EKeys::MouseY, -3.5f);

	TSet<FKey> HeldKeys;
	HeldKeys.Add(EKeys::W);
	HeldKeys.Add(EKeys::LeftShift);
	HeldKeys.Add(EKeys::LeftMouseButton);

	auto ReadAxis = [&AxisValues](const FKey& Key) -> float
	{
		const float* Value = AxisValues.Find(Key);
		return Value ? *Value : 0.0f;
	};

	auto IsPressed = [&HeldKeys](const FKey& Key) -> bool
	{
		return HeldKeys.Contains(Key);
	};

	const FDatasetInputSample Sample = FDatasetInputSampler::Sample(42, ReadAxis, IsPressed);

	TestEqual(TEXT("Frame number carried"), Sample.FrameNumber, 42);

	TestTrue(TEXT("W recorded"), Sample.IsKeyHeld(EDatasetKeyboardButton::KeyW));
	TestTrue(TEXT("Left shift recorded"), Sample.IsKeyHeld(EDatasetKeyboardButton::LeftShift));
	TestTrue(TEXT("Left mouse button recorded"), Sample.IsKeyHeld(EDatasetKeyboardButton::MouseLeft));
	TestFalse(TEXT("Unpressed key not recorded"), Sample.IsKeyHeld(EDatasetKeyboardButton::KeyS));

	TestEqual(TEXT("Mouse delta X sampled from MouseX"), Sample.MouseDeltaX, 8.0f);
	TestEqual(TEXT("Mouse delta Y sampled from MouseY"), Sample.MouseDeltaY, -3.5f);

	// The gamepad fields stay empty, which is the honest record: no pad was touched. A
	// normalised scheme would have invented stick values here.
	TestEqual(TEXT("Left stick untouched"), Sample.LeftStickY, 0.0f);
	TestEqual(TEXT("Gamepad mask untouched"), Sample.Buttons, static_cast<uint32>(0));

	// And a keyboard-driven frame is not neutral, so the dataset can tell the player was
	// moving.
	TestFalse(TEXT("A keyboard-driven frame is not neutral"), Sample.IsNeutral());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetKeyboardChannelsTest,
	"KimodoDatasetCapture.KeyboardMouse.SurvivesTheSectionAndSidecar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetKeyboardChannelsTest::RunTest(const FString& Parameters)
{
	UMovieSceneDatasetInputSection* Section = NewObject<UMovieSceneDatasetInputSection>(GetTransientPackage());
	if (!Section)
	{
		AddError(TEXT("Failed to construct a section."));
		return false;
	}

	FDatasetInputSample Written;
	Written.FrameNumber = 800;
	Written.SetKeyHeld(EDatasetKeyboardButton::KeyW, true);
	Written.SetKeyHeld(EDatasetKeyboardButton::Space, true);
	Written.MouseDeltaX = 6.25f;
	Written.MouseDeltaY = -2.5f;

	Section->WriteSample(Written);

	FDatasetInputSample ReadBack;
	TestTrue(TEXT("Sample read back"), Section->TryReadSample(FFrameNumber(800), ReadBack));
	TestTrue(TEXT("Keyboard bits survived the channels"), ReadBack.IsKeyHeld(EDatasetKeyboardButton::KeyW));
	TestTrue(TEXT("Second keyboard bit survived"), ReadBack.IsKeyHeld(EDatasetKeyboardButton::Space));
	TestEqual(TEXT("Mouse delta X survived"), ReadBack.MouseDeltaX, 6.25f);
	TestEqual(TEXT("Mouse delta Y survived"), ReadBack.MouseDeltaY, -2.5f);
	TestTrue(TEXT("Whole sample round-trips through the section"), ReadBack == Written);

	// The sidecar schema had to change to carry a new device, so its version must have moved
	// past 1 (it is 3 since synthetic clips). A reader that speaks v1 must not silently accept
	// a newer file.
	TestTrue(TEXT("Schema version moved past 1"), KDatasetSidecarSchemaVersion >= 2);
	TestTrue(TEXT("Version 2 is supported"), FDatasetSidecarSerializer::IsSupportedSchemaVersion(2));
	TestFalse(TEXT("Version 1 is no longer supported"), FDatasetSidecarSerializer::IsSupportedSchemaVersion(1));

	FDatasetSidecar Sidecar;
	Sidecar.AddLayer(EDatasetCaptureLayer::RawGamepadInput);
	Sidecar.AddLayer(EDatasetCaptureLayer::RawKeyboardMouseInput);
	Sidecar.AddSample(Written);

	FString Json;
	TestTrue(TEXT("Sidecar serialises"), FDatasetSidecarSerializer::ToJson(Sidecar, Json));

	FDatasetSidecar Restored;
	FText FailureReason;
	TestTrue(TEXT("Sidecar parses"), FDatasetSidecarSerializer::FromJson(Json, Restored, FailureReason));
	TestTrue(TEXT("Sidecar round-trips with keyboard and mouse"), Restored == Sidecar);
	TestTrue(TEXT("Keyboard layer recorded"), Restored.HasLayer(EDatasetCaptureLayer::RawKeyboardMouseInput));

	FDatasetInputSample FromSidecar;
	TestTrue(TEXT("Sidecar carries the sample"), Restored.TryGetSampleAtFrame(800, FromSidecar));
	TestTrue(TEXT("Sidecar preserved the keyboard bits"), FromSidecar.IsKeyHeld(EDatasetKeyboardButton::KeyW));
	TestEqual(TEXT("Sidecar preserved the mouse delta"), FromSidecar.MouseDeltaX, 6.25f);

	return true;
}
