// Copyright Animotive. All Rights Reserved.

#include "ImpactJointLimits.h"

FImpactJointAngles FImpactJointAngles::FromRelative(const FQuat& ChildInParent)
{
	// q and -q are the same rotation; keep W non-negative so the twist angle is the short way round.
	FQuat Q = ChildInParent.GetNormalized();
	if (Q.W < 0.0)
	{
		Q = FQuat(-Q.X, -Q.Y, -Q.Z, -Q.W);
	}
	FQuat Swing, Twist;
	Q.ToSwingTwist(FVector::ForwardVector, Swing, Twist);

	FImpactJointAngles Angles;
	Angles.TwistDeg = float(FMath::RadiansToDegrees(Twist.GetTwistAngle(FVector::ForwardVector)));
	// As Chaos limits it: the swing angle about its axis, split along Z (swing1) and Y (swing2).
	FVector SwingAxis;
	double SwingAngle = 0.0;
	Swing.ToAxisAndAngle(SwingAxis, SwingAngle);
	const double Degrees = FMath::RadiansToDegrees(SwingAngle);
	Angles.Swing1Deg = float(Degrees * SwingAxis.Z);
	Angles.Swing2Deg = float(Degrees * SwingAxis.Y);
	return Angles;
}

bool FImpactJointLimit::IsHinge() const
{
	const bool bSwing1Only = Swing1Deg > 0.0f && Swing1Deg < 180.0f && Swing2Deg == 0.0f;
	const bool bSwing2Only = Swing2Deg > 0.0f && Swing2Deg < 180.0f && Swing1Deg == 0.0f;
	return (bSwing1Only || bSwing2Only) && TwistDeg == 0.0f;
}

bool FImpactJointRange::Fit(float AssetLimitDeg, float ReferenceDeg, float& OutCentreDeg, float& OutHalfDeg) const
{
	const float Low = FMath::Max(ReferenceDeg + MinDeg, -AssetLimitDeg);
	const float High = FMath::Min(ReferenceDeg + MaxDeg, AssetLimitDeg);
	if (Low > High)
	{
		return false;
	}
	OutCentreDeg = 0.5f * (Low + High);
	OutHalfDeg = 0.5f * (High - Low);
	return true;
}

FQuat FImpactJointRange::TurnSwing1(const FQuat& ParentFrame, float CentreDeg)
{
	// Measured swing1 is the child's turn about Z relative to this frame; turning the frame by the
	// centre measures from there.
	return ParentFrame * FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(CentreDeg));
}

namespace
{
	bool IsCone(float Swing1Deg, float Swing2Deg)
	{
		return Swing1Deg > 0.0f && Swing1Deg < 180.0f && Swing2Deg > 0.0f && Swing2Deg < 180.0f;
	}

	/** How far Angle is past a limit of LimitDeg, opened on Allowed's side as far as Allowed. */
	float AxisExcess(float AngleDeg, float LimitDeg, float AllowedDeg)
	{
		const bool bAllowedSide = AllowedDeg != 0.0f && (AngleDeg > 0.0f) == (AllowedDeg > 0.0f);
		return FMath::Abs(AngleDeg) - (bAllowedSide ? FMath::Max(LimitDeg, FMath::Abs(AllowedDeg)) : LimitDeg);
	}

	/** How far the swing runs past an elliptical cone along its own direction. */
	float ConeExcess(const FImpactJointAngles& Angles, float Swing1Deg, float Swing2Deg)
	{
		const float Angle = FMath::Sqrt(FMath::Square(Angles.Swing1Deg) + FMath::Square(Angles.Swing2Deg));
		if (Angle <= UE_KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}
		const float Along1 = Angles.Swing1Deg / Angle / Swing1Deg;
		const float Along2 = Angles.Swing2Deg / Angle / Swing2Deg;
		return Angle - 1.0f / FMath::Sqrt(FMath::Square(Along1) + FMath::Square(Along2));
	}
}

float FImpactJointLimit::Excess(const FImpactJointAngles& Angles) const
{
	return ExcessBeyond(Angles, FImpactJointAngles());
}

float FImpactJointLimit::ExcessBeyond(const FImpactJointAngles& Angles, const FImpactJointAngles& Allowed) const
{
	const float Twist = AxisExcess(Angles.TwistDeg, TwistDeg, Allowed.TwistDeg);
	float Swing = 0.0f;
	if (IsCone(Swing1Deg, Swing2Deg))
	{
		// A cone has no sides to open one at a time: it is scaled until Allowed is on it.
		const float Reach = FMath::Sqrt(FMath::Square(Allowed.Swing1Deg / Swing1Deg) + FMath::Square(Allowed.Swing2Deg / Swing2Deg));
		const float Scale = FMath::Max(1.0f, Reach);
		Swing = ConeExcess(Angles, Swing1Deg * Scale, Swing2Deg * Scale);
	}
	else
	{
		// A hinge, a locked or a free swing: each axis on its own.
		Swing = FMath::Max(AxisExcess(Angles.Swing1Deg, Swing1Deg, Allowed.Swing1Deg), AxisExcess(Angles.Swing2Deg, Swing2Deg, Allowed.Swing2Deg));
	}
	return FMath::Max(0.0f, FMath::Max(Twist, Swing));
}

float FImpactJointLimit::ExcessBeyondAny(const FImpactJointAngles& Angles, TConstArrayView<FImpactJointAngles> Allowed) const
{
	if (Allowed.IsEmpty())
	{
		return Excess(Angles);
	}
	float Least = TNumericLimits<float>::Max();
	for (const FImpactJointAngles& Target : Allowed)
	{
		Least = FMath::Min(Least, ExcessBeyond(Angles, Target));
	}
	return Least;
}
