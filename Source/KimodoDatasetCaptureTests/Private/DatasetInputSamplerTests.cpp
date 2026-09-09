// Copyright Animotive. All Rights Reserved.

#include "DatasetInputSampler.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetInputSamplerBindingsTest,
	"KimodoDatasetCapture.InputSampler.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetInputSamplerBindingsTest::RunTest(const FString& Parameters)
{
	const TArray<FDatasetInputSampler::FButtonBinding>& Bindings = FDatasetInputSampler::GetButtonBindings();

	// Fourteen declared bits, fourteen bindings. A bit with no key would never be recorded,
	// and a key with no bit would be silently discarded.
	TestEqual(TEXT("Every declared button bit is bound"), Bindings.Num(), 14);

	// No key bound twice, and no bit bound twice: either would make one input overwrite
	// another and the dataset would disagree with what the operator pressed.
	TSet<FKey> SeenKeys;
	TSet<uint32> SeenBits;
	for (const FDatasetInputSampler::FButtonBinding& Binding : Bindings)
	{
		if (!Binding.Key.IsValid())
		{
			AddError(TEXT("A binding has an invalid key."));
			continue;
		}

		bool bKeyAlreadySeen = false;
		SeenKeys.Add(Binding.Key, &bKeyAlreadySeen);
		if (bKeyAlreadySeen)
		{
			AddError(FString::Printf(TEXT("Key '%s' is bound more than once."), *Binding.Key.ToString()));
		}

		bool bBitAlreadySeen = false;
		SeenBits.Add(static_cast<uint32>(Binding.Button), &bBitAlreadySeen);
		if (bBitAlreadySeen)
		{
			AddError(FString::Printf(TEXT("Button bit %u is bound more than once."), static_cast<uint32>(Binding.Button)));
		}
	}

	// The two buttons the product guide names explicitly must be bound to the conventional
	// keys, since a caption that says "presses jump" has to correspond to the jump button.
	const FDatasetInputSampler::FButtonBinding* JumpBinding = Bindings.FindByPredicate(
		[](const FDatasetInputSampler::FButtonBinding& Binding)
		{
			return Binding.Button == EDatasetGamepadButton::FaceBottom;
		});
	TestTrue(TEXT("FaceBottom is bound"), JumpBinding != nullptr);
	if (JumpBinding)
	{
		TestTrue(TEXT("FaceBottom is the bottom face button"), JumpBinding->Key == EKeys::Gamepad_FaceButton_Bottom);
	}

	const FDatasetInputSampler::FButtonBinding* InteractBinding = Bindings.FindByPredicate(
		[](const FDatasetInputSampler::FButtonBinding& Binding)
		{
			return Binding.Button == EDatasetGamepadButton::FaceLeft;
		});
	TestTrue(TEXT("FaceLeft is bound"), InteractBinding != nullptr);
	if (InteractBinding)
	{
		TestTrue(TEXT("FaceLeft is the left face button"), InteractBinding->Key == EKeys::Gamepad_FaceButton_Left);
	}

	// Six analog keys, in the field order of the sample, none of them duplicated.
	const TArray<FKey>& AxisKeys = FDatasetInputSampler::GetAxisKeys();
	TestEqual(TEXT("Six analog axes are bound"), AxisKeys.Num(), 6);
	if (AxisKeys.Num() == 6)
	{
		TestTrue(TEXT("First axis is left stick X"), AxisKeys[0] == EKeys::Gamepad_LeftX);
		TestTrue(TEXT("Second axis is left stick Y"), AxisKeys[1] == EKeys::Gamepad_LeftY);
		TestTrue(TEXT("Third axis is right stick X"), AxisKeys[2] == EKeys::Gamepad_RightX);
		TestTrue(TEXT("Fourth axis is right stick Y"), AxisKeys[3] == EKeys::Gamepad_RightY);
		TestTrue(TEXT("Fifth axis is left trigger"), AxisKeys[4] == EKeys::Gamepad_LeftTriggerAxis);
		TestTrue(TEXT("Sixth axis is right trigger"), AxisKeys[5] == EKeys::Gamepad_RightTriggerAxis);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetInputSamplerSamplingTest,
	"KimodoDatasetCapture.InputSampler.SamplesEachFieldFromItsOwnKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetInputSamplerSamplingTest::RunTest(const FString& Parameters)
{
	// A distinct value per axis, so a field reading the wrong key is caught rather than
	// masked by two axes sharing a value. This is the whole reason the readers are injected.
	TMap<FKey, float> AxisValues;
	AxisValues.Add(EKeys::Gamepad_LeftX, 0.125f);
	AxisValues.Add(EKeys::Gamepad_LeftY, -0.25f);
	AxisValues.Add(EKeys::Gamepad_RightX, 0.375f);
	AxisValues.Add(EKeys::Gamepad_RightY, -0.5f);
	AxisValues.Add(EKeys::Gamepad_LeftTriggerAxis, 0.625f);
	AxisValues.Add(EKeys::Gamepad_RightTriggerAxis, 0.75f);

	TSet<FKey> HeldKeys;
	HeldKeys.Add(EKeys::Gamepad_FaceButton_Bottom);
	HeldKeys.Add(EKeys::Gamepad_LeftShoulder);

	auto ReadAxis = [&AxisValues](const FKey& Key) -> float
	{
		const float* Value = AxisValues.Find(Key);
		return Value ? *Value : 0.0f;
	};

	auto IsPressed = [&HeldKeys](const FKey& Key) -> bool
	{
		return HeldKeys.Contains(Key);
	};

	const FDatasetInputSample Sample = FDatasetInputSampler::Sample(1234, ReadAxis, IsPressed);

	TestEqual(TEXT("Frame number carried"), Sample.FrameNumber, 1234);
	TestEqual(TEXT("Left stick X from its own key"), Sample.LeftStickX, 0.125f);
	TestEqual(TEXT("Left stick Y from its own key"), Sample.LeftStickY, -0.25f);
	TestEqual(TEXT("Right stick X from its own key"), Sample.RightStickX, 0.375f);
	TestEqual(TEXT("Right stick Y from its own key"), Sample.RightStickY, -0.5f);
	TestEqual(TEXT("Left trigger from its own key"), Sample.LeftTrigger, 0.625f);
	TestEqual(TEXT("Right trigger from its own key"), Sample.RightTrigger, 0.75f);

	TestTrue(TEXT("Held face button recorded"), Sample.IsHeld(EDatasetGamepadButton::FaceBottom));
	TestTrue(TEXT("Held shoulder recorded"), Sample.IsHeld(EDatasetGamepadButton::ShoulderLeft));
	TestFalse(TEXT("Unheld button not recorded"), Sample.IsHeld(EDatasetGamepadButton::FaceTop));
	TestFalse(TEXT("Unheld shoulder not recorded"), Sample.IsHeld(EDatasetGamepadButton::ShoulderRight));

	// Raw values are stored EXACTLY as read: no deadzone, no clamping, no filtering. A
	// dataset must carry what the hardware reported, and a resting-stick opinion belongs in
	// the neutrality query rather than in the recorded data.
	TMap<FKey, float> DriftValues;
	DriftValues.Add(EKeys::Gamepad_LeftX, 0.004f);
	auto ReadDrift = [&DriftValues](const FKey& Key) -> float
	{
		const float* Value = DriftValues.Find(Key);
		return Value ? *Value : 0.0f;
	};
	auto NothingPressed = [](const FKey&) -> bool { return false; };

	const FDatasetInputSample Drifting = FDatasetInputSampler::Sample(0, ReadDrift, NothingPressed);
	TestEqual(TEXT("Tiny stick drift is stored unmodified"), Drifting.LeftStickX, 0.004f);
	// ...while the neutrality query still treats it as at rest, which is the correct place
	// for that judgement.
	TestTrue(TEXT("Drifting sample still reads as neutral"), Drifting.IsNeutral());

	// A frame with nothing touched is a valid sample, not a skipped one: the dataset needs to
	// know the player was standing still.
	auto ReadNothing = [](const FKey&) -> float { return 0.0f; };
	const FDatasetInputSample Idle = FDatasetInputSampler::Sample(7, ReadNothing, NothingPressed);
	TestEqual(TEXT("Idle sample keeps its frame"), Idle.FrameNumber, 7);
	TestEqual(TEXT("Idle sample holds no buttons"), Idle.Buttons, static_cast<uint32>(0));
	TestTrue(TEXT("Idle sample is neutral"), Idle.IsNeutral());

	return true;
}
