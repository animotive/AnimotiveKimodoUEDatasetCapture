// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "DatasetSkeletonFingerprint.h"
#include "Engine/SkeletalMesh.h"
#include "ImpactClipSimulator.h"
#include "ImpactSidecarBuilder.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"

namespace ImpactSidecarBuilderTests
{
	/** A run clip shot in the left foot by a two-hit burst, set up and recorded by hand. */
	void MakeClip(const FImpactorArchetype& Bullet, USkeletalMesh* Mesh, UAnimSequence* Source, FImpactClipSetup& Setup, FImpactClipRecording& Recording)
	{
		Setup.Mesh = Mesh;
		Setup.Source = Source;
		Setup.BatchSeed = 3;
		Setup.ClipIndex = 7;
		Setup.FrameRate = 60;
		Setup.Strength.WorldStrength = 8.0f;
		Setup.Strength.ParentStrength = 16.0f;
		Setup.Strength.ReleasedToneStart = 0.14f;
		Setup.Strength.ReleasedToneEnd = 0.042f;

		FImpactParams& Params = Setup.Params;
		Params.Impactor.Archetype = &Bullet;
		Params.Impactor.MassKg = 0.008f;
		Params.Impactor.SpeedMetresPerSecond = 370.0f;
		Params.Impactor.ContactRadiusCm = 0.75f;
		Params.TransferredImpulseKgCmPerSecond = 302.4f;
		Params.ReactionGain = 60.0f;
		Params.ReactionImpulseKgCmPerSecond = 18142.0f;
		Params.Region = EImpactBodyRegion::FootLeft;
		Params.DirectionLocal = FVector(-1, 0, 0);
		Params.SourcePhase = 0.25f;
		Params.Intensity = EImpactIntensity::Medium;

		Setup.Schedule.Mode = EImpactDeliveryMode::Burst;
		Setup.Schedule.Hits.Add({ 0, 0, 10000.0f, FVector(-1, 0, 0) });
		Setup.Schedule.Hits.Add({ 3, 0, 8142.0f, FVector(-0.9, 0.1, 0).GetSafeNormal() });

		Recording.Frames.SetNum(109);
		Recording.ImpactFrame = 31;
		Recording.bMovingSource = true;
		Recording.StruckBody = TEXT("foot_l");
		Recording.CharacterToWorld = FQuat(FVector::UpVector, FMath::DegreesToRadians(90.0));
		Recording.End = EImpactClipEnd::Settled;
		Recording.Outcome.bValid = true;
		Recording.Outcome.Outcome = EImpactOutcome::Knockdown;
		Recording.Outcome.OutcomeFrame = 11;
		Recording.Outcome.Steps = 0;
		Recording.Outcome.bTrip = true;
	}

