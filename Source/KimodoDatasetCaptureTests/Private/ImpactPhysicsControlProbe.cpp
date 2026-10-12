// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/AutomationTest.h"
#include "PhysicsControlComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

/**
 * Feasibility probe for impact_dataset_20261012, Phase 1.
 *
 * Answers, headlessly and before any generator code exists: on the GameAnimationSample
 * mannequin, can a Physics Control driven character (a) hold an animated pose, (b) react to a
 * chest impulse and recover while controls are on, and (c) collapse when controls are released?
 *
 * Findings that shaped this probe, and that the runner must honour:
 *  - The tick task manager runs each tick function at most once per GFrameCounter. Every
 *    simulated step must be its own frame, or actors, components and physics run once and stop.
 *  - A bare world has no game mode, so BeginPlay never reaches actors and components never
 *    register their ticks without WorldSettings->NotifyBeginPlay().
 *  - One world per process. A world per run crashed the Water world subsystem on the seventh
 *    world; teardown also needs EndPlay before destroy.
 *  - Targets are explicit. Physics Control's "use skeletal animation" path did not hold the
 *    character up in a headless world, so the source sequence is sampled directly and every
 *    control target is set by hand. That is also deterministic and needs no anim graph.
 *
 * Needs the GameAnimationSample content. In any other host project the assets are absent and the
 * probe skips with a warning rather than failing, because the plugin does not ship that content.
 */
namespace KimodoImpactProbe
{
	const TCHAR* MeshPath = TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin");
	const TCHAR* PhysicsAssetPath = TEXT("/Game/Characters/UEFN_Mannequin/Rigs/PA_UEFN_Mannequin.PA_UEFN_Mannequin");
	const TCHAR* RunAnimPath = TEXT("/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F.M_Neutral_Run_Loop_F");
	const TCHAR* IdleAnimPath = TEXT("/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop.M_Neutral_Stand_Idle_Loop");

	const FName PelvisBone(TEXT("pelvis"));
	const FName ChestBone(TEXT("spine_03"));
	const FName ModifierSet(TEXT("Modifiers"));
	const FName WorldSet(TEXT("World"));

	constexpr float StepSeconds = 1.0f / 60.0f;
	constexpr int32 PreRollFrames = 30;
	constexpr int32 ReactionFrames = 150;
	/** Frames over which world-space control strength returns from zero after a hit. */
	constexpr int32 RecoverFrames = 60;

	enum class EScenario : uint8
	{
		Baseline,   // controls on, no hit
		Controlled, // controls on, chest hit
		Ragdoll,    // controls released at the hit
	};

	struct FSettings
	{
		float WorldStrength = 8.0f;
		float ParentStrength = 16.0f;
		float ImpulseKgCmPerS = 0.0f;
		bool bRecordPose = false;
	};

	struct FTrace
	{
		TArray<FVector> Chest;
		TArray<FVector> TargetChest;
		TArray<float> PelvisHeight;
		TArray<FString> Notes;
		/** Per frame, per mesh bone, parent-space transforms ready to bake. Filled when recording. */
		TArray<TArray<FTransform>> LocalPoses;
		int32 BodyCount = 0;
		int32 ControlCount = 0;
		float SphereDrop = 0.0f;
	};

	/** Samples a sequence onto a mesh's reference skeleton in component space, carrying root motion across loops. */
	struct FPoseSampler
	{
		const FReferenceSkeleton* MeshRef = nullptr;
		const UAnimSequence* Anim = nullptr;
		TArray<int32> SkeletonIndex;
		double Length = 0.0;
		FTransform LoopDelta = FTransform::Identity;

		FPoseSampler(const USkeletalMesh* Mesh, const UAnimSequence* InAnim)
			: MeshRef(&Mesh->GetRefSkeleton()), Anim(InAnim), Length(InAnim->GetPlayLength())
		{
			const FReferenceSkeleton& SkeletonRef = InAnim->GetSkeleton()->GetReferenceSkeleton();
			for (int32 Bone = 0; Bone < MeshRef->GetNum(); ++Bone)
			{
				SkeletonIndex.Add(SkeletonRef.FindBoneIndex(MeshRef->GetBoneName(Bone)));
			}
			TArray<FTransform> Start, End;
			SampleLoop(0.0, Start);
			SampleLoop(Length, End);
			// Next loop's root = this loop's root re-based onto where the last loop ended.
			LoopDelta = Start[0].Inverse() * End[0];
		}

