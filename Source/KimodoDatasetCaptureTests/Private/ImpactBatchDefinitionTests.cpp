// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetData.h"
#include "ImpactBatchDefinition.h"
#include "Misc/AutomationTest.h"

namespace ImpactBatchTests
{
	const TCHAR* SkeletonA = TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin.SK_UEFN_Mannequin");
	const TCHAR* SkeletonB = TEXT("/Game/Characters/Other/SK_Other.SK_Other");

	FAssetData Anim(const TCHAR* Folder, const TCHAR* Name, const TCHAR* Skeleton, UClass* Class = UAnimSequence::StaticClass())
	{
		FAssetDataTagMap Tags;
		// The registry stores the skeleton as an export-text path.
		Tags.Add(TEXT("Skeleton"), FString::Printf(TEXT("/Script/Engine.Skeleton'%s'"), Skeleton));
		const FString Package = FString(Folder) / Name;
		return FAssetData(FName(*Package), FName(Folder), FName(Name), Class->GetClassPathName(), Tags);
	}

	UImpactBatchDefinition* MakeBatch()
	{
		UImpactBatchDefinition* Batch = NewObject<UImpactBatchDefinition>();
		Batch->RequiredSkeleton = TSoftObjectPtr<USkeleton>(FSoftObjectPath(SkeletonA));
		return Batch;
	}

