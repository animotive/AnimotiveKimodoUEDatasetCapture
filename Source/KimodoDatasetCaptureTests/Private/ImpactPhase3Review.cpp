// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "ImpactClipSimulator.h"
#include "ImpactParams.h"
#include "ImpactRandom.h"
#include "ImpactRegionTable.h"
#include "ImpactSchedule.h"
#include "ImpactSimWorld.h"
#include "ImpactStrengthProfile.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "TwoBoneIK.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"

/**
 * Phase 3 review: runs the production clip simulator on hand-picked scenarios and bakes each clip
 * for a person to watch. The recording-to-bones step here is the spike's approved recorder (body
 * rotations, positions rebuilt from bone lengths, leg IK); Phase 4 moves it into the module with
 * its own tests.
 */
namespace ImpactPhase3Review
{
	const TCHAR* Folder = TEXT("KimodoImpactReviewP3");

	struct FScenario
	{
		const TCHAR* Name;
		const TCHAR* Source;
		const TCHAR* Impactor;
		EImpactBodyRegion Region;
		/** Direction of travel in character space (X forward, Y right). */
		FVector Direction;
		float Phase;
	};

	TArray<FTransform> ToLocal(const FReferenceSkeleton& Ref, const FImpactClipRecording& Recording, int32 FrameIndex, bool bMoving)
	{
		const FImpactRecordedFrame& Frame = Recording.Frames[FrameIndex];
		const TArrayView<const FTransform> Bodies = Recording.BodyPoses.GetFrame(FrameIndex);
		const TArrayView<const FTransform> Source = Recording.SourcePoses.GetFrame(FrameIndex);
		const int32 Bones = Ref.GetNum();
		TArray<int32> BodyOf;
		BodyOf.Init(INDEX_NONE, Bones);
		for (int32 Body = 0; Body < Recording.BodyBoneIndices.Num(); ++Body)
		{
			BodyOf[Recording.BodyBoneIndices[Body]] = Body;
		}

		TArray<FTransform> Component;
		Component.SetNum(Bones);
		for (int32 Bone = 0; Bone < Bones; ++Bone)
		{
			const int32 Parent = Ref.GetParentIndex(Bone);
			if (Parent == INDEX_NONE)
			{
				Component[Bone] = Frame.Root;
				continue;
			}
			const FTransform SourceLocal = Source[Bone].GetRelativeTransform(Source[Parent]);
			if (BodyOf[Bone] != INDEX_NONE)
			{
				const FTransform Physical = Bodies[BodyOf[Bone]].GetRelativeTransform(Recording.Reference);
				const bool bParentBody = BodyOf[Parent] != INDEX_NONE;
				// Rotation from physics; position from the bone's length below its parent, except the
				// first body under the root, which keeps its simulated position.
				Component[Bone] = bParentBody
					? FTransform(Physical.GetRotation(), Component[Parent].TransformPosition(SourceLocal.GetTranslation()))
					: Physical;
			}
			else
			{
				Component[Bone] = SourceLocal * Component[Parent];
			}
		}

		// Leg IK: planner-placed feet land exactly; on a moving source, feet the source has on the
		// ground go to its ground height.
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const TCHAR* Suffix = Side == 0 ? TEXT("_l") : TEXT("_r");
			const int32 Thigh = Ref.FindBoneIndex(*(FString(TEXT("thigh")) + Suffix));
			const int32 Calf = Ref.FindBoneIndex(*(FString(TEXT("calf")) + Suffix));
			const int32 Foot = Ref.FindBoneIndex(*(FString(TEXT("foot")) + Suffix));
			if (Thigh == INDEX_NONE || Calf == INDEX_NONE || Foot == INDEX_NONE || Frame.bReleased)
			{
				continue;
			}
			FVector Effector;
			if (Frame.bFootPlaced[Side])
			{
				Effector = Recording.Reference.InverseTransformPosition(Frame.FootTarget[Side]);
			}
			else if (bMoving && Source[Foot].GetLocation().Z < 12.0)
			{
				const FVector Current = Component[Foot].GetLocation();
				Effector = FVector(Current.X, Current.Y, Source[Foot].GetLocation().Z);
			}
			else
			{
				continue;
			}
			FTransform ThighCS = Component[Thigh], CalfCS = Component[Calf], FootCS = Component[Foot];
			const FQuat FootRotation = FootCS.GetRotation();
			const FVector Pole = CalfCS.GetLocation() + FVector(0.0, 30.0, 0.0);
			const double Upper = (CalfCS.GetLocation() - ThighCS.GetLocation()).Size();
			const double Lower = (FootCS.GetLocation() - CalfCS.GetLocation()).Size();
			AnimationCore::SolveTwoBoneIK(ThighCS, CalfCS, FootCS, Pole, Effector, Upper, Lower, false, 1.0, 1.0);
			FootCS.SetRotation(FootRotation);
			Component[Thigh] = ThighCS;
			Component[Calf] = CalfCS;
			Component[Foot] = FootCS;
			for (int32 Child = Foot + 1; Child < Bones; ++Child)
			{
				const int32 ChildParent = Ref.GetParentIndex(Child);
				if (ChildParent == Foot || (ChildParent > Foot && Ref.BoneIsChildOf(ChildParent, Foot)))
				{
					Component[Child] = Source[Child].GetRelativeTransform(Source[ChildParent]) * Component[ChildParent];
				}
			}
		}