		void SampleLoop(double Time, TArray<FTransform>& OutComponentSpace) const
		{
			OutComponentSpace.SetNum(MeshRef->GetNum());
			const TArray<FTransform>& RefPose = MeshRef->GetRefBonePose();
			for (int32 Bone = 0; Bone < MeshRef->GetNum(); ++Bone)
			{
				FTransform Local = RefPose[Bone];
				if (SkeletonIndex[Bone] != INDEX_NONE)
				{
					Anim->GetBoneTransform(Local, FSkeletonPoseBoneIndex(SkeletonIndex[Bone]), Time, false);
				}
				const int32 Parent = MeshRef->GetParentIndex(Bone);
				OutComponentSpace[Bone] = Parent == INDEX_NONE ? Local : Local * OutComponentSpace[Parent];
			}
		}

		void Sample(double Time, TArray<FTransform>& OutComponentSpace) const
		{
			const int32 Loops = FMath::FloorToInt32(Time / Length);
			SampleLoop(Time - Loops * Length, OutComponentSpace);
			FTransform Offset = FTransform::Identity;
			for (int32 Loop = 0; Loop < Loops; ++Loop)
			{
				Offset = Offset * LoopDelta;
			}
			for (FTransform& Transform : OutComponentSpace)
			{
				Transform = Transform * Offset;
			}
		}
	};

	/**
	 * One world for every scenario; see the findings above for why it is never recreated per run.
	 */
	struct FProbeWorld
	{
		UWorld* World = nullptr;

		FProbeWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("KimodoImpactProbe"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			if (!World->HasBegunPlay())
			{
				World->GetWorldSettings()->NotifyBeginPlay();
			}

			// Floor: top surface at Z = 0.
			AActor* Floor = World->SpawnActor<AActor>();
			UBoxComponent* FloorBox = NewObject<UBoxComponent>(Floor);
			FloorBox->SetBoxExtent(FVector(5000.0, 5000.0, 10.0));
			FloorBox->SetCollisionProfileName(TEXT("BlockAll"));
			Floor->SetRootComponent(FloorBox);
			FloorBox->RegisterComponent();
			FloorBox->SetWorldLocation(FVector(0.0, 0.0, -10.0));
		}

		~FProbeWorld()
		{
			World->EndPlay(EEndPlayReason::Quit);
			World->BeginTearingDown();
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}

