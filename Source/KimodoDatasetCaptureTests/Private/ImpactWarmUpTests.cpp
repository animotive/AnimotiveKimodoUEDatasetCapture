// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "ImpactClipSimulator.h"
#include "ImpactParams.h"
#include "ImpactRegionTable.h"
#include "ImpactSchedule.h"
#include "ImpactSimWorld.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactWarmUpTest,
	"KimodoDatasetCapture.Impact.Simulator.PhysicsSettlesBeforeRecording",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactWarmUpTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Physics runs unrecorded before a clip starts"), FImpactClipSetup().WarmUpFrames >= 10);

	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin"));
	UPhysicsAsset* Physics = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Rigs/PA_UEFN_Mannequin.PA_UEFN_Mannequin"));
	UAnimSequence* Crouch = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Kimodo/ImpactSources/M_Kimodo_Crouch_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin.M_Kimodo_Crouch_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin"));
	UAnimSequence* Stand = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Kimodo/ImpactSources/M_Kimodo_Stand_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin.M_Kimodo_Stand_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin"));
	if (!SkeletalMesh || !Physics || !Crouch || !Stand)
	{
		AddWarning(TEXT("UEFN mannequin or Kimodo sources not found; warm-up checks skipped."));
		return true;
	}
	TStrongObjectPtr<UImpactorCatalog> Catalog(NewObject<UImpactorCatalog>());
	Catalog->FillWithStarterArchetypes();
	TStrongObjectPtr<UImpactRegionTable> Regions(NewObject<UImpactRegionTable>());
	Regions->FillWithUEFNMannequinEntries();
	const FImpactorArchetype* Baseball = Catalog->Archetypes.FindByPredicate([](const FImpactorArchetype& A) { return A.Id == TEXT("Baseball"); });
	if (!TestNotNull(TEXT("The catalog has a baseball"), Baseball))
	{
		return true;
	}

	const FReferenceSkeleton& Ref = SkeletalMesh->GetRefSkeleton();
	const int32 PelvisBone = Ref.FindBoneIndex(TEXT("pelvis"));
	const FName Feet[] = { TEXT("foot_l"), TEXT("foot_r") };

	for (UAnimSequence* Source : { Crouch, Stand })
	{
		const FString Name = Source == Crouch ? TEXT("Crouch idle") : TEXT("Stand idle");
		FImpactClipSetup Setup;
		Setup.Mesh = SkeletalMesh;
		Setup.PhysicsAsset = Physics;
		Setup.Source = Source;
		Setup.RegionTable = Regions.Get();
		Setup.BatchSeed = 7;
		Setup.MaxReactionFrames = 30;
		FImpactParams& Params = Setup.Params;
		Params.Impactor.Archetype = Baseball;
		Params.Impactor.MassKg = Baseball->MassKg.Min;
		Params.Impactor.SpeedMetresPerSecond = Baseball->SpeedMetresPerSecond.Min;
		Params.Impactor.ContactRadiusCm = Baseball->ContactRadiusCm.Min;
		Params.TransferredImpulseKgCmPerSecond = FImpactParamsSampler::TransferredImpulse(Params.Impactor.MassKg, Params.Impactor.SpeedMetresPerSecond, 75.0f, 0.2f);
		Params.ReactionGain = Baseball->ReactionGain;
		Params.ReactionImpulseKgCmPerSecond = Params.TransferredImpulseKgCmPerSecond * Params.ReactionGain;
		Params.Intensity = FImpactIntensityThresholds().Classify(Params.ReactionImpulseKgCmPerSecond);
		Params.Region = EImpactBodyRegion::LowerArmRight;
		Params.DirectionLocal = FVector(-1.0, 0.0, 0.0);
		Params.SourcePhase = 0.25f;

		FImpactClipRecording Out;
		FText Reason;
		FImpactSimWorld World;
		if (!TestTrue(*FString::Printf(TEXT("%s: the clip runs"), *Name), FImpactScheduleSampler::Sample(Params, FImpactDeliveryDistributions(), 7, 0, Setup.Schedule, Reason)
			&& FImpactClipSimulator::Run(World, Setup, Out, Reason)))
		{
			continue;
		}
		const int32 PelvisBody = Out.BodyBones.IndexOfByKey(FName(TEXT("pelvis")));
		const int32 FootBody[] = { Out.BodyBones.IndexOfByKey(Feet[0]), Out.BodyBones.IndexOfByKey(Feet[1]) };
		if (!TestTrue(*FString::Printf(TEXT("%s: pelvis and feet recorded"), *Name), PelvisBody != INDEX_NONE && FootBody[0] != INDEX_NONE && FootBody[1] != INDEX_NONE)
			|| !TestTrue(*FString::Printf(TEXT("%s: the pre-roll is recorded"), *Name), Out.BodyPoses.Num() > Out.ImpactFrame))
		{
			continue;
		}

		// Up to the hit the body follows its source: it neither drops as physics takes its weight
		// nor kicks its feet. The switch to physics dropped every body ~1.4 cm in one frame and
		// spun a standing foot at 190 deg/s.
		float WorstDropCm = 0.0f;
		float WorstFootDeg = 0.0f;
		for (int32 Frame = 1; Frame < Out.ImpactFrame; ++Frame)
		{
			const TArrayView<const FTransform> Body = Out.BodyPoses.GetFrame(Frame);
			const TArrayView<const FTransform> Prev = Out.BodyPoses.GetFrame(Frame - 1);
			const TArrayView<const FTransform> SourceNow = Out.SourcePoses.GetFrame(Frame);
			const TArrayView<const FTransform> SourcePrev = Out.SourcePoses.GetFrame(Frame - 1);
			const double BodyMove = Body[PelvisBody].GetLocation().Z - Prev[PelvisBody].GetLocation().Z;
			const double SourceMove = SourceNow[PelvisBone].GetLocation().Z - SourcePrev[PelvisBone].GetLocation().Z;
			WorstDropCm = FMath::Max(WorstDropCm, float(FMath::Abs(BodyMove - SourceMove)));
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const int32 Bone = Out.BodyBoneIndices[FootBody[Side]];
				const float BodyTurn = FMath::RadiansToDegrees(float(Body[FootBody[Side]].GetRotation().AngularDistance(Prev[FootBody[Side]].GetRotation())));
				const float SourceTurn = FMath::RadiansToDegrees(float(SourceNow[Bone].GetRotation().AngularDistance(SourcePrev[Bone].GetRotation())));
				WorstFootDeg = FMath::Max(WorstFootDeg, FMath::Abs(BodyTurn - SourceTurn));
			}
		}
		TestTrue(*FString::Printf(TEXT("%s: the pelvis keeps its source's height change within 0.5 cm a frame before the hit (worst %.2f)"), *Name, WorstDropCm), WorstDropCm < 0.5f);
		TestTrue(*FString::Printf(TEXT("%s: the feet turn with their source within 1.5 deg a frame before the hit (worst %.2f)"), *Name, WorstFootDeg), WorstFootDeg < 1.5f);
	}
	return true;
}
