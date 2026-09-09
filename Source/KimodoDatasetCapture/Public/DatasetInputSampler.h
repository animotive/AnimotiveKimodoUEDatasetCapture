// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasetInputSample.h"
#include "InputCoreTypes.h"

class UPlayerInput;

/**
 * Reads one frame of gamepad state.
 *
 * The engine keys are named in ONE table here, and the sampler takes its readers as
 * parameters, so the mapping from hardware to dataset can be tested without a running game.
 * That table is exactly where a silent miswiring would live: a stick bound to the wrong
 * field, or a button bit assigned to the wrong face button, would produce a dataset that
 * looks perfectly healthy and teaches the model the wrong thing.
 */
class KIMODODATASETCAPTURE_API FDatasetInputSampler
{
public:
	/** Reads an analog axis, in the engine's own units. */
	using FAxisReader = TFunctionRef<float(const FKey&)>;

	/** Reads whether a button is held. */
	using FButtonReader = TFunctionRef<bool(const FKey&)>;

	/** One engine key mapped to one dataset button bit. */
	struct FButtonBinding
	{
		FKey Key;
		EDatasetGamepadButton Button;
	};

	/** The complete key-to-bit table, in bit order. */
	static const TArray<FButtonBinding>& GetButtonBindings();

	/** The six analog keys, in the field order of FDatasetInputSample. */
	static const TArray<FKey>& GetAxisKeys();

	/**
	 * Builds a sample for FrameNumber from the supplied readers.
	 *
	 * Axis values are stored exactly as read. Nothing is deadzoned, clamped or filtered here:
	 * a dataset must carry what the hardware reported, and a plugin's opinion about a
	 * resting stick belongs in the neutrality query, not in the recorded data.
	 */
	static FDatasetInputSample Sample(int32 FrameNumber, FAxisReader ReadAxis, FButtonReader IsPressed);

	/** Builds a sample by reading a live UPlayerInput. */
	static FDatasetInputSample SampleFromPlayerInput(int32 FrameNumber, const UPlayerInput& PlayerInput);
};
