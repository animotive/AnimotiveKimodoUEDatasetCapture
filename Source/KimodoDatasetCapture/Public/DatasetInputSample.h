// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DatasetInputSample.generated.h"

/**
 * Gamepad buttons captured as a bitmask.
 *
 * Values are explicit and must never be renumbered: they are written into sidecar JSON,
 * and a renumbering would silently reinterpret every dataset already captured.
 */
enum class EDatasetGamepadButton : uint32
{
	None            = 0,

	FaceBottom      = 1u << 0,  // A / Cross      - conventionally Jump
	FaceRight       = 1u << 1,  // B / Circle
	FaceLeft        = 1u << 2,  // X / Square     - conventionally Interact
	FaceTop         = 1u << 3,  // Y / Triangle

	ShoulderLeft    = 1u << 4,
	ShoulderRight   = 1u << 5,

	ThumbLeft       = 1u << 6,  // left stick click
	ThumbRight      = 1u << 7,  // right stick click

	DPadUp          = 1u << 8,
	DPadDown        = 1u << 9,
	DPadLeft        = 1u << 10,
	DPadRight       = 1u << 11,

	SpecialLeft     = 1u << 12, // Back / Select
	SpecialRight    = 1u << 13, // Start / Options
};

ENUM_CLASS_FLAGS(EDatasetGamepadButton);

/**
 * One frame of raw gamepad state.
 *
 * Deliberately allocation-free: every member is a fixed-size value, so a capture buffer of
 * these can be reserved once at record start and never grows during the take. Nothing in
 * this struct may become a heap-allocating type.
 *
 * FrameNumber is a frame index on the take's own tick resolution, never a time in seconds.
 * Seconds would reintroduce the drift the MovieScene-based design exists to eliminate.
 */
USTRUCT()
struct KIMODODATASETCAPTURE_API FDatasetInputSample
{
	GENERATED_BODY()

	/** Frame this sample belongs to, on the take's tick resolution. */
	UPROPERTY()
	int32 FrameNumber = 0;

	/** Left stick, each axis in [-1, 1]. */
	UPROPERTY()
	float LeftStickX = 0.0f;

	UPROPERTY()
	float LeftStickY = 0.0f;

	/** Right stick, each axis in [-1, 1]. */
	UPROPERTY()
	float RightStickX = 0.0f;

	UPROPERTY()
	float RightStickY = 0.0f;

	/** Analog triggers, each in [0, 1]. */
	UPROPERTY()
	float LeftTrigger = 0.0f;

	UPROPERTY()
	float RightTrigger = 0.0f;

	/** Buttons held on this frame, as an EDatasetGamepadButton bitmask. */
	UPROPERTY()
	uint32 Buttons = 0;

	/** True when Button is held on this frame. */
	bool IsHeld(EDatasetGamepadButton Button) const;

	/** Marks Button held on this frame. */
	void SetHeld(EDatasetGamepadButton Button, bool bHeld);

	/** True when no button is held and every axis reads neutral. */
	bool IsNeutral() const;

	/**
	 * Buttons held on this sample that were not held on Previous — the presses that happened
	 * between the two frames. Derived rather than stored, so a press can never disagree with
	 * the button state it was derived from.
	 */
	static EDatasetGamepadButton GetPressedSince(const FDatasetInputSample& Previous, const FDatasetInputSample& Current);

	/** Buttons held on Previous that are no longer held on this sample. */
	static EDatasetGamepadButton GetReleasedSince(const FDatasetInputSample& Previous, const FDatasetInputSample& Current);

	/** True when Button transitions from up to down between Previous and Current. */
	static bool WasPressed(const FDatasetInputSample& Previous, const FDatasetInputSample& Current, EDatasetGamepadButton Button);

	/** True when Button transitions from down to up between Previous and Current. */
	static bool WasReleased(const FDatasetInputSample& Previous, const FDatasetInputSample& Current, EDatasetGamepadButton Button);

	bool operator==(const FDatasetInputSample& Other) const;
	bool operator!=(const FDatasetInputSample& Other) const { return !(*this == Other); }
};