		void Step()
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, StepSeconds);
		}
	};

	FName ParentBodyBone(const FReferenceSkeleton& Ref, const UPhysicsAsset* PhysicsAsset, FName Bone)
	{
		for (int32 Index = Ref.GetParentIndex(Ref.FindBoneIndex(Bone)); Index != INDEX_NONE; Index = Ref.GetParentIndex(Index))
		{
			if (PhysicsAsset->FindBodyIndex(Ref.GetBoneName(Index)) != INDEX_NONE)
			{
				return Ref.GetBoneName(Index);
			}
		}
		return NAME_None;
	}

	FName WorldControlName(FName Bone) { return *FString::Printf(TEXT("W_%s"), *Bone.ToString()); }
	FName ParentControlName(FName Bone) { return *FString::Printf(TEXT("P_%s"), *Bone.ToString()); }
	FName ModifierName(FName Bone) { return *FString::Printf(TEXT("M_%s"), *Bone.ToString()); }

	FTrace Run(FProbeWorld& ProbeWorld, USkeletalMesh* Mesh, UPhysicsAsset* PhysicsAsset, const UAnimSequence* Anim, EScenario Scenario, const FSettings& Settings)
	{
		FTrace Trace;
		UWorld* World = ProbeWorld.World;
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		const FPoseSampler Sampler(Mesh, Anim);
		{
			TArray<FTransform> P0, P1;
			Sampler.SampleLoop(0.0, P0);
			Sampler.SampleLoop(Sampler.Length, P1);
			Trace.Notes.Add(FString::Printf(TEXT("sampler: length %.3f s, root@0 %s, root@end %s, pelvis@0 %s, loop delta %s"),
				Sampler.Length, *P0[0].GetLocation().ToString(), *P1[0].GetLocation().ToString(),
				*P0[Ref.FindBoneIndex(PelvisBone)].GetLocation().ToString(), *Sampler.LoopDelta.GetLocation().ToString()));
		}

		// Control body: if this does not fall, the fault is the world, not the character.
		AActor* Ball = World->SpawnActor<AActor>();
		USphereComponent* Sphere = NewObject<USphereComponent>(Ball);
		Sphere->SetSphereRadius(10.0f);
		Sphere->SetCollisionProfileName(TEXT("PhysicsActor"));
		Ball->SetRootComponent(Sphere);
		Sphere->RegisterComponent();
		Sphere->SetWorldLocation(FVector(500.0, 0.0, 300.0));
		Sphere->SetSimulatePhysics(true);

		// No anim instance: the mesh is posed only by physics, and targets come from the sampler.
		AActor* Character = World->SpawnActor<AActor>();
		USkeletalMeshComponent* MeshComponent = NewObject<USkeletalMeshComponent>(Character);
		MeshComponent->SetSkeletalMesh(Mesh);
		MeshComponent->SetPhysicsAsset(PhysicsAsset);
		MeshComponent->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Character->SetRootComponent(MeshComponent);
		MeshComponent->RegisterComponent();
		Trace.BodyCount = MeshComponent->Bodies.Num();

		UPhysicsControlComponent* Control = NewObject<UPhysicsControlComponent>(Character);
		Control->RegisterComponent();

		FPhysicsControlData WorldData;
		WorldData.LinearStrength = Settings.WorldStrength;
		WorldData.AngularStrength = Settings.WorldStrength;
		WorldData.bUseSkeletalAnimation = false;
		FPhysicsControlData ParentData;
		ParentData.AngularStrength = Settings.ParentStrength;
		ParentData.bUseSkeletalAnimation = false;
		FPhysicsControlModifierData ModifierData;
		ModifierData.MovementType = EPhysicsMovementType::Kinematic;
		ModifierData.KinematicTargetSpace = EPhysicsControlKinematicTargetSpace::World;

		struct FBodyBinding { FName Bone; int32 BoneIndex; FName ParentBone; int32 ParentIndex; };
		TArray<FBodyBinding> Bindings;
		for (const TObjectPtr<USkeletalBodySetup>& BodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			const FName Bone = BodySetup->BoneName;
			const FName Parent = ParentBodyBone(Ref, PhysicsAsset, Bone);
			Bindings.Add({ Bone, Ref.FindBoneIndex(Bone), Parent, Parent.IsNone() ? INDEX_NONE : Ref.FindBoneIndex(Parent) });

			Control->CreateNamedBodyModifier(ModifierName(Bone), MeshComponent, Bone, ModifierSet, ModifierData);
			Trace.ControlCount += Control->CreateNamedControl(WorldControlName(Bone), nullptr, NAME_None, MeshComponent, Bone, WorldData, FPhysicsControlTarget(), WorldSet) ? 1 : 0;
			if (!Parent.IsNone())
			{
				Trace.ControlCount += Control->CreateNamedControl(ParentControlName(Bone), MeshComponent, Parent, MeshComponent, Bone, ParentData, FPhysicsControlTarget(), NAME_None) ? 1 : 0;
			}
		}

		// Start every body exactly on the first sampled pose.
		TArray<FTransform> Pose;
		Sampler.Sample(0.0, Pose);
		for (const FBodyBinding& Binding : Bindings)
		{
			if (FBodyInstance* Body = MeshComponent->GetBodyInstance(Binding.Bone))
			{
				Body->SetBodyTransform(Pose[Binding.BoneIndex] * MeshComponent->GetComponentTransform(), ETeleportType::TeleportPhysics);
			}
		}

		// Targets live in a fixed frame captured at spawn. The mesh component follows its simulated
		// root body, so targets built from the live component transform chase the body itself and
		// the character drifts away (it floated upward at ~4 m/s before this).
		const FTransform ReferenceTM = MeshComponent->GetComponentTransform();
		const float SphereStartZ = float(Sphere->GetComponentLocation().Z);

		// Recorder lookup: body bones are read from their rigid bodies, every other bone (fingers,
		// twists, root) from the sampled source pose.
		TArray<FBodyInstance*> BodyByBone;
		BodyByBone.Init(nullptr, Ref.GetNum());
		for (const FBodyBinding& Binding : Bindings)
		{
			BodyByBone[Binding.BoneIndex] = MeshComponent->GetBodyInstance(Binding.Bone);
		}
		if (Settings.bRecordPose)
		{
			Trace.LocalPoses.Reserve(PreRollFrames + ReactionFrames);
		}
		TArray<FTransform> RecordedComponentSpace;
		RecordedComponentSpace.SetNum(Ref.GetNum());
		for (int32 Frame = 0; Frame < PreRollFrames + ReactionFrames; ++Frame)
		{
			Sampler.Sample(Frame * StepSeconds, Pose);
			for (const FBodyBinding& Binding : Bindings)
			{
				const FTransform Target = Pose[Binding.BoneIndex] * ReferenceTM;
				Control->SetBodyModifierKinematicTarget(ModifierName(Binding.Bone), Target.GetLocation(), Target.Rotator(), false);
				// bEnableControl false: setting a target must never re-enable a released control.
				Control->SetControlTargetPositionAndOrientation(WorldControlName(Binding.Bone), Target.GetLocation(), Target.Rotator(), StepSeconds, false, false, true, false);
				if (Binding.ParentIndex != INDEX_NONE)
				{
					const FTransform Relative = Pose[Binding.BoneIndex].GetRelativeTransform(Pose[Binding.ParentIndex]);
					Control->SetControlTargetPositionAndOrientation(ParentControlName(Binding.Bone), Relative.GetLocation(), Relative.Rotator(), StepSeconds, false, false, true, false);
				}
			}

			if (Frame == PreRollFrames)
			{
				// Pre-roll is the exact source motion; the reaction starts here.
				Control->SetBodyModifiersInSetMovementType(ModifierSet, EPhysicsMovementType::Simulated);
				if (Scenario == EScenario::Ragdoll)
				{
					Control->SetControlsInSetEnabled(TEXT("All"), false);
				}
			}
			constexpr int32 HitFrame = PreRollFrames + 1;
			if (Scenario == EScenario::Controlled && Frame >= HitFrame)
			{
				// Strength profile: world-space controls let go at the hit and blend back, so the
				// reaction is visible; parent-space controls keep the body's shape throughout.
				// At full strength the controls cancelled the hit within one frame.
				const float Alpha = FMath::SmoothStep(0.0f, 1.0f, float(Frame - HitFrame) / RecoverFrames);
				FPhysicsControlMultiplier Multiplier;
				Multiplier.LinearStrengthMultiplier = FVector(Alpha);
				Multiplier.AngularStrengthMultiplier = Alpha;
				Control->SetControlMultipliersInSet(WorldSet, Multiplier, false);
			}
			if (Frame == HitFrame && Scenario != EScenario::Baseline)
			{
				// One frame after the switch: an impulse on a body that is still kinematic is dropped,
				// and body modifiers only apply during the control component's own tick.
				// Struck from the front. The UEFN mannequin faces +Y, so the push is along -Y.
				MeshComponent->AddImpulse(ReferenceTM.TransformVectorNoScale(FVector(0.0, -Settings.ImpulseKgCmPerS, 0.0)), ChestBone);
				if (const FBodyInstance* ChestBody = MeshComponent->GetBodyInstance(ChestBone))
				{
					Trace.Notes.Add(FString::Printf(TEXT("chest simulating %d, mass %.1f kg, velocity right after AddImpulse %s"),
						ChestBody->IsInstanceSimulatingPhysics() ? 1 : 0, ChestBody->GetBodyMass(), *ChestBody->GetUnrealWorldVelocity().ToString()));
				}
			}

			ProbeWorld.Step();

			if (Settings.bRecordPose)
			{
				TArray<FTransform>& Local = Trace.LocalPoses.AddDefaulted_GetRef();
				Local.SetNum(Ref.GetNum());
				for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
				{
					const int32 Parent = Ref.GetParentIndex(Bone);
					if (BodyByBone[Bone])
					{
						RecordedComponentSpace[Bone] = BodyByBone[Bone]->GetUnrealWorldTransform().GetRelativeTransform(ReferenceTM);
					}
					else
					{
						const FTransform Sampled = Parent == INDEX_NONE ? Pose[Bone] : Pose[Bone].GetRelativeTransform(Pose[Parent]);
						RecordedComponentSpace[Bone] = Parent == INDEX_NONE ? Sampled : Sampled * RecordedComponentSpace[Parent];
					}
					Local[Bone] = Parent == INDEX_NONE ? RecordedComponentSpace[Bone] : RecordedComponentSpace[Bone].GetRelativeTransform(RecordedComponentSpace[Parent]);
				}
			}

			if (Scenario != EScenario::Baseline && Frame >= PreRollFrames + 1 && Frame <= PreRollFrames + 3)
			{
				if (const FBodyInstance* ChestBody = MeshComponent->GetBodyInstance(ChestBone))
				{
					Trace.Notes.Add(FString::Printf(TEXT("frame %d after step: chest velocity %s"), Frame, *ChestBody->GetUnrealWorldVelocity().ToString()));
				}
			}

			if (Frame >= PreRollFrames)
			{
				const FBodyInstance* ChestBody = MeshComponent->GetBodyInstance(ChestBone);
				const FBodyInstance* PelvisBody = MeshComponent->GetBodyInstance(PelvisBone);
				Trace.Chest.Add(ChestBody ? ChestBody->GetUnrealWorldTransform().GetLocation() : FVector::ZeroVector);
				Trace.PelvisHeight.Add(PelvisBody ? float(PelvisBody->GetUnrealWorldTransform().GetLocation().Z) : 0.0f);
				TArray<FTransform> Next;
				Sampler.Sample((Frame + 1) * StepSeconds, Next);
				Trace.TargetChest.Add((Next[Ref.FindBoneIndex(ChestBone)] * ReferenceTM).GetLocation());
			}
		}

		{
			FString Heights;
			for (int32 Index = 0; Index < Trace.PelvisHeight.Num(); Index += 10)
			{
				Heights += FString::Printf(TEXT("%.0f "), Trace.PelvisHeight[Index]);
			}
			Trace.Notes.Add(FString::Printf(TEXT("pelvis every 10 frames: %s"), *Heights));
		}
		Trace.SphereDrop = SphereStartZ - float(Sphere->GetComponentLocation().Z);
		Ball->Destroy();
		Character->Destroy();
		return Trace;
	}

	/** Largest and final distance between the chest body and where the animation wants it. */
	void TrackingError(const FTrace& Trace, float& OutMax, float& OutFinal)
	{
		OutMax = 0.0f;
		OutFinal = 0.0f;
		for (int32 Index = 0; Index < Trace.Chest.Num(); ++Index)
		{
			const float Error = float(FVector::Dist(Trace.Chest[Index], Trace.TargetChest[Index]));
			OutMax = FMath::Max(OutMax, Error);
			OutFinal = Error;
		}
	}
}

