// Copyright Animotive. All Rights Reserved.

#include "ImpactRootTracker.h"

FImpactRootTracker::FImpactRootTracker(const FTransform& ReferencePelvis, const FVector& InMeshForward, float InGroundHeight, const FImpactRootTrackerConfig& InConfig)
	: Config(InConfig)
	, MeshForward(InMeshForward.GetSafeNormal2D())
	, GroundHeight(InGroundHeight)
{
	// Which pelvis axis means "forward" differs per rig; read it off the reference pose.
	PelvisForwardAxis = ReferencePelvis.GetRotation().UnrotateVector(MeshForward);
}

float FImpactRootTracker::PelvisYaw(const FTransform& Pelvis, bool& bOutValid) const
{
	const FVector Forward = Pelvis.GetRotation().RotateVector(PelvisForwardAxis);
	const FVector Flat(Forward.X, Forward.Y, 0.0);
	bOutValid = Flat.Size() >= Config.MinFlatForward;
	if (!bOutValid)
	{
		return SmoothedYaw;
	}
	const float Heading = FMath::RadiansToDegrees(FMath::Atan2(float(Flat.Y), float(Flat.X)));
	const float MeshHeading = FMath::RadiansToDegrees(FMath::Atan2(float(MeshForward.Y), float(MeshForward.X)));
	return FMath::UnwindDegrees(Heading - MeshHeading);
}

FTransform FImpactRootTracker::Project(const FTransform& Pelvis) const
{
	bool bValid = false;
	const float Yaw = PelvisYaw(Pelvis, bValid);
	const FVector Location(Pelvis.GetLocation().X, Pelvis.GetLocation().Y, GroundHeight);
	return FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)), Location);
}

FTransform FImpactRootTracker::Step(const FTransform& SourceRoot, const FTransform& Pelvis, bool bReleased)
{
	if (!bReleased)
	{
		ReleasedFrames = 0;
		return SourceRoot;
	}

	bool bValid = false;
	const float Yaw = PelvisYaw(Pelvis, bValid);
	if (bValid)
	{
		if (!bHaveYaw)
		{
			SmoothedYaw = Yaw;
			bHaveYaw = true;
		}
		else
		{
			// Short way round across +-180.
			SmoothedYaw = FMath::UnwindDegrees(SmoothedYaw + FMath::UnwindDegrees(Yaw - SmoothedYaw) * Config.YawSmoothing);
		}
	}

	const FTransform Projected(
		FQuat(FVector::UpVector, FMath::DegreesToRadians(SmoothedYaw)),
		FVector(Pelvis.GetLocation().X, Pelvis.GetLocation().Y, GroundHeight));

	++ReleasedFrames;
	const float Alpha = FMath::Clamp(float(ReleasedFrames) / float(FMath::Max(1, Config.BlendFrames)), 0.0f, 1.0f);
	return FTransform(
		FQuat::Slerp(SourceRoot.GetRotation(), Projected.GetRotation(), Alpha),
		FMath::Lerp(SourceRoot.GetLocation(), Projected.GetLocation(), double(Alpha)));
}
