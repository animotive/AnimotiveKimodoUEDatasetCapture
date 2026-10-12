// Copyright Animotive. All Rights Reserved.

#include "ImpactSchedule.h"

#include "ImpactParams.h"
#include "ImpactRandom.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactSchedule)

#define LOCTEXT_NAMESPACE "ImpactSchedule"

float FImpactSchedule::TotalScriptedImpulse() const
{
	float Total = 0.0f;
	for (const FImpactHit& Hit : Hits)
	{
		Total += Hit.ImpulseKgCmPerSecond;
	}
	return Total;
}

namespace
{
	bool ValidateDelivery(const FImpactDeliveryDistributions& Distributions, FText& OutReason)
	{
		if (Distributions.BurstHits.Min < 2 || Distributions.BurstHits.Min > Distributions.BurstHits.Max)
		{
			OutReason = LOCTEXT("BadBurstHits", "A burst needs at least two hits, with min no greater than max.");
			return false;
		}
		if (Distributions.BurstSpacingFrames.Min < 1 || Distributions.BurstSpacingFrames.Min > Distributions.BurstSpacingFrames.Max)
		{
			OutReason = LOCTEXT("BadBurstSpacing", "Burst hits must be at least one frame apart, with min no greater than max.");
			return false;
		}
		if (Distributions.SustainedFrames.Min < 1 || Distributions.SustainedFrames.Min > Distributions.SustainedFrames.Max)
		{
			OutReason = LOCTEXT("BadSustained", "A sustained push must last at least one frame, with min no greater than max.");
			return false;
		}
		const FFloatInterval& Launch = Distributions.ProjectileLaunchDistanceCm;
		if (!(Launch.Min > 0.0f) || Launch.Min > Launch.Max)
		{
			OutReason = LOCTEXT("BadLaunch", "Projectile launch distance must be above zero, with min no greater than max.");
			return false;
		}
		if (!Distributions.ModeOverride.IsEmpty())
		{
			float Total = 0.0f;
			for (const FImpactDeliveryWeight& Weight : Distributions.ModeOverride)
			{
				if (Weight.Weight < 0.0f)
				{
					OutReason = LOCTEXT("NegativeModeWeight", "A delivery mode has a negative weight.");
					return false;
				}
				Total += Weight.Weight;
			}
			if (Total <= 0.0f)
			{
				OutReason = LOCTEXT("NoModeWeight", "Every overriding delivery mode has zero weight. Give one a positive weight, or remove the override.");
				return false;
			}
		}
		return true;
	}

	EImpactDeliveryMode ChooseMode(const FImpactParams& Params, const FImpactDeliveryDistributions& Distributions, FImpactRandom& Random)
	{
		if (Distributions.ModeOverride.IsEmpty())
		{
			return Params.Impactor.Archetype->DefaultDelivery;
		}
		double Total = 0.0;
		for (const FImpactDeliveryWeight& Weight : Distributions.ModeOverride)
		{
			Total += Weight.Weight;
		}
		const double Pick = Random.Range(0.0, Total);
		double Cumulative = 0.0;
		EImpactDeliveryMode Last = Distributions.ModeOverride[0].Mode;
		for (const FImpactDeliveryWeight& Weight : Distributions.ModeOverride)
		{
			if (Weight.Weight <= 0.0f)
			{
				continue;
			}
			Last = Weight.Mode;
			Cumulative += Weight.Weight;
			if (Pick < Cumulative)
			{
				return Weight.Mode;
			}
		}
		return Last;
	}

	/** Uniform direction within a cone of HalfAngleDegrees around Axis. */
	FVector JitterWithinCone(const FVector& Axis, float HalfAngleDegrees, FImpactRandom& Random)
	{
		const double CosMax = FMath::Cos(FMath::DegreesToRadians(double(HalfAngleDegrees)));
		const double CosTheta = Random.Range(CosMax, 1.0);
		const double SinTheta = FMath::Sqrt(FMath::Max(0.0, 1.0 - CosTheta * CosTheta));
		const double Phi = Random.Range(0.0, 2.0 * UE_DOUBLE_PI);
		FVector Tangent, Bitangent;
		Axis.FindBestAxisVectors(Tangent, Bitangent);
		return (Axis * CosTheta + Tangent * (SinTheta * FMath::Cos(Phi)) + Bitangent * (SinTheta * FMath::Sin(Phi))).GetSafeNormal();
	}
}

bool FImpactScheduleSampler::Sample(
	const FImpactParams& Params, const FImpactDeliveryDistributions& Distributions,
	int32 BatchSeed, int32 ClipIndex, FImpactSchedule& OutSchedule, FText& OutReason)
{
	if (!Params.Impactor.Archetype)
	{
		OutReason = LOCTEXT("NoImpactor", "The impact has no impactor, so it cannot be delivered.");
		return false;
	}
	if (!ValidateDelivery(Distributions, OutReason))
	{
		return false;
	}

	FImpactRandom Random(BatchSeed, ClipIndex, EImpactRandomStream::Delivery);
	FImpactSchedule Schedule;
	Schedule.Mode = ChooseMode(Params, Distributions, Random);
	const FVector Direction = Params.DirectionLocal.GetSafeNormal();
	const float Impulse = Params.ReactionImpulseKgCmPerSecond;

	switch (Schedule.Mode)
	{
	case EImpactDeliveryMode::Instant:
		Schedule.Hits.Add({ 0, 0, Impulse, Direction });
		break;

	case EImpactDeliveryMode::Burst:
	{
		const int32 Count = Random.RangeInt(Distributions.BurstHits.Min, Distributions.BurstHits.Max);
		int32 Frame = 0;
		float Assigned = 0.0f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Index > 0)
			{
				Frame += Random.RangeInt(Distributions.BurstSpacingFrames.Min, Distributions.BurstSpacingFrames.Max);
			}
			// Equal shares; the last takes the remainder so the burst conserves the impulse exactly.
			const float Share = Index + 1 < Count ? Impulse / Count : Impulse - Assigned;
			Assigned += Share;
			Schedule.Hits.Add({ Frame, 0, Share, JitterWithinCone(Direction, Distributions.BurstDirectionJitterDegrees, Random) });
		}
		break;
	}

	case EImpactDeliveryMode::Sustained:
		Schedule.Hits.Add({ 0, Random.RangeInt(Distributions.SustainedFrames.Min, Distributions.SustainedFrames.Max), Impulse, Direction });
		break;

	case EImpactDeliveryMode::Projectile:
	{
		FImpactProjectileSpec Spec;
		Spec.Shape = Params.Impactor.Archetype->Shape;
		Spec.SizeCm = Params.Impactor.Archetype->SizeCm;
		Spec.MassKg = Params.Impactor.MassKg;
		Spec.SpeedMetresPerSecond = Params.Impactor.SpeedMetresPerSecond;
		Spec.LaunchDistanceCm = float(Random.Range(Distributions.ProjectileLaunchDistanceCm.Min, Distributions.ProjectileLaunchDistanceCm.Max));
		// The collision transfers the physical momentum; anything the reaction gain adds on top is
		// applied at the contact.
		Spec.ExtraReactionImpulseKgCmPerSecond = FMath::Max(0.0f, Impulse - Params.TransferredImpulseKgCmPerSecond);
		Schedule.Projectile = Spec;
		break;
	}
	}

	OutSchedule = MoveTemp(Schedule);
	return true;
}

#undef LOCTEXT_NAMESPACE
