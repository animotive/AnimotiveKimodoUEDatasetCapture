// Copyright Animotive. All Rights Reserved.

#include "ImpactOutcome.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactOutcome)

FImpactOutcomeResult FImpactOutcomeClassifier::Classify(TConstArrayView<FImpactFrameState> Frames, const FImpactOutcomeThresholds& Thresholds)
{
	FImpactOutcomeResult Result;
	if (Frames.IsEmpty())
	{
		return Result;
	}

	// One pass: the first frame each event happens, and the extremes.
	int32 FirstRelease = INDEX_NONE;
	int32 FirstDown = INDEX_NONE;
	int32 FirstDisturbed = INDEX_NONE;
	int32 FirstCollapse = INDEX_NONE;
	for (int32 Index = 0; Index < Frames.Num(); ++Index)
	{
		const FImpactFrameState& Frame = Frames[Index];
		if (!(Frame.StandingPelvisHeightCm > 0.0f))
		{
			return FImpactOutcomeResult();
		}
		// Each frame is judged against its own source height: a crouch stands lower than a run.
		const float Fraction = Frame.PelvisHeightCm / Frame.StandingPelvisHeightCm;
		Result.MinPelvisFraction = FMath::Min(Result.MinPelvisFraction, Fraction);
		Result.Steps = FMath::Max(Result.Steps, Frame.Steps);
		Result.bTrip |= Frame.bTrip;

		if (FirstCollapse == INDEX_NONE && Frame.bCollapsed)
		{
			FirstCollapse = Index;
		}
		if (FirstRelease == INDEX_NONE && Frame.bControlsReleased)
		{
			FirstRelease = Index;
		}
		if (FirstDown == INDEX_NONE && Fraction < Thresholds.KnockdownPelvisFraction)
		{
			FirstDown = Index;
		}
		const bool bDisturbed = Frame.Steps > 0 || Frame.bOutsideSupport || Fraction < Thresholds.FlinchPelvisFraction;
		if (FirstDisturbed == INDEX_NONE && bDisturbed)
		{
			FirstDisturbed = Index;
		}
	}

	Result.bValid = true;
	if (FirstCollapse != INDEX_NONE)
	{
		// Incapacitated: neither thrown nor beaten in a fight for balance.
		Result.Outcome = EImpactOutcome::Collapse;
		Result.OutcomeFrame = FirstCollapse;
	}
	else if (FirstRelease != INDEX_NONE && FirstRelease <= Thresholds.ImmediateReleaseFrames)
	{
		// Overwhelmed at once: no balance was attempted.
		Result.Outcome = EImpactOutcome::Ragdoll;
		Result.OutcomeFrame = FirstRelease;
	}
	else if (FirstRelease != INDEX_NONE || FirstDown != INDEX_NONE)
	{
		// Went down, or gave up balance after trying: whichever happened first decides it.
		Result.Outcome = EImpactOutcome::Knockdown;
		Result.OutcomeFrame = FirstRelease == INDEX_NONE ? FirstDown
			: FirstDown == INDEX_NONE ? FirstRelease
			: FMath::Min(FirstRelease, FirstDown);
	}
	else if (FirstDisturbed != INDEX_NONE)
	{
		Result.Outcome = EImpactOutcome::Stagger;
		Result.OutcomeFrame = FirstDisturbed;
	}
	else
	{
		Result.Outcome = EImpactOutcome::Flinch;
		Result.OutcomeFrame = 0;
	}
	return Result;
}
