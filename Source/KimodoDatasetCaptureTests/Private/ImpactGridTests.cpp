// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "ImpactBatchDefinition.h"
#include "ImpactBatchRunner.h"
#include "ImpactClipSimulator.h"
#include "ImpactGrid.h"
#include "ImpactRegionTable.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/StrongObjectPtr.h"

namespace ImpactGridTests
{
	UImpactBatchDefinition* MakeBatch()
	{
		UImpactBatchDefinition* Batch = NewObject<UImpactBatchDefinition>();
		const UEnum* RegionEnum = StaticEnum<EImpactBodyRegion>();
		for (int32 Index = 0; Index < RegionEnum->NumEnums() - 1; ++Index)
		{
			Batch->Impact.Regions.Add({ EImpactBodyRegion(RegionEnum->GetValueByIndex(Index)), 1.0f });
		}
		Batch->Impact.ElevationDegrees = FFloatInterval(-90.0f, 90.0f);
		Batch->Impact.AzimuthDegrees = FFloatInterval(0.0f, 360.0f);
		Batch->Impact.SourcePhase = FFloatInterval(0.0f, 1.0f);
		Batch->bFullGrid = true;
		Batch->Grid.AzimuthSteps = 4;
		Batch->Grid.ElevationSteps = 2;
		Batch->Grid.StrengthLevels = 3;
		Batch->Grid.PhasesPerSource = 2;
		return Batch;
	}

	UImpactorCatalog* MakeCatalog()
	{
		UImpactorCatalog* Catalog = NewObject<UImpactorCatalog>();
		Catalog->Archetypes = UImpactorCatalog::MakeStarterArchetypes();
		return Catalog;
	}

	int32 StrikablePairs(const UImpactBatchDefinition& Batch, const UImpactorCatalog& Catalog)
	{
		int32 Pairs = 0;
		for (const FImpactorArchetype& Archetype : Catalog.Archetypes)
		{
			if (Archetype.Weight <= 0.0f)
			{
				continue;
			}
			for (const FImpactRegionWeight& Region : Batch.Impact.Regions)
			{
				Pairs += Region.Weight > 0.0f && Archetype.CanStrike(Region.Region) ? 1 : 0;
			}
		}
		return Pairs;
	}

