// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "ImpactSourceClock.h"
#include "ImpactReactionCharacter.h"
#include "ImpactSimWorld.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactHardLimitsTest,
	"KimodoDatasetCapture.Impact.Character.LimitsAreHardThroughTheReaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactHardLimitsTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin"));
	UPhysicsAsset* PhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Rigs/PA_UEFN_Mannequin.PA_UEFN_Mannequin"));
	// A standing idle: its elbows and knees sit inside the asset's range, so nothing opens them.
	UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Kimodo/ImpactSources/M_Kimodo_Stand_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin.M_Kimodo_Stand_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin"));
	if (!Mesh || !PhysicsAsset || !Idle)
	{
		AddWarning(TEXT("UEFN mannequin or Kimodo sources not found; hard-limit checks skipped."));
		return true;
	}

	FImpactSimWorld World;
	AImpactReactionCharacter* Character = World.GetWorld()->SpawnActor<AImpactReactionCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
	FText Reason;
	if (!TestNotNull(TEXT("Character spawned"), Character) || !TestTrue(*FString::Printf(TEXT("Character set up: %s"), *Reason.ToString()),
		Character->Setup(Mesh, PhysicsAsset, 8.0f, 16.0f, Reason)))
	{
		return true;
	}
	const FImpactPoseSampler Sampler(*Mesh, *Idle, false);
	Character->OpenLimitsToHold(Sampler, 1.0 / 30.0, 5.0f);

	int32 Checked = 0;
	for (const AImpactReactionCharacter::FBody& Body : Character->GetBodies())
	{
		const FConstraintInstance* Constraint = Body.Constraint;
		if (!Constraint)
		{
			continue;
		}
		++Checked;
		const FString Name = Body.Bone.ToString();
		if (Constraint->GetAngularSwing1Motion() == ACM_Limited || Constraint->GetAngularSwing2Motion() == ACM_Limited)
		{
			TestFalse(*FString::Printf(TEXT("%s: a limited swing is hard, not the asset's soft limit"), *Name), Constraint->GetIsSoftSwingLimit());
		}
		if (Constraint->GetAngularTwistMotion() == ACM_Limited)
		{
			TestFalse(*FString::Printf(TEXT("%s: a limited twist is hard"), *Name), Constraint->GetIsSoftTwistLimit());
		}
		if (Name.StartsWith(TEXT("lowerarm")) || Name.StartsWith(TEXT("calf")))
		{
			// Hardened, not opened: the hinge keeps the asset's range.
			TestEqual(*FString::Printf(TEXT("%s keeps the asset's 70 degree hinge"), *Name), float(Constraint->GetAngularSwing1Limit()), 70.0f, 0.01f);
		}
	}
	TestTrue(TEXT("The mannequin has jointed bodies to check"), Checked > 10);
	Character->Destroy();
	return true;
}
