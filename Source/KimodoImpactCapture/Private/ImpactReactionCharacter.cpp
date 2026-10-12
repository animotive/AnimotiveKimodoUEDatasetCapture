// Copyright Animotive. All Rights Reserved.

#include "ImpactReactionCharacter.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ImpactContactSpread.h"
#include "ImpactValidity.h"
#include "PhysicsControlComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactReactionCharacter)

#define LOCTEXT_NAMESPACE "ImpactReactionCharacter"

namespace
{
	const FName ModifierSet(TEXT("Modifiers"));
	const FName WorldSet(TEXT("World"));

	bool IsLegBone(FName Bone)
	{
		const FString Name = Bone.ToString();
		return Name.StartsWith(TEXT("thigh")) || Name.StartsWith(TEXT("calf")) || Name.StartsWith(TEXT("foot")) || Name.StartsWith(TEXT("ball"));
	}
}

FName AImpactReactionCharacter::WorldControlName(FName Bone) { return *FString::Printf(TEXT("W_%s"), *Bone.ToString()); }
FName AImpactReactionCharacter::ParentControlName(FName Bone) { return *FString::Printf(TEXT("P_%s"), *Bone.ToString()); }
FName AImpactReactionCharacter::ModifierName(FName Bone) { return *FString::Printf(TEXT("M_%s"), *Bone.ToString()); }

AImpactReactionCharacter::AImpactReactionCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AImpactReactionCharacter::Setup(USkeletalMesh* Mesh, UPhysicsAsset* PhysicsAsset, float WorldStrength, float ParentStrength, FText& OutReason)
{
	if (!Mesh || !PhysicsAsset)
	{
		OutReason = LOCTEXT("NoAssets", "The reaction character needs a skeletal mesh and a physics asset.");
		return false;
	}

	// No anim instance: the mesh is posed only by physics; targets come from the clip simulator.
	MeshComponent = NewObject<USkeletalMeshComponent>(this);
	MeshComponent->SetSkeletalMesh(Mesh);
	MeshComponent->SetPhysicsAsset(PhysicsAsset);
	MeshComponent->SetCollisionProfileName(TEXT("Ragdoll"));
	// Headless, nothing is rendered; refresh bones regardless so poses never go stale.
	MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	SetRootComponent(MeshComponent);
	MeshComponent->RegisterComponent();

	for (FConstraintInstance* Constraint : MeshComponent->Constraints)
	{
		if (Constraint)
		{
			// A joint pulled apart snaps back each step instead of stretching.
			Constraint->SetProjectionParams(true, 1.0f, 1.0f, 1.0f, 5.0f);
		}
	}

	Control = NewObject<UPhysicsControlComponent>(this);
	Control->RegisterComponent();

	FPhysicsControlData WorldData;
	WorldData.LinearStrength = WorldStrength;
	WorldData.AngularStrength = WorldStrength;
	WorldData.bUseSkeletalAnimation = false;
	FPhysicsControlData ParentData;
	ParentData.AngularStrength = ParentStrength;
	ParentData.bUseSkeletalAnimation = false;
	FPhysicsControlModifierData ModifierData;
	ModifierData.MovementType = EPhysicsMovementType::Kinematic;
	ModifierData.KinematicTargetSpace = EPhysicsControlKinematicTargetSpace::World;

	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	Bodies.Reset();
	for (const TObjectPtr<USkeletalBodySetup>& Setup : PhysicsAsset->SkeletalBodySetups)
	{
		FBody Body;
		Body.Bone = Setup->BoneName;
		Body.BoneIndex = Ref.FindBoneIndex(Body.Bone);
		if (Body.BoneIndex == INDEX_NONE)
		{
			OutReason = FText::Format(LOCTEXT("BodyNotInMesh", "Physics body {0} has no bone in the mesh."), FText::FromName(Body.Bone));
			return false;
		}
		for (int32 Index = Ref.GetParentIndex(Body.BoneIndex); Index != INDEX_NONE; Index = Ref.GetParentIndex(Index))
		{
			if (PhysicsAsset->FindBodyIndex(Ref.GetBoneName(Index)) != INDEX_NONE)
			{
				Body.ParentBone = Ref.GetBoneName(Index);
				Body.ParentBoneIndex = Index;
				break;
			}
		}
		Body.bLeg = IsLegBone(Body.Bone);
		Body.bPelvis = Body.Bone == FName(TEXT("pelvis"));
		Body.Instance = MeshComponent->GetBodyInstance(Body.Bone);
		Bodies.Add(Body);

		Control->CreateNamedBodyModifier(ModifierName(Body.Bone), MeshComponent, Body.Bone, ModifierSet, ModifierData);
		Control->CreateNamedControl(WorldControlName(Body.Bone), nullptr, NAME_None, MeshComponent, Body.Bone, WorldData, FPhysicsControlTarget(), WorldSet);
		if (!Body.ParentBone.IsNone())
		{
			Control->CreateNamedControl(ParentControlName(Body.Bone), MeshComponent, Body.ParentBone, MeshComponent, Body.Bone, ParentData, FPhysicsControlTarget(), NAME_None);
		}
	}
	for (FBody& Body : Bodies)
	{
		Body.ParentBody = Body.ParentBone.IsNone() ? INDEX_NONE : FindBody(Body.ParentBone);
	}
	return true;
}

