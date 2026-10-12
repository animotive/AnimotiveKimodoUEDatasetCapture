// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ImpactReactionCharacter.generated.h"

class UPhysicsAsset;
class UPhysicsControlComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
struct FBodyInstance;
struct FImpactBodySample;
struct FImpactBodyShare;

/**
 * The character a clip is simulated on. Posed only by physics: the mesh has no anim instance, and
 * every physics body is driven by named Physics Control controls toward targets the clip
 * simulator sets by hand each frame (the Phase 1 spike found Physics Control's own animation
 * targets did not hold the character up headless).
 *
 * Per body: W_<bone> world-space control, P_<bone> parent-space control (to the nearest ancestor
 * with a body), M_<bone> body modifier. Joint projection is on for every constraint so a joint
 * pulled apart snaps back rather than stretching.
 */
UCLASS(NotPlaceable, Transient)
class KIMODOIMPACTCAPTURE_API AImpactReactionCharacter : public AActor
{
	GENERATED_BODY()

public:
	struct FBody
	{
		FName Bone;
		int32 BoneIndex = INDEX_NONE;
		/** Nearest ancestor bone that has a body, or none for the root body. */
		FName ParentBone;
		int32 ParentBoneIndex = INDEX_NONE;
		/** Index into GetBodies() of the parent body. */
		int32 ParentBody = INDEX_NONE;
		bool bLeg = false;
		bool bPelvis = false;
		FBodyInstance* Instance = nullptr;
	};

	AImpactReactionCharacter();

	/** Builds the mesh, physics and controls. False with a reason when the assets cannot be used. */
	bool Setup(USkeletalMesh* Mesh, UPhysicsAsset* PhysicsAsset, float WorldStrength, float ParentStrength, FText& OutReason);

	const TArray<FBody>& GetBodies() const { return Bodies; }
	int32 FindBody(FName Bone) const;
	USkeletalMeshComponent* GetMeshComponent() const { return MeshComponent; }

	/** Places every body exactly on a component-space pose, expressed in Frame. */
	void TeleportToPose(const TArray<FTransform>& PoseComponentSpace, const FTransform& Frame);

	/**
	 * Sets every body's kinematic and control targets from a component-space pose in Frame, moved
	 * by Offset. LocationOverrides replace the world target location of listed bodies (planted feet).
	 */
	void SetTargets(const TArray<FTransform>& PoseComponentSpace, const FTransform& Frame, const FVector& Offset,
		const TMap<int32, FVector>& LocationOverrides, float DeltaSeconds);

	/** Every body simulated: the reaction begins. */
	void SetAllSimulated();

	/** One body kinematic (a planted foot) or simulated. */
	void SetBodyKinematic(int32 Body, bool bKinematic);

	/**
	 * Strength multipliers for one body. WorldAngular below zero means "same as World".
	 * DampingRatio multiplies both world-space damping ratios.
	 */
	void SetStrength(int32 Body, float World, float Joint, float WorldAngular = -1.0f, float DampingRatio = 1.0f);

	/** Every control off and every body simulated: a ragdoll. */
	void Release();

	float TotalMass() const;
	FVector CenterOfMass() const;
	FVector CenterOfMassVelocity() const;
	FTransform BodyTransform(int32 Body) const;
	/** Rough radius of a body: the smallest half-extent of its bounds. */
	float BodyRadius(int32 Body) const;

	/** Validity samples for every body, with joint gaps measured against the pose's bone offsets. */
	void CollectSamples(const TArray<FTransform>& PoseComponentSpace, TArray<FImpactBodySample>& OutSamples) const;

	/** Applies spread impulse shares along DirectionWorld, each at its own point. */
	void ApplyShares(TConstArrayView<FImpactBodyShare> Shares, const FVector& DirectionWorld, float Scale = 1.0f);

	static FName WorldControlName(FName Bone);
	static FName ParentControlName(FName Bone);
	static FName ModifierName(FName Bone);

private:
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	UPROPERTY()
	TObjectPtr<UPhysicsControlComponent> Control;

	TArray<FBody> Bodies;
};
