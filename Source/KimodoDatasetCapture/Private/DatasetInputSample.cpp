// Copyright Animotive. All Rights Reserved.

#include "DatasetInputSample.h"

namespace
{
	/**
	 * Deflection below which a stick or trigger reads as neutral.
	 *
	 * Sticks rest slightly off centre on real hardware, so an exact zero test would report
	 * almost every resting frame as active. This threshold is for the neutrality query only:
	 * raw axis values are stored unmodified, because a dataset must carry what the hardware
	 * reported rather than what the plugin thought of it.
	 */
	constexpr float NeutralAxisThreshold = 0.02f;

	FORCEINLINE bool IsAxisNeutral(float Value)
	{
		return FMath::Abs(Value) <= NeutralAxisThreshold;
	}
}

bool FDatasetInputSample::IsHeld(EDatasetGamepadButton Button) const
{
	const uint32 Mask = static_cast<uint32>(Button);
	return Mask != 0 && (Buttons & Mask) == Mask;
}

void FDatasetInputSample::SetHeld(EDatasetGamepadButton Button, bool bHeld)
{
	const uint32 Mask = static_cast<uint32>(Button);
	if (bHeld)
	{
		Buttons |= Mask;
	}
	else
	{
		Buttons &= ~Mask;
	}
}

bool FDatasetInputSample::IsKeyHeld(EDatasetKeyboardButton Key) const
{
	const uint32 Mask = static_cast<uint32>(Key);
	return Mask != 0 && (KeyboardButtons & Mask) == Mask;
}

void FDatasetInputSample::SetKeyHeld(EDatasetKeyboardButton Key, bool bHeld)
{
	const uint32 Mask = static_cast<uint32>(Key);
	if (bHeld)
	{
		KeyboardButtons |= Mask;
	}
	else
	{
		KeyboardButtons &= ~Mask;
	}
}

bool FDatasetInputSample::IsNeutral() const
{
	// FrameNumber is deliberately excluded: it is bookkeeping, not input.
	//
	// Mouse delta uses the same axis threshold as the sticks. It is not a perfect fit - a
	// mouse reports pixels rather than a normalised deflection - but a sub-pixel delta is
	// noise by any measure, and treating it as movement would mark every frame of a
	// keyboard-and-mouse session as active.
	return Buttons == 0
		&& KeyboardButtons == 0
		&& IsAxisNeutral(MouseDeltaX)
		&& IsAxisNeutral(MouseDeltaY)
		&& IsAxisNeutral(LeftStickX)
		&& IsAxisNeutral(LeftStickY)
		&& IsAxisNeutral(RightStickX)
		&& IsAxisNeutral(RightStickY)
		&& IsAxisNeutral(LeftTrigger)
		&& IsAxisNeutral(RightTrigger);
}

EDatasetGamepadButton FDatasetInputSample::GetPressedSince(const FDatasetInputSample& Previous, const FDatasetInputSample& Current)
{
	// Held now and not held before. A button held across both frames is not a new press.
	return static_cast<EDatasetGamepadButton>(Current.Buttons & ~Previous.Buttons);
}

EDatasetGamepadButton FDatasetInputSample::GetReleasedSince(const FDatasetInputSample& Previous, const FDatasetInputSample& Current)
{
	return static_cast<EDatasetGamepadButton>(Previous.Buttons & ~Current.Buttons);
}

bool FDatasetInputSample::WasPressed(const FDatasetInputSample& Previous, const FDatasetInputSample& Current, EDatasetGamepadButton Button)
{
	const uint32 Mask = static_cast<uint32>(Button);
	return Mask != 0 && (static_cast<uint32>(GetPressedSince(Previous, Current)) & Mask) == Mask;
}

bool FDatasetInputSample::WasReleased(const FDatasetInputSample& Previous, const FDatasetInputSample& Current, EDatasetGamepadButton Button)
{
	const uint32 Mask = static_cast<uint32>(Button);
	return Mask != 0 && (static_cast<uint32>(GetReleasedSince(Previous, Current)) & Mask) == Mask;
}

bool FDatasetInputSample::WasKeyPressed(const FDatasetInputSample& Previous, const FDatasetInputSample& Current, EDatasetKeyboardButton Key)
{
	const uint32 Mask = static_cast<uint32>(Key);
	return Mask != 0 && ((Current.KeyboardButtons & ~Previous.KeyboardButtons) & Mask) == Mask;
}

bool FDatasetInputSample::WasKeyReleased(const FDatasetInputSample& Previous, const FDatasetInputSample& Current, EDatasetKeyboardButton Key)
{
	const uint32 Mask = static_cast<uint32>(Key);
	return Mask != 0 && ((Previous.KeyboardButtons & ~Current.KeyboardButtons) & Mask) == Mask;
}

bool FDatasetInputSample::operator==(const FDatasetInputSample& Other) const
{
	// Exact float comparison is intended. This operator backs the JSON round-trip tests,
	// where any difference at all means data was lost or reinterpreted in transit.
	return FrameNumber == Other.FrameNumber
		&& LeftStickX == Other.LeftStickX
		&& LeftStickY == Other.LeftStickY
		&& RightStickX == Other.RightStickX
		&& RightStickY == Other.RightStickY
		&& LeftTrigger == Other.LeftTrigger
		&& RightTrigger == Other.RightTrigger
		&& Buttons == Other.Buttons
		&& KeyboardButtons == Other.KeyboardButtons
		&& MouseDeltaX == Other.MouseDeltaX
		&& MouseDeltaY == Other.MouseDeltaY;
}
