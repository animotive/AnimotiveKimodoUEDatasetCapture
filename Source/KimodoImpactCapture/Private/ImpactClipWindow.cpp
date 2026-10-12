// Copyright Animotive. All Rights Reserved.

#include "ImpactClipWindow.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactClipWindow)

#define LOCTEXT_NAMESPACE "ImpactClipWindow"

bool FImpactClipWindowConfig::Validate(FText& OutReason) const
{
	if (PreRollFrames < 0)
	{
		OutReason = LOCTEXT("NegativePreRoll", "Pre-roll cannot be negative.");
		return false;
	}
	if (MaxReactionFrames < 1)
	{
		OutReason = LOCTEXT("NoCeiling", "The reaction ceiling must be at least one frame.");
		return false;
	}
	if (SettleFrames < 1)
	{
		OutReason = LOCTEXT("NoSettle", "Settling must take at least one frame.");
		return false;
	}
	if (LastHitOffsetFrames < 0 || LastHitOffsetFrames >= MaxReactionFrames)
	{
		OutReason = LOCTEXT("LastHitBeyondCeiling", "The last scheduled hit lands after the reaction ceiling, so it would never be delivered.");
		return false;
	}
	return true;
}

FImpactClipWindow::FImpactClipWindow(const FImpactClipWindowConfig& InConfig)
	: Config(InConfig)
{
	Phase = Config.PreRollFrames > 0 ? EImpactClipPhase::PreRoll : EImpactClipPhase::Reaction;
}

EImpactClipPhase FImpactClipWindow::Advance(float BodySpeedCmPerSecond)
{
	if (Phase == EImpactClipPhase::Done)
	{
		return Phase;
	}

	const int32 Frame = FramesConsumed++;
	if (Frame < Config.PreRollFrames)
	{
		Phase = FramesConsumed < Config.PreRollFrames ? EImpactClipPhase::PreRoll : EImpactClipPhase::Reaction;
		return Phase;
	}

	const int32 SinceHit = Frame - Config.PreRollFrames;
	// Quiet only counts once every hit has landed: a burst's pauses are not a recovery.
	const bool bHitsDelivered = SinceHit >= Config.LastHitOffsetFrames;
	const bool bQuiet = BodySpeedCmPerSecond < Config.SettleSpeedCmPerSecond;
	QuietFrames = (bQuiet && bHitsDelivered) ? QuietFrames + 1 : 0;

	if (QuietFrames >= Config.SettleFrames)
	{
		Phase = EImpactClipPhase::Done;
		End = EImpactClipEnd::Settled;
	}
	else if (SinceHit + 1 >= Config.MaxReactionFrames)
	{
		Phase = EImpactClipPhase::Done;
		End = EImpactClipEnd::ReactionCeiling;
	}
	else
	{
		Phase = QuietFrames > 0 ? EImpactClipPhase::Settle : EImpactClipPhase::Reaction;
	}
	return Phase;
}

void FImpactClipWindow::Invalidate(const FText& Reason)
{
	Phase = EImpactClipPhase::Done;
	End = EImpactClipEnd::Invalidated;
	InvalidReason = Reason;
}

EImpactClipPhase FImpactClipWindow::PhaseOfNextFrame() const
{
	if (Phase == EImpactClipPhase::Done)
	{
		return EImpactClipPhase::Done;
	}
	if (FramesConsumed < Config.PreRollFrames)
	{
		return EImpactClipPhase::PreRoll;
	}
	return Phase == EImpactClipPhase::Settle ? EImpactClipPhase::Settle : EImpactClipPhase::Reaction;
}

bool FImpactClipWindow::IsNextFrameFirstHit() const
{
	return Phase != EImpactClipPhase::Done && FramesConsumed == Config.PreRollFrames;
}

#undef LOCTEXT_NAMESPACE
