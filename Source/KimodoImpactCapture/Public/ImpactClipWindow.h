// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ImpactClipWindow.generated.h"

enum class EImpactClipPhase : uint8
{
	/** Exact source motion before the first hit. */
	PreRoll,
	/** From the first hit until the character starts to settle. */
	Reaction,
	/** Every hit delivered and the body quiet; counting quiet frames. */
	Settle,
	Done,
};

/** Why a clip ended. Serialised by name in sidecars: append only. */
UENUM()
enum class EImpactClipEnd : uint8
{
	None,
	/** The body came to rest (recovered or lying still). */
	Settled,
	/** The reaction ran to the batch's ceiling without settling. Recorded, never silent. */
	ReactionCeiling,
	/** The clip was abandoned; the reason is recorded. */
	Invalidated,
};

struct KIMODOIMPACTCAPTURE_API FImpactClipWindowConfig
{
	int32 PreRollFrames = 30;
	int32 MaxReactionFrames = 240;
	int32 SettleFrames = 20;
	/** Below this speed the body counts as quiet, cm/s. */
	float SettleSpeedCmPerSecond = 15.0f;
	/** Frames after the first hit at which the last scheduled hit lands. */
	int32 LastHitOffsetFrames = 0;

	/** False with a reason when the window could never run or never end. */
	bool Validate(FText& OutReason) const;
};

/**
 * Decides, frame by frame, which phase a clip is in and when it ends. Pure: it sees only the
 * frame count and a measured body speed, so it is tested without a physics scene.
 */
class KIMODOIMPACTCAPTURE_API FImpactClipWindow
{
public:
	explicit FImpactClipWindow(const FImpactClipWindowConfig& InConfig);

	/** Consumes one simulated frame with the body speed measured on it. No-op once Done. */
	EImpactClipPhase Advance(float BodySpeedCmPerSecond);

	/** Ends the clip as invalid, recording why. */
	void Invalidate(const FText& Reason);

	EImpactClipPhase GetPhase() const { return Phase; }
	EImpactClipEnd GetEnd() const { return End; }
	const FText& GetInvalidReason() const { return InvalidReason; }

	/** Frames consumed so far; the clip's length once Done. */
	int32 GetFramesConsumed() const { return FramesConsumed; }

	/** Phase the next call to Advance will run in: the frame about to be simulated. */
	EImpactClipPhase PhaseOfNextFrame() const;

	/** True when the next frame to be simulated is the first hit. */
	bool IsNextFrameFirstHit() const;

private:
	FImpactClipWindowConfig Config;
	EImpactClipPhase Phase = EImpactClipPhase::PreRoll;
	EImpactClipEnd End = EImpactClipEnd::None;
	FText InvalidReason;
	int32 FramesConsumed = 0;
	int32 QuietFrames = 0;
};