	FString Key(const FImpactGridCell& Cell)
	{
		return FString::Printf(TEXT("%d/%d/%d/%d/%d/%d/%d"), Cell.Source, Cell.Archetype, int32(Cell.Region), Cell.Azimuth, Cell.Elevation, Cell.Strength, Cell.Phase);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactGridLayoutTest,
	"KimodoDatasetCapture.Impact.Grid.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactGridLayoutTest::RunTest(const FString& Parameters)
{
	using namespace ImpactGridTests;
	UImpactBatchDefinition* Batch = MakeBatch();
	UImpactorCatalog* Catalog = MakeCatalog();

	FImpactGrid Grid;
	FText Reason;
	if (!TestTrue(*FString::Printf(TEXT("The grid lays out: %s"), *Reason.ToString()), Grid.Build(*Batch, *Catalog, 2, Reason)))
	{
		return true;
	}
	const int32 Expected = 2 * StrikablePairs(*Batch, *Catalog) * 4 * 2 * 3 * 2;
	TestEqual(TEXT("Every source x impactor x strikable region x direction x strength x phase"), Grid.Num(), Expected);

	TSet<FString> Seen;
	TSet<FString> Triples;
	bool bAllStrikable = true;
	bool bInRange = true;
	for (int32 Index = 0; Index < Grid.Num(); ++Index)
	{
		const FImpactGridCell Cell = Grid.Cell(Index);
		Seen.Add(Key(Cell));
		Triples.Add(FString::Printf(TEXT("%d/%d/%d"), Cell.Source, Cell.Archetype, int32(Cell.Region)));
		bAllStrikable &= Catalog->Archetypes.IsValidIndex(Cell.Archetype) && Catalog->Archetypes[Cell.Archetype].CanStrike(Cell.Region);
		bInRange &= Cell.Source >= 0 && Cell.Source < 2 && Cell.Azimuth >= 0 && Cell.Azimuth < 4 && Cell.Elevation >= 0 && Cell.Elevation < 2
			&& Cell.Strength >= 0 && Cell.Strength < 3 && Cell.Phase >= 0 && Cell.Phase < 2;
	}
	TestEqual(TEXT("Every index names a different cell"), Seen.Num(), Grid.Num());
	TestEqual(TEXT("Every source, impactor and strikable region appears"), Triples.Num(), 2 * StrikablePairs(*Batch, *Catalog));
	TestTrue(TEXT("Only regions the impactor can strike"), bAllStrikable);
	TestTrue(TEXT("Every axis within its steps"), bInRange);
	TestTrue(TEXT("The same index, the same cell"), Grid.Cell(123) == Grid.Cell(123));

	// Excluded impactors and regions leave the grid.
	Catalog->Archetypes[0].Weight = 0.0f;
	Batch->Impact.Regions[0].Weight = 0.0f;
	FImpactGrid Smaller;
	Smaller.Build(*Batch, *Catalog, 2, Reason);
	TestEqual(TEXT("A zero-weight impactor and a zero-weight region are left out"), Smaller.Num(), 2 * StrikablePairs(*Batch, *Catalog) * 4 * 2 * 3 * 2);
	TestTrue(TEXT("and it is smaller"), Smaller.Num() < Grid.Num());

	FImpactGrid Empty;
	Reason = FText::GetEmpty();
	TestFalse(TEXT("No sources: no grid"), Empty.Build(*Batch, *Catalog, 0, Reason));
	TestFalse(TEXT("with a reason"), Reason.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactGridParamsTest,
	"KimodoDatasetCapture.Impact.Grid.Params",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactGridParamsTest::RunTest(const FString& Parameters)
{
	using namespace ImpactGridTests;
	UImpactBatchDefinition* Batch = MakeBatch();
	UImpactorCatalog* Catalog = MakeCatalog();
	const int32 Car = Catalog->Archetypes.IndexOfByPredicate([](const FImpactorArchetype& A) { return A.Id == TEXT("CarBumper"); });
	const FImpactorArchetype& CarArchetype = Catalog->Archetypes[Car];

	FImpactGridCell Cell;
	Cell.Archetype = Car;
	Cell.Region = EImpactBodyRegion::ThighLeft;
	Cell.Azimuth = 1;
	Cell.Elevation = 0;
	Cell.Strength = 0;
	Cell.Phase = 1;
	const FImpactParams Weakest = FImpactGrid::Params(Cell, *Batch, *Catalog);
	TestTrue(TEXT("The cell's impactor"), Weakest.Impactor.Archetype == &CarArchetype);
	TestEqual(TEXT("The cell's region"), Weakest.Region, EImpactBodyRegion::ThighLeft);
	TestEqual(TEXT("Lowest strength: the lightest"), Weakest.Impactor.MassKg, CarArchetype.MassKg.Min);
	TestEqual(TEXT("and slowest"), Weakest.Impactor.SpeedMetresPerSecond, CarArchetype.SpeedMetresPerSecond.Min);
	const float Elevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(float(Weakest.DirectionLocal.Z), -1.0f, 1.0f)));
	TestEqual(TEXT("First elevation step: the bottom of the car's band"), Elevation, CarArchetype.ElevationDegrees.Min, 0.01f);
	TestEqual(TEXT("Second of four azimuth steps: 90 degrees"),
		float(FMath::RadiansToDegrees(FMath::Atan2(Weakest.DirectionLocal.Y, Weakest.DirectionLocal.X))), 90.0f, 0.01f);
	TestEqual(TEXT("A unit direction"), float(Weakest.DirectionLocal.Size()), 1.0f, 1e-4f);
	TestEqual(TEXT("Second of two phases: three quarters"), Weakest.SourcePhase, 0.75f, 1e-5f);
	TestEqual(TEXT("Transferred impulse from mass and speed"), Weakest.TransferredImpulseKgCmPerSecond,
		FImpactParamsSampler::TransferredImpulse(CarArchetype.MassKg.Min, CarArchetype.SpeedMetresPerSecond.Min, Batch->CharacterMassKg, Batch->Impact.Restitution), 1.0f);
	TestEqual(TEXT("Reaction through the impactor's gain"), Weakest.ReactionImpulseKgCmPerSecond, Weakest.TransferredImpulseKgCmPerSecond * CarArchetype.ReactionGain, 1.0f);
	TestEqual(TEXT("Intensity from the reaction"), Weakest.Intensity, Batch->Impact.Intensity.Classify(Weakest.ReactionImpulseKgCmPerSecond));

	Cell.Strength = 2;
	Cell.Elevation = 1;
	const FImpactParams Strongest = FImpactGrid::Params(Cell, *Batch, *Catalog);
	TestEqual(TEXT("Highest strength: the heaviest"), Strongest.Impactor.MassKg, CarArchetype.MassKg.Max);
	TestEqual(TEXT("and fastest"), Strongest.Impactor.SpeedMetresPerSecond, CarArchetype.SpeedMetresPerSecond.Max);
	TestEqual(TEXT("Last elevation step: the top of the car's band"),
		FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(float(Strongest.DirectionLocal.Z), -1.0f, 1.0f))), CarArchetype.ElevationDegrees.Max, 0.01f);
	TestTrue(TEXT("Stronger is a harder hit"), Strongest.ReactionImpulseKgCmPerSecond > Weakest.ReactionImpulseKgCmPerSecond);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactGridFactoryTest,
	"KimodoDatasetCapture.Impact.Grid.FactoryAndCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactGridFactoryTest::RunTest(const FString& Parameters)
{
	using namespace ImpactGridTests;
	const FSoftObjectPath Idle(TEXT("/Game/Kimodo/ImpactSources/M_Kimodo_Stand_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin.M_Kimodo_Stand_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin"));
	const FSoftObjectPath Run(TEXT("/Game/Kimodo/ImpactSources/M_Kimodo_Run_Loop_F_S1_KimodoRetargeted_SK_UEFN_Mannequin.M_Kimodo_Run_Loop_F_S1_KimodoRetargeted_SK_UEFN_Mannequin"));
	if (!LoadObject<UAnimSequence>(nullptr, *Idle.ToString()) || !LoadObject<UAnimSequence>(nullptr, *Run.ToString()))
	{
		AddWarning(TEXT("Kimodo sources not found; grid factory test skipped."));
		return true;
	}
	TStrongObjectPtr<UImpactBatchDefinition> Batch(MakeBatch());
	TStrongObjectPtr<UImpactorCatalog> Catalog(MakeCatalog());
	TStrongObjectPtr<UImpactRegionTable> Regions(NewObject<UImpactRegionTable>());
	Regions->FillWithUEFNMannequinEntries();
	Batch->ExplicitAnimations = { TSoftObjectPtr<UAnimSequence>(Idle), TSoftObjectPtr<UAnimSequence>(Run) };
	Batch->RequiredSkeleton = TSoftObjectPtr<USkeleton>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin.SK_UEFN_Mannequin")));
	Batch->Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin")));
	Batch->PhysicsAsset = TSoftObjectPtr<UPhysicsAsset>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Rigs/PA_UEFN_Mannequin.PA_UEFN_Mannequin")));
	Batch->RegionTable = Regions.Get();
	Batch->Catalog = Catalog.Get();

	FImpactBatchAssets Assets;
	FText Reason;
	if (!TestTrue(*FString::Printf(TEXT("The grid batch loads: %s"), *Reason.ToString()), FImpactBatchAssets::Load(*Batch, Assets, Reason)))
	{
		return true;
	}
	TestEqual(TEXT("The grid covers both sources"), Assets.Grid.Num(), 2 * StrikablePairs(*Batch, *Catalog) * 4 * 2 * 3 * 2);
	TestEqual(TEXT("A grid batch has as many clips as cells"), Assets.ClipCount(*Batch), Assets.Grid.Num());

	for (const int32 Index : { 0, 1, 777, Assets.Grid.Num() - 1 })
	{
		const FImpactGridCell Cell = Assets.Grid.Cell(Index);
		FImpactClipSetup Setup;
		FString Id;
		if (!TestTrue(*FString::Printf(TEXT("Clip %d is made"), Index), FImpactClipFactory::Make(*Batch, Assets, Index, Setup, Id, Reason)))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("Clip %d plays its cell's source"), Index), Setup.Source == Assets.Sources[Cell.Source].Get());
		TestEqual(*FString::Printf(TEXT("Clip %d strikes its cell's region"), Index), Setup.Params.Region, Cell.Region);
		TestTrue(*FString::Printf(TEXT("Clip %d uses its cell's impactor"), Index), Setup.Params.Impactor.Archetype == &Catalog->Archetypes[Cell.Archetype]);
	}

	Batch->bFullGrid = false;
	Batch->ClipCount = 50;
	TestEqual(TEXT("A sampled batch has ClipCount clips"), Assets.ClipCount(*Batch), 50);
	return true;
}