namespace KimodoImpactProbe
{
	/** Bakes recorded parent-space poses to an AnimSequence and saves it as a real package file. Returns bytes written. */
	int64 BakeAndSave(USkeletalMesh* Mesh, const TArray<TArray<FTransform>>& Frames, const FString& Name, double& OutBakeSeconds, double& OutSaveSeconds)
	{
		const double BakeStart = FPlatformTime::Seconds();
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/KimodoImpactSpike/%s"), *Name));
		UAnimSequence* Sequence = NewObject<UAnimSequence>(Package, *Name, RF_Public | RF_Standalone);
		Sequence->SetSkeleton(Mesh->GetSkeleton());

		IAnimationDataController& Controller = Sequence->GetController();
		Controller.InitializeModel();
		Controller.OpenBracket(INVTEXT("Bake impact clip"), false);
		Controller.SetFrameRate(FFrameRate(60, 1), false);
		Controller.SetNumberOfFrames(FFrameNumber(Frames.Num() - 1), false);

		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		TArray<FVector3f> Positions;
		TArray<FQuat4f> Rotations;
		TArray<FVector3f> Scales;
		for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
		{
			Positions.Reset(Frames.Num());
			Rotations.Reset(Frames.Num());
			Scales.Reset(Frames.Num());
			for (const TArray<FTransform>& Frame : Frames)
			{
				Positions.Add(FVector3f(Frame[Bone].GetLocation()));
				Rotations.Add(FQuat4f(Frame[Bone].GetRotation()));
				Scales.Add(FVector3f(Frame[Bone].GetScale3D()));
			}
			const FName BoneName = Ref.GetBoneName(Bone);
			Controller.AddBoneCurve(BoneName, false);
			Controller.SetBoneTrackKeys(BoneName, Positions, Rotations, Scales, false);
		}
		Controller.NotifyPopulated();
		Controller.CloseBracket(false);
		OutBakeSeconds = FPlatformTime::Seconds() - BakeStart;

		// Saved/, never Content/: a spike must not add assets to the project.
		const FString File = FPaths::ProjectSavedDir() / TEXT("KimodoImpactSpike") / (Name + FPackageName::GetAssetPackageExtension());
		const double SaveStart = FPlatformTime::Seconds();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const bool bSaved = UPackage::SavePackage(Package, Sequence, *File, Args);
		OutSaveSeconds = FPlatformTime::Seconds() - SaveStart;

		Sequence->ClearFlags(RF_Public | RF_Standalone);
		Package->MarkAsGarbage();
		return bSaved ? IFileManager::Get().FileSize(*File) : -1;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKimodoImpactThroughputProbe,
	"KimodoDatasetCapture.Impact.Spike.ThroughputProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKimodoImpactThroughputProbe::RunTest(const FString& Parameters)
{
	using namespace KimodoImpactProbe;

	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, MeshPath);
	UPhysicsAsset* PhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, PhysicsAssetPath);
	UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, IdleAnimPath);
	UAnimSequence* RunAnim = LoadObject<UAnimSequence>(nullptr, RunAnimPath);
	if (!Mesh || !PhysicsAsset || !Idle || !RunAnim)
	{
		AddWarning(TEXT("GameAnimationSample content not found; probe skipped. Run it in GameAnimationSample."));
		return true;
	}

	constexpr int32 ClipCount = 20;
	FProbeWorld ProbeWorld;
	FSettings Settings;
	Settings.ImpulseKgCmPerS = 30000.0f;
	Settings.bRecordPose = true;

	double SimSeconds = 0.0, BakeSeconds = 0.0, SaveSeconds = 0.0;
	int64 Bytes = 0;
	int32 Frames = 0;
	for (int32 Clip = 0; Clip < ClipCount; ++Clip)
	{
		const double SimStart = FPlatformTime::Seconds();
		const FTrace Trace = Run(ProbeWorld, Mesh, PhysicsAsset, (Clip % 2) ? RunAnim : Idle, EScenario::Controlled, Settings);
		SimSeconds += FPlatformTime::Seconds() - SimStart;
		Frames += Trace.LocalPoses.Num();

		double Bake = 0.0, Save = 0.0;
		const int64 Size = BakeAndSave(Mesh, Trace.LocalPoses, FString::Printf(TEXT("SpikeClip_%02d"), Clip), Bake, Save);
		TestTrue(*FString::Printf(TEXT("Clip %d saved"), Clip), Size > 0);
		BakeSeconds += Bake;
		SaveSeconds += Save;
		Bytes += FMath::Max<int64>(Size, 0);
	}
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	const double ClipSeconds = (SimSeconds + BakeSeconds + SaveSeconds) / ClipCount;
	AddInfo(FString::Printf(TEXT("Throughput: %d clips, %d frames (%.1f s of motion), %d bones"),
		ClipCount, Frames, Frames * StepSeconds, Mesh->GetRefSkeleton().GetNum()));
	AddInfo(FString::Printf(TEXT("Throughput: sim %.2f ms/step (%.1fx real time), bake %.1f ms/clip, save %.1f ms/clip"),
		1000.0 * SimSeconds / Frames, (Frames * StepSeconds) / SimSeconds, 1000.0 * BakeSeconds / ClipCount, 1000.0 * SaveSeconds / ClipCount));
	AddInfo(FString::Printf(TEXT("Throughput: %.0f ms per clip end to end -> %.0f clips/hour/process, %.0f KB per clip on disk"),
		1000.0 * ClipSeconds, 3600.0 / ClipSeconds, Bytes / 1024.0 / ClipCount));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKimodoImpactPhysicsControlProbe,
	"KimodoDatasetCapture.Impact.Spike.PhysicsControlProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKimodoImpactPhysicsControlProbe::RunTest(const FString& Parameters)
{
	using namespace KimodoImpactProbe;

	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, MeshPath);
	UPhysicsAsset* PhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, PhysicsAssetPath);
	UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, IdleAnimPath);
	UAnimSequence* RunAnim = LoadObject<UAnimSequence>(nullptr, RunAnimPath);
	if (!Mesh || !PhysicsAsset || !Idle || !RunAnim)
	{
		AddWarning(TEXT("GameAnimationSample content not found; probe skipped. Run it in GameAnimationSample."));
		return true;
	}

	FProbeWorld ProbeWorld;

	for (UAnimSequence* Anim : { Idle, RunAnim })
	{
		for (const float Strength : { 4.0f, 8.0f, 16.0f })
		{
			FSettings Settings;
			Settings.WorldStrength = Strength;
			Settings.ParentStrength = Strength * 2.0f;
			const FTrace Trace = Run(ProbeWorld, Mesh, PhysicsAsset, Anim, EScenario::Baseline, Settings);
			float MaxError, FinalError;
			TrackingError(Trace, MaxError, FinalError);
			AddInfo(FString::Printf(TEXT("Sweep: %s strength %.0f -> pelvis min %.1f max %.1f cm, chest tracking error max %.1f final %.1f cm, sphere drop %.0f cm"),
				*Anim->GetName(), Strength, FMath::Min(Trace.PelvisHeight), FMath::Max(Trace.PelvisHeight), MaxError, FinalError, Trace.SphereDrop));
		}
	}

	FSettings Settings;
	Settings.ImpulseKgCmPerS = 30000.0f;
	for (UAnimSequence* Anim : { Idle, RunAnim })
	{
		const FTrace Baseline = Run(ProbeWorld, Mesh, PhysicsAsset, Anim, EScenario::Baseline, Settings);
		const FTrace Hit = Run(ProbeWorld, Mesh, PhysicsAsset, Anim, EScenario::Controlled, Settings);
		const FTrace Ragdoll = Run(ProbeWorld, Mesh, PhysicsAsset, Anim, EScenario::Ragdoll, Settings);

		float BaselineMax, BaselineFinal, HitMax, HitFinal;
		TrackingError(Baseline, BaselineMax, BaselineFinal);
		TrackingError(Hit, HitMax, HitFinal);
		const FString Name = Anim->GetName();

		AddInfo(FString::Printf(TEXT("%s: bodies %d, controls %d"), *Name, Baseline.BodyCount, Baseline.ControlCount));
		for (const FString& Note : Baseline.Notes) { AddInfo(FString::Printf(TEXT("%s baseline %s"), *Name, *Note)); }
		for (const FString& Note : Hit.Notes) { AddInfo(FString::Printf(TEXT("%s hit %s"), *Name, *Note)); }
		AddInfo(FString::Printf(TEXT("%s baseline: tracking error max %.1f cm, pelvis min %.1f cm"), *Name, BaselineMax, FMath::Min(Baseline.PelvisHeight)));
		AddInfo(FString::Printf(TEXT("%s hit: tracking error max %.1f final %.1f cm, pelvis min %.1f cm"), *Name, HitMax, HitFinal, FMath::Min(Hit.PelvisHeight)));
		AddInfo(FString::Printf(TEXT("%s ragdoll: final pelvis %.1f cm"), *Name, Ragdoll.PelvisHeight.Last()));

		TestTrue(*FString::Printf(TEXT("%s: controls created for every body"), *Name), Baseline.ControlCount >= Baseline.BodyCount);
		TestTrue(*FString::Printf(TEXT("%s: baseline tracks the animation"), *Name), BaselineMax < 15.0f);
		TestTrue(*FString::Printf(TEXT("%s: hit visibly displaces the chest"), *Name), HitMax > BaselineMax + 10.0f);
		TestTrue(*FString::Printf(TEXT("%s: hit recovers toward the animation"), *Name), HitFinal < HitMax * 0.5f);
		TestTrue(*FString::Printf(TEXT("%s: released character collapses"), *Name), Ragdoll.PelvisHeight.Last() < 40.0f);
	}

	return true;
}
