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
#include "TwoBoneIK.h"
#include "Misc/FileHelper.h"
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
		/** Capture-point stepping balance layer (spike). */
		bool bStepping = false;
	};

	struct FTrace
	{
		TArray<FVector> Chest;
		TArray<FVector> TargetChest;
		TArray<float> PelvisHeight;
		TArray<FString> Notes;
		/** Per frame, per mesh bone, parent-space transforms ready to bake. Filled when recording. */
		TArray<TArray<FTransform>> LocalPoses;
		/** Source animation's parent-space pose on the same frames, for comparison. */
		TArray<TArray<FTransform>> SourceLocalPoses;
		/** Component-space recorded and source poses on the same frames. */
		TArray<TArray<FTransform>> RecordedComponentPoses;
		TArray<TArray<FTransform>> SourceComponentPoses;
		/** Per-frame foot diagnostics as CSV rows (spike). */
		TArray<FString> FootCsv;
		/** Largest raw physics separation between a calf and its foot (spike diagnostic). */
		float MaxAnkleGapCm = 0.0f;
		/** Per recorded frame: raw physics calf_l parent-space rotation deviation from source (deg). */
		TArray<float> RawCalfDeviation;
		int32 Steps = 0;
		bool bFell = false;
		FVector FinalOffset = FVector::ZeroVector;
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

		// Projection pulls a separated joint back together each step instead of letting it stretch.
		for (FConstraintInstance* Constraint : MeshComponent->Constraints)
		{
			if (Constraint)
			{
				Constraint->SetProjectionParams(true, 1.0f, 1.0f, 1.0f, 5.0f);
			}
		}

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

		// Body-tree distance from the contact body, for local release around the hit.
		TArray<int32> ContactDistance;
		ContactDistance.Init(MAX_int32, Bindings.Num());
		{
			auto IndexOf = [&Bindings](FName Bone) { return Bindings.IndexOfByPredicate([Bone](const FBodyBinding& B) { return B.Bone == Bone; }); };
			TArray<int32> Queue;
			const int32 Contact = IndexOf(ChestBone);
			if (Contact != INDEX_NONE)
			{
				ContactDistance[Contact] = 0;
				Queue.Add(Contact);
			}
			for (int32 Head = 0; Head < Queue.Num(); ++Head)
			{
				const int32 Current = Queue[Head];
				for (int32 Other = 0; Other < Bindings.Num(); ++Other)
				{
					const bool bAdjacent = Bindings[Other].ParentBone == Bindings[Current].Bone || Bindings[Current].ParentBone == Bindings[Other].Bone;
					if (bAdjacent && ContactDistance[Other] == MAX_int32)
					{
						ContactDistance[Other] = ContactDistance[Current] + 1;
						Queue.Add(Other);
					}
				}
			}
		}
		auto IsLeg = [](FName Bone)
		{
			const FString Name = Bone.ToString();
			return Name.StartsWith(TEXT("thigh")) || Name.StartsWith(TEXT("calf")) || Name.StartsWith(TEXT("foot")) || Name.StartsWith(TEXT("ball"));
		};

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

		// ---- Capture-point stepping (spike) ----------------------------------------------------
		// Offset: horizontal displacement of the whole target frame, driven by the body's excess
		// momentum, so a push moves the character instead of springing it back.
		// Feet: after the hit, each foot is pinned where it stood (foot IK) until the planner
		// steps it along an arc to the capture point.
		const int32 HitFrameIndex = PreRollFrames + 1;
		constexpr int32 SwingFrames = 15;
		constexpr float SwingLiftCm = 15.0f;
		constexpr float StepTriggerCm = 15.0f;
		constexpr float MaxStepCm = 70.0f;
		constexpr float StanceHalfWidthCm = 11.0f;
		constexpr int32 SettleHoldFrames = 20;
		constexpr int32 SettleBlendFrames = 20;
		struct FFoot
		{
			int32 Binding = INDEX_NONE;
			bool bPinned = false;
			FVector Planted = FVector::ZeroVector;
			bool bSwing = false;
			FVector SwingFrom = FVector::ZeroVector;
			FVector SwingTo = FVector::ZeroVector;
			int32 SwingStart = 0;
			FVector Current = FVector::ZeroVector;
		};
		FFoot Feet[2];
		FVector FootIKTarget[2] = { FVector::ZeroVector, FVector::ZeroVector };
		// Lowest source foot height per side over one loop: the source's ground level for each foot.
		float SourceFootGroundZ[2] = { MAX_flt, MAX_flt };
		{
			TArray<FTransform> Probe;
			for (int32 Sample = 0; Sample <= 60; ++Sample)
			{
				Sampler.SampleLoop(Sampler.Length * Sample / 60.0, Probe);
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const int32 FootIndex = Ref.FindBoneIndex(Side == 0 ? TEXT("foot_l") : TEXT("foot_r"));
					if (FootIndex != INDEX_NONE)
					{
						SourceFootGroundZ[Side] = FMath::Min(SourceFootGroundZ[Side], float(Probe[FootIndex].GetLocation().Z));
					}
				}
			}
		}
		bool bFootIK[2] = { false, false };
		Feet[0].Binding = Bindings.IndexOfByPredicate([](const FBodyBinding& B) { return B.Bone == FName(TEXT("foot_l")); });
		Feet[1].Binding = Bindings.IndexOfByPredicate([](const FBodyBinding& B) { return B.Bone == FName(TEXT("foot_r")); });
		FVector Offset = FVector::ZeroVector;
		FVector FootingAtHit = FVector::ZeroVector;
		FVector SmoothedExcess = FVector::ZeroVector;
		int32 UncaughtSteps = 0;
		int32 LastSwingSide = INDEX_NONE;
		int32 LastLandFrame = INDEX_NONE;
		constexpr int32 MinStanceFrames = 8;
		constexpr int32 MaxSteps = 5;
		bool bFeetReleased = false;
		constexpr int32 MaxUncaughtSteps = 3;
		constexpr float FallComHeightCm = 55.0f;
		int32 CalmFrames = 0;
		int32 SettleStart = INDEX_NONE;
		// Classified once from the source's own (unwarped) root speed. Re-deciding every frame from
		// the warped speed reclassified a slowed run as idle mid-stumble: its feet were pinned
		// mid-stride and leg IK wrenched the legs up to 136 degrees, leaving the feet floating.
		const bool bMovingSource = Sampler.LoopDelta.GetLocation().Size2D() / FMath::Max(Sampler.Length, UE_SMALL_NUMBER) > 100.0;
		const bool bPlanner = Settings.bStepping && Scenario == EScenario::Controlled && Feet[0].Binding != INDEX_NONE && Feet[1].Binding != INDEX_NONE;
		TArray<FTransform> NextPose;

		// Source clock. A hit against the direction of travel slows the source playback (and its
		// root motion) in proportion to the momentum it removed, then recovers.
		double SourceTime = 0.0;
		float PlaybackDrop = 0.0f;
		constexpr int32 PlaybackRecoverFrames = 48;
		auto PlaybackRate = [&](int32 Frame)
		{
			if (Frame <= HitFrameIndex + 2)
			{
				return 1.0f;
			}
			const float Recover = FMath::SmoothStep(0.0f, 1.0f, float(Frame - (HitFrameIndex + 2)) / PlaybackRecoverFrames);
			return 1.0f - PlaybackDrop * (1.0f - Recover);
		};
		// Leg multipliers are smoothed so strength never switches in a single frame.
		TArray<float> LegWorldScale;
		LegWorldScale.Init(1.0f, Bindings.Num());

		for (int32 Frame = 0; Frame < PreRollFrames + ReactionFrames; ++Frame)
		{
			Sampler.Sample(SourceTime, Pose);

			if (bPlanner && Frame >= HitFrameIndex && !Trace.bFell)
			{
				// Source pelvis velocity, for the momentum the animation itself carries.
				Sampler.Sample(SourceTime + StepSeconds * PlaybackRate(Frame), NextPose);
				const int32 PelvisIndex = Ref.FindBoneIndex(PelvisBone);
				const FVector SourceVelocity = ReferenceTM.TransformVectorNoScale(NextPose[PelvisIndex].GetLocation() - Pose[PelvisIndex].GetLocation()) / StepSeconds;

				// Simulated centre of mass and velocity.
				float Mass = 0.0f;
				FVector Com = FVector::ZeroVector, ComVelocity = FVector::ZeroVector;
				for (const FBodyBinding& Binding : Bindings)
				{
					if (const FBodyInstance* Body = MeshComponent->GetBodyInstance(Binding.Bone))
					{
						const float M = Body->GetBodyMass();
						Mass += M;
						Com += Body->GetCOMPosition() * M;
						ComVelocity += Body->GetUnrealWorldVelocity() * M;
					}
				}
				Com /= FMath::Max(Mass, 1.0f);
				ComVelocity /= FMath::Max(Mass, 1.0f);
				FVector Excess = ComVelocity - SourceVelocity;
				Excess.Z = 0.0;
				if (Frame == HitFrameIndex + 2 && bMovingSource && Settings.bStepping)
				{
					const FVector Travel = SourceVelocity.GetSafeNormal2D();
					const float Lost = -float(FVector::DotProduct(Excess, Travel));
					PlaybackDrop = FMath::Clamp(Lost / FMath::Max(float(SourceVelocity.Size2D()), 1.0f) * 2.5f, 0.0f, 0.85f);
					Trace.Notes.Add(FString::Printf(TEXT("run deceleration: lost %.0f cm/s of %.0f, playback drop %.2f"), Lost, SourceVelocity.Size2D(), PlaybackDrop));
				}

				// Low-pass: an arm or the struck chest spikes for a frame or two; the capture point
				// should follow the whole body's momentum, not that spike.
				SmoothedExcess = FMath::Lerp(SmoothedExcess, Excess, 0.3f);
				Excess = SmoothedExcess;

				if (Com.Z < FallComHeightCm)
				{
					Trace.bFell = true;
					Control->SetControlsInSetEnabled(TEXT("All"), false);
				}

				// The target frame carries the excess momentum while it lasts.
				if (bMovingSource)
				{
					// Along the direction of travel the time warp owns deceleration; integrating that
					// component too dragged the frame forward against it (run 30k drifted 2 m).
					const FVector Travel = SourceVelocity.GetSafeNormal2D();
					Offset += (Excess - Travel * FVector::DotProduct(Excess, Travel)) * StepSeconds;
				}
				else
				{
					Offset += Excess * StepSeconds * 0.8f;
				}
				if (!bMovingSource && Trace.Steps > 0 && Feet[0].bPinned && Feet[1].bPinned)
				{
					// Once the character has stepped, its new home is over its new footing.
					FVector FootingShift = (Feet[0].Planted + Feet[1].Planted) * 0.5f - FootingAtHit;
					FootingShift.Z = 0.0;
					Offset = FMath::Lerp(Offset, FootingShift, 0.08f);
				}

				if (!bMovingSource && !Trace.bFell)
				{
					const FVector CapturePoint = Com + Excess * FMath::Sqrt(FMath::Max(float(Com.Z), 10.0f) / 980.0f);

					for (FFoot& Foot : Feet)
					{
						if (!Foot.bPinned)
						{
							// Pin each foot where it stands at the hit.
							Foot.bPinned = true;
							Foot.Planted = MeshComponent->GetBodyInstance(Bindings[Foot.Binding].Bone)->GetUnrealWorldTransform().GetLocation();
							Foot.Current = Foot.Planted;
							// A spring-held foot sat 8-13 cm off its pin and rocked under the body's
							// weight. Kinematic feet are exact: the simulated leg between the pelvis and
							// the fixed foot resolves knee and hip, which is foot IK done by physics.
							Control->SetBodyModifierMovementType(ModifierName(Bindings[Foot.Binding].Bone), EPhysicsMovementType::Kinematic);
						}
					}
					if (FootingAtHit.IsZero())
					{
						FootingAtHit = (Feet[0].Planted + Feet[1].Planted) * 0.5f;
					}
					const FVector Mid = (Feet[0].Planted + Feet[1].Planted) * 0.5f;
					const float Error = float(FVector::Dist2D(CapturePoint, Mid));
					const bool bAnySwing = Feet[0].bSwing || Feet[1].bSwing;

					const bool bStanceLongEnough = LastLandFrame == INDEX_NONE || Frame - LastLandFrame >= MinStanceFrames;
					if (!bAnySwing && SettleStart == INDEX_NONE && Error > StepTriggerCm && bStanceLongEnough)
					{
						CalmFrames = 0;
						// Step with the foot farther from the capture point; keep stance width.
						// Feet alternate. Choosing "the foot farther from the capture point" every time
						// picked the same foot eight steps running and pedalled it in place.
						const int32 SwingSide = LastSwingSide == INDEX_NONE
							? (FVector::Dist2D(CapturePoint, Feet[0].Planted) > FVector::Dist2D(CapturePoint, Feet[1].Planted) ? 0 : 1)
							: 1 - LastSwingSide;
						FFoot& Swing = Feet[SwingSide];
						const FFoot& Stance = (&Swing == &Feet[0]) ? Feet[1] : Feet[0];
						FVector Direction = (CapturePoint - Mid);
						Direction.Z = 0.0;
						Direction = Direction.GetSafeNormal();
						FVector Lateral(-Direction.Y, Direction.X, 0.0);
						const float Side = FMath::Sign(float(FVector::DotProduct(Swing.Planted - Stance.Planted, Lateral)));
						FVector Landing = CapturePoint + Lateral * (Side * StanceHalfWidthCm);
						Landing.Z = Swing.Planted.Z;
						// Step as far as a leg reaches toward the capture point. A step that falls short
						// buys time for the next one; repeated short steps mean balance is lost.
						FVector Reach = Landing - Swing.Planted;
						Reach.Z = 0.0;
						if (Reach.Size() > MaxStepCm)
						{
							Landing = Swing.Planted + Reach.GetSafeNormal() * MaxStepCm;
							++UncaughtSteps;
						}
						else
						{
							UncaughtSteps = 0;
						}
						if (UncaughtSteps > MaxUncaughtSteps || Trace.Steps >= MaxSteps)
						{
							Trace.bFell = true;
							Control->SetControlsInSetEnabled(TEXT("All"), false);
						}
						else
						{
							Swing.bSwing = true;
							LastSwingSide = SwingSide;
							Swing.SwingFrom = Swing.Current;
							Swing.SwingTo = Landing;
							Swing.SwingStart = Frame;
						}
					}
					else if (!bAnySwing && SettleStart == INDEX_NONE)
					{
						CalmFrames = (Error < StepTriggerCm * 0.5f && Excess.Size2D() < 20.0f) ? CalmFrames + 1 : 0;
						if (CalmFrames >= SettleHoldFrames)
						{
							SettleStart = Frame;
						}
					}

					for (FFoot& Foot : Feet)
					{
						if (Foot.bSwing)
						{
							const float Phase = FMath::Clamp(float(Frame - Foot.SwingStart) / SwingFrames, 0.0f, 1.0f);
							Foot.Current = FMath::Lerp(Foot.SwingFrom, Foot.SwingTo, FMath::SmoothStep(0.0f, 1.0f, Phase));
							Foot.Current.Z += SwingLiftCm * FMath::Sin(PI * Phase);
							if (Phase >= 1.0f)
							{
								Foot.bSwing = false;
								Foot.Planted = Foot.SwingTo;
								Foot.Current = Foot.Planted;
								LastLandFrame = Frame;
								++Trace.Steps;
							}
						}
					}
				}
			}

			if (bPlanner && !bFeetReleased && Trace.bFell)
			{
				// Fallen: feet go back to simulation. A settled character keeps kinematic feet on the
				// blended source pose; handing them back to a spring left a foot hovering ~9 cm up.
				bFeetReleased = true;
				for (const FFoot& Foot : Feet)
				{
					Control->SetBodyModifierMovementType(ModifierName(Bindings[Foot.Binding].Bone), EPhysicsMovementType::Simulated);
				}
			}

			for (int32 BindingIndex = 0; BindingIndex < Bindings.Num(); ++BindingIndex)
			{
				const FBodyBinding& Binding = Bindings[BindingIndex];
				FTransform Target = Pose[Binding.BoneIndex] * ReferenceTM;
				Target.AddToTranslation(Offset);
				if (bPlanner && !bMovingSource)
				{
					for (const FFoot& Foot : Feet)
					{
						if (Foot.Binding == BindingIndex && Foot.bPinned)
						{
							// Feet follow the planner; once settled, they blend back onto the source pose.
							const float Settle = SettleStart == INDEX_NONE ? 0.0f : FMath::SmoothStep(0.0f, 1.0f, float(Frame - SettleStart) / SettleBlendFrames);
							Target.SetLocation(FMath::Lerp(Foot.Current, Target.GetLocation(), Settle));
							const int32 Side = (&Foot == &Feet[0]) ? 0 : 1;
							FootIKTarget[Side] = Target.GetLocation();
							bFootIK[Side] = !Trace.bFell;
						}
					}
				}
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
				// Strength profile, per body. Releasing world-space control everywhere (the first
				// attempt) let the whole character rotate ~60 degrees as a stiff column about its
				// feet. Now: legs keep world-space control so feet stay planted; release is
				// strongest at the contact body and fades along the skeleton; joint strength also
				// drops near the contact so the hit area bends locally. All of it ramps back.
				const float Alpha = FMath::SmoothStep(0.0f, 1.0f, float(Frame - HitFrame) / RecoverFrames);
				for (int32 Index = 0; Index < Bindings.Num(); ++Index)
				{
					const FBodyBinding& Binding = Bindings[Index];
					const int32 Distance = ContactDistance[Index];
					const float WorldRelease = IsLeg(Binding.Bone) ? 0.0f : FMath::Clamp(1.0f - 0.25f * Distance, 0.4f, 1.0f);
					const float JointRelease = Distance == 0 ? 0.8f : Distance == 1 ? 0.6f : Distance == 2 ? 0.3f : 0.0f;

					// Impact phase: for the first frames after contact the whole upper body (pelvis
					// included) lets go almost completely, so the torso, head and arms visibly take the
					// hit before recovery begins. Without it a run hit read as a slowdown, not a hit.
					constexpr int32 ImpactFrames = 10;
					constexpr int32 ImpactBlendFrames = 20;
					const float Impact = Frame - HitFrame < ImpactFrames ? 1.0f
						: 1.0f - FMath::SmoothStep(0.0f, 1.0f, float(Frame - HitFrame - ImpactFrames) / ImpactBlendFrames);
					const bool bUpper = !IsLeg(Binding.Bone);

					FPhysicsControlMultiplier WorldMultiplier;
					float WorldScale = FMath::Lerp(1.0f - WorldRelease, 1.0f, Alpha);
					if (bUpper)
					{
						// On a run the pelvis keeps some hold: released to 2% it tore away from the legs.
						const bool bPelvis = Binding.Bone == PelvisBone;
						WorldScale = FMath::Lerp(WorldScale, (bMovingSource && bPelvis) ? 0.3f : 0.02f, Impact);
					}
					else if (bMovingSource)
					{
						// A running leg travels with the hips through the impact (joint hold only) instead
						// of being held to the run's path while the hips fly: that tug of war twisted the
						// legs up to 140 degrees and left the feet floating.
						WorldScale = FMath::Lerp(WorldScale, 0.2f, Impact);
					}
					WorldMultiplier.LinearStrengthMultiplier = FVector(WorldScale);
					WorldMultiplier.AngularStrengthMultiplier = WorldScale;
					Control->SetControlMultiplier(WorldControlName(Binding.Bone), WorldMultiplier, false);

					if (Binding.ParentIndex != INDEX_NONE)
					{
						FPhysicsControlMultiplier Joint;
						Joint.AngularStrengthMultiplier = FMath::Lerp(1.0f - JointRelease, 1.0f, Alpha);
						if (bUpper)
						{
							Joint.AngularStrengthMultiplier = FMath::Lerp(Joint.AngularStrengthMultiplier, 0.15f, Impact);
						}
						Control->SetControlMultiplier(ParentControlName(Binding.Bone), Joint, false);
					}
				}

				if (bPlanner && !Trace.bFell)
				{
					// Pinned feet hold hard (foot IK); a swinging leg's thigh and calf go compliant so
					// the knee bends to follow the foot. For a moving source the legs loosen partly so
					// the stride visibly breaks. Every change eases in over a few frames: switching
					// strength in one frame was the leg jitter.
					for (const FFoot& Foot : Feet)
					{
						const FString Side = Bindings[Foot.Binding].Bone.ToString().Right(2);
						for (int32 Index = 0; Index < Bindings.Num(); ++Index)
						{
							const FString Name = Bindings[Index].Bone.ToString();
							if (!Name.EndsWith(Side) || !IsLeg(Bindings[Index].Bone))
							{
								continue;
							}
							const bool bFoot = Index == Foot.Binding;
							float Wanted = 1.0f;
							if (bMovingSource)
							{
								// Same impact curve as the upper body: legs go with the hips for the first frames,
								// then return to full hold. (A flat 50% for the whole reaction stretched the ankle
								// up to 37 cm.)
								const int32 SinceHit = Frame - HitFrameIndex;
								const float LegImpact = SinceHit < 10 ? 1.0f : 1.0f - FMath::SmoothStep(0.0f, 1.0f, float(SinceHit - 10) / 20.0f);
								Wanted = FMath::Lerp(1.0f, 0.2f, LegImpact);
							}
							else
							{
								Wanted = bFoot ? 3.0f : (Foot.bSwing ? 0.3f : 1.0f);
							}
							LegWorldScale[Index] = FMath::Lerp(LegWorldScale[Index], Wanted, 0.35f);
							FPhysicsControlMultiplier Leg;
							Leg.LinearStrengthMultiplier = FVector(LegWorldScale[Index]);
							// A pinned foot also holds its orientation hard and heavily damped: a weak angular
							// hold let the foot chatter against the floor (15x source jitter).
							Leg.AngularStrengthMultiplier = bFoot && !bMovingSource ? 2.5f : LegWorldScale[Index];
							Leg.LinearDampingRatioMultiplier = FVector(1.5f);
							Leg.AngularDampingRatioMultiplier = bFoot ? 3.0f : 1.5f;
							Control->SetControlMultiplier(WorldControlName(Bindings[Index].Bone), Leg, false);
						}
					}
				}
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
			SourceTime += StepSeconds * PlaybackRate(Frame);

			if (bPlanner && Frame >= PreRollFrames - 5)
			{
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const FFoot& Foot = Feet[Side];
					const FBodyInstance* Body = MeshComponent->GetBodyInstance(Bindings[Foot.Binding].Bone);
					if (!Body)
					{
						continue;
					}
					const FTransform TM = Body->GetUnrealWorldTransform();
					const FVector V = Body->GetUnrealWorldVelocity();
					const FVector W = FMath::RadiansToDegrees(Body->GetUnrealWorldAngularVelocityInRadians());
					const FRotator R = TM.Rotator();
					Trace.FootCsv.Add(FString::Printf(TEXT("%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.2f,%.2f,%.2f,%d,%d,%d"),
						Frame, Side, TM.GetLocation().X, TM.GetLocation().Y, TM.GetLocation().Z,
						V.X, V.Y, V.Z, R.Pitch, R.Yaw, R.Roll, W.Size(),
						Foot.Current.Z, FVector::Dist2D(Foot.Current, TM.GetLocation()),
						Foot.bPinned ? 1 : 0, Foot.bSwing ? 1 : 0, Trace.Steps));
				}
			}

			if (Settings.bRecordPose)
			{
				TArray<FTransform>& Local = Trace.LocalPoses.AddDefaulted_GetRef();
				Local.SetNum(Ref.GetNum());
				for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
				{
					const int32 Parent = Ref.GetParentIndex(Bone);
					if (BodyByBone[Bone])
					{
						// Rotation from physics; position rebuilt down the chain from real bone lengths,
						// so a stretched physics constraint can never separate joints in the output.
						// The pelvis (first body under the root) keeps its simulated position.
						const FTransform Physical = BodyByBone[Bone]->GetUnrealWorldTransform().GetRelativeTransform(ReferenceTM);
						const bool bParentHasBody = Parent != INDEX_NONE && BodyByBone[Parent] != nullptr;
						if (bParentHasBody)
						{
							const FVector BoneOffset = Pose[Bone].GetRelativeTransform(Pose[Parent]).GetTranslation();
							RecordedComponentSpace[Bone] = FTransform(Physical.GetRotation(), RecordedComponentSpace[Parent].TransformPosition(BoneOffset));
							const float Gap = float(FVector::Dist(Physical.GetLocation(), RecordedComponentSpace[Parent].TransformPosition(BoneOffset)));
							if (Ref.GetBoneName(Bone).ToString().StartsWith(TEXT("foot")))
							{
								Trace.MaxAnkleGapCm = FMath::Max(Trace.MaxAnkleGapCm, Gap);
							}
						}
						else
						{
							RecordedComponentSpace[Bone] = Physical;
						}
					}
					else
					{
						FTransform Sampled = Parent == INDEX_NONE ? Pose[Bone] : Pose[Bone].GetRelativeTransform(Pose[Parent]);
						if (Parent == INDEX_NONE)
						{
							// The root carries the stumble's displacement, so the baked clip travels with it.
							Sampled.AddToTranslation(ReferenceTM.InverseTransformVectorNoScale(Offset));
						}
						RecordedComponentSpace[Bone] = Parent == INDEX_NONE ? Sampled : Sampled * RecordedComponentSpace[Parent];
					}
					Local[Bone] = Parent == INDEX_NONE ? RecordedComponentSpace[Bone] : RecordedComponentSpace[Bone].GetRelativeTransform(RecordedComponentSpace[Parent]);
				}

				{
					const int32 Calf = Ref.FindBoneIndex(TEXT("calf_l"));
					const int32 Thigh = Ref.FindBoneIndex(TEXT("thigh_l"));
					if (BodyByBone[Calf] && BodyByBone[Thigh])
					{
						const FQuat RawLocal = BodyByBone[Calf]->GetUnrealWorldTransform().GetRelativeTransform(BodyByBone[Thigh]->GetUnrealWorldTransform()).GetRotation();
						const FQuat SourceLocal = Pose[Calf].GetRelativeTransform(Pose[Thigh]).GetRotation();
						Trace.RawCalfDeviation.Add(FMath::RadiansToDegrees(float(RawLocal.AngularDistance(SourceLocal))));
					}
				}

				// Leg IK: a planted foot lands exactly on its planned spot. The simulated knee is the
				// pole, so the knee bends the way the physics bent it. Only thigh, calf and foot change;
				// everything below the foot keeps its local transform and follows.
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const TCHAR* Suffix = Side == 0 ? TEXT("_l") : TEXT("_r");
					const int32 SourceFoot = Ref.FindBoneIndex(*(FString(TEXT("foot")) + Suffix));
					bool bGroundContact = false;
					FVector GroundTarget = FVector::ZeroVector;
					if (bMovingSource && Frame >= HitFrameIndex && SourceFoot != INDEX_NONE)
					{
						// Source foot within 3 cm of its lowest height in the clip counts as on the ground.
						const float SourceZ = float(Pose[SourceFoot].GetLocation().Z);
						if (SourceZ < SourceFootGroundZ[Side] + 3.0f)
						{
							bGroundContact = true;
							const FVector Recorded = RecordedComponentSpace[SourceFoot].GetLocation();
							GroundTarget = ReferenceTM.TransformPosition(FVector(Recorded.X, Recorded.Y, SourceZ));
						}
					}
					if (!bFootIK[Side] && !bGroundContact)
					{
						continue;
					}
					if (bGroundContact)
					{
						FootIKTarget[Side] = GroundTarget;
					}
					const int32 Thigh = Ref.FindBoneIndex(*(FString(TEXT("thigh")) + Suffix));
					const int32 Calf = Ref.FindBoneIndex(*(FString(TEXT("calf")) + Suffix));
					const int32 FootBone = Ref.FindBoneIndex(*(FString(TEXT("foot")) + Suffix));
					if (Thigh == INDEX_NONE || Calf == INDEX_NONE || FootBone == INDEX_NONE)
					{
						continue;
					}
					FTransform ThighCS = RecordedComponentSpace[Thigh];
					FTransform CalfCS = RecordedComponentSpace[Calf];
					FTransform FootCS = RecordedComponentSpace[FootBone];
					const FQuat FootRotation = FootCS.GetRotation();
					const FVector Effector = ReferenceTM.InverseTransformPosition(FootIKTarget[Side]);
					// Pole: the simulated knee, pushed forward a little so a straight leg stays stable.
					const FVector Pole = CalfCS.GetLocation() + ReferenceTM.InverseTransformVectorNoScale(FVector(0.0, 30.0, 0.0));
					const double Upper = (CalfCS.GetLocation() - ThighCS.GetLocation()).Size();
					const double Lower = (FootCS.GetLocation() - CalfCS.GetLocation()).Size();
					AnimationCore::SolveTwoBoneIK(ThighCS, CalfCS, FootCS, Pole, Effector, Upper, Lower, false, 1.0, 1.0);
					FootCS.SetRotation(FootRotation);
					RecordedComponentSpace[Thigh] = ThighCS;
					RecordedComponentSpace[Calf] = CalfCS;
					RecordedComponentSpace[FootBone] = FootCS;
					Local[Thigh] = ThighCS.GetRelativeTransform(RecordedComponentSpace[Ref.GetParentIndex(Thigh)]);
					Local[Calf] = CalfCS.GetRelativeTransform(ThighCS);
					Local[FootBone] = FootCS.GetRelativeTransform(CalfCS);
					// Bones below the foot keep their locals; refresh their component-space copies.
					for (int32 Child = FootBone + 1; Child < Ref.GetNum(); ++Child)
					{
						const int32 ChildParent = Ref.GetParentIndex(Child);
						if (ChildParent >= FootBone && (ChildParent == FootBone || Ref.BoneIsChildOf(ChildParent, FootBone)))
						{
							RecordedComponentSpace[Child] = Local[Child] * RecordedComponentSpace[ChildParent];
						}
					}
				}
				TArray<FTransform>& SourceLocal = Trace.SourceLocalPoses.AddDefaulted_GetRef();
				SourceLocal.SetNum(Ref.GetNum());
				for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
				{
					const int32 Parent = Ref.GetParentIndex(Bone);
					SourceLocal[Bone] = Parent == INDEX_NONE ? Pose[Bone] : Pose[Bone].GetRelativeTransform(Pose[Parent]);
				}
				Trace.RecordedComponentPoses.Add(RecordedComponentSpace);
				Trace.SourceComponentPoses.Add(Pose);
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
				Sampler.Sample(SourceTime, Next);
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
		Trace.FinalOffset = Offset;
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
	int64 BakeAndSave(USkeletalMesh* Mesh, const TArray<TArray<FTransform>>& Frames, const FString& Name, double& OutBakeSeconds, double& OutSaveSeconds, const FString& Folder = TEXT("KimodoImpactSpikeProbe"))
	{
		const double BakeStart = FPlatformTime::Seconds();
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/%s/%s"), *Folder, *Name));
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
		const FString File = FPaths::ProjectSavedDir() / Folder / (Name + FPackageName::GetAssetPackageExtension());
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

namespace KimodoImpactProbe
{
	/** Describes how a recorded clip differs from its source: joint deviation, whole-body tilt, foot slide. */
	TArray<FString> AnalyseMotion(const FTrace& Trace, const FReferenceSkeleton& Ref, const UPhysicsAsset* PhysicsAsset)
	{
		TArray<FString> Lines;
		const int32 HitIndex = PreRollFrames + 1;
		if (Trace.LocalPoses.Num() <= HitIndex)
		{
			return Lines;
		}

		// 1. Per body bone: largest joint (parent-space) rotation deviation from the source after the hit.
		FString Joints;
		float LargestJoint = 0.0f;
		for (const TObjectPtr<USkeletalBodySetup>& BodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			const int32 Bone = Ref.FindBoneIndex(BodySetup->BoneName);
			float MaxDegrees = 0.0f;
			for (int32 Frame = HitIndex; Frame < Trace.LocalPoses.Num(); ++Frame)
			{
				const float Radians = float(Trace.LocalPoses[Frame][Bone].GetRotation().AngularDistance(Trace.SourceLocalPoses[Frame][Bone].GetRotation()));
				MaxDegrees = FMath::Max(MaxDegrees, FMath::RadiansToDegrees(Radians));
			}
			LargestJoint = FMath::Max(LargestJoint, MaxDegrees);
			Joints += FString::Printf(TEXT("%s %.1f, "), *BodySetup->BoneName.ToString(), MaxDegrees);
		}
		Lines.Add(FString::Printf(TEXT("joint deviation from source after hit (deg, max per body): %s"), *Joints));

		// 2. Whole-body: pelvis component-space rotation and position deviation from source.
		const int32 Pelvis = Ref.FindBoneIndex(PelvisBone);
		float PelvisDegrees = 0.0f, PelvisCm = 0.0f;
		for (int32 Frame = HitIndex; Frame < Trace.RecordedComponentPoses.Num(); ++Frame)
		{
			const FTransform& Recorded = Trace.RecordedComponentPoses[Frame][Pelvis];
			const FTransform& Source = Trace.SourceComponentPoses[Frame][Pelvis];
			PelvisDegrees = FMath::Max(PelvisDegrees, FMath::RadiansToDegrees(float(Recorded.GetRotation().AngularDistance(Source.GetRotation()))));
			PelvisCm = FMath::Max(PelvisCm, float(FVector::Dist(Recorded.GetLocation(), Source.GetLocation())));
		}
		Lines.Add(FString::Printf(TEXT("pelvis vs source after hit: max tilt %.1f deg, max offset %.1f cm; largest joint deviation %.1f deg"),
			PelvisDegrees, PelvisCm, LargestJoint));

		// Impact visibility: peak upper-body angular speed after the hit vs the source's own peak.
		{
			FString Peaks;
			for (const TCHAR* UpperBone : { TEXT("spine_03"), TEXT("neck_01"), TEXT("head"), TEXT("upperarm_l"), TEXT("upperarm_r") })
			{
				const int32 Bone = Ref.FindBoneIndex(UpperBone);
				auto PeakSpeed = [&](const TArray<TArray<FTransform>>& Poses)
				{
					float Peak = 0.0f;
					for (int32 Frame = HitIndex; Frame + 1 < Poses.Num() && Frame < HitIndex + 30; ++Frame)
					{
						Peak = FMath::Max(Peak, FMath::RadiansToDegrees(float(Poses[Frame][Bone].GetRotation().AngularDistance(Poses[Frame + 1][Bone].GetRotation()))) / StepSeconds);
					}
					return Peak;
				};
				const float Recorded = PeakSpeed(Trace.RecordedComponentPoses);
				const float Source = FMath::Max(PeakSpeed(Trace.SourceComponentPoses), 1.0f);
				Peaks += FString::Printf(TEXT("%s %.0f deg/s (%.1fx), "), UpperBone, Recorded, Recorded / Source);
			}
			Lines.Add(FString::Printf(TEXT("impact visibility, peak angular speed in first 0.5 s (x source): %s"), *Peaks));
		}

		// Per-frame trace of the left leg: output local calf deviation, raw physics calf deviation,
		// output and source foot height. Answers when the leg goes wrong and whether physics or the
		// recorder does it.
		{
			const int32 Calf = Ref.FindBoneIndex(TEXT("calf_l"));
			const int32 FootL = Ref.FindBoneIndex(TEXT("foot_l"));
			FString Rows;
			for (int32 Frame = 0; Frame < Trace.LocalPoses.Num(); Frame += 6)
			{
				const float Output = FMath::RadiansToDegrees(float(Trace.LocalPoses[Frame][Calf].GetRotation().AngularDistance(Trace.SourceLocalPoses[Frame][Calf].GetRotation())));
				const float Raw = Trace.RawCalfDeviation.IsValidIndex(Frame) ? Trace.RawCalfDeviation[Frame] : -1.0f;
				Rows += FString::Printf(TEXT("[f%d out %.0f raw %.0f footZ %.0f/%.0f] "), Frame, Output, Raw,
					Trace.RecordedComponentPoses[Frame][FootL].GetLocation().Z, Trace.SourceComponentPoses[Frame][FootL].GetLocation().Z);
			}
			Lines.Add(FString::Printf(TEXT("left leg trace: %s"), *Rows));
		}

		// Floating: over the last 30 frames, pelvis and lowest-foot height relative to the source.
		{
			const int32 PelvisBoneIndex = Ref.FindBoneIndex(PelvisBone);
			const int32 FootL = Ref.FindBoneIndex(TEXT("foot_l"));
			const int32 FootR = Ref.FindBoneIndex(TEXT("foot_r"));
			float PelvisDelta = 0.0f, FootDelta = 0.0f;
			int32 Count = 0;
			for (int32 Frame = FMath::Max(HitIndex, Trace.RecordedComponentPoses.Num() - 30); Frame < Trace.RecordedComponentPoses.Num(); ++Frame)
			{
				const TArray<FTransform>& R = Trace.RecordedComponentPoses[Frame];
				const TArray<FTransform>& S = Trace.SourceComponentPoses[Frame];
				PelvisDelta += float(R[PelvisBoneIndex].GetLocation().Z - S[PelvisBoneIndex].GetLocation().Z);
				FootDelta += float(FMath::Min(R[FootL].GetLocation().Z, R[FootR].GetLocation().Z) - FMath::Min(S[FootL].GetLocation().Z, S[FootR].GetLocation().Z));
				++Count;
			}
			Lines.Add(FString::Printf(TEXT("end of clip vs source (last 30 frames, mean): pelvis height %+.1f cm, lowest foot %+.1f cm"),
				PelvisDelta / FMath::Max(Count, 1), FootDelta / FMath::Max(Count, 1)));
		}

		// Leg jitter: mean absolute frame-to-frame angular acceleration of parent-space rotations.
		{
			FString Jitter;
			for (const TCHAR* LegBone : { TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r") })
			{
				const int32 Bone = Ref.FindBoneIndex(LegBone);
				auto Acceleration = [&](const TArray<TArray<FTransform>>& Poses)
				{
					double Sum = 0.0;
					int32 Count = 0;
					for (int32 Frame = HitIndex + 1; Frame + 1 < Poses.Num(); ++Frame)
					{
						const float A = float(Poses[Frame - 1][Bone].GetRotation().AngularDistance(Poses[Frame][Bone].GetRotation()));
						const float B = float(Poses[Frame][Bone].GetRotation().AngularDistance(Poses[Frame + 1][Bone].GetRotation()));
						Sum += FMath::Abs(FMath::RadiansToDegrees(B - A));
						++Count;
					}
					return Count ? Sum / Count : 0.0;
				};
				Jitter += FString::Printf(TEXT("%s %.2f/%.2f, "), LegBone, Acceleration(Trace.LocalPoses), Acceleration(Trace.SourceLocalPoses));
			}
			Lines.Add(FString::Printf(TEXT("leg jitter, mean |angular accel| deg/frame^2 recorded/source: %s"), *Jitter));
		}

		// 3. Feet: drift from source (component space) and ground height after the hit.
		for (const TCHAR* FootName : { TEXT("foot_l"), TEXT("foot_r") })
		{
			const int32 Foot = Ref.FindBoneIndex(FootName);
			if (Foot == INDEX_NONE)
			{
				continue;
			}
			float MaxDrift = 0.0f, MaxHeight = 0.0f;
			for (int32 Frame = HitIndex; Frame < Trace.RecordedComponentPoses.Num(); ++Frame)
			{
				const FVector Recorded = Trace.RecordedComponentPoses[Frame][Foot].GetLocation();
				const FVector Source = Trace.SourceComponentPoses[Frame][Foot].GetLocation();
				MaxDrift = FMath::Max(MaxDrift, float(FVector::Dist2D(Recorded, Source)));
				MaxHeight = FMath::Max(MaxHeight, float(Recorded.Z - Source.Z));
			}
			Lines.Add(FString::Printf(TEXT("%s after hit: max horizontal drift from source %.1f cm, max lift above source %.1f cm"), FootName, MaxDrift, MaxHeight));
		}
		return Lines;
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
		if (Clip < 2)
		{
			for (const FString& Line : AnalyseMotion(Trace, Mesh->GetRefSkeleton(), PhysicsAsset))
			{
				AddInfo(FString::Printf(TEXT("Motion %s: %s"), Clip ? TEXT("run") : TEXT("idle"), *Line));
			}
		}

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
	FKimodoImpactStumbleProbe,
	"KimodoDatasetCapture.Impact.Spike.StumbleProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKimodoImpactStumbleProbe::RunTest(const FString& Parameters)
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
		for (const float Impulse : { 15000.0f, 30000.0f, 50000.0f })
		{
			FSettings Settings;
			Settings.ImpulseKgCmPerS = Impulse;
			Settings.bRecordPose = true;
			Settings.bStepping = true;
			const FTrace Trace = Run(ProbeWorld, Mesh, PhysicsAsset, Anim, EScenario::Controlled, Settings);
			const FString Label = FString::Printf(TEXT("%s_%dk"), Anim == Idle ? TEXT("Idle") : TEXT("Run"), int32(Impulse / 1000.0f));
			AddInfo(FString::Printf(TEXT("Stumble %s: steps %d, fell %d, displacement %.1f cm, raw physics ankle gap max %.1f cm (output rebuilt, gap 0)"),
				*Label, Trace.Steps, Trace.bFell ? 1 : 0, Trace.FinalOffset.Size2D(), Trace.MaxAnkleGapCm));
			for (const FString& Note : Trace.Notes)
			{
				if (Note.StartsWith(TEXT("run deceleration")))
				{
					AddInfo(FString::Printf(TEXT("Stumble %s: %s"), *Label, *Note));
				}
			}
			for (const FString& Line : AnalyseMotion(Trace, Mesh->GetRefSkeleton(), PhysicsAsset))
			{
				AddInfo(FString::Printf(TEXT("Stumble %s: %s"), *Label, *Line));
			}
			if (!Trace.FootCsv.IsEmpty())
			{
				FFileHelper::SaveStringToFile(
					TEXT("frame,side,x,y,z,vx,vy,vz,pitch,yaw,roll,angspeed,targetz,targetdist2d,pinned,swing,steps\n") + FString::Join(Trace.FootCsv, TEXT("\n")),
					*(FPaths::ProjectSavedDir() / TEXT("KimodoImpactSpikeV9") / FString::Printf(TEXT("Feet_%s.csv"), *Label)));
			}
			double Bake = 0.0, Save = 0.0;
			TestTrue(*FString::Printf(TEXT("%s saved"), *Label),
				BakeAndSave(Mesh, Trace.LocalPoses, FString::Printf(TEXT("Stumble_%s"), *Label), Bake, Save, TEXT("KimodoImpactSpikeV9")) > 0);
		}
	}
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
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
