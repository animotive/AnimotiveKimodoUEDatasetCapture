// Copyright Animotive. All Rights Reserved.

#include "ImpactLegIK.h"

#include "ReferenceSkeleton.h"

TOptional<FVector> FImpactLegIK::ChooseEffector(bool bReleased, bool bFootPlaced, const FVector& PlacedTarget, bool bMovingSource,
	const FVector& SourceFoot, const FVector& CurrentFoot)
{
	if (bReleased)
	{
		return TOptional<FVector>();
	}
	if (bFootPlaced)
	{
		return PlacedTarget;
	}
	if (bMovingSource && SourceFoot.Z < SourceContactHeightCm)
	{
		return FVector(CurrentFoot.X, CurrentFoot.Y, SourceFoot.Z);
	}
	return TOptional<FVector>();
}

void FImpactLegIK::Solve(const FReferenceSkeleton& Ref, const FImpactLegChain& Leg, const FVector& Effector, const FVector& Forward,
	TConstArrayView<FTransform> Source, TArrayView<FTransform> Pose)
{
	if (!Leg.IsValid() || !Source.IsValidIndex(Leg.Foot) || !Pose.IsValidIndex(Leg.Foot))
	{
		return;
	}
	const double Upper = FVector::Dist(Source[Leg.Thigh].GetLocation(), Source[Leg.Calf].GetLocation());
	const double Lower = FVector::Dist(Source[Leg.Calf].GetLocation(), Source[Leg.Foot].GetLocation());
	const FVector Hip = Pose[Leg.Thigh].GetLocation();
	const FVector OldKnee = Pose[Leg.Calf].GetLocation();
	const FVector OldAnkle = Pose[Leg.Foot].GetLocation();

	const FVector ToTarget = Effector - Hip;
	const double Distance = ToTarget.Size();
	const FVector Direction = Distance > UE_KINDA_SMALL_NUMBER ? ToTarget / Distance : (OldAnkle - Hip).GetSafeNormal();

	// The bend plane: the knee goes toward Forward, taken square to the leg; failing that, where it was.
	FVector Bend = Forward - Direction * FVector::DotProduct(Forward, Direction);
	if (!Bend.Normalize())
	{
		const FVector Knee = OldKnee - Hip;
		Bend = Knee - Direction * FVector::DotProduct(Knee, Direction);
		if (!Bend.Normalize())
		{
			Bend = FVector::CrossProduct(Direction, FVector::RightVector).GetSafeNormal();
		}
	}

	FVector Knee, Ankle;
	if (Distance >= Upper + Lower)
	{
		// Out of reach: straight toward it, never stretched.
		Knee = Hip + Direction * Upper;
		Ankle = Hip + Direction * (Upper + Lower);
	}
	else
	{
		const double Reach = FMath::Max(Distance, FMath::Abs(Upper - Lower) + UE_KINDA_SMALL_NUMBER);
		const double CosHip = FMath::Clamp((Upper * Upper + Reach * Reach - Lower * Lower) / (2.0 * Upper * Reach), -1.0, 1.0);
		const double SinHip = FMath::Sqrt(FMath::Max(0.0, 1.0 - CosHip * CosHip));
		Knee = Hip + Direction * (Upper * CosHip) + Bend * (Upper * SinHip);
		Ankle = Hip + Direction * Reach;
	}

	// Each bone turns by the least rotation that carries its old direction onto its new one.
	const FQuat ThighTurn = FQuat::FindBetweenNormals((OldKnee - Hip).GetSafeNormal(), (Knee - Hip).GetSafeNormal());
	const FQuat CalfTurn = FQuat::FindBetweenNormals((OldAnkle - OldKnee).GetSafeNormal(), (Ankle - Knee).GetSafeNormal());
	Pose[Leg.Thigh].SetRotation((ThighTurn * Pose[Leg.Thigh].GetRotation()).GetNormalized());
	Pose[Leg.Calf] = FTransform((CalfTurn * Pose[Leg.Calf].GetRotation()).GetNormalized(), Knee, Pose[Leg.Calf].GetScale3D());
	Pose[Leg.Foot].SetLocation(Ankle);

	// Every other bone below the thigh (twist bones, toes) follows its parent with its source local.
	for (int32 Bone = Leg.Thigh + 1; Bone < Ref.GetNum() && Bone < Pose.Num(); ++Bone)
	{
		if (Bone == Leg.Calf || Bone == Leg.Foot || !Ref.BoneIsChildOf(Bone, Leg.Thigh))
		{
			continue;
		}
		const int32 Parent = Ref.GetParentIndex(Bone);
		Pose[Bone] = Source[Bone].GetRelativeTransform(Source[Parent]) * Pose[Parent];
	}
}
