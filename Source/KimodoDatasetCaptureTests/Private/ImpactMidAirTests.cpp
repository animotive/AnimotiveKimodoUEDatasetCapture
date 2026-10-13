// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "ImpactClipSimulator.h"
#include "ImpactParams.h"
#include "ImpactRegionTable.h"
#include "ImpactSchedule.h"
#include "ImpactSimWorld.h"
#include "ImpactSourceClock.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/StrongObjectPtr.h"

namespace ImpactMidAirTests
{
	const TCHAR* Mesh = TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin");
	const TCHAR* Jump = TEXT("/Game/Kimodo/ImpactSources/M_Kimodo_Jump_In_Place_KimodoRetargeted_SK_UEFN_Mannequin.M_Kimodo_Jump_In_Place_KimodoRetargeted_SK_UEFN_Mannequin");
	const TCHAR* Run = TEXT("/Game/Kimodo/ImpactSources/M_Kimodo_Run_Loop_F_S1_KimodoRetargeted_SK_UEFN_Mannequin.M_Kimodo_Run_Loop_F_S1_KimodoRetargeted_SK_UEFN_Mannequin");
	const TCHAR* Sprint = TEXT("/Game/Kimodo/ImpactSources/M_Kimodo_Sprint_Loop_F_L4_KimodoRetargeted_SK_UEFN_Mannequin.M_Kimodo_Sprint_Loop_F_L4_KimodoRetargeted_SK_UEFN_Mannequin");

