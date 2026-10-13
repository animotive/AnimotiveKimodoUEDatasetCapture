// Copyright Animotive. All Rights Reserved.

#include "ImpactPoseRebuild.h"
#include "Misc/AutomationTest.h"
#include "ReferenceSkeleton.h"

namespace ImpactPoseRebuildTests
{
	enum EBone { Root, Pelvis, Spine, Neck02, Head, ThighL, CalfL, FootL, BallL, ThighTwistL, BoneCount };

	/** A small humanoid: neck_02, ball_l and a thigh twist bone have no physics body. */
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
			Add(TEXT("pelvis"), Root, FVector(0, 0, 100));
			Add(TEXT("spine"), Pelvis, FVector(0, 0, 20), FRotator(5, 0, 0));
			Add(TEXT("neck_02"), Spine, FVector(0, 0, 25));
			Add(TEXT("head"), Neck02, FVector(0, 0, 10));
			Add(TEXT("thigh_l"), Pelvis, FVector(0, -10, -5));
			Add(TEXT("calf_l"), ThighL, FVector(0, 0, -45), FRotator(-10, 0, 0));
			Add(TEXT("foot_l"), CalfL, FVector(0, 0, -45));
			Add(TEXT("ball_l"), FootL, FVector(0, 12, -5));
			Add(TEXT("thigh_twist_l"), ThighL, FVector(0, 0, -20), FRotator(0, 0, 15));
		}
		return Ref;
	}

	const TArray<int32> BodyBones = { Pelvis, Spine, Head, ThighL, CalfL, FootL };

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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactPoseRebuildTest,
	"KimodoDatasetCapture.Impact.PoseRebuild.Rebuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactPoseRebuildTest::RunTest(const FString& Parameters)
{
	using namespace ImpactPoseRebuildTests;
	const FReferenceSkeleton Ref = MakeSkeleton();
	const TArray<FTransform> Source = ComponentPose(Ref);
	const FImpactPoseRebuild Rebuild(Ref, BodyBones);
	TestEqual(TEXT("Prepared for every bone"), Rebuild.GetBoneCount(), int32(BoneCount));

	TArray<FTransform> Out;
	Out.Init(FTransform(FVector(999.0)), BoneCount);

	// Bodies exactly on the source: the source comes back.
	{
		TArray<FTransform> Bodies;
		for (const int32 Bone : BodyBones)
		{
			Bodies.Add(Source[Bone]);
		}
		Rebuild.Rebuild(Bodies, Source, Source[Root], Out);
		bool bSame = true;
		for (int32 Bone = 0; Bone < BoneCount; ++Bone)
		{
			bSame &= Out[Bone].Equals(Source[Bone], 1e-3);
		}
		TestTrue(TEXT("Bodies on the source pose rebuild the source pose"), bSame);
	}

	// Bodies turned and pulled apart, as a stretched simulation leaves them.
	FRandomStream Random(7);
	TArray<FTransform> Bodies;
	for (const int32 Bone : BodyBones)
	{
		const FQuat Turn(Random.GetUnitVector(), FMath::DegreesToRadians(Random.FRandRange(10.0f, 60.0f)));
		Bodies.Add(FTransform(Turn * Source[Bone].GetRotation(), Source[Bone].GetLocation() + Random.GetUnitVector() * 8.0));
	}
	const FTransform GivenRoot(FQuat(FVector::UpVector, 0.3), FVector(40, -12, 0));
	Rebuild.Rebuild(Bodies, Source, GivenRoot, Out);

	TestTrue(TEXT("The root bone is the one given"), Out[Root].Equals(GivenRoot, 1e-4));
	bool bRotationsKept = true;
	for (int32 Body = 0; Body < BodyBones.Num(); ++Body)
	{
		bRotationsKept &= Out[BodyBones[Body]].GetRotation().Equals(Bodies[Body].GetRotation(), 1e-4);
	}
	TestTrue(TEXT("Every body bone takes its body's rotation"), bRotationsKept);
	TestTrue(TEXT("The top body keeps its simulated position"), Out[Pelvis].GetLocation().Equals(Bodies[0].GetLocation(), 1e-3));

	// No joint separates: every bone sits at its source offset from its parent.
	bool bConnected = true;
	for (int32 Bone = 1; Bone < BoneCount; ++Bone)
	{
		const int32 Parent = Ref.GetParentIndex(Bone);
		if (Parent == Root)
		{
			continue;
		}
		const FVector Offset = Source[Bone].GetRelativeTransform(Source[Parent]).GetTranslation();
		bConnected &= Out[Bone].GetLocation().Equals(Out[Parent].TransformPosition(Offset), 1e-3);
	}
	TestTrue(TEXT("No joint separates for any input"), bConnected);
	TestFalse(TEXT("The head is not left at its stretched body position"), Out[Head].GetLocation().Equals(Bodies[2].GetLocation(), 0.5));

	// Bones without a body follow the source locally.
	for (const int32 Bone : { int32(Neck02), int32(BallL), int32(ThighTwistL) })
	{
		const int32 Parent = Ref.GetParentIndex(Bone);
		TestTrue(*FString::Printf(TEXT("%s keeps its source local transform"), *Ref.GetBoneName(Bone).ToString()),
			Out[Bone].GetRelativeTransform(Out[Parent]).Equals(Source[Bone].GetRelativeTransform(Source[Parent]), 1e-3));
	}
	return true;
}