	FImpactSourceMotion Running()
	{
		FImpactSourceMotion Motion;
		Motion.Participle = TEXT("running");
		Motion.Type = TEXT("run");
		Motion.bTravels = true;
		return Motion;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactSidecarBuilderTest,
	"KimodoDatasetCapture.Impact.Sidecar.Build",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactSidecarBuilderTest::RunTest(const FString& Parameters)
{
	using namespace ImpactSidecarBuilderTests;
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin"));
	UAnimSequence* Source = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F.M_Neutral_Run_Loop_F"));
	if (!Mesh || !Source)
	{
		AddWarning(TEXT("GameAnimationSample content not found; sidecar builder test skipped."));
		return true;
	}
	const TArray<FImpactorArchetype> Archetypes = UImpactorCatalog::MakeStarterArchetypes();
	const FImpactorArchetype* Bullet = Archetypes.FindByPredicate([](const FImpactorArchetype& A) { return A.Id == FName(TEXT("SmallBullet")); });
	if (!TestNotNull(TEXT("Small bullet archetype"), Bullet))
	{
		return false;
	}

	FImpactClipSetup Setup;
	FImpactClipRecording Recording;
	MakeClip(*Bullet, Mesh, Source, Setup, Recording);
	FImpactSidecarContext Context;
	Context.ClipId = TEXT("imp_0123456789abcdef");
	Context.AnimSequencePath = TEXT("/Game/ImpactDataset/Batch01/imp_0123456789abcdef.imp_0123456789abcdef");
	Context.Motion = Running();

	const FDatasetSidecar Sidecar = FImpactSidecarBuilder::Build(Setup, Recording, Context);

	// Source and identity.
	TestTrue(TEXT("A synthetic impact clip"), Sidecar.IsSyntheticImpact());
	TestEqual(TEXT("Current schema"), Sidecar.SchemaVersion, KDatasetSidecarSchemaVersion);
	TestEqual(TEXT("Named by its clip id"), Sidecar.Identity.TakeName, Context.ClipId);
	TestEqual(TEXT("Points at the baked asset"), Sidecar.Identity.AnimSequencePath, Context.AnimSequencePath);
	TestEqual(TEXT("Frame rate numerator"), Sidecar.Identity.FrameRateNumerator, 60);
	TestEqual(TEXT("Frame rate denominator"), Sidecar.Identity.FrameRateDenominator, 1);
	TestEqual(TEXT("Duration is the recorded frames"), Sidecar.Identity.DurationFrames, 109);
	TestTrue(TEXT("No raw input layers"), Sidecar.Layers.IsEmpty() && Sidecar.InputStream.Samples.IsEmpty());

	// Skeleton.
	TestEqual(TEXT("Skeleton path"), Sidecar.Skeleton.SkeletonPath, Mesh->GetSkeleton()->GetPathName());
	TestEqual(TEXT("Bone count"), Sidecar.Skeleton.BoneCount, Mesh->GetRefSkeleton().GetNum());
	TestEqual(TEXT("Bone fingerprint"), Sidecar.Skeleton.BoneHierarchyFingerprint, FDatasetSkeletonFingerprint::Compute(Mesh->GetRefSkeleton()));

	// Labels: from the caption generator, over the whole clip, valid.
	const FImpactCaptionInput CaptionInput = FImpactSidecarBuilder::CaptionInput(Setup, Recording, Context.Motion);
	TestEqual(TEXT("Caption input impactor"), CaptionInput.Impactor, Bullet);
	TestTrue(TEXT("Caption input carries outcome and trip"), CaptionInput.Outcome == EImpactOutcome::Knockdown && CaptionInput.bTrip);
	TestEqual(TEXT("Caption from the caption generator"), Sidecar.Labels.Caption, FImpactCaption::Describe(CaptionInput, 3, 7));
	TestTrue(TEXT("Caption is written"), Sidecar.Labels.Caption.Contains(TEXT("a small bullet")));
	TestEqual(TEXT("Tags from the caption generator"), Sidecar.Labels.Tags, FImpactCaption::Tags(CaptionInput));
	TestEqual(TEXT("Trim starts at the first frame"), Sidecar.Labels.TrimInFrame, 0);
	TestEqual(TEXT("Trim ends at the last frame"), Sidecar.Labels.TrimOutFrame, 108);
	TestTrue(TEXT("A settled clip is valid"), Sidecar.Labels.bIsValid);

	// The impact block.
	const FDatasetSidecarImpact& Impact = Sidecar.Impact;
	TestEqual(TEXT("Clip id"), Impact.ClipId, Context.ClipId);
	TestEqual(TEXT("Seed"), Impact.BatchSeed, 3);
	TestEqual(TEXT("Clip index"), Impact.ClipIndex, 7);
	TestEqual(TEXT("Source animation"), Impact.SourceAnimation, Source->GetPathName());
	TestEqual(TEXT("Source start time"), Impact.SourceStartSeconds, float(0.25 * Source->GetPlayLength()), 1e-4f);
	TestEqual(TEXT("Source motion"), Impact.SourceMotion, FString(TEXT("running")));
	TestEqual(TEXT("Intensity"), Impact.Intensity, FString(TEXT("Medium")));
	TestEqual(TEXT("Transferred impulse"), Impact.TransferredImpulseKgCmPerSecond, 302.4f);
	TestEqual(TEXT("Reaction gain"), Impact.ReactionGain, 60.0f);
	TestEqual(TEXT("Control world strength"), Impact.Control.WorldStrength, 8.0f);
	TestEqual(TEXT("Control parent strength"), Impact.Control.ParentStrength, 16.0f);
	TestEqual(TEXT("Control tone start"), Impact.Control.ReleasedToneStart, 0.14f);
	TestEqual(TEXT("Control tone end"), Impact.Control.ReleasedToneEnd, 0.042f);
	TestEqual(TEXT("Outcome"), Impact.Outcome, FString(TEXT("Knockdown")));
	TestEqual(TEXT("Outcome frame"), Impact.OutcomeFrame, 11);
	TestTrue(TEXT("Trip"), Impact.bTrip);
	TestEqual(TEXT("End"), Impact.End, FString(TEXT("Settled")));

	if (TestEqual(TEXT("One entry per hit"), Impact.Hits.Num(), 2))
	{
		const FDatasetSidecarImpactHit& First = Impact.Hits[0];
		const FDatasetSidecarImpactHit& Second = Impact.Hits[1];
		TestEqual(TEXT("First hit on the impact frame"), First.Frame, 31);
		TestEqual(TEXT("Second hit at its offset"), Second.Frame, 34);
		TestEqual(TEXT("An instant hit lasts the impactor's contact time"), First.DurationFrames, Bullet->ContactFrames(60));
		TestEqual(TEXT("Impactor id"), First.ImpactorId, FString(TEXT("SmallBullet")));
		TestEqual(TEXT("Impactor noun"), First.ImpactorNoun, Bullet->NounPhrase);
		TestEqual(TEXT("Impactor description"), First.ImpactorDescription, Bullet->Description);
		TestEqual(TEXT("Impactor size"), First.ImpactorSizeCm, Bullet->SizeCm);
		TestEqual(TEXT("Impactor mass"), First.ImpactorMassKg, 0.008f);
		TestEqual(TEXT("Impactor speed"), First.ImpactorSpeedMetresPerSecond, 370.0f);
		TestEqual(TEXT("Region"), First.Region, FString(TEXT("FootLeft")));
		TestEqual(TEXT("Struck body"), First.Body, FString(TEXT("foot_l")));
		TestTrue(TEXT("Local direction"), Second.DirectionLocal.Equals(Setup.Schedule.Hits[1].DirectionLocal, 1e-5));
		TestTrue(TEXT("World direction turned by the character's orientation"),
			First.DirectionWorld.Equals(Recording.CharacterToWorld.RotateVector(FVector(-1, 0, 0)), 1e-5));
		TestEqual(TEXT("Each hit's own impulse"), Second.ImpulseKgCmPerSecond, 8142.0f);
		TestEqual(TEXT("Contact radius"), First.ContactRadiusCm, 0.75f);
		TestEqual(TEXT("Delivery mode"), First.DeliveryMode, FString(TEXT("Burst")));
		TestFalse(TEXT("Not a projectile"), First.bProjectile);
	}

	// A projectile clip: one hit describing the thrown object.
	{
		FImpactClipSetup Thrown = Setup;
		Thrown.Schedule.Hits.Reset();
		Thrown.Schedule.Mode = EImpactDeliveryMode::Projectile;
		FImpactProjectileSpec Spec;
		Spec.Shape = EImpactorShape::Sphere;
		Spec.SizeCm = 7.4f;
		Spec.MassKg = 0.145f;
		Spec.SpeedMetresPerSecond = 35.0f;
		Spec.LaunchDistanceCm = 150.0f;
		Thrown.Schedule.Projectile = Spec;
		const FDatasetSidecar Ball = FImpactSidecarBuilder::Build(Thrown, Recording, Context);
		if (TestEqual(TEXT("A projectile is one hit"), Ball.Impact.Hits.Num(), 1))
		{
			const FDatasetSidecarImpactHit& Hit = Ball.Impact.Hits[0];
			TestTrue(TEXT("Marked as a projectile"), Hit.bProjectile);
			TestEqual(TEXT("Projectile shape"), Hit.ProjectileShape, FString(TEXT("Sphere")));
			TestEqual(TEXT("Launch distance"), Hit.ProjectileLaunchDistanceCm, 150.0f);
			TestEqual(TEXT("Launched on the impact frame"), Hit.Frame, 31);
			TestEqual(TEXT("Carries the reaction impulse"), Hit.ImpulseKgCmPerSecond, 18142.0f);
			TestEqual(TEXT("Delivery mode"), Hit.DeliveryMode, FString(TEXT("Projectile")));
		}
	}

	// An invalidated clip says so, with its reason.
	{
		FImpactClipRecording Broken = Recording;
		Broken.End = EImpactClipEnd::Invalidated;
		Broken.InvalidReason = FText::FromString(TEXT("Body lowerarm_r: bent 23 degrees past its joint limit."));
		const FDatasetSidecar Invalid = FImpactSidecarBuilder::Build(Setup, Broken, Context);
		TestFalse(TEXT("An invalidated clip is not valid"), Invalid.Labels.bIsValid);
		TestEqual(TEXT("and carries its reason"), Invalid.Labels.InvalidReason, Broken.InvalidReason.ToString());
		TestEqual(TEXT("End is Invalidated"), Invalid.Impact.End, FString(TEXT("Invalidated")));
	}
	return true;
}
