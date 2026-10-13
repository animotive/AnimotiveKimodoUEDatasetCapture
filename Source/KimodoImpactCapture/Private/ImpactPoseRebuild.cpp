// Copyright Animotive. All Rights Reserved.

#include "ImpactPoseRebuild.h"

#include "ReferenceSkeleton.h"

FImpactPoseRebuild::FImpactPoseRebuild(const FReferenceSkeleton& Ref, TConstArrayView<int32> BodyBoneIndices)
{
	const int32 Bones = Ref.GetNum();
	Parents.SetNum(Bones);
	BodyOf.Init(INDEX_NONE, Bones);
	HasBodyAbove.Init(false, Bones);
	for (int32 Body = 0; Body < BodyBoneIndices.Num(); ++Body)
	{
		if (BodyBoneIndices[Body] >= 0 && BodyBoneIndices[Body] < Bones)
		{
			BodyOf[BodyBoneIndices[Body]] = Body;
		}
	}
	// Parents always precede their children in a reference skeleton.
	for (int32 Bone = 0; Bone < Bones; ++Bone)
	{
		const int32 Parent = Ref.GetParentIndex(Bone);
		Parents[Bone] = Parent;
		HasBodyAbove[Bone] = Parent != INDEX_NONE && (BodyOf[Parent] != INDEX_NONE || HasBodyAbove[Parent]);
	}
}

void FImpactPoseRebuild::Rebuild(TConstArrayView<FTransform> BodyComponent, TConstArrayView<FTransform> SourceComponent, const FTransform& Root,
	TArrayView<FTransform> OutComponent) const
{
	const int32 Bones = FMath::Min3(Parents.Num(), SourceComponent.Num(), OutComponent.Num());
	for (int32 Bone = 0; Bone < Bones; ++Bone)
	{
		const int32 Parent = Parents[Bone];
		if (Parent == INDEX_NONE)
		{
			OutComponent[Bone] = Root;
			continue;
		}
		const FTransform SourceLocal = SourceComponent[Bone].GetRelativeTransform(SourceComponent[Parent]);
		const int32 Body = BodyOf[Bone];
		if (Body == INDEX_NONE || !BodyComponent.IsValidIndex(Body))
		{
			OutComponent[Bone] = SourceLocal * OutComponent[Parent];
		}
		else if (HasBodyAbove[Bone])
		{
			// Rotation from physics, position from the bone's length below its parent: no separation.
			OutComponent[Bone] = FTransform(BodyComponent[Body].GetRotation(), OutComponent[Parent].TransformPosition(SourceLocal.GetTranslation()),
				SourceComponent[Bone].GetScale3D());
		}
		else
		{
			// The top body carries the simulated displacement.
			OutComponent[Bone] = FTransform(BodyComponent[Body].GetRotation(), BodyComponent[Body].GetLocation(), SourceComponent[Bone].GetScale3D());
		}
	}
}
