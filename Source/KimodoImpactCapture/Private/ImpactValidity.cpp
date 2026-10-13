// Copyright Animotive. All Rights Reserved.

#include "ImpactValidity.h"

#define LOCTEXT_NAMESPACE "ImpactValidity"

bool FImpactValidity::Check(TConstArrayView<FImpactBodySample> Bodies, float GroundHeight, const FImpactValidityThresholds& Thresholds, FText& OutReason)
{
	if (Bodies.IsEmpty())
	{
		OutReason = LOCTEXT("NoBodies", "The frame has no physics bodies to check.");
		return false;
	}

	for (const FImpactBodySample& Body : Bodies)
	{
		const FText Name = FText::FromName(Body.Name);
		auto Fail = [&OutReason, &Name](const FText& Problem)
		{
			OutReason = FText::Format(LOCTEXT("BodyProblem", "Body {0}: {1}"), Name, Problem);
			return false;
		};

		if (Body.Location.ContainsNaN() || !FMath::IsFinite(Body.Location.X) || !FMath::IsFinite(Body.Location.Y) || !FMath::IsFinite(Body.Location.Z))
		{
			return Fail(LOCTEXT("BadLocation", "position is not a finite number; the simulation broke."));
		}
		if (Body.Rotation.ContainsNaN() || !FMath::IsFinite(Body.Rotation.X) || !FMath::IsFinite(Body.Rotation.Y) || !FMath::IsFinite(Body.Rotation.Z) || !FMath::IsFinite(Body.Rotation.W))
		{
			return Fail(LOCTEXT("BadRotation", "rotation is not a finite number; the simulation broke."));
		}
		if (Body.LinearVelocity.ContainsNaN() || Body.AngularVelocityDegPerSecond.ContainsNaN())
		{
			return Fail(LOCTEXT("BadVelocity", "velocity is not a finite number; the simulation broke."));
		}

		const float Speed = float(Body.LinearVelocity.Size());
		if (Speed > Thresholds.MaxBodySpeedCmPerSecond)
		{
			return Fail(FText::Format(LOCTEXT("TooFast", "moving at {0} cm/s, beyond {1}; the simulation exploded."),
				FText::AsNumber(FMath::RoundToInt(Speed)), FText::AsNumber(FMath::RoundToInt(Thresholds.MaxBodySpeedCmPerSecond))));
		}
		const float Spin = float(Body.AngularVelocityDegPerSecond.Size());
		if (Spin > Thresholds.MaxAngularSpeedDegPerSecond)
		{
			return Fail(FText::Format(LOCTEXT("TooSpin", "spinning at {0} deg/s, beyond {1}; the simulation exploded."),
				FText::AsNumber(FMath::RoundToInt(Spin)), FText::AsNumber(FMath::RoundToInt(Thresholds.MaxAngularSpeedDegPerSecond))));
		}
		const float Depth = GroundHeight - float(Body.Location.Z);
		if (Depth > Thresholds.MaxGroundPenetrationCm)
		{
			return Fail(FText::Format(LOCTEXT("Sunk", "{0} cm below the floor; ground contact failed."), FText::AsNumber(FMath::RoundToInt(Depth))));
		}
		if (Body.JointGapCm > Thresholds.MaxJointGapCm)
		{
			return Fail(FText::Format(LOCTEXT("Torn", "pulled {0} cm away from its joint; the skeleton tore."), FText::AsNumber(FMath::RoundToInt(Body.JointGapCm))));
		}
		if (Body.JointLimitExcessDeg > (Body.bHingeJoint ? Thresholds.MaxHingeLimitExcessDeg : Thresholds.MaxJointLimitExcessDeg))
		{
			return Fail(FText::Format(LOCTEXT("PastLimit", "bent {0} degrees past its joint limit; the limb folded the wrong way."),
				FText::AsNumber(FMath::RoundToInt(Body.JointLimitExcessDeg))));
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
