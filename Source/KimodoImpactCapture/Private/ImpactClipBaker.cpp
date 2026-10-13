// Copyright Animotive. All Rights Reserved.

#include "ImpactClipBaker.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "ImpactPoseBuffer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "ImpactClipBaker"

FString FImpactClipBaker::PackageName(const FString& OutputRoot, const FString& BatchName, const FString& ClipId)
{
	FString Root = OutputRoot;
	while (Root.EndsWith(TEXT("/")))
	{
		Root.LeftChopInline(1);
	}
	return Root / BatchName / ClipId;
}

UAnimSequence* FImpactClipBaker::Bake(UObject* Outer, FName Name, USkeleton* Skeleton, const FReferenceSkeleton& Ref,
	const FImpactPoseBuffer& ComponentPoses, int32 FrameRate, FText& OutReason)
{
	if (!Skeleton)
	{
		OutReason = LOCTEXT("NoSkeleton", "There is no skeleton to bake the clip onto.");
		return nullptr;
	}
	if (ComponentPoses.Num() == 0)
	{
		OutReason = LOCTEXT("NoFrames", "The clip recorded no frames.");
		return nullptr;
	}
	const int32 Bones = Ref.GetNum();
	if (ComponentPoses.GetBoneCount() != Bones)
	{
		OutReason = FText::Format(LOCTEXT("BoneMismatch", "The recording has {0} bones per frame; the skeleton has {1}."),
			FText::AsNumber(ComponentPoses.GetBoneCount()), FText::AsNumber(Bones));
		return nullptr;
	}
	if (FrameRate <= 0)
	{
		OutReason = LOCTEXT("BadRate", "The frame rate must be positive.");
		return nullptr;
	}

	UAnimSequence* Sequence = NewObject<UAnimSequence>(Outer ? Outer : GetTransientPackage(), Name, RF_Public | RF_Standalone);
	Sequence->SetSkeleton(Skeleton);
	IAnimationDataController& Controller = Sequence->GetController();
	Controller.InitializeModel();
	Controller.OpenBracket(LOCTEXT("BakeBracket", "Bake impact clip"), false);
	Controller.SetFrameRate(FFrameRate(FrameRate, 1), false);
	Controller.SetNumberOfFrames(FFrameNumber(ComponentPoses.Num() - 1), false);

	TArray<FVector3f> Positions;
	TArray<FQuat4f> Rotations;
	TArray<FVector3f> Scales;
	Positions.Reserve(ComponentPoses.Num());
	Rotations.Reserve(ComponentPoses.Num());
	Scales.Reserve(ComponentPoses.Num());
	for (int32 Bone = 0; Bone < Bones; ++Bone)
	{
		Positions.Reset();
		Rotations.Reset();
		Scales.Reset();
		const int32 Parent = Ref.GetParentIndex(Bone);
		for (int32 Frame = 0; Frame < ComponentPoses.Num(); ++Frame)
		{
			const TArrayView<const FTransform> Pose = ComponentPoses.GetFrame(Frame);
			const FTransform Local = Parent == INDEX_NONE ? Pose[Bone] : Pose[Bone].GetRelativeTransform(Pose[Parent]);
			Positions.Add(FVector3f(Local.GetLocation()));
			Rotations.Add(FQuat4f(Local.GetRotation().GetNormalized()));
			Scales.Add(FVector3f(Local.GetScale3D()));
		}
		const FName BoneName = Ref.GetBoneName(Bone);
		Controller.AddBoneCurve(BoneName, false);
		Controller.SetBoneTrackKeys(BoneName, Positions, Rotations, Scales, false);
	}
	Controller.NotifyPopulated();
	Controller.CloseBracket(false);
	return Sequence;
}

bool FImpactClipBaker::Save(UAnimSequence* Sequence, const FString& Filename, FText& OutReason)
{
	if (!Sequence)
	{
		OutReason = LOCTEXT("NothingToSave", "There is no baked clip to save.");
		return false;
	}
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Sequence->GetPackage(), Sequence, *Filename, Args))
	{
		OutReason = FText::Format(LOCTEXT("SaveFailed", "The clip could not be saved to {0}."), FText::FromString(Filename));
		return false;
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
