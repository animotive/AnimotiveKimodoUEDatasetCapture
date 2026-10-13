// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "ImpactClipBaker.h"
#include "ImpactPoseBuffer.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactClipBakerPathTest,
	"KimodoDatasetCapture.Impact.Bake.PackageName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactClipBakerPathTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Root, batch and clip joined"), FImpactClipBaker::PackageName(TEXT("/Game/ImpactDataset"), TEXT("Batch01"), TEXT("imp_00ff")),
		FString(TEXT("/Game/ImpactDataset/Batch01/imp_00ff")));
	TestEqual(TEXT("A trailing slash on the root is not doubled"), FImpactClipBaker::PackageName(TEXT("/Game/ImpactDataset/"), TEXT("Batch01"), TEXT("imp_00ff")),
		FString(TEXT("/Game/ImpactDataset/Batch01/imp_00ff")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactClipBakerBakeTest,
	"KimodoDatasetCapture.Impact.Bake.Sequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactClipBakerBakeTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin"));
	if (!Mesh)
	{
		AddWarning(TEXT("GameAnimationSample content not found; bake test skipped."));
		return true;
	}
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const int32 Bones = Ref.GetNum();
	const int32 Pelvis = Ref.FindBoneIndex(TEXT("pelvis"));
	const int32 Hand = Ref.FindBoneIndex(TEXT("hand_r"));

	// Five frames: the root slides forward, the pelvis turns, the rest follows the reference pose.
	const int32 FrameCount = 5;
	FImpactPoseBuffer Poses;
	Poses.Reserve(FrameCount, Bones);
	TArray<TArray<FTransform>> ExpectedLocal;
	for (int32 Frame = 0; Frame < FrameCount; ++Frame)
	{
		TArray<FTransform> Local = Ref.GetRefBonePose();
		Local[0].SetLocation(FVector(0.0, 10.0 * Frame, 0.0));
		Local[Pelvis].SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(10.0 * Frame)) * Local[Pelvis].GetRotation());
		TArrayView<FTransform> Component = Poses.AddFrame();
		for (int32 Bone = 0; Bone < Bones; ++Bone)
		{
			const int32 Parent = Ref.GetParentIndex(Bone);
			Component[Bone] = Parent == INDEX_NONE ? Local[Bone] : Local[Bone] * Component[Parent];
		}
		ExpectedLocal.Add(MoveTemp(Local));
	}

	FText Reason;
	UPackage* Package = CreatePackage(TEXT("/Temp/KimodoImpactBakeTest/imp_baketest"));
	UAnimSequence* Sequence = FImpactClipBaker::Bake(Package, TEXT("imp_baketest"), Mesh->GetSkeleton(), Ref, Poses, 60, Reason);
	if (!TestNotNull(*FString::Printf(TEXT("Bakes: %s"), *Reason.ToString()), Sequence))
	{
		return false;
	}
	const IAnimationDataModel* Model = Sequence->GetDataModel();
	TestEqual(TEXT("On the source skeleton"), Sequence->GetSkeleton(), Mesh->GetSkeleton());
	TestEqual(TEXT("One key per recorded frame"), Model->GetNumberOfKeys(), FrameCount);
	TestTrue(TEXT("At the recording frame rate"), Model->GetFrameRate() == FFrameRate(60, 1));
	TestEqual(TEXT("One track per bone"), Model->GetNumBoneTracks(), Bones);

	for (const int32 Frame : { 0, FrameCount / 2, FrameCount - 1 })
	{
		for (const int32 Bone : { 0, Pelvis, Hand })
		{
			const FTransform Baked = Model->GetBoneTrackTransform(Ref.GetBoneName(Bone), FFrameNumber(Frame));
			TestTrue(*FString::Printf(TEXT("%s at frame %d matches what was recorded"), *Ref.GetBoneName(Bone).ToString(), Frame),
				Baked.Equals(ExpectedLocal[Frame][Bone], 1e-3));
		}
	}

	// Saving writes the asset file.
	const FString File = FPaths::ProjectSavedDir() / TEXT("Automation/KimodoImpactBakeTest/imp_baketest.uasset");
	IFileManager::Get().Delete(*File, false, true, true);
	TestTrue(*FString::Printf(TEXT("Saves: %s"), *Reason.ToString()), FImpactClipBaker::Save(Sequence, File, Reason));
	TestTrue(TEXT("The asset file exists"), IFileManager::Get().FileExists(*File));
	IFileManager::Get().Delete(*File, false, true, true);
	Package->MarkAsGarbage();

	// Nothing to bake, or inputs that disagree: refused with a reason.
	FImpactPoseBuffer Empty;
	Empty.Reserve(4, Bones);
	Reason = FText();
	TestNull(TEXT("An empty recording does not bake"), FImpactClipBaker::Bake(GetTransientPackage(), NAME_None, Mesh->GetSkeleton(), Ref, Empty, 60, Reason));
	TestFalse(TEXT("and says why"), Reason.IsEmpty());
	FImpactPoseBuffer Wrong;
	Wrong.Reserve(1, Bones - 1);
	Wrong.AddFrame();
	Reason = FText();
	TestNull(TEXT("A pose of the wrong bone count does not bake"), FImpactClipBaker::Bake(GetTransientPackage(), NAME_None, Mesh->GetSkeleton(), Ref, Wrong, 60, Reason));
	TestFalse(TEXT("and says why"), Reason.IsEmpty());
	Reason = FText();
	TestNull(TEXT("No skeleton does not bake"), FImpactClipBaker::Bake(GetTransientPackage(), NAME_None, nullptr, Ref, Poses, 60, Reason));
	TestFalse(TEXT("and says why"), Reason.IsEmpty());
	return true;
}
