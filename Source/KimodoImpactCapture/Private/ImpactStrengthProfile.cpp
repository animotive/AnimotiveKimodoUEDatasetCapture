// Copyright Animotive. All Rights Reserved.

#include "ImpactStrengthProfile.h"

#include "ImpactRandom.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactStrengthProfile)

FImpactStrengthProfile FImpactStrengthProfile::Sample(const FImpactStrengthDistribution& Distribution, FImpactRandom& Random)
{
	FImpactStrengthProfile Profile;
	Profile.WorldStrength = float(Random.Range(Distribution.WorldStrength.Min, Distribution.WorldStrength.Max));
	Profile.ParentStrength = Profile.WorldStrength * float(Random.Range(Distribution.ParentToWorldRatio.Min, Distribution.ParentToWorldRatio.Max));
	// Drawn after the strengths, so clips sampled before the ragdoll tone existed keep their values.
	Profile.ReleasedToneStart = float(Random.Range(Distribution.ReleasedTone.Min, Distribution.ReleasedTone.Max));
	Profile.ReleasedToneEnd = Profile.ReleasedToneStart * float(Random.Range(Distribution.ReleasedToneEndRatio.Min, Distribution.ReleasedToneEndRatio.Max));
	return Profile;
}

FImpactBodyStrength FImpactStrengthProfile::Evaluate(const FImpactBodyStrengthQuery& Body, int32 FramesSinceHit) const
{
	FImpactBodyStrength Strength;
	if (Body.bReleased)
	{
		// No world hold; the joints keep a tone that fades smoothly toward its end value.
		const float Fade = FMath::SmoothStep(0.0f, 1.0f, float(FMath::Max(0, Body.FramesSinceRelease)) / float(FMath::Max(1, ReleasedToneFadeFrames)));
		Strength.World = 0.0f;
		Strength.Joint = FMath::Lerp(ReleasedToneStart, ReleasedToneEnd, Fade);
		Strength.JointDamping = ReleasedJointDamping;
		return Strength;
	}
	if (FramesSinceHit < 0)
	{
		return Strength;
	}

	const int32 Distance = FMath::Max(0, Body.ContactDistance);

	// Local release, strongest at the contact and fading along the body tree; returns over RecoverFrames.
	const float Recover = FMath::SmoothStep(0.0f, 1.0f, float(FramesSinceHit) / float(FMath::Max(1, RecoverFrames)));
	const float WorldRelease = Body.bLeg ? 0.0f : FMath::Clamp(1.0f - 0.25f * Distance, 0.4f, 1.0f);
	const float JointRelease = Distance == 0 ? 0.8f : Distance == 1 ? 0.6f : Distance == 2 ? 0.3f : 0.0f;
	Strength.World = FMath::Lerp(1.0f - WorldRelease, 1.0f, Recover);
	Strength.Joint = FMath::Lerp(1.0f - JointRelease, 1.0f, Recover);

	// Impact phase: fully on for ImpactFrames, then smoothly blended out.
	const float Impact = FramesSinceHit < ImpactFrames
		? 1.0f
		: 1.0f - FMath::SmoothStep(0.0f, 1.0f, float(FramesSinceHit - ImpactFrames) / float(FMath::Max(1, ImpactBlendFrames)));

	if (!Body.bLeg)
	{
		const float ImpactWorld = (Body.bMovingSource && Body.bPelvis) ? ImpactMovingPelvisWorld : ImpactUpperWorld;
		Strength.World = FMath::Lerp(Strength.World, ImpactWorld, Impact);
		Strength.Joint = FMath::Lerp(Strength.Joint, ImpactUpperJoint, Impact);
	}
	else if (Body.bMovingSource)
	{
		// A running leg travels with the hips rather than being held to the run's path.
		Strength.World = FMath::Lerp(Strength.World, ImpactMovingLegWorld, Impact);
	}
	return Strength;
}
