// Copyright Animotive. All Rights Reserved.

#include "ImpactClipSimulator.h"

#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "ImpactContactSpread.h"
#include "ImpactRandom.h"
#include "ImpactReactionCharacter.h"
#include "ImpactRegionTable.h"
#include "ImpactSimWorld.h"
#include "KimodoImpactCaptureLog.h"
#include "PhysicsEngine/BodyInstance.h"

#define LOCTEXT_NAMESPACE "ImpactClipSimulator"

namespace
{
	/** Leg side a region belongs to (0 left, 1 right), or INDEX_NONE. */
	int32 LegSide(EImpactBodyRegion Region)
	{
		switch (Region)
		{
		case EImpactBodyRegion::ThighLeft:
		case EImpactBodyRegion::ShinLeft:
		case EImpactBodyRegion::FootLeft:
			return 0;
		case EImpactBodyRegion::ThighRight:
		case EImpactBodyRegion::ShinRight:
		case EImpactBodyRegion::FootRight:
			return 1;
		default:
			return INDEX_NONE;
		}
	}

	/** Body-tree distance from Struck to every body. */
	TArray<int32> ContactDistances(const TArray<AImpactReactionCharacter::FBody>& Bodies, int32 Struck)
	{
		TArray<int32> Distance;
		Distance.Init(MAX_int32, Bodies.Num());
		if (!Bodies.IsValidIndex(Struck))
		{
			return Distance;
		}
		TArray<int32> Queue = { Struck };
		Distance[Struck] = 0;
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 Current = Queue[Head];
			for (int32 Other = 0; Other < Bodies.Num(); ++Other)
			{
				const bool bAdjacent = Bodies[Other].ParentBody == Current || Bodies[Current].ParentBody == Other;
				if (bAdjacent && Distance[Other] == MAX_int32)
				{
					Distance[Other] = Distance[Current] + 1;
					Queue.Add(Other);
				}
			}
		}
		return Distance;
	}

	/** Spawns a physical projectile that will actually collide with the character. */
	AActor* SpawnProjectile(UWorld* World, const FImpactProjectileSpec& Spec, const FVector& Start, const FVector& Direction)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UPrimitiveComponent* Shape = nullptr;
		const float Half = FMath::Max(Spec.SizeCm * 0.5f, 0.5f);
		switch (Spec.Shape)
		{
		case EImpactorShape::Box:
		{
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Box->SetBoxExtent(FVector(Half));
			Shape = Box;
			break;
		}
		case EImpactorShape::Capsule:
		{
			UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(Actor);
			Capsule->SetCapsuleSize(Half * 0.35f, Half);
			Shape = Capsule;
			break;
		}
		default:
		{
			USphereComponent* Sphere = NewObject<USphereComponent>(Actor);
			Sphere->SetSphereRadius(Half);
			Shape = Sphere;
			break;
		}
		}
		Shape->SetCollisionProfileName(TEXT("PhysicsActor"));
		Actor->SetRootComponent(Shape);
		Shape->RegisterComponent();
		Shape->SetWorldLocation(Start);
		Shape->SetWorldRotation(FRotationMatrix::MakeFromZ(Direction).Rotator());
		Shape->SetSimulatePhysics(true);
		Shape->SetEnableGravity(false);
		if (FBodyInstance* Body = Shape->GetBodyInstance())
		{
			// Fast small objects tunnel without continuous collision.
			Body->bUseCCD = true;
			Body->SetMassOverride(FMath::Max(Spec.MassKg, 0.001f), true);
			Body->UpdateMassProperties();
		}
		Shape->SetPhysicsLinearVelocity(Direction.GetSafeNormal() * (Spec.SpeedMetresPerSecond * 100.0f));
		return Actor;
	}
}