	TArray<FString> Names(const TArray<FSoftObjectPath>& Paths)
	{
		TArray<FString> Out;
		for (const FSoftObjectPath& Path : Paths)
		{
			Out.Add(Path.GetAssetName());
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactBatchFilterTest,
	"KimodoDatasetCapture.Impact.Batch.SourceFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactBatchFilterTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBatchTests;
	const TArray<FAssetData> Candidates = {
		Anim(TEXT("/Game/Anim/Run"), TEXT("M_Run_Loop_F"), SkeletonA),
		Anim(TEXT("/Game/Anim/Run"), TEXT("M_Run_Turn_90_L"), SkeletonA),
		Anim(TEXT("/Game/Anim/Idle"), TEXT("M_Idle_Loop"), SkeletonA),
		Anim(TEXT("/Game/Anim/Run"), TEXT("M_Run_Loop_F_Other"), SkeletonB),
		Anim(TEXT("/Game/Anim/Run"), TEXT("BS_Run"), SkeletonA, UBlendSpace::StaticClass()),
		// Duplicate of the first, as when a folder and an explicit entry name the same asset.
		Anim(TEXT("/Game/Anim/Run"), TEXT("M_Run_Loop_F"), SkeletonA),
	};

	UImpactBatchDefinition* Batch = MakeBatch();
	const TArray<FString> All = Names(FImpactSourceResolver::Filter(*Batch, Candidates));
	TestEqual(TEXT("Only matching sequences, de-duplicated"), All.Num(), 3);
	TestFalse(TEXT("Other skeleton excluded"), All.Contains(TEXT("M_Run_Loop_F_Other")));
	TestFalse(TEXT("Blend spaces excluded: only sequences bake"), All.Contains(TEXT("BS_Run")));

	// Sorted by object path, so clip N always draws from the same list.
	TArray<FString> Sorted = All;
	Sorted.Sort();
	TestTrue(TEXT("Result is sorted"), FImpactSourceResolver::Filter(*Batch, Candidates) == FImpactSourceResolver::Filter(*Batch, TArray<FAssetData>({ Candidates[2], Candidates[1], Candidates[0] })));
	TestEqual(TEXT("Idle folder sorts before Run folder"), All.Num() > 0 ? All[0] : FString(), FString(TEXT("M_Idle_Loop")));

	// Name filters are case-insensitive; excludes win over includes.
	Batch->NameIncludes = { TEXT("loop") };
	const TArray<FString> Loops = Names(FImpactSourceResolver::Filter(*Batch, Candidates));
	TestEqual(TEXT("Include keeps only matching names"), Loops.Num(), 2);
	Batch->NameExcludes = { TEXT("IDLE") };
	const TArray<FString> RunLoops = Names(FImpactSourceResolver::Filter(*Batch, Candidates));
	TestEqual(TEXT("Exclude removes matching names"), RunLoops.Num(), 1);
	TestTrue(TEXT("Remaining clip is the run loop"), RunLoops.Num() == 1 && RunLoops[0] == TEXT("M_Run_Loop_F"));

	// No required skeleton set: nothing qualifies, because nothing can be baked safely.
	UImpactBatchDefinition* NoSkeleton = NewObject<UImpactBatchDefinition>();
	TestEqual(TEXT("Without a required skeleton nothing qualifies"), FImpactSourceResolver::Filter(*NoSkeleton, Candidates).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactBatchResolveTest,
	"KimodoDatasetCapture.Impact.Batch.ResolveGASP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactBatchResolveTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBatchTests;
	UImpactBatchDefinition* Batch = MakeBatch();

	// Empty set: a named failure, never an empty batch that runs and produces nothing.
	Batch->AnimationFolders = { { TEXT("/Game/DoesNotExist/Anywhere") } };
	TArray<FSoftObjectPath> Animations;
	FText Reason;
	TestFalse(TEXT("No qualifying animations is a failure"), FImpactSourceResolver::Resolve(*Batch, Animations, Reason));
	TestTrue(TEXT("Failure names the folder searched"), Reason.ToString().Contains(TEXT("/Game/DoesNotExist/Anywhere")));

	if (!LoadObject<USkeleton>(nullptr, SkeletonA))
	{
		AddWarning(TEXT("GameAnimationSample content not found; real-folder resolution skipped."));
		return true;
	}

	// The real GASP run folder resolves to many sequences, all on the UEFN skeleton.
	Batch->AnimationFolders = { { TEXT("/Game/Characters/UEFN_Mannequin/Animations/Run") } };
	Batch->NameIncludes = { TEXT("Loop") };
	TestTrue(TEXT("GASP run loops resolve"), FImpactSourceResolver::Resolve(*Batch, Animations, Reason));
	TestTrue(TEXT("At least five run loops"), Animations.Num() >= 5);
	bool bAllLoops = true;
	for (const FSoftObjectPath& Path : Animations)
	{
		bAllLoops &= Path.GetAssetName().Contains(TEXT("Loop"));
	}
	TestTrue(TEXT("Every resolved name passes the include filter"), bAllLoops);

	// An explicit entry outside the folders is added.
	Batch->ExplicitAnimations = { TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop.M_Neutral_Stand_Idle_Loop"))) };
	TArray<FSoftObjectPath> WithIdle;
	FImpactSourceResolver::Resolve(*Batch, WithIdle, Reason);
	TestEqual(TEXT("Explicit animation added"), WithIdle.Num(), Animations.Num() + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactBatchSettingsTest,
	"KimodoDatasetCapture.Impact.Batch.Settings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactBatchSettingsTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBatchTests;
	auto MakeComplete = []()
	{
		UImpactBatchDefinition* Batch = MakeBatch();
		Batch->Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/M.M")));
		Batch->PhysicsAsset = TSoftObjectPtr<UPhysicsAsset>(FSoftObjectPath(TEXT("/Game/P.P")));
		Batch->RegionTable = TSoftObjectPtr<UImpactRegionTable>(FSoftObjectPath(TEXT("/Game/R.R")));
		Batch->Catalog = TSoftObjectPtr<UImpactorCatalog>(FSoftObjectPath(TEXT("/Game/C.C")));
		Batch->AnimationFolders = { { TEXT("/Game/Anim") } };
		return Batch;
	};

	FText Reason;
	TestTrue(TEXT("Complete settings validate"), MakeComplete()->ValidateSettings(Reason));

	auto Rejects = [this, &MakeComplete](const TCHAR* What, TFunction<void(UImpactBatchDefinition&)> Break)
	{
		UImpactBatchDefinition* Batch = MakeComplete();
		Break(*Batch);
		FText Reason;
		TestFalse(*FString::Printf(TEXT("%s is rejected"), What), Batch->ValidateSettings(Reason));
		TestFalse(*FString::Printf(TEXT("%s states a reason"), What), Reason.IsEmpty());
	};
	Rejects(TEXT("No mesh"), [](UImpactBatchDefinition& B) { B.Mesh.Reset(); });
	Rejects(TEXT("No physics asset"), [](UImpactBatchDefinition& B) { B.PhysicsAsset.Reset(); });
	Rejects(TEXT("No region table"), [](UImpactBatchDefinition& B) { B.RegionTable.Reset(); });
	Rejects(TEXT("No catalog"), [](UImpactBatchDefinition& B) { B.Catalog.Reset(); });
	Rejects(TEXT("No required skeleton"), [](UImpactBatchDefinition& B) { B.RequiredSkeleton.Reset(); });
	Rejects(TEXT("No folders and no explicit animations"), [](UImpactBatchDefinition& B) { B.AnimationFolders.Empty(); });
	Rejects(TEXT("Zero clips"), [](UImpactBatchDefinition& B) { B.ClipCount = 0; });
	Rejects(TEXT("Zero frame rate"), [](UImpactBatchDefinition& B) { B.FrameRate = 0; });
	Rejects(TEXT("Negative pre-roll"), [](UImpactBatchDefinition& B) { B.PreRollFrames = -1; });
	Rejects(TEXT("Zero reaction ceiling"), [](UImpactBatchDefinition& B) { B.MaxReactionFrames = 0; });
	Rejects(TEXT("Zero settle frames"), [](UImpactBatchDefinition& B) { B.SettleFrames = 0; });
	Rejects(TEXT("Output outside a content root"), [](UImpactBatchDefinition& B) { B.OutputRoot = TEXT("C:/Temp/Clips"); });
	return true;
}
