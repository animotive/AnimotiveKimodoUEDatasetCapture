// Copyright Animotive. All Rights Reserved.

#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "ImpactClipSimulator.h"
#include "ImpactReactionCharacter.h"
#include "ImpactSimWorld.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactSolverIterationsTest,
	"KimodoDatasetCapture.Impact.Character.SolverIterations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactSolverIterationsTest::RunTest(const FString& Parameters)
{
	const FImpactClipSetup Defaults;
	TestEqual(TEXT("Clips solve with 32 position iterations"), Defaults.SolverPositionIterations, 32);
	TestEqual(TEXT("and 4 velocity iterations"), Defaults.SolverVelocityIterations, 4);

	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin"));
	UPhysicsAsset* PhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Rigs/PA_UEFN_Mannequin.PA_UEFN_Mannequin"));
	if (!Mesh || !PhysicsAsset)
	{
		AddWarning(TEXT("UEFN mannequin not found; solver iteration checks skipped."));
		return true;
	}
	FImpactSimWorld World;
	AImpactReactionCharacter* Character = World.GetWorld()->SpawnActor<AImpactReactionCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
	FText Reason;
	if (!TestNotNull(TEXT("Character spawned"), Character) || !TestTrue(TEXT("Character set up"), Character->Setup(Mesh, PhysicsAsset, 8.0f, 16.0f, Reason)))
	{
		return true;
	}
	Character->SetSolverIterations(32, 4);
	int32 Checked = 0;
	for (const AImpactReactionCharacter::FBody& Body : Character->GetBodies())
	{
		if (!Body.Instance)
		{
			continue;
		}
		++Checked;
		TestEqual(*FString::Printf(TEXT("%s: 32 position iterations"), *Body.Bone.ToString()), Body.Instance->GetPositionSolverIterationCount(), 32);
		TestEqual(*FString::Printf(TEXT("%s: 4 velocity iterations"), *Body.Bone.ToString()), Body.Instance->GetVelocitySolverIterationCount(), 4);
	}
	TestTrue(TEXT("Every body checked"), Checked > 10);
	Character->Destroy();
	return true;
}