	/** Times (60 Hz) at which the source is in flight. */
	TArray<double> FlightTimes(const USkeletalMesh& SkeletalMesh, const UAnimSequence& Sequence, bool bPlayOnce)
	{
		const FImpactPoseSampler Sampler(SkeletalMesh, Sequence, bPlayOnce);
		const FReferenceSkeleton& Ref = SkeletalMesh.GetRefSkeleton();
		const int32 Left = Ref.FindBoneIndex(TEXT("foot_l"));
		const int32 Right = Ref.FindBoneIndex(TEXT("foot_r"));
		TArray<double> Times;
		for (double Time = 0.0; Time < Sampler.GetLength(); Time += 1.0 / 60.0)
		{
			if (FImpactSourceFlight::IsInFlight(Sampler, Ref, Left, Right, Time))
			{
				Times.Add(Time);
			}
		}
		return Times;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactFlightRuleTest,
	"KimodoDatasetCapture.Impact.MidAir.FlightRule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactFlightRuleTest::RunTest(const FString& Parameters)
{
	const TArray<float> High = { 20.0f, 25.0f, 30.0f };
	const TArray<float> Dips = { 20.0f, 8.0f, 30.0f };
	TestTrue(TEXT("Both feet clear throughout: in flight"), FImpactSourceFlight::IsInFlight(High, High, 12.0f));
	TestFalse(TEXT("One foot touches down within the window: not in flight"), FImpactSourceFlight::IsInFlight(High, Dips, 12.0f));
	TestFalse(TEXT("Exactly at the threshold is not clear"), FImpactSourceFlight::IsInFlight(TArray<float>({ 12.0f }), TArray<float>({ 30.0f }), 12.0f));
	TestFalse(TEXT("No samples: not in flight"), FImpactSourceFlight::IsInFlight(TArray<float>(), TArray<float>(), 12.0f));

	using namespace ImpactMidAirTests;
	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, Mesh);
	UAnimSequence* JumpSequence = LoadObject<UAnimSequence>(nullptr, Jump);
	UAnimSequence* RunSequence = LoadObject<UAnimSequence>(nullptr, Run);
	UAnimSequence* SprintSequence = LoadObject<UAnimSequence>(nullptr, Sprint);
	if (!SkeletalMesh || !JumpSequence || !RunSequence || !SprintSequence)
	{
		AddWarning(TEXT("Kimodo sources not found; source flight checks skipped."));
		return true;
	}
	const TArray<double> JumpFlight = FlightTimes(*SkeletalMesh, *JumpSequence, true);
	TestTrue(*FString::Printf(TEXT("The jump has a flight (%d samples)"), JumpFlight.Num()), JumpFlight.Num() >= 6);
	TestFalse(TEXT("It is not in flight at its start"), JumpFlight.Contains(0.0));
	TestEqual(TEXT("Running strides are never a sustained flight"), FlightTimes(*SkeletalMesh, *RunSequence, false).Num(), 0);
	TestEqual(TEXT("Nor are sprinting strides"), FlightTimes(*SkeletalMesh, *SprintSequence, false).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactMidAirReleaseTest,
	"KimodoDatasetCapture.Impact.MidAir.HardHitReleases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactMidAirReleaseTest::RunTest(const FString& Parameters)
{
	using namespace ImpactMidAirTests;
	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, Mesh);
	UPhysicsAsset* Physics = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Rigs/PA_UEFN_Mannequin.PA_UEFN_Mannequin"));
	UAnimSequence* JumpSequence = LoadObject<UAnimSequence>(nullptr, Jump);
	if (!SkeletalMesh || !Physics || !JumpSequence)
	{
		AddWarning(TEXT("Kimodo jump not found; mid-air release test skipped."));
		return true;
	}
	TStrongObjectPtr<UImpactorCatalog> Catalog(NewObject<UImpactorCatalog>());
	Catalog->FillWithStarterArchetypes();
	TStrongObjectPtr<UImpactRegionTable> Regions(NewObject<UImpactRegionTable>());
	Regions->FillWithUEFNMannequinEntries();

	// The middle of the flight; the play-once start window puts the hit there at this phase.
	const TArray<double> Flight = FlightTimes(*SkeletalMesh, *JumpSequence, true);
	if (!TestTrue(TEXT("The jump has a flight"), !Flight.IsEmpty()))
	{
		return true;
	}
	const double Apex = Flight[Flight.Num() / 2];
	const float ApexPhase = float(Apex / (JumpSequence->GetPlayLength() - 0.5));

	auto Simulate = [&](const TCHAR* ImpactorId, float Phase, FImpactClipRecording& Out) -> bool
	{
		FImpactClipSetup Setup;
		Setup.Mesh = SkeletalMesh;
		Setup.PhysicsAsset = Physics;
		Setup.Source = JumpSequence;
		Setup.bSourcePlaysOnce = true;
		Setup.RegionTable = Regions.Get();
		Setup.BatchSeed = 5;
		Setup.MaxReactionFrames = 120;
		const FImpactorArchetype* Archetype = Catalog->Archetypes.FindByPredicate([ImpactorId](const FImpactorArchetype& A) { return A.Id == FName(ImpactorId); });
		FImpactParams& Params = Setup.Params;
		Params.Impactor.Archetype = Archetype;
		Params.Impactor.MassKg = Archetype->MassKg.Max;
		Params.Impactor.SpeedMetresPerSecond = Archetype->SpeedMetresPerSecond.Max;
		Params.Impactor.ContactRadiusCm = Archetype->ContactRadiusCm.Min;
		Params.TransferredImpulseKgCmPerSecond = FImpactParamsSampler::TransferredImpulse(Params.Impactor.MassKg, Params.Impactor.SpeedMetresPerSecond, 75.0f, 0.2f);
		Params.ReactionGain = Archetype->ReactionGain;
		Params.ReactionImpulseKgCmPerSecond = Params.TransferredImpulseKgCmPerSecond * Params.ReactionGain;
		Params.Intensity = FImpactIntensityThresholds().Classify(Params.ReactionImpulseKgCmPerSecond);
		Params.Region = EImpactBodyRegion::Pelvis;
		Params.DirectionLocal = FVector(0.0, 1.0, 0.0);
		Params.SourcePhase = Phase;
		FText Reason;
		FImpactSimWorld World;
		return FImpactScheduleSampler::Sample(Params, FImpactDeliveryDistributions(), 5, 0, Setup.Schedule, Reason)
			&& FImpactClipSimulator::Run(World, Setup, Out, Reason);
	};

	FImpactClipRecording CarInAir, BallInAir, CarOnGround;
	if (!TestTrue(TEXT("A car hits mid-jump"), Simulate(TEXT("CarBumper"), ApexPhase, CarInAir))
		|| !TestTrue(TEXT("A baseball hits mid-jump"), Simulate(TEXT("Baseball"), ApexPhase, BallInAir))
		|| !TestTrue(TEXT("A car hits before take-off"), Simulate(TEXT("CarBumper"), 0.0f, CarOnGround)))
	{
		return true;
	}
	TestTrue(TEXT("A hard hit in the air releases the body at the hit"),
		CarInAir.Frames.IsValidIndex(CarInAir.ImpactFrame) && CarInAir.Frames[CarInAir.ImpactFrame].bReleased);
	TestFalse(TEXT("A light hit in the air rides the jump to its landing"),
		BallInAir.Frames.IsValidIndex(BallInAir.ImpactFrame) && BallInAir.Frames[BallInAir.ImpactFrame].bReleased);
	TestFalse(TEXT("A hard hit on the ground is not released at the hit"),
		CarOnGround.Frames.IsValidIndex(CarOnGround.ImpactFrame) && CarOnGround.Frames[CarOnGround.ImpactFrame].bReleased);
	return true;
}
