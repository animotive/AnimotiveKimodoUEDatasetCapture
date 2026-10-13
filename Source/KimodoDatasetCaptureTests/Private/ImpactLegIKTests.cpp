// Copyright Animotive. All Rights Reserved.

#include "ImpactLegIK.h"
#include "Misc/AutomationTest.h"
#include "ReferenceSkeleton.h"

namespace ImpactLegIKTests
{
	enum EBone { Root, Pelvis, ThighL, CalfL, FootL, BallL, BoneCount };

	FReferenceSkeleton MakeSkeleton()
	{
		FReferenceSkeleton Ref;
		{
			FReferenceSkeletonModifier Modifier(Ref, nullptr);
			auto Add = [&Modifier](const TCHAR* Name, int32 Parent, const FVector& Offset, const FRotator& Rotation = FRotator::ZeroRotator)
			{
				Modifier.Add(FMeshBoneInfo(Name, Name, Parent), FTransform(Rotation, Offset));
			};
			Add(TEXT("root"), INDEX_NONE, FVector::ZeroVector);
			Add(TEXT("pelvis"), Root, FVector(0, 0, 95));
			Add(TEXT("thigh_l"), Pelvis, FVector(0, -10, 0));
			// A slight knee bend in the reference pose, as on the mannequin.
			Add(TEXT("calf_l"), ThighL, FVector(0, 2, -45));
			Add(TEXT("foot_l"), CalfL, FVector(0, -2, -45), FRotator(0, 0, 10));
			Add(TEXT("ball_l"), FootL, FVector(0, 12, -5));
		}
		return Ref;
	}

	TArray<FTransform> ComponentPose(const FReferenceSkeleton& Ref)
	{
		TArray<FTransform> Pose;
		Pose.SetNum(Ref.GetNum());
		for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
		{
			const int32 Parent = Ref.GetParentIndex(Bone);
			Pose[Bone] = Parent == INDEX_NONE ? Ref.GetRefBonePose()[Bone] : Ref.GetRefBonePose()[Bone] * Pose[Parent];
		}
		return Pose;
	}

