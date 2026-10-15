// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "ImpactBatchDefinition.h"
#include "ImpactClipSimulator.h"
#include "ImpactGrid.h"
#include "ImpactSourceClock.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"

namespace ImpactFlightMomentTests
{
	const TCHAR* Mesh = TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin");
	const TCHAR* Jump = TEXT("/Game/Kimodo/ImpactSources/M_Kimodo_Jump_In_Place_KimodoRetargeted_SK_UEFN_Mannequin.M_Kimodo_Jump_In_Place_KimodoRetargeted_SK_UEFN_Mannequin");
	const TCHAR* Run = TEXT("/Game/Kimodo/ImpactSources/M_Kimodo_Run_Loop_F_S1_KimodoRetargeted_SK_UEFN_Mannequin.M_Kimodo_Run_Loop_F_S1_KimodoRetargeted_SK_UEFN_Mannequin");

	const EImpactSourceMoment FlightMoments[] = { EImpactSourceMoment::BeforeTakeOff, EImpactSourceMoment::Apex, EImpactSourceMoment::AfterLanding };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactGridFlightMomentsTest,
	"KimodoDatasetCapture.Impact.Grid.FlightMoments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactGridFlightMomentsTest::RunTest(const FString& Parameters)
{
	using namespace ImpactFlightMomentTests;
	UImpactBatchDefinition* Batch = NewObject<UImpactBatchDefinition>();
	Batch->Impact.Regions = { { EImpactBodyRegion::Chest, 1.0f }, { EImpactBodyRegion::Head, 1.0f } };
	Batch->Impact.ElevationDegrees = FFloatInterval(-90.0f, 90.0f);
	Batch->bFullGrid = true;
	Batch->Grid.AzimuthSteps = 4;
	Batch->Grid.ElevationSteps = 1;
	Batch->Grid.StrengthLevels = 2;
	Batch->Grid.PhasesPerSource = 2;
	UImpactorCatalog* Catalog = NewObject<UImpactorCatalog>();
	Catalog->Archetypes = UImpactorCatalog::MakeStarterArchetypes();

	TestTrue(TEXT("A grid uses flight moments by default"), FImpactGridSettings().bFlightMoments);

	FText Reason;
	FImpactGrid Looping;
	if (!TestTrue(TEXT("A looping-only grid lays out"), Looping.Build(*Batch, *Catalog, 1, Reason)))
	{
		return true;
	}
	// Per source: impactor x region x direction x strength, times its phases or moments.
	const int32 PerStep = Looping.Num() / 2;

	const TArray<bool> PlaysOnce = { false, true };
	FImpactGrid Grid;
	if (!TestTrue(TEXT("A grid with a play-once source lays out"), Grid.Build(*Batch, *Catalog, 2, PlaysOnce, Reason)))
	{
		return true;
	}
	TestEqual(TEXT("The looping source keeps its two phases; the jump gets three moments instead"), Grid.Num(), PerStep * 2 + PerStep * 3);

	TSet<FString> Seen;
	TMap<EImpactSourceMoment, int32> JumpMoments;
	bool bLoopUsesPhase = true;
	bool bParamsCarryMoment = true;
	for (int32 Index = 0; Index < Grid.Num(); ++Index)
	{
		const FImpactGridCell Cell = Grid.Cell(Index);
		Seen.Add(FString::Printf(TEXT("%d/%d/%d/%d/%d/%d/%d/%d"), Cell.Source, Cell.Archetype, int32(Cell.Region), Cell.Azimuth, Cell.Elevation, Cell.Strength,
			Cell.Phase, int32(Cell.Moment)));
		if (Cell.Source == 0)
		{
			bLoopUsesPhase &= Cell.Moment == EImpactSourceMoment::Phase && Cell.Phase >= 0 && Cell.Phase < 2;
		}
		else
		{
			JumpMoments.FindOrAdd(Cell.Moment)++;
		}
		bParamsCarryMoment &= FImpactGrid::Params(Cell, *Batch, *Catalog).SourceMoment == Cell.Moment;
	}
	TestEqual(TEXT("Every index names a different cell"), Seen.Num(), Grid.Num());
	TestTrue(TEXT("The looping source is hit at its phases"), bLoopUsesPhase);
	for (const EImpactSourceMoment Moment : FlightMoments)
	{
		TestEqual(*FString::Printf(TEXT("The jump is hit at moment %d in every impact cell"), int32(Moment)), JumpMoments.FindRef(Moment), PerStep);
	}
	TestFalse(TEXT("The jump is never hit at a phase"), JumpMoments.Contains(EImpactSourceMoment::Phase));
	TestTrue(TEXT("A cell's moment reaches its impact"), bParamsCarryMoment);

	Batch->Grid.bFlightMoments = false;
	FImpactGrid Phased;
	Phased.Build(*Batch, *Catalog, 2, PlaysOnce, Reason);
	TestEqual(TEXT("Without flight moments a play-once source takes the phase steps"), Phased.Num(), PerStep * 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactFlightMomentTimesTest,
	"KimodoDatasetCapture.Impact.MidAir.FlightMoments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactFlightMomentTimesTest::RunTest(const FString& Parameters)
{
	using namespace ImpactFlightMomentTests;
	TestEqual(TEXT("Before take-off"), FImpactSourceFlight::MomentSeconds(EImpactSourceMoment::BeforeTakeOff, 1.0, 1.5), 0.8, 1e-9);
	TestEqual(TEXT("Apex: the middle of the flight"), FImpactSourceFlight::MomentSeconds(EImpactSourceMoment::Apex, 1.0, 1.5), 1.25, 1e-9);
	TestEqual(TEXT("After landing"), FImpactSourceFlight::MomentSeconds(EImpactSourceMoment::AfterLanding, 1.0, 1.5), 1.65, 1e-9);

	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, Mesh);
	UAnimSequence* JumpSequence = LoadObject<UAnimSequence>(nullptr, Jump);
	UAnimSequence* RunSequence = LoadObject<UAnimSequence>(nullptr, Run);
	if (!SkeletalMesh || !JumpSequence || !RunSequence)
	{
		AddWarning(TEXT("Kimodo sources not found; flight moment checks skipped."));
		return true;
	}
	const FReferenceSkeleton& Ref = SkeletalMesh->GetRefSkeleton();
	const int32 Left = Ref.FindBoneIndex(TEXT("foot_l"));
	const int32 Right = Ref.FindBoneIndex(TEXT("foot_r"));

	double TakeOff = 0.0, Landing = 0.0;
	const FImpactPoseSampler RunSampler(*SkeletalMesh, *RunSequence, false);
	TestFalse(TEXT("A run never leaves the ground for a whole flight"), FImpactSourceFlight::FindFlight(RunSampler, Ref, Left, Right, TakeOff, Landing));

	const FImpactPoseSampler JumpSampler(*SkeletalMesh, *JumpSequence, true);
	if (!TestTrue(TEXT("The jump has a flight"), FImpactSourceFlight::FindFlight(JumpSampler, Ref, Left, Right, TakeOff, Landing)))
	{
		return true;
	}
	TestTrue(*FString::Printf(TEXT("Take-off %.3f s, landing %.3f s, inside the jump (%.3f s)"), TakeOff, Landing, JumpSampler.GetLength()),
		TakeOff > 0.2 && TakeOff < Landing && Landing + 0.15 < JumpSampler.GetLength());
	TestTrue(TEXT("In flight at take-off"), FImpactSourceFlight::IsInFlight(JumpSampler, Ref, Left, Right, TakeOff));
	TestFalse(TEXT("Not yet clear just before"), FImpactSourceFlight::IsInFlight(JumpSampler, Ref, Left, Right, TakeOff - 1.0 / 60.0));
	TestTrue(TEXT("In flight at the apex"),
		FImpactSourceFlight::IsInFlight(JumpSampler, Ref, Left, Right, FImpactSourceFlight::MomentSeconds(EImpactSourceMoment::Apex, TakeOff, Landing), 0.0));
	TestFalse(TEXT("On the ground after landing"),
		FImpactSourceFlight::IsInFlight(JumpSampler, Ref, Left, Right, FImpactSourceFlight::MomentSeconds(EImpactSourceMoment::AfterLanding, TakeOff, Landing), 0.0));

	// The simulator starts the clip so its first hit lands on the moment.
	FImpactClipSetup Setup;
	Setup.Mesh = SkeletalMesh;
	Setup.Source = JumpSequence;
	Setup.bSourcePlaysOnce = true;
	const double Lead = double(Setup.PreRollFrames + 1) / double(Setup.FrameRate);
	for (const EImpactSourceMoment Moment : FlightMoments)
	{
		Setup.Params.SourceMoment = Moment;
		TestEqual(*FString::Printf(TEXT("The hit lands on moment %d"), int32(Moment)), FImpactClipSimulator::SourceStartSeconds(Setup) + Lead,
			FImpactSourceFlight::MomentSeconds(Moment, TakeOff, Landing), 1e-6);
	}
	Setup.Params.SourceMoment = EImpactSourceMoment::Phase;
	Setup.Params.SourcePhase = 0.3f;
	TestEqual(TEXT("A phase clip starts at its phase"), FImpactClipSimulator::SourceStartSeconds(Setup),
		FImpactSourceClock::StartSeconds(double(Setup.Params.SourcePhase), JumpSequence->GetPlayLength(), true, Lead), 1e-9);
	return true;
}
