// Copyright Animotive. All Rights Reserved.

#include "ImpactSidecarBuilder.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "DatasetSkeletonFingerprint.h"
#include "Engine/SkeletalMesh.h"
#include "ImpactClipSimulator.h"
#include "ImpactorCatalog.h"

namespace
{
	template <typename TEnum>
	FString EnumName(TEnum Value)
	{
		return StaticEnum<TEnum>()->GetNameStringByValue(int64(Value));
	}

	/** The impactor's identity on a hit entry. */
	FDatasetSidecarImpactHit HitBase(const FImpactClipSetup& Setup, const FImpactClipRecording& Recording)
	{
		const FImpactorSample& Impactor = Setup.Params.Impactor;
		FDatasetSidecarImpactHit Hit;
		if (const FImpactorArchetype* Archetype = Impactor.Archetype)
		{
			Hit.ImpactorId = Archetype->Id.ToString();
			Hit.ImpactorNoun = Archetype->NounPhrase;
			Hit.ImpactorDescription = Archetype->Description;
			Hit.ImpactorSizeCm = Archetype->SizeCm;
		}
		Hit.ImpactorMassKg = Impactor.MassKg;
		Hit.ImpactorSpeedMetresPerSecond = Impactor.SpeedMetresPerSecond;
		Hit.ContactRadiusCm = Impactor.ContactRadiusCm;
		Hit.Region = EnumName(Setup.Params.Region);
		Hit.Body = Recording.StruckBody.ToString();
		Hit.DeliveryMode = EnumName(Setup.Schedule.Mode);
		return Hit;
	}
}

FImpactCaptionInput FImpactSidecarBuilder::CaptionInput(const FImpactClipSetup& Setup, const FImpactClipRecording& Recording, const FImpactSourceMotion& Motion)
{
	FImpactCaptionInput Input;
	Input.Impactor = Setup.Params.Impactor.Archetype;
	Input.Motion = Motion;
	Input.Region = Setup.Params.Region;
	Input.DirectionLocal = Setup.Params.DirectionLocal;
	Input.Intensity = Setup.Params.Intensity;
	Input.Outcome = Recording.Outcome.Outcome;
	Input.bTrip = Recording.Outcome.bTrip;
	Input.Steps = Recording.Outcome.Steps;
	return Input;
}

