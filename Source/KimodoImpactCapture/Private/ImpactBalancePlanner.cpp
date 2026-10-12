// Copyright Animotive. All Rights Reserved.

#include "ImpactBalancePlanner.h"

#include "ImpactSwingArc.h"

bool FImpactFallRules::IsOverwhelming(float ReactionImpulseKgCmPerSecond, float BodyMassKg, float StruckBodyMassKg) const
{
	return false; // Red
}

void FImpactBalancePlanner::ReleaseImmediately()
{
	// Red
}

FImpactBalancePlanner::FImpactBalancePlanner(const FImpactBalanceConfig& InConfig)
	: Config(InConfig)
{
}

FVector FImpactBalancePlanner::CapturePoint(const FVector& Com, const FVector& ExcessVelocity, float GravityCmPerSecondSquared)
{
	// Linear inverted pendulum: where the feet must be for the body to come to rest.
	const float Height = FMath::Max(float(Com.Z), 10.0f);
	const FVector Horizontal(ExcessVelocity.X, ExcessVelocity.Y, 0.0);
	return Com + Horizontal * FMath::Sqrt(Height / FMath::Max(GravityCmPerSecondSquared, 1.0f));
}

FImpactBalanceOutput FImpactBalancePlanner::Step(const FImpactBalanceInput& Input)
{
	FImpactBalanceOutput Out;
	const int32 Now = Frame++;

	if (!bFell)
	{
		FVector Excess = Input.ComVelocity - Input.SourceVelocity;
		Excess.Z = 0.0;
		// An arm or the struck chest spikes for a frame or two; balance follows the whole body.
		SmoothedExcess = FMath::Lerp(SmoothedExcess, Excess, double(Config.ExcessSmoothing));

		if (Input.Com.Z < Config.FallComHeightCm)
		{
			bFell = true;
			Out.bJustFell = true;
		}

		if (Input.bMovingSource)
		{
			// Along the direction of travel the source clock owns deceleration; only sideways
			// momentum moves the frame.
			const FVector Travel = Input.SourceVelocity.GetSafeNormal2D();
			Offset += (SmoothedExcess - Travel * FVector::DotProduct(SmoothedExcess, Travel)) * Input.DeltaSeconds;
		}
		else
		{
			Offset += SmoothedExcess * (Input.DeltaSeconds * Config.StandingOffsetFollow);
			if (Steps > 0)
			{
				// Once the character has stepped, its new home is over its new footing.
				FVector FootingShift = (Feet[0].Planted + Feet[1].Planted) * 0.5 - FootingAtHit;
				FootingShift.Z = 0.0;
				Offset = FMath::Lerp(Offset, FootingShift, double(Config.FootingPull));
			}
		}
		Offset.Z = 0.0;
		LastCapturePoint = CapturePoint(Input.Com, SmoothedExcess, Config.GravityCmPerSecondSquared);
	}

	if (!bFell && !Input.bMovingSource)
	{
		for (int32 Side = 0; Side < 2; ++Side)
		{
			if (!Feet[Side].bPinned)
			{
				// Pinned where it stands at the hit: foot IK from here on.
				Feet[Side].bPinned = true;
				Feet[Side].Planted = Input.FootPosition[Side];
				Feet[Side].Current = Feet[Side].Planted;
			}
		}
		if (Now == 0)
		{
			FootingAtHit = (Feet[0].Planted + Feet[1].Planted) * 0.5;
		}

		const FVector Mid = (Feet[0].Planted + Feet[1].Planted) * 0.5;
		const float Error = float(FVector::Dist2D(LastCapturePoint, Mid));
		const bool bAnySwing = Feet[0].bSwing || Feet[1].bSwing;
		const bool bStanceLongEnough = LastLandFrame == INDEX_NONE || Now - LastLandFrame >= Config.MinStanceFrames;

		if (!bAnySwing && SettleStart == INDEX_NONE && Error > Config.StepTriggerCm && bStanceLongEnough)
		{
			CalmFrames = 0;
			// Feet alternate; the first step is taken by the foot farther from the capture point.
			const int32 SwingSide = LastSwingSide == INDEX_NONE
				? (FVector::Dist2D(LastCapturePoint, Feet[0].Planted) > FVector::Dist2D(LastCapturePoint, Feet[1].Planted) ? 0 : 1)
				: 1 - LastSwingSide;
			FFoot& Swing = Feet[SwingSide];
			const FFoot& Stance = Feet[1 - SwingSide];

			FVector Direction = LastCapturePoint - Mid;
			Direction.Z = 0.0;
			Direction = Direction.GetSafeNormal();
			const FVector Lateral(-Direction.Y, Direction.X, 0.0);
			const float SideSign = FMath::Sign(float(FVector::DotProduct(Swing.Planted - Stance.Planted, Lateral)));
			FVector Landing = LastCapturePoint + Lateral * (SideSign * Config.StanceHalfWidthCm);
			Landing.Z = Swing.Planted.Z;

			// Step as far as a leg reaches. A step that falls short buys time for the next.
			FVector Reach = Landing - Swing.Planted;
			Reach.Z = 0.0;
			if (Reach.Size() > Config.MaxStepCm)
			{
				Landing = Swing.Planted + Reach.GetSafeNormal() * Config.MaxStepCm;
				++UncaughtSteps;
			}
			else
			{
				UncaughtSteps = 0;
			}

			if (UncaughtSteps > Config.MaxUncaughtSteps || Steps >= Config.MaxSteps)
			{
				bFell = true;
				Out.bJustFell = true;
			}
			else
			{
				Swing.bSwing = true;
				Swing.SwingFrom = Swing.Current;
				Swing.SwingTo = Landing;
				Swing.SwingStart = Now;
				LastSwingSide = SwingSide;
			}
		}
		else if (!bAnySwing && SettleStart == INDEX_NONE)
		{
			const bool bCalm = Error < Config.StepTriggerCm * 0.5f && SmoothedExcess.Size2D() < Config.CalmExcessSpeedCmPerSecond;
			CalmFrames = bCalm ? CalmFrames + 1 : 0;
			if (CalmFrames >= Config.SettleHoldFrames)
			{
				SettleStart = Now;
			}
		}

		if (!bFell)
		{
			for (FFoot& Foot : Feet)
			{
				if (!Foot.bSwing)
				{
					continue;
				}
				const float Phase = FMath::Clamp(float(Now - Foot.SwingStart + 1) / float(FMath::Max(1, Config.SwingFrames)), 0.0f, 1.0f);
				Foot.Current = FImpactSwingArc::Evaluate(Foot.SwingFrom, Foot.SwingTo, Phase, Config.SwingLiftCm);
				if (Phase >= 1.0f)
				{
					Foot.bSwing = false;
					Foot.Planted = Foot.SwingTo;
					Foot.Current = Foot.Planted;
					LastLandFrame = Now;
					++Steps;
				}
			}
		}
	}

	Out.Offset = Offset;
	Out.CapturePoint = LastCapturePoint;
	Out.Steps = Steps;
	Out.bFell = bFell;
	Out.bSettling = SettleStart != INDEX_NONE;
	const float SettleAlpha = SettleStart == INDEX_NONE
		? 0.0f
		: FMath::SmoothStep(0.0f, 1.0f, float(Now - SettleStart) / float(FMath::Max(1, Config.SettleBlendFrames)));
	for (int32 Side = 0; Side < 2; ++Side)
	{
		Out.Feet[Side].bPinned = Feet[Side].bPinned && !bFell;
		Out.Feet[Side].bSwing = Feet[Side].bSwing;
		Out.Feet[Side].Position = Feet[Side].Current;
		Out.Feet[Side].SettleAlpha = SettleAlpha;
	}
	return Out;
}
