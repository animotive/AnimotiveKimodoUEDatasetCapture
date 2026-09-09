// Copyright Animotive. All Rights Reserved.

#include "DatasetInputSample.h"

// RED PHASE: every body below deliberately returns a neutral value so the tests written
// against this API fail at runtime rather than failing to compile. Implemented in the
// Green task that follows.

bool FDatasetInputSample::IsHeld(EDatasetGamepadButton Button) const
{
	return false;
}

void FDatasetInputSample::SetHeld(EDatasetGamepadButton Button, bool bHeld)
{
}

bool FDatasetInputSample::IsNeutral() const
{
	return false;
}

EDatasetGamepadButton FDatasetInputSample::GetPressedSince(const FDatasetInputSample& Previous, const FDatasetInputSample& Current)
{
	return EDatasetGamepadButton::None;
}

EDatasetGamepadButton FDatasetInputSample::GetReleasedSince(const FDatasetInputSample& Previous, const FDatasetInputSample& Current)
{
	return EDatasetGamepadButton::None;
}

bool FDatasetInputSample::WasPressed(const FDatasetInputSample& Previous, const FDatasetInputSample& Current, EDatasetGamepadButton Button)
{
	return false;
}

bool FDatasetInputSample::WasReleased(const FDatasetInputSample& Previous, const FDatasetInputSample& Current, EDatasetGamepadButton Button)
{
	return false;
}

bool FDatasetInputSample::operator==(const FDatasetInputSample& Other) const
{
	return false;
}