		TArray<FTransform> Local;
		Local.SetNum(Bones);
		for (int32 Bone = 0; Bone < Bones; ++Bone)
		{
			const int32 Parent = Ref.GetParentIndex(Bone);
			Local[Bone] = Parent == INDEX_NONE ? Component[Bone] : Component[Bone].GetRelativeTransform(Component[Parent]);
		}
		return Local;
	}

	bool Bake(USkeletalMesh* Mesh, const TArray<TArray<FTransform>>& Frames, const FString& Name)
	{
		const FString PackageName = FString::Printf(TEXT("/Game/%s/%s"), Folder, *Name);
		if (FPackageName::DoesPackageExist(PackageName))
		{
			// A review copy is already in Content. Never collide with it, and never skip silently:
			// a skipped bake leaves an old clip looking current. Remove the copy first.
			UE_LOG(LogTemp, Error, TEXT("%s already exists in Content; delete it to re-bake."), *PackageName);
			return false;
		}
		UPackage* Package = CreatePackage(*PackageName);
		UAnimSequence* Sequence = NewObject<UAnimSequence>(Package, *Name, RF_Public | RF_Standalone);
		Sequence->SetSkeleton(Mesh->GetSkeleton());
		IAnimationDataController& Controller = Sequence->GetController();
		Controller.InitializeModel();
		Controller.OpenBracket(INVTEXT("Bake review clip"), false);
		Controller.SetFrameRate(FFrameRate(60, 1), false);
		Controller.SetNumberOfFrames(FFrameNumber(Frames.Num() - 1), false);
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		TArray<FVector3f> P; TArray<FQuat4f> R; TArray<FVector3f> S;
		for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
		{
			P.Reset(); R.Reset(); S.Reset();
			for (const TArray<FTransform>& Frame : Frames)
			{
				P.Add(FVector3f(Frame[Bone].GetLocation()));
				R.Add(FQuat4f(Frame[Bone].GetRotation()));
				S.Add(FVector3f(Frame[Bone].GetScale3D()));
			}
			Controller.AddBoneCurve(Ref.GetBoneName(Bone), false);
			Controller.SetBoneTrackKeys(Ref.GetBoneName(Bone), P, R, S, false);
		}
		Controller.NotifyPopulated();
		Controller.CloseBracket(false);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const FString File = FPaths::ProjectSavedDir() / Folder / (Name + FPackageName::GetAssetPackageExtension());
		const bool bSaved = UPackage::SavePackage(Package, Sequence, *File, Args);
		Sequence->ClearFlags(RF_Public | RF_Standalone);
		Package->MarkAsGarbage();
		return bSaved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactPhase3Review,
	"KimodoDatasetCapture.Impact.Review.Phase3",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactPhase3Review::RunTest(const FString& Parameters)
{
	using namespace ImpactPhase3Review;
	const FString Anims = TEXT("/Game/Characters/UEFN_Mannequin/Animations/");
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin"));
	UPhysicsAsset* Physics = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Rigs/PA_UEFN_Mannequin.PA_UEFN_Mannequin"));
	if (!Mesh || !Physics)
	{
		AddWarning(TEXT("GameAnimationSample content not found; Phase 3 review skipped."));
		return true;
	}

	// Held strongly: simulation ticks can collect unreferenced objects mid-run (the shotgun scenario
	// lost its catalog that way).
	TStrongObjectPtr<UImpactorCatalog> Catalog(NewObject<UImpactorCatalog>());
	Catalog->FillWithStarterArchetypes();
	TStrongObjectPtr<UImpactRegionTable> Regions(NewObject<UImpactRegionTable>());
	Regions->FillWithUEFNMannequinEntries();

	const FVector FromFront(-1, 0, 0);
	const FVector FromLeft(0, 1, 0);
	const FScenario Scenarios[] = {
		{ TEXT("Idle_Punch_Chest"), TEXT("Idle/M_Neutral_Stand_Idle_Loop"), TEXT("Punch"), EImpactBodyRegion::Chest, FromFront, 0.2f },
		{ TEXT("Idle_Shove_Chest"), TEXT("Idle/M_Neutral_Stand_Idle_Loop"), TEXT("Shove"), EImpactBodyRegion::Chest, FromFront, 0.2f },
		{ TEXT("Idle_SmallBullet_Head"), TEXT("Idle/M_Neutral_Stand_Idle_Loop"), TEXT("SmallBullet"), EImpactBodyRegion::Head, FromFront, 0.2f },
		{ TEXT("Run_SmallBullet_FootLeft"), TEXT("Run/M_Neutral_Run_Loop_F"), TEXT("SmallBullet"), EImpactBodyRegion::FootLeft, FromFront, 0.3f },
		{ TEXT("Run_Punch_Chest"), TEXT("Run/M_Neutral_Run_Loop_F"), TEXT("Punch"), EImpactBodyRegion::Chest, FromFront, 0.3f },
		{ TEXT("Idle_Car_Pelvis"), TEXT("Idle/M_Neutral_Stand_Idle_Loop"), TEXT("CarBumper"), EImpactBodyRegion::Pelvis, FromLeft, 0.2f },
		{ TEXT("Run_Baseball_Chest"), TEXT("Run/M_Neutral_Run_Loop_F"), TEXT("Baseball"), EImpactBodyRegion::Chest, FromFront, 0.3f },
		{ TEXT("Idle_ShotgunBlast_Abdomen"), TEXT("Idle/M_Neutral_Stand_Idle_Loop"), TEXT("ShotgunBlast"), EImpactBodyRegion::Abdomen, FromFront, 0.2f },
	};

	FImpactSimWorld World;
	TArray<FString> Summary;
	int32 Index = 0;
	for (const FScenario& Scenario : Scenarios)
	{
		UAnimSequence* Source = LoadObject<UAnimSequence>(nullptr, *(Anims + Scenario.Source + TEXT(".") + FPaths::GetBaseFilename(Scenario.Source)));
		const FImpactorArchetype* Archetype = Catalog->Archetypes.FindByPredicate([&](const FImpactorArchetype& A) { return A.Id == FName(Scenario.Impactor); });
		if (!TestNotNull(*FString::Printf(TEXT("%s source"), Scenario.Name), Source) || !TestNotNull(*FString::Printf(TEXT("%s impactor"), Scenario.Name), Archetype))
		{
			continue;
		}

		FImpactClipSetup Setup;
		Setup.Mesh = Mesh;
		Setup.PhysicsAsset = Physics;
		Setup.Source = Source;
		Setup.RegionTable = Regions.Get();
		Setup.BatchSeed = 3;
		Setup.ClipIndex = Index++;

		FImpactParams& Params = Setup.Params;
		Params.Impactor.Archetype = Archetype;
		Params.Impactor.MassKg = (Archetype->MassKg.Min + Archetype->MassKg.Max) * 0.5f;
		Params.Impactor.SpeedMetresPerSecond = (Archetype->SpeedMetresPerSecond.Min + Archetype->SpeedMetresPerSecond.Max) * 0.5f;
		Params.Impactor.ContactRadiusCm = (Archetype->ContactRadiusCm.Min + Archetype->ContactRadiusCm.Max) * 0.5f;
		Params.TransferredImpulseKgCmPerSecond = FImpactParamsSampler::TransferredImpulse(Params.Impactor.MassKg, Params.Impactor.SpeedMetresPerSecond, 75.0f, 0.2f);
		Params.ReactionGain = Archetype->ReactionGain;
		Params.ReactionImpulseKgCmPerSecond = Params.TransferredImpulseKgCmPerSecond * Params.ReactionGain;
		Params.Intensity = FImpactIntensityThresholds().Classify(Params.ReactionImpulseKgCmPerSecond);
		Params.Region = Scenario.Region;
		Params.DirectionLocal = Scenario.Direction.GetSafeNormal();
		Params.SourcePhase = Scenario.Phase;

		FText Reason;
		TestTrue(*FString::Printf(TEXT("%s schedule"), Scenario.Name), FImpactScheduleSampler::Sample(Params, FImpactDeliveryDistributions(), 3, Setup.ClipIndex, Setup.Schedule, Reason));

		FImpactClipRecording Recording;
		if (!TestTrue(*FString::Printf(TEXT("%s simulates: %s"), Scenario.Name, *Reason.ToString()), FImpactClipSimulator::Run(World, Setup, Recording, Reason)))
		{
			continue;
		}

		TArray<TArray<FTransform>> Frames;
		for (int32 FrameIndex = 0; FrameIndex < Recording.Frames.Num(); ++FrameIndex)
		{
			Frames.Add(ToLocal(Mesh->GetRefSkeleton(), Recording, FrameIndex, Recording.bMovingSource));
		}
		TestTrue(*FString::Printf(TEXT("%s baked"), Scenario.Name), Bake(Mesh, Frames, FString::Printf(TEXT("P3_%s"), Scenario.Name)));

		const FString Line = FString::Printf(TEXT("%s: impulse %.0f (%s, %s), outcome %s at +%d, steps %d, trip %d, overwhelmed %d, end %s%s, %d frames, worst joint %.1f deg past limit (%s)"),
			Scenario.Name, Params.ReactionImpulseKgCmPerSecond,
			*StaticEnum<EImpactIntensity>()->GetNameStringByValue(int64(Params.Intensity)),
			*StaticEnum<EImpactDeliveryMode>()->GetNameStringByValue(int64(Setup.Schedule.Mode)),
			*StaticEnum<EImpactOutcome>()->GetNameStringByValue(int64(Recording.Outcome.Outcome)), Recording.Outcome.OutcomeFrame,
			Recording.Outcome.Steps, Recording.Outcome.bTrip ? 1 : 0, Recording.bOverwhelmed ? 1 : 0,
			*StaticEnum<EImpactClipEnd>()->GetNameStringByValue(int64(Recording.End)),
			Recording.InvalidReason.IsEmpty() ? TEXT("") : *(TEXT(" (") + Recording.InvalidReason.ToString() + TEXT(")")),
			Recording.Frames.Num(), Recording.MaxJointLimitExcessDeg, *Recording.MaxJointLimitExcessBody.ToString());
		Summary.Add(Line);
		AddInfo(Line);
	}
	FFileHelper::SaveStringToFile(FString::Join(Summary, TEXT("\n")), *(FPaths::ProjectSavedDir() / Folder / TEXT("Summary.txt")));
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	return true;
}
