// Copyright Animotive. All Rights Reserved.

#include "DatasetInputSampler.h"

#include "GameFramework/PlayerInput.h"

const TArray<FDatasetInputSampler::FButtonBinding>& FDatasetInputSampler::GetButtonBindings()
{
	// Declared in bit order, so this table can be read against EDatasetGamepadButton and any
	// omission is visible. A bit with no key here would never be recorded; a key bound to the
	// wrong bit would produce a dataset that looks healthy and teaches the model the wrong
	// thing, which is why a test asserts this table rather than trusting it.
	static const TArray<FButtonBinding> Bindings =
	{
		{ EKeys::Gamepad_FaceButton_Bottom, EDatasetGamepadButton::FaceBottom },
		{ EKeys::Gamepad_FaceButton_Right,  EDatasetGamepadButton::FaceRight },
		{ EKeys::Gamepad_FaceButton_Left,   EDatasetGamepadButton::FaceLeft },
		{ EKeys::Gamepad_FaceButton_Top,    EDatasetGamepadButton::FaceTop },

		{ EKeys::Gamepad_LeftShoulder,      EDatasetGamepadButton::ShoulderLeft },
		{ EKeys::Gamepad_RightShoulder,     EDatasetGamepadButton::ShoulderRight },

		{ EKeys::Gamepad_LeftThumbstick,    EDatasetGamepadButton::ThumbLeft },
		{ EKeys::Gamepad_RightThumbstick,   EDatasetGamepadButton::ThumbRight },

		{ EKeys::Gamepad_DPad_Up,           EDatasetGamepadButton::DPadUp },
		{ EKeys::Gamepad_DPad_Down,         EDatasetGamepadButton::DPadDown },
		{ EKeys::Gamepad_DPad_Left,         EDatasetGamepadButton::DPadLeft },
		{ EKeys::Gamepad_DPad_Right,        EDatasetGamepadButton::DPadRight },

		{ EKeys::Gamepad_Special_Left,      EDatasetGamepadButton::SpecialLeft },
		{ EKeys::Gamepad_Special_Right,     EDatasetGamepadButton::SpecialRight },
	};

	return Bindings;
}

const TArray<FKey>& FDatasetInputSampler::GetAxisKeys()
{
	// Order matches the field order of FDatasetInputSample, and a test asserts that, because
	// two axes swapped here would mirror a character's movement in every clip.
	static const TArray<FKey> AxisKeys =
	{
		EKeys::Gamepad_LeftX,
		EKeys::Gamepad_LeftY,
		EKeys::Gamepad_RightX,
		EKeys::Gamepad_RightY,
		EKeys::Gamepad_LeftTriggerAxis,
		EKeys::Gamepad_RightTriggerAxis,
	};

	return AxisKeys;
}

FDatasetInputSample FDatasetInputSampler::Sample(int32 FrameNumber, FAxisReader ReadAxis, FButtonReader IsPressed)
{
	FDatasetInputSample Sample;
	Sample.FrameNumber = FrameNumber;

	// Read through the same table the tests assert, rather than by naming keys again here.
	// Two lists of keys would eventually disagree, and the disagreement would be invisible.
	const TArray<FKey>& AxisKeys = GetAxisKeys();
	if (AxisKeys.Num() == 6)
	{
		// Stored exactly as read: no deadzone, no clamp, no filter. A dataset carries what
		// the hardware reported; the plugin's view of a resting stick lives in IsNeutral.
		Sample.LeftStickX = ReadAxis(AxisKeys[0]);
		Sample.LeftStickY = ReadAxis(AxisKeys[1]);
		Sample.RightStickX = ReadAxis(AxisKeys[2]);
		Sample.RightStickY = ReadAxis(AxisKeys[3]);
		Sample.LeftTrigger = ReadAxis(AxisKeys[4]);
		Sample.RightTrigger = ReadAxis(AxisKeys[5]);
	}

	for (const FButtonBinding& Binding : GetButtonBindings())
	{
		if (IsPressed(Binding.Key))
		{
			Sample.SetHeld(Binding.Button, true);
		}
	}

	return Sample;
}

FDatasetInputSample FDatasetInputSampler::SampleFromPlayerInput(int32 FrameNumber, const UPlayerInput& PlayerInput)
{
	// GetKeyValue rather than GetRawKeyValue: the value the game acted on is the value that
	// explains the animation, and the animation is what the dataset pairs it with. The raw
	// device reading would describe a different causal chain than the one recorded.
	auto ReadAxis = [&PlayerInput](const FKey& Key) -> float
	{
		return PlayerInput.GetKeyValue(Key);
	};

	auto IsPressed = [&PlayerInput](const FKey& Key) -> bool
	{
		return PlayerInput.IsPressed(Key);
	};

	return Sample(FrameNumber, ReadAxis, IsPressed);
}