double FImpactClipSimulator::SourceStartSeconds(const FImpactClipSetup& Setup)
{
	const double LeadIn = double(Setup.PreRollFrames + 1) / double(FMath::Max(1, Setup.FrameRate));
	if (Setup.Params.SourceMoment != EImpactSourceMoment::Phase && Setup.bSourcePlaysOnce && Setup.Mesh && Setup.Source)
	{
		const FImpactPoseSampler Sampler(*Setup.Mesh, *Setup.Source, true);
		const FReferenceSkeleton& Ref = Setup.Mesh->GetRefSkeleton();
		double TakeOff = 0.0, Landing = 0.0;
		if (FImpactSourceFlight::FindFlight(Sampler, Ref, Ref.FindBoneIndex(TEXT("foot_l")), Ref.FindBoneIndex(TEXT("foot_r")), TakeOff, Landing))
		{
			return FImpactSourceFlight::MomentSeconds(Setup.Params.SourceMoment, TakeOff, Landing) - LeadIn;
		}
	}
	return FImpactSourceClock::StartSeconds(Setup.Params.SourcePhase, Setup.Source ? Setup.Source->GetPlayLength() : 0.0, Setup.bSourcePlaysOnce, LeadIn);
}

bool FImpactClipSimulator::Run(FImpactSimWorld& SimWorld, const FImpactClipSetup& Setup, FImpactClipRecording& Out, FText& OutReason)
{
	if (!Setup.Mesh || !Setup.PhysicsAsset || !Setup.Source || !Setup.RegionTable || !Setup.Params.Impactor.Archetype)
	{
		OutReason = LOCTEXT("Incomplete", "The clip setup is missing its mesh, physics asset, source animation, region table or impactor.");
		return false;
	}
	UWorld* World = SimWorld.GetWorld();
	const float Dt = 1.0f / float(FMath::Max(1, Setup.FrameRate));
	const FReferenceSkeleton& Ref = Setup.Mesh->GetRefSkeleton();

	// --- Character -----------------------------------------------------------------------------
	const FTransform Reference(FQuat::Identity, FVector(0.0, 0.0, SimWorld.GetGroundHeight()));
	AImpactReactionCharacter* Character = World->SpawnActor<AImpactReactionCharacter>(Reference.GetLocation(), FRotator::ZeroRotator);
	if (!Character || !Character->Setup(Setup.Mesh, Setup.PhysicsAsset, Setup.Strength.WorldStrength, Setup.Strength.ParentStrength, OutReason))
	{
		if (Character)
		{
			Character->Destroy();
		}
		return false;
	}
	ON_SCOPE_EXIT { Character->Destroy(); };
	const TArray<AImpactReactionCharacter::FBody>& Bodies = Character->GetBodies();

	// --- Source --------------------------------------------------------------------------------
	const FImpactPoseSampler Sampler(*Setup.Mesh, *Setup.Source, Setup.bSourcePlaysOnce);
	Character->OpenLimitsToHold(Sampler, 1.0 / 30.0, Setup.SourceLimitMarginDeg);
	FImpactSourceClock Clock(Sampler.GetLength(), Sampler.GetLoopDelta().GetLocation(), Setup.Clock);
	const double StartTime = SourceStartSeconds(Setup);
	const bool bMoving = Clock.IsMoving();
	const int32 PelvisBone = Ref.FindBoneIndex(TEXT("pelvis"));

	// --- Contact -------------------------------------------------------------------------------
	FImpactRandom ContactRandom(Setup.BatchSeed, Setup.ClipIndex, EImpactRandomStream::Contact);
	const TArray<FName> RegionBodies = Setup.RegionTable->BodiesFor(Setup.Params.Region);
	if (RegionBodies.IsEmpty())
	{
		OutReason = LOCTEXT("NoRegionBodies", "The struck region maps to no physics body.");
		return false;
	}
	const int32 Struck = Character->FindBody(RegionBodies[ContactRandom.RangeInt(0, RegionBodies.Num() - 1)]);
	if (Struck == INDEX_NONE)
	{
		OutReason = LOCTEXT("StruckMissing", "The struck body is not in the character's physics asset.");
		return false;
	}
	const TArray<int32> Distance = ContactDistances(Bodies, Struck);

	// Character space (X forward, Y right, Z up) to world, from the mesh's forward.
	const FQuat CharacterToWorld = FRotationMatrix::MakeFromXZ(Reference.TransformVectorNoScale(Setup.MeshForward), FVector::UpVector).ToQuat();

	// --- Timing --------------------------------------------------------------------------------
	// A single-frame hit is pushed over the impactor's contact time.
	const int32 ContactFrames = Setup.Params.Impactor.Archetype->ContactFrames(Setup.FrameRate);
	auto HitFrames = [ContactFrames](const FImpactHit& Hit) { return Hit.DurationFrames > 0 ? Hit.DurationFrames : ContactFrames; };
	// The first hit lands on the frame after the pre-roll.
	const int32 ImpactFrame = Setup.PreRollFrames + 1;

	// A low stance (a crouch) at the hit: its body is held over its feet rather than balanced.
	bool bLowStance = false;
	if (PelvisBone != INDEX_NONE)
	{
		const float RefPelvisHeight = float(FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, PelvisBone).GetLocation().Z);
		TArray<FTransform> HitPose;
		Sampler.Sample(StartTime + double(ImpactFrame) * double(Dt), HitPose);
		bLowStance = RefPelvisHeight > 0.0f && HitPose.IsValidIndex(PelvisBone)
			&& float(HitPose[PelvisBone].GetLocation().Z) < Setup.LowStancePelvisFraction * RefPelvisHeight;
	}
	int32 LastHitOffset = 0;
	for (const FImpactHit& Hit : Setup.Schedule.Hits)
	{
		LastHitOffset = FMath::Max(LastHitOffset, Hit.FrameOffset + HitFrames(Hit) - 1);
	}
	int32 ProjectileArrival = 0;
	if (Setup.Schedule.Projectile.IsSet())
	{
		const FImpactProjectileSpec& Spec = Setup.Schedule.Projectile.GetValue();
		const float CmPerFrame = FMath::Max(Spec.SpeedMetresPerSecond * 100.0f * Dt, 0.1f);
		ProjectileArrival = FMath::CeilToInt32(Spec.LaunchDistanceCm / CmPerFrame);
		LastHitOffset = FMath::Max(LastHitOffset, ProjectileArrival);
	}
	FImpactClipWindowConfig WindowConfig;
	WindowConfig.PreRollFrames = ImpactFrame;
	WindowConfig.MaxReactionFrames = FMath::Max(Setup.MaxReactionFrames, LastHitOffset + 1);
	WindowConfig.SettleFrames = Setup.SettleFrames;
	WindowConfig.SettleSpeedCmPerSecond = bMoving ? Setup.SettleSpeedMovingCmPerSecond : Setup.SettleSpeedCmPerSecond;
	WindowConfig.LastHitOffsetFrames = LastHitOffset;
	if (!WindowConfig.Validate(OutReason))
	{
		return false;
	}
	FImpactClipWindow Window(WindowConfig);

	// A hard hit while the source is in the air cannot be resisted: the body lets go at the hit
	// rather than following the source's own landing.
	const bool bReleaseInFlight = uint8(Setup.Params.Intensity) >= uint8(Setup.MidAirReleaseIntensity)
		&& FImpactSourceFlight::IsInFlight(Sampler, Ref, Ref.FindBoneIndex(TEXT("foot_l")), Ref.FindBoneIndex(TEXT("foot_r")),
			StartTime + double(ImpactFrame) * double(Dt));

	// The speed change a push spread over RadiusCm gives the part it lands on, from the bodies'
	// current places: what decides whether one part is struck violently.
	auto StruckDeltaV = [&](float ImpulseKgCmPerSecond, float RadiusCm)
	{
		TArray<FImpactBodyProxy> Proxies;
		for (int32 Index = 0; Index < Bodies.Num(); ++Index)
		{
			Proxies.Add({ Bodies[Index].Bone, Bodies[Index].Instance ? Bodies[Index].Instance->GetCOMPosition() : FVector::ZeroVector, Character->BodyRadius(Index),
				Bodies[Index].Instance ? Bodies[Index].Instance->GetBodyMass() : 0.0f });
		}
		return FImpactContactSpread::ContactDeltaV(Proxies, Struck, Proxies[Struck].Center, RadiusCm, ImpulseKgCmPerSecond);
	};

	FImpactBalancePlanner Planner(Setup.Balance);
	const int32 FootBody[2] = { Character->FindBody(TEXT("foot_l")), Character->FindBody(TEXT("foot_r")) };
	const bool bHaveFeet = FootBody[0] != INDEX_NONE && FootBody[1] != INDEX_NONE;

	TArray<FTransform> PelvisReference;
	Sampler.SampleLoop(0.0, PelvisReference);
	FImpactRootTracker RootTracker(PelvisReference[PelvisBone], Setup.MeshForward, 0.0f, Setup.Root);

	Out = FImpactClipRecording();
	Out.Reference = Reference;
	Out.ImpactFrame = ImpactFrame;
	Out.StruckBody = Bodies[Struck].Bone;
	Out.CharacterToWorld = CharacterToWorld;
	Out.bMovingSource = bMoving;
	for (const AImpactReactionCharacter::FBody& Body : Bodies)
	{
		Out.BodyBones.Add(Body.Bone);
		Out.BodyBoneIndices.Add(Body.BoneIndex);
	}
	// Everything a clip records is reserved here, for its longest possible length.
	const int32 MaxFrames = WindowConfig.PreRollFrames + WindowConfig.MaxReactionFrames + 1;
	Out.Frames.Reserve(MaxFrames);
	Out.BodyPoses.Reserve(MaxFrames, Bodies.Num());
	Out.SourcePoses.Reserve(MaxFrames, Ref.GetNum());

	// --- Run -----------------------------------------------------------------------------------
	TArray<FTransform> Pose, NextPose;
	Sampler.Sample(StartTime, Pose);
	// The last few targets, pre-sized so recording them never allocates: joints are judged against
	// these, since a held joint trails its target.
	const int32 TargetWindow = FMath::Max(1, Setup.Validity.TargetWindowFrames);
	TArray<TArray<FTransform>> RecentTargets;
	RecentTargets.SetNum(TargetWindow);
	for (TArray<FTransform>& Target : RecentTargets)
	{
		Target = Pose;
	}
	TArray<const TArray<FTransform>*, TInlineAllocator<8>> RecentTargetViews;
	RecentTargetViews.Reserve(TargetWindow);

	// Warm-up: simulated from WarmUpFrames before the clip, following the source, unrecorded. Bodies
	// sag a few centimetres as the world hold takes their weight; that settles here, not in the clip.
	{
		const int32 WarmUp = FMath::Max(0, Setup.WarmUpFrames);
		// Before its start a loop plays its previous cycle, carried back by the loop's root delta, so
		// a runner is already running; a play-once source holds its first frame.
		auto SampleBefore = [&Sampler, &Setup](double Time, TArray<FTransform>& OutPose)
		{
			int32 Loops = 0;
			while (!Setup.bSourcePlaysOnce && Time < 0.0 && Sampler.GetLength() > 0.0)
			{
				Time += Sampler.GetLength();
				++Loops;
			}
			Sampler.Sample(Time, OutPose);
			const FTransform Back = FImpactSourceClock::LoopOffset(Sampler.GetLoopDelta(), Loops).Inverse();
			for (FTransform& Transform : OutPose)
			{
				Transform = Transform * Back;
			}
		};
		SampleBefore(StartTime - double(WarmUp) * double(Dt), NextPose);
		Character->TeleportToPose(NextPose, Reference);
		Character->SetAllSimulated();
		for (int32 Index = 0; Index < Bodies.Num(); ++Index)
		{
			// Pre-hit strength: full hold, the same every frame until the hit.
			const FImpactBodyStrength Strength = Setup.Strength.Evaluate(FImpactBodyStrengthQuery(), -1);
			Character->SetStrength(Index, Strength.World, Strength.Joint, -1.0f, 1.0f, Strength.JointDamping);
		}
		const TMap<int32, FVector> NoOverrides;
		for (int32 Step = 0; Step < WarmUp; ++Step)
		{
			SampleBefore(StartTime - double(WarmUp - Step) * double(Dt), NextPose);
			Character->SetTargets(NextPose, Reference, FVector::ZeroVector, NoOverrides, Dt);
			SimWorld.Step(Dt, Setup.SimSubsteps);
		}
	}

	TArray<FImpactFrameState> OutcomeFrames;
	OutcomeFrames.Reserve(WindowConfig.MaxReactionFrames + 1);
	TMap<int32, FVector> Overrides;
	Overrides.Reserve(2);
	TArray<FImpactBodySample> Samples;
	TArray<float> LegScale;
	LegScale.Init(1.0f, Bodies.Num());
	FImpactBalanceOutput Balance;
	FVector LastCom = Character->CenterOfMass();
	// The hit's physical impulse over the whole struck leg: whether it can stop a stride.
	float StruckLegDeltaV = 0.0f;
	if (const int32 LegSideStruck = LegSide(Setup.Params.Region); LegSideStruck != INDEX_NONE)
	{
		float LegMass = 0.0f;
		for (const AImpactReactionCharacter::FBody& Body : Bodies)
		{
			if (Body.bLeg && Body.Bone.ToString().EndsWith(LegSideStruck == 0 ? TEXT("_l") : TEXT("_r")) && Body.Instance)
			{
				LegMass += Body.Instance->GetBodyMass();
			}
		}
		StruckLegDeltaV = LegMass > 0.0f ? Setup.Params.TransferredImpulseKgCmPerSecond / LegMass : 0.0f;
	}
	bool bReleased = false;
	int32 ReleaseFrame = INDEX_NONE;
	// The pose the ragdoll's joint tone holds: the source pose at release, not the source playing on.
	TArray<FTransform> ReleasePose;
	bool bFeetKinematic[2] = { false, false };
	AActor* Projectile = nullptr;
	ON_SCOPE_EXIT { if (Projectile) { Projectile->Destroy(); } };
	const FVector BaseDirection = CharacterToWorld.RotateVector(Setup.Params.DirectionLocal).GetSafeNormal();

	for (int32 Frame = 0; Window.GetPhase() != EImpactClipPhase::Done; ++Frame)
	{
		const double SourceTime = StartTime + Clock.GetTime();
		Sampler.Sample(SourceTime, Pose);
		Sampler.Sample(SourceTime + double(Dt) * Clock.RateAt(Frame), NextPose);
		const FVector SourceVelocity = Reference.TransformVectorNoScale(NextPose[PelvisBone].GetLocation() - Pose[PelvisBone].GetLocation()) / Dt;
		const int32 SinceHit = Frame - ImpactFrame;

		if (Frame == ImpactFrame && bReleaseInFlight && !bReleased)
		{
			bReleased = true;
			ReleaseFrame = Frame;
			ReleasePose = Pose;
			Character->Release(Setup.Ragdoll, ReleasePose, Setup.SourceLimitMarginDeg);
			bFeetKinematic[0] = bFeetKinematic[1] = false;
		}

		// Balance, from the state the last step left.
		Overrides.Reset();
		if (SinceHit >= 0)
		{
			if (SinceHit == 0 && Setup.FallRules.IsOverwhelming(Setup.Params.ReactionImpulseKgCmPerSecond, Character->TotalMass(),
				StruckDeltaV(Setup.Params.ReactionImpulseKgCmPerSecond, FMath::Max(Setup.Params.Impactor.ContactRadiusCm, Setup.ReactionSpreadRadiusCm))))
			{
				Out.bOverwhelmed = true;
				Planner.ReleaseImmediately();
			}

			FImpactBalanceInput Input;
			Input.DeltaSeconds = Dt;
			Input.Com = Character->CenterOfMass();
			Input.ComVelocity = Character->CenterOfMassVelocity();
			Input.SourceVelocity = SourceVelocity;
			Input.bMovingSource = bMoving;
			Input.StruckLegSide = LegSide(Setup.Params.Region);
			Input.StruckLegDeltaVCmPerSecond = StruckLegDeltaV;
			Input.SourcePelvisHeightCm = PelvisBone != INDEX_NONE ? float(Pose[PelvisBone].GetLocation().Z) : 0.0f;
			Input.bLowStance = bLowStance;
			for (int32 Side = 0; Side < 2 && bHaveFeet; ++Side)
			{
				Input.FootPosition[Side] = Character->BodyTransform(FootBody[Side]).GetLocation();
			}
			Balance = Planner.Step(Input);
			UE_LOG(LogKimodoImpactCapture, Verbose, TEXT("Balance +%d: com %s vel %s source vel %s, capture %s, support error %.1f, steps %d, trip %d, fell %d"),
				SinceHit, *Input.Com.ToCompactString(), *Input.ComVelocity.ToCompactString(), *Input.SourceVelocity.ToCompactString(),
				*Balance.CapturePoint.ToCompactString(), Balance.SupportErrorCm, Balance.Steps,
				Balance.bTrip ? 1 : 0, Balance.bFell ? 1 : 0);

			if (Balance.bJustFell && !bReleased)
			{
				bReleased = true;
				ReleaseFrame = Frame;
				ReleasePose = Pose;
				Character->Release(Setup.Ragdoll, ReleasePose, Setup.SourceLimitMarginDeg);
				bFeetKinematic[0] = bFeetKinematic[1] = false;
			}

			if (SinceHit == Setup.Clock.MeasureDelayFrames)
			{
				const FVector Travel = SourceVelocity.GetSafeNormal2D();
				Clock.ApplyHit(ImpactFrame, -float(FVector::DotProduct(Input.ComVelocity - SourceVelocity, Travel)));
			}

			// Planted and stepping feet are placed exactly: foot IK done by physics.
			for (int32 Side = 0; Side < 2 && bHaveFeet && !bReleased; ++Side)
			{
				const FImpactFootTarget& Foot = Balance.Feet[Side];
				if (Foot.bPinned)
				{
					const FVector SourceTarget = (Pose[Bodies[FootBody[Side]].BoneIndex] * Reference).GetLocation() + Balance.Offset;
					Overrides.Add(FootBody[Side], FMath::Lerp(Foot.Position, SourceTarget, double(Foot.SettleAlpha)));
				}
				if (Foot.bPinned != bFeetKinematic[Side])
				{
					Character->SetBodyKinematic(FootBody[Side], Foot.bPinned);
					bFeetKinematic[Side] = Foot.bPinned;
				}
			}
		}

		// Strengths.
		for (int32 Index = 0; Index < Bodies.Num(); ++Index)
		{
			FImpactBodyStrengthQuery Query;
			Query.ContactDistance = Distance[Index] == MAX_int32 ? 99 : Distance[Index];
			Query.bLeg = Bodies[Index].bLeg;
			Query.bPelvis = Bodies[Index].bPelvis;
			const FString BoneName = Bodies[Index].Bone.ToString();
			Query.bCore = BoneName.StartsWith(TEXT("spine")) || BoneName.StartsWith(TEXT("neck")) || BoneName == TEXT("head");
			Query.bMovingSource = bMoving;
			Query.bReleased = bReleased;
			Query.FramesSinceRelease = bReleased ? Frame - ReleaseFrame : 0;
			const FImpactBodyStrength Strength = Setup.Strength.Evaluate(Query, SinceHit);

			float WorldScale = Strength.World;
			float WorldAngular = -1.0f;
			float Damping = 1.0f;
			if (SinceHit >= 0 && !bReleased && !bMoving && Bodies[Index].bLeg && bHaveFeet)
			{
				// Standing legs belong to the planner: planted feet hold hard and heavily damped, a
				// swinging leg's knee goes compliant. Eased, never switched in one frame.
				const int32 Side = Bodies[Index].Bone.ToString().EndsWith(TEXT("_l")) ? 0 : 1;
				const bool bFoot = Index == FootBody[Side];
				const float Wanted = bFoot ? 3.0f : (Balance.Feet[Side].bSwing ? 0.3f : 1.0f);
				LegScale[Index] = FMath::Lerp(LegScale[Index], Wanted, 0.35f);
				WorldScale = LegScale[Index];
				WorldAngular = bFoot ? 2.5f : LegScale[Index];
				Damping = bFoot ? 3.0f : 1.5f;
			}
			Character->SetStrength(Index, WorldScale, Strength.Joint, WorldAngular, Damping, Strength.JointDamping);
		}

		const TArray<FTransform>& Target = bReleased ? ReleasePose : Pose;
		Character->SetTargets(Target, Reference, Balance.Offset, Overrides, Dt);
		RecentTargets[Frame % TargetWindow] = Target;

		// Delivery.
		if (SinceHit >= 0)
		{
			for (const FImpactHit& Hit : Setup.Schedule.Hits)
			{
				const int32 Duration = HitFrames(Hit);
				const int32 Into = SinceHit - Hit.FrameOffset;
				if (Into < 0 || Into >= Duration)
				{
					continue;
				}
				const FVector Direction = CharacterToWorld.RotateVector(Hit.DirectionLocal).GetSafeNormal();
				const FVector Center = Bodies[Struck].Instance->GetCOMPosition();
				const float Radius = Character->BodyRadius(Struck);
				FImpactRandom HitRandom(Setup.BatchSeed, Setup.ClipIndex * 64 + Hit.FrameOffset, EImpactRandomStream::Contact);
				const FVector Contact = FImpactSurfaceContact::Contact(Center, Radius, Direction,
					FImpactSurfaceContact::AimOffset(Direction, Radius, 0.8f, HitRandom));

				TArray<FImpactBodyProxy> Proxies;
				for (int32 Index = 0; Index < Bodies.Num(); ++Index)
				{
					Proxies.Add({ Bodies[Index].Bone, Bodies[Index].Instance ? Bodies[Index].Instance->GetCOMPosition() : FVector::ZeroVector, Character->BodyRadius(Index),
							Bodies[Index].Instance ? Bodies[Index].Instance->GetBodyMass() : 0.0f });
				}
				// The physical push lands where the impactor does, over its own contact area; the part the
				// reaction gain adds is a regional response, spread at least ReactionSpreadRadiusCm.
				const float HitImpulse = Hit.ImpulseKgCmPerSecond / Duration;
				const float Physical = FMath::Min(HitImpulse, HitImpulse / FMath::Max(Setup.Params.ReactionGain, 1.0f));
				const float Reaction = HitImpulse - Physical;
				Character->ApplyShares(FImpactContactSpread::Distribute(Proxies, Struck, Contact, Setup.Params.Impactor.ContactRadiusCm, Physical), Direction);
				if (Reaction > 0.0f)
				{
					const float SpreadRadius = FMath::Max(Setup.Params.Impactor.ContactRadiusCm, Setup.ReactionSpreadRadiusCm);
					Character->ApplyShares(FImpactContactSpread::Distribute(Proxies, Struck, Contact, SpreadRadius, Reaction), Direction);
				}
			}

			if (Setup.Schedule.Projectile.IsSet())
			{
				const FImpactProjectileSpec& Spec = Setup.Schedule.Projectile.GetValue();
				const FVector Center = Bodies[Struck].Instance->GetCOMPosition();
				if (SinceHit == 0)
				{
					Projectile = SpawnProjectile(World, Spec, Center - BaseDirection * Spec.LaunchDistanceCm, BaseDirection);
				}
				if (SinceHit == ProjectileArrival && Spec.ExtraReactionImpulseKgCmPerSecond > 0.0f)
				{
					// The collision carries the physical momentum; the reaction gain's extra arrives with it.
					const FVector Contact = FImpactSurfaceContact::Contact(Center, Character->BodyRadius(Struck), BaseDirection, FVector::ZeroVector);
					TArray<FImpactBodyProxy> Proxies;
					for (int32 Index = 0; Index < Bodies.Num(); ++Index)
					{
						Proxies.Add({ Bodies[Index].Bone, Bodies[Index].Instance ? Bodies[Index].Instance->GetCOMPosition() : FVector::ZeroVector, Character->BodyRadius(Index),
								Bodies[Index].Instance ? Bodies[Index].Instance->GetBodyMass() : 0.0f });
					}
					const float SpreadRadius = FMath::Max(Spec.SizeCm * 0.5f, Setup.ReactionSpreadRadiusCm);
					Character->ApplyShares(FImpactContactSpread::Distribute(Proxies, Struck, Contact, SpreadRadius, Spec.ExtraReactionImpulseKgCmPerSecond), BaseDirection);
				}
			}
		}

		SimWorld.Step(Dt, Setup.SimSubsteps);

		// Measure and record.
		RecentTargetViews.Reset();
		for (int32 Back = 0; Back < FMath::Min(Frame + 1, TargetWindow); ++Back)
		{
			RecentTargetViews.Add(&RecentTargets[(Frame - Back) % TargetWindow]);
		}
		Character->CollectSamples(Pose, RecentTargetViews, Samples);
		for (const FImpactBodySample& Sample : Samples)
		{
			if (Sample.JointLimitExcessDeg > Out.MaxJointLimitExcessDeg)
			{
				Out.MaxJointLimitExcessDeg = Sample.JointLimitExcessDeg;
				Out.MaxJointLimitExcessBody = Sample.Name;
			}
		}
		FText Invalid;
		if (!FImpactValidity::Check(Samples, SimWorld.GetGroundHeight(), Setup.Validity, Invalid))
		{
			Window.Invalidate(Invalid);
		}

		const TArrayView<FTransform> BodyPose = Out.BodyPoses.AddFrame();
		const TArrayView<FTransform> SourcePose = Out.SourcePoses.AddFrame();
		if (BodyPose.IsEmpty() || SourcePose.IsEmpty())
		{
			Window.Invalidate(Out.BodyPoses.HasOverflowed() ? Out.BodyPoses.GetOverflowReason() : Out.SourcePoses.GetOverflowReason());
			break;
		}
		for (int32 Index = 0; Index < Bodies.Num(); ++Index)
		{
			BodyPose[Index] = Character->BodyTransform(Index);
		}
		FMemory::Memcpy(SourcePose.GetData(), Pose.GetData(), sizeof(FTransform) * FMath::Min(Pose.Num(), SourcePose.Num()));

		FImpactRecordedFrame Recorded;
		Recorded.Offset = Balance.Offset;
		Recorded.bReleased = bReleased;
		FTransform SourceRoot = Pose[0];
		SourceRoot.AddToTranslation(Reference.InverseTransformVectorNoScale(Balance.Offset));
		const int32 PelvisBody = Character->FindBody(TEXT("pelvis"));
		const FTransform PelvisNow = PelvisBody != INDEX_NONE ? Character->BodyTransform(PelvisBody).GetRelativeTransform(Reference) : Pose[PelvisBone];
		Recorded.Root = RootTracker.Step(SourceRoot, PelvisNow, bReleased);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			Recorded.bFootPlaced[Side] = Overrides.Contains(bHaveFeet ? FootBody[Side] : INDEX_NONE);
			Recorded.FootTarget[Side] = Recorded.bFootPlaced[Side] ? Overrides[FootBody[Side]] : FVector::ZeroVector;
		}
		Out.Frames.Add(Recorded);

		// Averaged over the recorded frame, not read at its last physics substep: substeps pick up
		// contact jitter, and a run measured instantaneously never read as settled.
		const FVector Com = Character->CenterOfMass();
		const FVector ComVelocity = (Com - LastCom) / Dt;
		LastCom = Com;
		const float Speed = bReleased ? float(ComVelocity.Size()) : float((ComVelocity - SourceVelocity).Size());
		if (SinceHit >= 0)
		{
			FImpactFrameState State;
			State.PelvisHeightCm = PelvisBody != INDEX_NONE ? float(Character->BodyTransform(PelvisBody).GetLocation().Z - SimWorld.GetGroundHeight()) : 0.0f;
			State.StandingPelvisHeightCm = float(Pose[PelvisBone].GetLocation().Z);
			const FVector FeetMid = bHaveFeet ? (Character->BodyTransform(FootBody[0]).GetLocation() + Character->BodyTransform(FootBody[1]).GetLocation()) * 0.5 : Character->CenterOfMass();
			State.bOutsideSupport = !bMoving && FVector::Dist2D(Balance.CapturePoint, FeetMid) > Setup.Balance.StepTriggerCm;
			State.Steps = Balance.Steps;
			State.bControlsReleased = bReleased;
			State.bTrip = Balance.bTrip;
			OutcomeFrames.Add(State);
		}
		Window.Advance(SinceHit >= 0 ? Speed : 0.0f);
		Clock.Advance(Frame, Dt);
	}

	Out.End = Window.GetEnd();
	Out.InvalidReason = Window.GetInvalidReason();
	Out.Outcome = FImpactOutcomeClassifier::Classify(OutcomeFrames, Setup.Outcome);
	return true;
}

#undef LOCTEXT_NAMESPACE
