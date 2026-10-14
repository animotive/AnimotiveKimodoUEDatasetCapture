// Copyright Animotive. All Rights Reserved.

#include "Animation/Skeleton.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "ImpactBatchDefinition.h"
#include "ImpactRegionTable.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

/**
 * Writes the sample batch assets to /Game/KimodoImpactSample: an impactor catalog with the starter
 * archetypes, the UEFN mannequin region table, and a batch over every Kimodo source in
 * /Game/Kimodo/ImpactSources. The headless host runs it:
 *
 *   UnrealEditor-Cmd <Project>.uproject -run=ImpactBatch -Batch=/Game/KimodoImpactSample/IB_KimodoSample.IB_KimodoSample
 *
 * Regenerated on every run, so the sample tracks the code's defaults.
 */
namespace ImpactSampleBatchAssets
{
	const TCHAR* Folder = TEXT("/Game/KimodoImpactSample");

	template <typename T>
	T* NewAsset(const TCHAR* Name)
	{
		UPackage* Package = CreatePackage(*(FString(Folder) / Name));
		Package->FullyLoad();
		if (T* Existing = FindObject<T>(Package, Name))
		{
			Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		}
		return NewObject<T>(Package, Name, RF_Public | RF_Standalone);
	}

	bool Save(UObject* Asset)
	{
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		const FString Filename = FPackageName::LongPackageNameToFilename(Asset->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension());
		return UPackage::Save(Asset->GetPackage(), Asset, *Filename, Args).Result == ESavePackageResult::Success;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactSampleBatchAssetsGen,
	"KimodoDatasetCapture.Impact.SampleBatchAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactSampleBatchAssetsGen::RunTest(const FString& Parameters)
{
	using namespace ImpactSampleBatchAssets;
	if (!LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin")))
	{
		AddWarning(TEXT("GameAnimationSample content not found; sample batch assets not written."));
		return true;
	}

	UImpactorCatalog* Catalog = NewAsset<UImpactorCatalog>(TEXT("DA_ImpactorCatalog"));
	Catalog->FillWithStarterArchetypes();
	UImpactRegionTable* Regions = NewAsset<UImpactRegionTable>(TEXT("DA_UEFNRegionTable"));
	Regions->FillWithUEFNMannequinEntries();

	UImpactBatchDefinition* Batch = NewAsset<UImpactBatchDefinition>(TEXT("IB_KimodoSample"));
	FDirectoryPath Sources;
	Sources.Path = TEXT("/Game/Kimodo/ImpactSources");
	Batch->AnimationFolders = { Sources };
	Batch->RequiredSkeleton = TSoftObjectPtr<USkeleton>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin.SK_UEFN_Mannequin")));
	Batch->Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin")));
	Batch->PhysicsAsset = TSoftObjectPtr<UPhysicsAsset>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Rigs/PA_UEFN_Mannequin.PA_UEFN_Mannequin")));
	Batch->RegionTable = Regions;
	Batch->Catalog = Catalog;
	const UEnum* RegionEnum = StaticEnum<EImpactBodyRegion>();
	for (int32 Index = 0; Index < RegionEnum->NumEnums() - 1; ++Index)
	{
		Batch->Impact.Regions.Add({ EImpactBodyRegion(RegionEnum->GetValueByIndex(Index)), 1.0f });
	}
	Batch->Seed = 1;
	Batch->ClipCount = 1000;
	Batch->OutputRoot = TEXT("/Game/KimodoImpactDataset");

	// The full grid over the same sources: every impactor, region it can strike, 8 directions,
	// 3 elevations and 3 strengths. Invalid clips are recorded with their sidecar, not baked.
	UImpactBatchDefinition* Grid = NewAsset<UImpactBatchDefinition>(TEXT("IB_KimodoGrid"));
	UEngine::CopyPropertiesForUnrelatedObjects(Batch, Grid);
	Grid->bFullGrid = true;
	Grid->Grid = FImpactGridSettings();
	Grid->bDiscardInvalid = true;

	// A tenth of the full grid that still reaches every source, impactor and region it can strike:
	// 4 directions, level, the lightest and the heaviest strength (about 11k clips).
	UImpactBatchDefinition* Grid10k = NewAsset<UImpactBatchDefinition>(TEXT("IB_KimodoGrid10k"));
	UEngine::CopyPropertiesForUnrelatedObjects(Grid, Grid10k);
	Grid10k->Grid.AzimuthSteps = 4;
	Grid10k->Grid.ElevationSteps = 1;
	Grid10k->Grid.StrengthLevels = 2;
	Grid10k->Grid.PhasesPerSource = 1;

	FText Reason;
	TestTrue(*FString::Printf(TEXT("The sample batch is valid: %s"), *Reason.ToString()), Batch->ValidateSettings(Reason));
	TestTrue(*FString::Printf(TEXT("The grid batch is valid: %s"), *Reason.ToString()), Grid->ValidateSettings(Reason));
	TestTrue(*FString::Printf(TEXT("The 10k grid batch is valid: %s"), *Reason.ToString()), Grid10k->ValidateSettings(Reason));
	TestTrue(TEXT("10k grid batch saved"), Save(Grid10k));
	TestTrue(TEXT("Catalog saved"), Save(Catalog));
	TestTrue(TEXT("Region table saved"), Save(Regions));
	TestTrue(TEXT("Batch saved"), Save(Batch));
	TestTrue(TEXT("Grid batch saved"), Save(Grid));
	return true;
}
