// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "ImpactCaption.h"
#include "ImpactClipSimulator.h"
#include "ImpactOutcome.h"
#include "ImpactParams.h"
#include "ImpactRegionTable.h"
#include "ImpactSchedule.h"
#include "ImpactSimWorld.h"
#include "ImpactStrengthProfile.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/StrongObjectPtr.h"

namespace ImpactCollapseTests
{
	const FImpactorArchetype* Find(const TArray<FImpactorArchetype>& Archetypes, const TCHAR* Id)
	{
		return Archetypes.FindByPredicate([Id](const FImpactorArchetype& A) { return A.Id == FName(Id); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactCollapseRuleTest,
	"KimodoDatasetCapture.Impact.Collapse.Rule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactCollapseRuleTest::RunTest(const FString& Parameters)
{
	using namespace ImpactCollapseTests;
	const TArray<FImpactorArchetype> Archetypes = UImpactorCatalog::MakeStarterArchetypes();
	for (const TCHAR* Bullet : { TEXT("SmallBullet"), TEXT("RifleRound"), TEXT("ShotgunBlast") })
	{
		const FImpactorArchetype* Archetype = Find(Archetypes, Bullet);
		if (!TestNotNull(Bullet, Archetype))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s to the head collapses"), Bullet), Archetype->Collapses(EImpactBodyRegion::Head));
		TestTrue(*FString::Printf(TEXT("%s to the neck collapses"), Bullet), Archetype->Collapses(EImpactBodyRegion::Neck));
		TestFalse(*FString::Printf(TEXT("%s to the chest does not"), Bullet), Archetype->Collapses(EImpactBodyRegion::Chest));
		TestFalse(*FString::Printf(TEXT("%s to a thigh does not"), Bullet), Archetype->Collapses(EImpactBodyRegion::ThighLeft));
	}
	for (const FImpactorArchetype& Archetype : Archetypes)
	{
		if (!Archetype.bIncapacitatesHeadAndNeck)
		{
			TestFalse(*FString::Printf(TEXT("%s to the head does not collapse"), *Archetype.Id.ToString()), Archetype.Collapses(EImpactBodyRegion::Head));
		}
	}
	TestFalse(TEXT("A punch is not incapacitating"), Find(Archetypes, TEXT("Punch"))->bIncapacitatesHeadAndNeck);
	TestFalse(TEXT("Nor is a bat"), Find(Archetypes, TEXT("SwingingBat"))->bIncapacitatesHeadAndNeck);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactCollapseStrengthTest,
	"KimodoDatasetCapture.Impact.Collapse.Strength",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactCollapseStrengthTest::RunTest(const FString& Parameters)
{
	const FImpactStrengthProfile Profile;
	auto Tone = [&Profile](bool bLeg, bool bCore, int32 Since)
	{
		FImpactBodyStrengthQuery Query;
		Query.bLeg = bLeg;
		Query.bCore = bCore;
		Query.bReleased = true;
		Query.bCollapsed = true;
		Query.FramesSinceRelease = Since;
		return Profile.Evaluate(Query, 5 + Since);
	};
	for (int32 Since : { 0, 20, 90 })
	{
		const FImpactBodyStrength Leg = Tone(true, false, Since);
		const FImpactBodyStrength Arm = Tone(false, false, Since);
		const FImpactBodyStrength Core = Tone(false, true, Since);
		TestEqual(*FString::Printf(TEXT("No world hold %d frames into the collapse"), Since), Leg.World + Arm.World + Core.World, 0.0f);
		TestEqual(TEXT("The legs give way"), Leg.Joint, Profile.CollapseLegTone, 1e-5f);
		TestEqual(TEXT("The arms hang"), Arm.Joint, Profile.CollapseArmTone, 1e-5f);
		TestEqual(TEXT("The trunk and neck stay braced"), Core.Joint, Profile.CollapseCoreTone, 1e-5f);
		TestEqual(TEXT("Overdamped like a ragdoll"), Core.JointDamping, Profile.ReleasedJointDamping, 1e-5f);
	}
	TestTrue(TEXT("Legs weakest, trunk strongest"), Profile.CollapseLegTone < Profile.CollapseArmTone && Profile.CollapseArmTone < Profile.CollapseCoreTone);
	TestTrue(TEXT("The trunk is firmer than the firmest ragdoll's"),
		Profile.CollapseCoreTone > FImpactStrengthDistribution().ReleasedTone.Max * Profile.ReleasedCoreToneScale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactCollapseOutcomeTest,
	"KimodoDatasetCapture.Impact.Collapse.Outcome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactCollapseOutcomeTest::RunTest(const FString& Parameters)
{
	TArray<FImpactFrameState> Frames;
	for (int32 Index = 0; Index < 60; ++Index)
	{
		FImpactFrameState Frame;
		Frame.StandingPelvisHeightCm = 95.0f;
		Frame.PelvisHeightCm = FMath::Max(20.0f, 95.0f - 3.0f * FMath::Max(0, Index - 2));
		Frame.bCollapsed = Index >= 2;
		Frame.bControlsReleased = Index >= 2;
		Frames.Add(Frame);
	}
	const FImpactOutcomeResult Result = FImpactOutcomeClassifier::Classify(Frames, FImpactOutcomeThresholds());
	TestEqual(TEXT("A collapse is its own outcome, not a ragdoll or a knockdown"), Result.Outcome, EImpactOutcome::Collapse);
	TestEqual(TEXT("Decided when it collapsed"), Result.OutcomeFrame, 2);

	// Captioned as a collapse.
	const TArray<FImpactorArchetype> Archetypes = UImpactorCatalog::MakeStarterArchetypes();
	FImpactCaptionInput Input;
	Input.Impactor = ImpactCollapseTests::Find(Archetypes, TEXT("SmallBullet"));
	Input.Region = EImpactBodyRegion::Head;
	Input.Outcome = EImpactOutcome::Collapse;
	for (int32 Clip = 0; Clip < 12; ++Clip)
	{
		const FString Caption = FImpactCaption::Describe(Input, 3, Clip);
		TestTrue(*FString::Printf(TEXT("Caption says it collapses: %s"), *Caption),
			Caption.Contains(TEXT("collapse")) || Caption.Contains(TEXT("crumple")) || Caption.Contains(TEXT("drops")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactCollapseSimulationTest,
	"KimodoDatasetCapture.Impact.Collapse.HeadShotCollapses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactCollapseSimulationTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin"));
	UPhysicsAsset* Physics = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Rigs/PA_UEFN_Mannequin.PA_UEFN_Mannequin"));
	UAnimSequence* Stand = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Kimodo/ImpactSources/M_Kimodo_Stand_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin.M_Kimodo_Stand_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin"));
	if (!SkeletalMesh || !Physics || !Stand)
	{
		AddWarning(TEXT("UEFN mannequin or Kimodo sources not found; collapse simulation skipped."));
		return true;
	}
	TStrongObjectPtr<UImpactorCatalog> Catalog(NewObject<UImpactorCatalog>());
	Catalog->FillWithStarterArchetypes();
	TStrongObjectPtr<UImpactRegionTable> Regions(NewObject<UImpactRegionTable>());
	Regions->FillWithUEFNMannequinEntries();
	const FImpactorArchetype* Bullet = ImpactCollapseTests::Find(Catalog->Archetypes, TEXT("SmallBullet"));

	auto Simulate = [&](EImpactBodyRegion Region, FImpactClipRecording& Out)
	{
		FImpactClipSetup Setup;
		Setup.Mesh = SkeletalMesh;
		Setup.PhysicsAsset = Physics;
		Setup.Source = Stand;
		Setup.RegionTable = Regions.Get();
		Setup.BatchSeed = 11;
		Setup.MaxReactionFrames = 180;
		FImpactParams& Params = Setup.Params;
		Params.Impactor.Archetype = Bullet;
		// The lightest round: a collapse does not depend on the hit's strength.
		Params.Impactor.MassKg = Bullet->MassKg.Min;
		Params.Impactor.SpeedMetresPerSecond = Bullet->SpeedMetresPerSecond.Min;
		Params.Impactor.ContactRadiusCm = Bullet->ContactRadiusCm.Min;
		Params.TransferredImpulseKgCmPerSecond = FImpactParamsSampler::TransferredImpulse(Params.Impactor.MassKg, Params.Impactor.SpeedMetresPerSecond, 75.0f, 0.2f);
		Params.ReactionGain = Bullet->ReactionGain;
		Params.ReactionImpulseKgCmPerSecond = Params.TransferredImpulseKgCmPerSecond * Params.ReactionGain;
		Params.Intensity = FImpactIntensityThresholds().Classify(Params.ReactionImpulseKgCmPerSecond);
		Params.Region = Region;
		Params.DirectionLocal = FVector(-1.0, 0.0, 0.0);
		Params.SourcePhase = 0.3f;
		FText Reason;
		FImpactSimWorld World;
		return FImpactScheduleSampler::Sample(Params, FImpactDeliveryDistributions(), 11, 0, Setup.Schedule, Reason)
			&& FImpactClipSimulator::Run(World, Setup, Out, Reason);
	};

	FImpactClipRecording Head, Chest;
	if (!TestTrue(TEXT("A head shot simulates"), Simulate(EImpactBodyRegion::Head, Head))
		|| !TestTrue(TEXT("A chest shot simulates"), Simulate(EImpactBodyRegion::Chest, Chest)))
	{
		return true;
	}
	TestEqual(TEXT("Shot in the head, a standing person collapses"), Head.Outcome.Outcome, EImpactOutcome::Collapse);
	TestEqual(TEXT("A couple of frames after the hit"), Head.Outcome.OutcomeFrame, 2);
	TestTrue(*FString::Printf(TEXT("And goes down (pelvis at %.2f of standing)"), Head.Outcome.MinPelvisFraction), Head.Outcome.MinPelvisFraction < 0.6f);
	TestTrue(*FString::Printf(TEXT("Without breaking (%s)"), *Head.InvalidReason.ToString()), Head.End != EImpactClipEnd::Invalidated);
	TestNotEqual(TEXT("Shot in the chest, the same round does not collapse"), Chest.Outcome.Outcome, EImpactOutcome::Collapse);
	return true;
}