int32 AImpactReactionCharacter::FindBody(FName Bone) const
{
	return Bodies.IndexOfByPredicate([Bone](const FBody& Body) { return Body.Bone == Bone; });
}

void AImpactReactionCharacter::TeleportToPose(const TArray<FTransform>& PoseComponentSpace, const FTransform& Frame)
{
	for (const FBody& Body : Bodies)
	{
		if (Body.Instance)
		{
			Body.Instance->SetBodyTransform(PoseComponentSpace[Body.BoneIndex] * Frame, ETeleportType::TeleportPhysics);
		}
	}
}

void AImpactReactionCharacter::SetTargets(const TArray<FTransform>& PoseComponentSpace, const FTransform& Frame, const FVector& Offset,
	const TMap<int32, FVector>& LocationOverrides, float DeltaSeconds)
{
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
	{
		const FBody& Body = Bodies[Index];
		FTransform Target = PoseComponentSpace[Body.BoneIndex] * Frame;
		Target.AddToTranslation(Offset);
		if (const FVector* Override = LocationOverrides.Find(Index))
		{
			Target.SetLocation(*Override);
		}
		Control->SetBodyModifierKinematicTarget(ModifierName(Body.Bone), Target.GetLocation(), Target.Rotator(), false);
		// bEnableControl false: setting a target must never re-enable a released control.
		Control->SetControlTargetPositionAndOrientation(WorldControlName(Body.Bone), Target.GetLocation(), Target.Rotator(), DeltaSeconds, false, false, true, false);
		if (Body.ParentBoneIndex != INDEX_NONE)
		{
			const FTransform Relative = PoseComponentSpace[Body.BoneIndex].GetRelativeTransform(PoseComponentSpace[Body.ParentBoneIndex]);
			Control->SetControlTargetPositionAndOrientation(ParentControlName(Body.Bone), Relative.GetLocation(), Relative.Rotator(), DeltaSeconds, false, false, true, false);
		}
	}
}

void AImpactReactionCharacter::SetAllSimulated()
{
	Control->SetBodyModifiersInSetMovementType(ModifierSet, EPhysicsMovementType::Simulated);
}

void AImpactReactionCharacter::SetBodyKinematic(int32 Body, bool bKinematic)
{
	if (Bodies.IsValidIndex(Body))
	{
		Control->SetBodyModifierMovementType(ModifierName(Bodies[Body].Bone), bKinematic ? EPhysicsMovementType::Kinematic : EPhysicsMovementType::Simulated);
	}
}

void AImpactReactionCharacter::SetStrength(int32 Body, float World, float Joint, float WorldAngular, float DampingRatio)
{
	if (!Bodies.IsValidIndex(Body))
	{
		return;
	}
	FPhysicsControlMultiplier WorldMultiplier;
	WorldMultiplier.LinearStrengthMultiplier = FVector(World);
	WorldMultiplier.AngularStrengthMultiplier = WorldAngular >= 0.0f ? WorldAngular : World;
	WorldMultiplier.LinearDampingRatioMultiplier = FVector(DampingRatio);
	WorldMultiplier.AngularDampingRatioMultiplier = DampingRatio;
	Control->SetControlMultiplier(WorldControlName(Bodies[Body].Bone), WorldMultiplier, false);
	if (Bodies[Body].ParentBoneIndex != INDEX_NONE)
	{
		FPhysicsControlMultiplier JointMultiplier;
		JointMultiplier.AngularStrengthMultiplier = Joint;
		Control->SetControlMultiplier(ParentControlName(Bodies[Body].Bone), JointMultiplier, false);
	}
}