	const FImpactLegChain Leg = { ThighL, CalfL, FootL };
	/** The mannequin faces +Y. */
	const FVector MeshForward(0, 1, 0);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactLegIKEffectorTest,
	"KimodoDatasetCapture.Impact.LegIK.Effector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactLegIKEffectorTest::RunTest(const FString& Parameters)
{
	const FVector Target(5, 20, 3), Current(8, 22, 6);
	const FVector Grounded(1, 2, FImpactLegIK::SourceContactHeightCm - 4.0f);
	const FVector Lifted(1, 2, FImpactLegIK::SourceContactHeightCm + 10.0f);

	const TOptional<FVector> Placed = FImpactLegIK::ChooseEffector(false, true, Target, false, Lifted, Current);
	TestTrue(TEXT("A placed foot goes exactly to its target"), Placed.IsSet() && Placed.GetValue().Equals(Target));
	const TOptional<FVector> PlacedMoving = FImpactLegIK::ChooseEffector(false, true, Target, true, Grounded, Current);
	TestTrue(TEXT("A placed foot wins on a moving source too"), PlacedMoving.IsSet() && PlacedMoving.GetValue().Equals(Target));

	const TOptional<FVector> Contact = FImpactLegIK::ChooseEffector(false, false, FVector::ZeroVector, true, Grounded, Current);
	TestTrue(TEXT("A moving source's grounded foot is solved"), Contact.IsSet());
	if (Contact.IsSet())
	{
		TestEqual(TEXT("It keeps its position across the ground (X)"), Contact.GetValue().X, Current.X);
		TestEqual(TEXT("It keeps its position across the ground (Y)"), Contact.GetValue().Y, Current.Y);
		TestEqual(TEXT("It takes the source's height"), Contact.GetValue().Z, Grounded.Z);
	}
	TestFalse(TEXT("A moving source's lifted foot is left alone"),
		FImpactLegIK::ChooseEffector(false, false, FVector::ZeroVector, true, Lifted, Current).IsSet());
	TestFalse(TEXT("A standing source's unplaced foot is left alone"),
		FImpactLegIK::ChooseEffector(false, false, FVector::ZeroVector, false, Grounded, Current).IsSet());
	TestFalse(TEXT("A ragdoll's placed foot is left to physics"),
		FImpactLegIK::ChooseEffector(true, true, Target, false, Grounded, Current).IsSet());
	TestFalse(TEXT("A ragdoll's grounded foot is left to physics"),
		FImpactLegIK::ChooseEffector(true, false, FVector::ZeroVector, true, Grounded, Current).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactLegIKSolveTest,
	"KimodoDatasetCapture.Impact.LegIK.Solve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactLegIKSolveTest::RunTest(const FString& Parameters)
{
	using namespace ImpactLegIKTests;
	const FReferenceSkeleton Ref = MakeSkeleton();
	const TArray<FTransform> Source = ComponentPose(Ref);
	const double Upper = FVector::Dist(Source[ThighL].GetLocation(), Source[CalfL].GetLocation());
	const double Lower = FVector::Dist(Source[CalfL].GetLocation(), Source[FootL].GetLocation());
	const FVector Hip = Source[ThighL].GetLocation();

	// Reachable: the foot lands exactly; lengths kept; knee forward; foot rotation and toes kept.
	{
		TArray<FTransform> Pose = Source;
		const FVector Effector = Source[FootL].GetLocation() + FVector(0, 15, 20);
		FImpactLegIK::Solve(Ref, Leg, Effector, MeshForward, Source, Pose);
		TestTrue(TEXT("The foot lands exactly on its target"), Pose[FootL].GetLocation().Equals(Effector, 0.01));
		TestEqual(TEXT("The thigh keeps its length"), FVector::Dist(Pose[ThighL].GetLocation(), Pose[CalfL].GetLocation()), Upper, 0.01);
		TestEqual(TEXT("The shin keeps its length"), FVector::Dist(Pose[CalfL].GetLocation(), Pose[FootL].GetLocation()), Lower, 0.01);
		TestTrue(TEXT("The hip does not move"), Pose[ThighL].GetLocation().Equals(Hip, 0.01));

		// The knee sits forward of the hip-to-ankle line.
		const FVector Line = (Effector - Hip).GetSafeNormal();
		const FVector Knee = Pose[CalfL].GetLocation() - Hip;
		const FVector OffLine = Knee - Line * FVector::DotProduct(Knee, Line);
		TestTrue(TEXT("The knee bends forward"), FVector::DotProduct(OffLine, MeshForward) > 1.0);

		TestTrue(TEXT("The foot keeps its rotation"), Pose[FootL].GetRotation().Equals(Source[FootL].GetRotation(), 1e-4));
		TestTrue(TEXT("Toes keep their local transform"),
			Pose[BallL].GetRelativeTransform(Pose[FootL]).Equals(Source[BallL].GetRelativeTransform(Source[FootL]), 1e-3));
	}

	// Unreachable: the leg straightens toward the target and nothing stretches.
	{
		TArray<FTransform> Pose = Source;
		const FVector Effector = Hip + FVector(0, 30, -200);
		FImpactLegIK::Solve(Ref, Leg, Effector, MeshForward, Source, Pose);
		TestEqual(TEXT("Unreachable: the thigh does not stretch"), FVector::Dist(Pose[ThighL].GetLocation(), Pose[CalfL].GetLocation()), Upper, 0.01);
		TestEqual(TEXT("Unreachable: the shin does not stretch"), FVector::Dist(Pose[CalfL].GetLocation(), Pose[FootL].GetLocation()), Lower, 0.01);
		const FVector Reach = Pose[FootL].GetLocation() - Hip;
		TestEqual(TEXT("Unreachable: the leg is straight"), Reach.Size(), Upper + Lower, 0.05);
		TestTrue(TEXT("Unreachable: the leg points at the target"), Reach.GetSafeNormal().Equals((Effector - Hip).GetSafeNormal(), 1e-3));
	}
	return true;
}