FDatasetSidecar FImpactSidecarBuilder::Build(const FImpactClipSetup& Setup, const FImpactClipRecording& Recording, const FImpactSidecarContext& Context)
{
	FDatasetSidecar Sidecar;
	Sidecar.ClipSource = DatasetClipSource::SyntheticImpact;

	const int32 Frames = Recording.Frames.Num();
	const int32 FrameRate = FMath::Max(1, Setup.FrameRate);
	Sidecar.Identity.TakeName = Context.ClipId;
	Sidecar.Identity.AnimSequencePath = Context.AnimSequencePath;
	Sidecar.Identity.FrameRateNumerator = FrameRate;
	Sidecar.Identity.FrameRateDenominator = 1;
	Sidecar.Identity.TickResolutionNumerator = FrameRate;
	Sidecar.Identity.TickResolutionDenominator = 1;
	Sidecar.Identity.DurationFrames = Frames;

	if (Setup.Mesh)
	{
		const FReferenceSkeleton& Ref = Setup.Mesh->GetRefSkeleton();
		Sidecar.Skeleton.SkeletonPath = Setup.Mesh->GetSkeleton() ? Setup.Mesh->GetSkeleton()->GetPathName() : FString();
		Sidecar.Skeleton.BoneCount = Ref.GetNum();
		Sidecar.Skeleton.BoneHierarchyFingerprint = FDatasetSkeletonFingerprint::Compute(Ref);
	}

	const FImpactCaptionInput Caption = CaptionInput(Setup, Recording, Context.Motion);
	Sidecar.Labels.Caption = FImpactCaption::Describe(Caption, Setup.BatchSeed, Setup.ClipIndex);
	Sidecar.Labels.Tags = FImpactCaption::Tags(Caption);
	Sidecar.Labels.TrimInFrame = 0;
	Sidecar.Labels.TrimOutFrame = FMath::Max(0, Frames - 1);
	Sidecar.Labels.bIsValid = Recording.End != EImpactClipEnd::Invalidated;
	Sidecar.Labels.InvalidReason = Sidecar.Labels.bIsValid ? FString() : Recording.InvalidReason.ToString();

	FDatasetSidecarImpact& Impact = Sidecar.Impact;
	Impact.ClipId = Context.ClipId;
	Impact.BatchSeed = Setup.BatchSeed;
	Impact.ClipIndex = Setup.ClipIndex;
	if (Setup.Source)
	{
		Impact.SourceAnimation = Setup.Source->GetPathName();
		Impact.SourceStartSeconds = float(double(Setup.Params.SourcePhase) * Setup.Source->GetPlayLength());
	}
	Impact.SourceMotion = Context.Motion.Participle;
	Impact.Intensity = EnumName(Setup.Params.Intensity);
	Impact.TransferredImpulseKgCmPerSecond = Setup.Params.TransferredImpulseKgCmPerSecond;
	Impact.ReactionGain = Setup.Params.ReactionGain;
	Impact.Control.WorldStrength = Setup.Strength.WorldStrength;
	Impact.Control.ParentStrength = Setup.Strength.ParentStrength;
	Impact.Control.ReleasedToneStart = Setup.Strength.ReleasedToneStart;
	Impact.Control.ReleasedToneEnd = Setup.Strength.ReleasedToneEnd;
	Impact.Outcome = EnumName(Recording.Outcome.Outcome);
	Impact.OutcomeFrame = Recording.Outcome.OutcomeFrame;
	Impact.Steps = Recording.Outcome.Steps;
	Impact.bTrip = Recording.Outcome.bTrip;
	Impact.bOverwhelmed = Recording.bOverwhelmed;
	Impact.End = EnumName(Recording.End);

	// A single-frame hit is pushed over the impactor's contact time, as the simulator does.
	const int32 ContactFrames = Setup.Params.Impactor.Archetype ? Setup.Params.Impactor.Archetype->ContactFrames(FrameRate) : 1;
	for (const FImpactHit& Source : Setup.Schedule.Hits)
	{
		FDatasetSidecarImpactHit Hit = HitBase(Setup, Recording);
		Hit.Frame = Recording.ImpactFrame + Source.FrameOffset;
		Hit.DurationFrames = Source.DurationFrames > 0 ? Source.DurationFrames : ContactFrames;
		Hit.DirectionLocal = Source.DirectionLocal;
		Hit.DirectionWorld = Recording.CharacterToWorld.RotateVector(Source.DirectionLocal);
		Hit.ImpulseKgCmPerSecond = Source.ImpulseKgCmPerSecond;
		Impact.Hits.Add(Hit);
	}
	if (Setup.Schedule.Projectile.IsSet())
	{
		const FImpactProjectileSpec& Spec = Setup.Schedule.Projectile.GetValue();
		FDatasetSidecarImpactHit Hit = HitBase(Setup, Recording);
		Hit.Frame = Recording.ImpactFrame;
		Hit.DurationFrames = 1;
		Hit.DirectionLocal = Setup.Params.DirectionLocal;
		Hit.DirectionWorld = Recording.CharacterToWorld.RotateVector(Setup.Params.DirectionLocal);
		Hit.ImpulseKgCmPerSecond = Setup.Params.ReactionImpulseKgCmPerSecond;
		Hit.bProjectile = true;
		Hit.ProjectileShape = EnumName(Spec.Shape);
		Hit.ProjectileLaunchDistanceCm = Spec.LaunchDistanceCm;
		Impact.Hits.Add(Hit);
	}
	return Sidecar;
}