void AImpactReactionCharacter::Release()
{
	Control->SetControlsInSetEnabled(TEXT("All"), false);
	SetAllSimulated();
}

float AImpactReactionCharacter::TotalMass() const
{
	float Mass = 0.0f;
	for (const FBody& Body : Bodies)
	{
		Mass += Body.Instance ? Body.Instance->GetBodyMass() : 0.0f;
	}
	return Mass;
}

FVector AImpactReactionCharacter::CenterOfMass() const
{
	FVector Sum = FVector::ZeroVector;
	float Mass = 0.0f;
	for (const FBody& Body : Bodies)
	{
		if (Body.Instance)
		{
			const float M = Body.Instance->GetBodyMass();
			Sum += Body.Instance->GetCOMPosition() * M;
			Mass += M;
		}
	}
	return Mass > 0.0f ? Sum / Mass : FVector::ZeroVector;
}

FVector AImpactReactionCharacter::CenterOfMassVelocity() const
{
	FVector Sum = FVector::ZeroVector;
	float Mass = 0.0f;
	for (const FBody& Body : Bodies)
	{
		if (Body.Instance)
		{
			const float M = Body.Instance->GetBodyMass();
			Sum += Body.Instance->GetUnrealWorldVelocity() * M;
			Mass += M;
		}
	}
	return Mass > 0.0f ? Sum / Mass : FVector::ZeroVector;
}

FTransform AImpactReactionCharacter::BodyTransform(int32 Body) const
{
	return Bodies.IsValidIndex(Body) && Bodies[Body].Instance ? Bodies[Body].Instance->GetUnrealWorldTransform() : FTransform::Identity;
}

float AImpactReactionCharacter::BodyRadius(int32 Body) const
{
	if (!Bodies.IsValidIndex(Body) || !Bodies[Body].Instance)
	{
		return 0.0f;
	}
	return float(Bodies[Body].Instance->GetBodyBounds().GetExtent().GetMin());
}

void AImpactReactionCharacter::CollectSamples(const TArray<FTransform>& PoseComponentSpace, TArray<FImpactBodySample>& OutSamples) const
{
	OutSamples.Reset(Bodies.Num());
	for (const FBody& Body : Bodies)
	{
		FImpactBodySample Sample;
		Sample.Name = Body.Bone;
		if (Body.Instance)
		{
			const FTransform TM = Body.Instance->GetUnrealWorldTransform();
			Sample.Location = TM.GetLocation();
			Sample.Rotation = TM.GetRotation();
			Sample.LinearVelocity = Body.Instance->GetUnrealWorldVelocity();
			Sample.AngularVelocityDegPerSecond = FMath::RadiansToDegrees(Body.Instance->GetUnrealWorldAngularVelocityInRadians());
			if (Body.ParentBody != INDEX_NONE && Bodies[Body.ParentBody].Instance)
			{
				// Where the parent's joint says this body should be, from the pose's bone offset.
				const FVector Offset = PoseComponentSpace[Body.BoneIndex].GetRelativeTransform(PoseComponentSpace[Body.ParentBoneIndex]).GetTranslation();
				const FVector Expected = Bodies[Body.ParentBody].Instance->GetUnrealWorldTransform().TransformPosition(Offset);
				Sample.JointGapCm = float(FVector::Dist(Sample.Location, Expected));
			}
		}
		OutSamples.Add(Sample);
	}
}

void AImpactReactionCharacter::ApplyShares(TConstArrayView<FImpactBodyShare> Shares, const FVector& DirectionWorld, float Scale)
{
	const FVector Direction = DirectionWorld.GetSafeNormal();
	for (const FImpactBodyShare& Share : Shares)
	{
		const int32 Body = FindBody(Share.Name);
		if (Body != INDEX_NONE && Bodies[Body].Instance)
		{
			// At the share's own point: an off-centre contact twists the body as well as pushing it.
			Bodies[Body].Instance->AddImpulseAtPosition(Direction * (Share.ImpulseKgCmPerSecond * Scale), Share.ApplyPoint);
		}
	}
}

#undef LOCTEXT_NAMESPACE
