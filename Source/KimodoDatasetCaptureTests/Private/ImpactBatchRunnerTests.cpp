// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "DatasetDescriptions.h"
#include "DatasetSidecar.h"
#include "DatasetSidecarSerializer.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "ImpactBatchDefinition.h"
#include "ImpactBatchRunner.h"
#include "ImpactClipId.h"
#include "ImpactClipSimulator.h"
#include "ImpactRegionTable.h"
#include "ImpactSourceClock.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace ImpactBatchRunnerTests
{
	const TCHAR* Sources = TEXT("/Game/Kimodo/ImpactSources/");
	const TCHAR* Idle = TEXT("M_Kimodo_Stand_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin");
	const TCHAR* Run = TEXT("M_Kimodo_Run_Loop_F_S1_KimodoRetargeted_SK_UEFN_Mannequin");
	const TCHAR* Jump = TEXT("M_Kimodo_Jump_In_Place_KimodoRetargeted_SK_UEFN_Mannequin");

	FSoftObjectPath SourcePath(const TCHAR* Name)
	{
		return FSoftObjectPath(FString(Sources) + Name + TEXT(".") + Name);
	}

	/** A small batch over Kimodo sources, with in-memory catalog and region table held by the caller. */
	struct FTestBatch
	{
		TStrongObjectPtr<UImpactBatchDefinition> Batch;
		TStrongObjectPtr<UImpactorCatalog> Catalog;
		TStrongObjectPtr<UImpactRegionTable> Regions;

		bool Make(FAutomationTestBase& Test, const TCHAR* Name, const FString& OutputRoot, int32 ClipCount, bool bWithJump)
		{
			if (!LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin"))
				|| !LoadObject<UAnimSequence>(nullptr, *SourcePath(Idle).ToString()))
			{
				Test.AddWarning(TEXT("GameAnimationSample or Kimodo source content not found; runner test skipped."));
				return false;
			}
			Catalog.Reset(NewObject<UImpactorCatalog>(GetTransientPackage(), *(FString(Name) + TEXT("_Catalog"))));
			Catalog->FillWithStarterArchetypes();
			Regions.Reset(NewObject<UImpactRegionTable>(GetTransientPackage(), *(FString(Name) + TEXT("_Regions"))));
			Regions->FillWithUEFNMannequinEntries();

			Batch.Reset(NewObject<UImpactBatchDefinition>(GetTransientPackage(), Name));
			Batch->ExplicitAnimations = { TSoftObjectPtr<UAnimSequence>(SourcePath(Idle)), TSoftObjectPtr<UAnimSequence>(SourcePath(Run)) };
			if (bWithJump)
			{
				Batch->ExplicitAnimations.Add(TSoftObjectPtr<UAnimSequence>(SourcePath(Jump)));
			}
			Batch->RequiredSkeleton = TSoftObjectPtr<USkeleton>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin.SK_UEFN_Mannequin")));
			Batch->Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin")));
			Batch->PhysicsAsset = TSoftObjectPtr<UPhysicsAsset>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Rigs/PA_UEFN_Mannequin.PA_UEFN_Mannequin")));
			Batch->RegionTable = Regions.Get();
			Batch->Catalog = Catalog.Get();
			for (EImpactBodyRegion Region : { EImpactBodyRegion::Chest, EImpactBodyRegion::Head, EImpactBodyRegion::Pelvis, EImpactBodyRegion::FootLeft })
			{
				Batch->Impact.Regions.Add({ Region, 1.0f });
			}
			Batch->Seed = 11;
			Batch->ClipCount = ClipCount;
			Batch->OutputRoot = OutputRoot;
			// Short clips keep the integration test quick.
			Batch->PreRollFrames = 10;
			Batch->MaxReactionFrames = 90;
			return true;
		}
	};

	void DeleteBatchOutput(const FString& BatchFolder)
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		IFileManager::Get().DeleteDirectory(*BatchFolder, false, true);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactSamplerPlayOnceTest,
	"KimodoDatasetCapture.Impact.Runner.PlayOnceHoldsLastFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactSamplerPlayOnceTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBatchRunnerTests;
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin"));
	UAnimSequence* JumpSequence = LoadObject<UAnimSequence>(nullptr, *SourcePath(Jump).ToString());
	if (!Mesh || !JumpSequence)
	{
		AddWarning(TEXT("Kimodo jump source not found; play-once test skipped."));
		return true;
	}
	const FImpactPoseSampler Looping(*Mesh, *JumpSequence);
	const FImpactPoseSampler Once(*Mesh, *JumpSequence, true);
	const int32 Pelvis = Mesh->GetRefSkeleton().FindBoneIndex(TEXT("pelvis"));
	const double Length = Once.GetLength();

	TArray<FTransform> End, Past, Wrapped, Inside, InsideOnce;
	Once.SampleLoop(Length, End);
	Once.Sample(Length + 0.5, Past);
	Looping.Sample(Length + 0.5, Wrapped);
	TestTrue(TEXT("Past its end, a play-once source holds its last frame"),
		Past[Pelvis].Equals(End[Pelvis], 0.01) && Past[0].Equals(End[0], 0.01));
	TestFalse(TEXT("A looping source wraps back into the take-off instead"), Wrapped[Pelvis].Equals(End[Pelvis], 0.01));

	Looping.Sample(Length * 0.5, Inside);
	Once.Sample(Length * 0.5, InsideOnce);
	TestTrue(TEXT("Within its length a play-once source plays as a looping one does"), Inside[Pelvis].Equals(InsideOnce[Pelvis], 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactClipFactoryTest,
	"KimodoDatasetCapture.Impact.Runner.ClipFactory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactClipFactoryTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBatchRunnerTests;
	FTestBatch Test;
	if (!Test.Make(*this, TEXT("ImpactFactoryTestBatch"), TEXT("/Game/KimodoImpactFactoryTest"), 200, true))
	{
		return true;
	}
	const UImpactBatchDefinition& Batch = *Test.Batch;

	TestTrue(TEXT("A jump plays once"), FImpactBatchAssets::PlaysOnce(Batch, Jump));
	TestFalse(TEXT("A run loops"), FImpactBatchAssets::PlaysOnce(Batch, Run));
	TestTrue(TEXT("Play-once names ignore case"), FImpactBatchAssets::PlaysOnce(Batch, TEXT("m_kimodo_JUMP_in_place")));

	FImpactBatchAssets Assets;
	FText Reason;
	if (!TestTrue(*FString::Printf(TEXT("The batch's assets load: %s"), *Reason.ToString()), FImpactBatchAssets::Load(Batch, Assets, Reason)))
	{
		return true;
	}
	TestEqual(TEXT("Every explicit source resolves"), Assets.Sources.Num(), 3);
	TestEqual(TEXT("A play-once flag per source"), Assets.SourcePlaysOnce.Num(), Assets.Sources.Num());
	TestTrue(TEXT("The mesh, physics asset, regions and catalog are held"),
		Assets.Mesh.IsValid() && Assets.PhysicsAsset.IsValid() && Assets.RegionTable.IsValid() && Assets.Catalog.IsValid());

	// A clip is a pure function of the batch and its index.
	FImpactClipSetup First, Again;
	FString FirstId, AgainId;
	TestTrue(TEXT("Clip 5 is made"), FImpactClipFactory::Make(Batch, Assets, 5, First, FirstId, Reason));
	FImpactClipFactory::Make(Batch, Assets, 5, Again, AgainId, Reason);
	TestEqual(TEXT("Same index, same id"), AgainId, FirstId);
	TestTrue(TEXT("Same index, same source"), First.Source == Again.Source);
	TestEqual(TEXT("Same index, same impulse"), Again.Params.ReactionImpulseKgCmPerSecond, First.Params.ReactionImpulseKgCmPerSecond);
	TestEqual(TEXT("Same index, same world strength"), Again.Strength.WorldStrength, First.Strength.WorldStrength);
	TestTrue(TEXT("The id is path-safe"), FirstId.StartsWith(TEXT("imp_")));

	TSet<FString> Ids;
	TSet<UAnimSequence*> Used;
	for (int32 Index = 0; Index < 200; ++Index)
	{
		FImpactClipSetup Setup;
		FString Id;
		if (!FImpactClipFactory::Make(Batch, Assets, Index, Setup, Id, Reason))
		{
			AddError(FString::Printf(TEXT("Clip %d: %s"), Index, *Reason.ToString()));
			break;
		}
		Ids.Add(Id);
		Used.Add(Setup.Source);
		const bool bJump = Setup.Source && Setup.Source->GetName().Contains(TEXT("Jump"));
		if (Setup.bSourcePlaysOnce != bJump)
		{
			AddError(FString::Printf(TEXT("Clip %d: play-once is %d for %s"), Index, Setup.bSourcePlaysOnce ? 1 : 0, *GetNameSafe(Setup.Source)));
		}
		if (Setup.ClipIndex != Index || Setup.BatchSeed != Batch.Seed || Setup.FrameRate != Batch.FrameRate
			|| Setup.PreRollFrames != Batch.PreRollFrames || Setup.MaxReactionFrames != Batch.MaxReactionFrames
			|| Setup.SettleFrames != Batch.SettleFrames || Setup.Mesh != Assets.Mesh.Get() || Setup.RegionTable != Assets.RegionTable.Get())
		{
			AddError(FString::Printf(TEXT("Clip %d does not carry the batch's settings"), Index));
		}
	}
	TestEqual(TEXT("Every clip has its own id"), Ids.Num(), 200);
	TestEqual(TEXT("Every source is drawn"), Used.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactBatchRunnerTest,
	"KimodoDatasetCapture.Impact.Runner.TinyBatchAndResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactBatchRunnerTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBatchRunnerTests;
	FTestBatch Test;
	if (!Test.Make(*this, TEXT("ImpactRunnerTestBatch"), TEXT("/Game/KimodoImpactRunnerTest"), 3, false))
	{
		return true;
	}
	UImpactBatchDefinition& Batch = *Test.Batch;
	TestEqual(TEXT("A batch's clips go under OutputRoot/BatchName"), FImpactBatchRunner::BatchPackagePath(Batch),
		FString(TEXT("/Game/KimodoImpactRunnerTest/ImpactRunnerTestBatch")));
	const FString Folder = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(FImpactBatchRunner::BatchPackagePath(Batch)));
	DeleteBatchOutput(Folder);

	// --- A whole run --------------------------------------------------------------------------------
	{
		FImpactBatchRunner Runner;
		FText Reason;
		if (!TestTrue(*FString::Printf(TEXT("The batch begins: %s"), *Reason.ToString()), Runner.Begin(Batch, FImpactBatchRunConfig(), Reason)))
		{
			return true;
		}
		TestEqual(TEXT("The runner works in the batch's folder"), FPaths::ConvertRelativePathToFull(Runner.GetBatchFolder()), Folder);
		TestEqual(TEXT("Three clips pending"), Runner.GetStats().Pending, 3);
		TestTrue(TEXT("Plenty of disk: no warning"), Runner.GetDiskWarning().IsEmpty());
		int32 Steps = 0;
		while (Runner.Step(Reason))
		{
			++Steps;
		}
		TestTrue(*FString::Printf(TEXT("The run ends without an error: %s"), *Reason.ToString()), Reason.IsEmpty());
		TestEqual(TEXT("One step per clip"), Steps, 3);
		TestTrue(TEXT("Done"), Runner.IsDone());
		TestEqual(TEXT("Every clip done"), Runner.GetStats().Done, 3);
		TestTrue(TEXT("The report is written"), Runner.Finish(Reason));
		TestEqual(TEXT("The report counts every clip"), Runner.GetReport().Total, 3);
		TestTrue(TEXT("The report is on disk"), IFileManager::Get().FileExists(*FImpactBatchRunner::ReportFilename(Runner.GetBatchFolder())));
	}

	FImpactBatchManifest All;
	FText Reason;
	FImpactBatchManifest::LoadAll(Folder, All, Reason);
	TestEqual(TEXT("The manifest records every clip"), All.Num(), 3);
	for (const FImpactManifestEntry& Entry : All.GetEntries())
	{
		TestTrue(*FString::Printf(TEXT("Clip %d has frames"), Entry.ClipIndex), Entry.Frames > 0);
		TestFalse(TEXT("Every entry names its source"), Entry.SourceAnimation.IsEmpty());
		TestFalse(TEXT("Every entry names its outcome"), Entry.Outcome.IsEmpty());
		TestTrue(*FString::Printf(TEXT("Clip %d has a readable name: %s"), Entry.ClipIndex, *Entry.FileName),
			Entry.FileName.StartsWith(FImpactClipName::IndexPrefix(Entry.ClipIndex)) && Entry.FileName.Contains(TEXT("_Run_")) != Entry.FileName.Contains(TEXT("_StandIdle_")));
		const FString Sidecar = Folder / Entry.FileName + TEXT(".json");
		FDatasetSidecar Read;
		if (TestTrue(*FString::Printf(TEXT("Clip %d's sidecar loads"), Entry.ClipIndex), FDatasetSidecarSerializer::LoadFromFile(Sidecar, Read, Reason)))
		{
			TestEqual(TEXT("A synthetic impact clip"), Read.ClipSource, FString(DatasetClipSource::SyntheticImpact));
			TestFalse(TEXT("With a caption"), Read.Labels.Caption.IsEmpty());
			TestEqual(TEXT("Its clip id"), Read.Impact.ClipId, Entry.ClipId);
			TestEqual(TEXT("Its frame count"), Read.Identity.DurationFrames, Entry.Frames);
		}
		if (Entry.bValid)
		{
			TestTrue(*FString::Printf(TEXT("Clip %d's animation is on disk"), Entry.ClipIndex), IFileManager::Get().FileExists(*(Folder / Entry.FileName + FPackageName::GetAssetPackageExtension())));
			TestTrue(TEXT("The asset is named like its file"), Entry.AnimSequencePath.EndsWith(TEXT(".") + Entry.FileName));
			TestTrue(TEXT("The entry records the bytes it took"), Entry.Bytes > 0);
		}
	}

	// --- A rerun has nothing to do ------------------------------------------------------------------
	{
		FImpactBatchRunner Runner;
		TestTrue(TEXT("The rerun begins"), Runner.Begin(Batch, FImpactBatchRunConfig(), Reason));
		TestEqual(TEXT("Nothing pending"), Runner.GetStats().Pending, 0);
		TestEqual(TEXT("Every clip skipped"), Runner.GetStats().Skipped, 3);
		TestFalse(TEXT("No step to take"), Runner.Step(Reason));
	}

	// --- A crash after clip 2 baked but before it was recorded: it is made again, once --------------
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Folder / FImpactBatchManifest::FilePattern), true, false);
		if (TestEqual(TEXT("One run, one manifest file"), Files.Num(), 1))
		{
			const FString File = Folder / Files[0];
			TArray<FString> Lines;
			FFileHelper::LoadFileToStringArray(Lines, *File);
			Lines.RemoveAll([](const FString& Line) { return Line.Contains(TEXT("\"clipIndex\":2")); });
			FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")) + TEXT("\n"), *File);
		}
		FImpactBatchRunner Runner;
		TestTrue(TEXT("The resumed run begins"), Runner.Begin(Batch, FImpactBatchRunConfig(), Reason));
		TestEqual(TEXT("Only the unrecorded clip is pending"), Runner.GetStats().Pending, 1);
		while (Runner.Step(Reason))
		{
		}
		TestTrue(*FString::Printf(TEXT("The resumed run ends without an error: %s"), *Reason.ToString()), Reason.IsEmpty());
		TestEqual(TEXT("It is made"), Runner.GetStats().Done, 1);
		FImpactBatchManifest Resumed;
		FImpactBatchManifest::LoadAll(Folder, Resumed, Reason);
		TestEqual(TEXT("The batch is whole, with no repeats"), Resumed.Num(), 3);
		TestEqual(TEXT("Nothing dropped"), Resumed.GetDroppedLines(), 0);
	}

	// --- A shard runs only its range, into its own manifest ------------------------------------------
	{
		DeleteBatchOutput(Folder);
		FImpactBatchRunConfig Shard;
		Shard.Start = 1;
		Shard.Count = 1;
		FImpactBatchRunner Runner;
		TestTrue(TEXT("The shard begins"), Runner.Begin(Batch, Shard, Reason));
		TestEqual(TEXT("One clip in the shard"), Runner.GetStats().Pending, 1);
		while (Runner.Step(Reason))
		{
		}
		TestTrue(*FString::Printf(TEXT("The shard ends without an error: %s"), *Reason.ToString()), Reason.IsEmpty());
		TestTrue(TEXT("The shard writes its own manifest"), IFileManager::Get().FileExists(*FImpactSharding::ManifestFilename(Folder, { 1, 1 })));
		FImpactBatchManifest Shards;
		FImpactBatchManifest::LoadAll(Folder, Shards, Reason);
		TestTrue(TEXT("And records only its clip"), Shards.Num() == 1 && Shards.Contains(1));
	}

	DeleteBatchOutput(Folder);
	// The output root itself, now empty.
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Folder), false, false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactBatchRunnerIsolationTest,
	"KimodoDatasetCapture.Impact.Runner.ClipsDoNotDependOnEarlierClips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactBatchRunnerIsolationTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBatchRunnerTests;
	// Run in one batch, a clip came out differently from the same clip run alone: in the shared
	// physics world each clip inherited the state the ones before it left (the 10k grid's clip 1695
	// was a Knockdown in its shard and a Flinch alone).
	constexpr int32 Clips = 6;
	FTestBatch Together, Alone;
	if (!Together.Make(*this, TEXT("ImpactIsolationTogether"), TEXT("/Game/KimodoImpactRunnerTest"), Clips, false)
		|| !Alone.Make(*this, TEXT("ImpactIsolationAlone"), TEXT("/Game/KimodoImpactRunnerTest"), Clips, false))
	{
		return true;
	}
	auto RunClips = [this](UImpactBatchDefinition& Batch, int32 Start, int32 Count)
	{
		FImpactBatchRunner Runner;
		FImpactBatchRunConfig Config;
		Config.Start = Start;
		Config.Count = Count;
		FText Reason;
		if (TestTrue(*FString::Printf(TEXT("Clips %d+%d begin: %s"), Start, Count, *Reason.ToString()), Runner.Begin(Batch, Config, Reason)))
		{
			while (Runner.Step(Reason))
			{
			}
			Runner.Finish(Reason);
		}
	};
	const FString TogetherFolder = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(FImpactBatchRunner::BatchPackagePath(*Together.Batch)));
	const FString AloneFolder = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(FImpactBatchRunner::BatchPackagePath(*Alone.Batch)));
	DeleteBatchOutput(TogetherFolder);
	DeleteBatchOutput(AloneFolder);

	RunClips(*Together.Batch, 0, Clips);
	for (int32 Clip = 0; Clip < Clips; ++Clip)
	{
		RunClips(*Alone.Batch, Clip, 1);
	}

	FImpactBatchManifest A, B;
	FText Reason;
	FImpactBatchManifest::LoadAll(TogetherFolder, A, Reason);
	FImpactBatchManifest::LoadAll(AloneFolder, B, Reason);
	for (int32 Clip = 0; Clip < Clips; ++Clip)
	{
		const FImpactManifestEntry* InBatch = A.Find(Clip);
		const FImpactManifestEntry* OnItsOwn = B.Find(Clip);
		if (!TestTrue(*FString::Printf(TEXT("Clip %d recorded both ways"), Clip), InBatch && OnItsOwn))
		{
			continue;
		}
		TestEqual(*FString::Printf(TEXT("Clip %d: same outcome in a batch as alone"), Clip), InBatch->Outcome, OnItsOwn->Outcome);
		TestEqual(*FString::Printf(TEXT("Clip %d: same length"), Clip), InBatch->Frames, OnItsOwn->Frames);
		TestEqual(*FString::Printf(TEXT("Clip %d: same validity"), Clip), InBatch->InvalidReason, OnItsOwn->InvalidReason);
	}
	DeleteBatchOutput(TogetherFolder);
	DeleteBatchOutput(AloneFolder);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactBatchRunnerDescriptionsTest,
	"KimodoDatasetCapture.Impact.Runner.WritesDescriptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactBatchRunnerDescriptionsTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBatchRunnerTests;
	FTestBatch Test;
	if (!Test.Make(*this, TEXT("ImpactDescriptionsBatch"), TEXT("/Game/KimodoImpactRunnerTest"), 3, false))
	{
		return true;
	}
	UImpactBatchDefinition& Batch = *Test.Batch;
	const FString Folder = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(FImpactBatchRunner::BatchPackagePath(Batch)));
	const FString File = FImpactBatchRunner::DescriptionsFilename(Batch);
	TestEqual(TEXT("Shared by every batch under the output root"), File, FPaths::GetPath(Folder) / TEXT("descriptions.json"));
	DeleteBatchOutput(Folder);
	IFileManager::Get().Delete(*File);

	// Already in the index: a stale clip of this batch, and another generator's animation.
	FText Reason;
	{
		FDatasetDescriptions Existing;
		FDatasetDescriptionEntry Stale;
		Stale.Name = TEXT("IMP_999999_Old");
		Stale.File = TEXT("ImpactDescriptionsBatch/IMP_999999_Old.uasset");
		Stale.Kind = TEXT("impact");
		Stale.Descriptions = { TEXT("A person falls.") };
		FDatasetDescriptionEntry Climb;
		Climb.Name = TEXT("CLB_000001_Traverse");
		Climb.File = TEXT("Climb/CLB_000001_Traverse.fbx");
		Climb.Kind = TEXT("climb");
		Climb.Descriptions = { TEXT("A person climbs sideways along a wall.") };
		Existing.Add(Stale, Reason);
		Existing.Add(Climb, Reason);
		Existing.SaveToFile(File, Reason);
	}

	{
		FImpactBatchRunner Runner;
		if (!TestTrue(*FString::Printf(TEXT("The batch begins: %s"), *Reason.ToString()), Runner.Begin(Batch, FImpactBatchRunConfig(), Reason)))
		{
			return true;
		}
		while (Runner.Step(Reason))
		{
		}
		TestTrue(*FString::Printf(TEXT("Finishes: %s"), *Reason.ToString()), Runner.Finish(Reason));
	}

	FImpactBatchManifest Manifest;
	FImpactBatchManifest::LoadAll(Folder, Manifest, Reason);
	FDatasetDescriptions Index;
	if (!TestTrue(*FString::Printf(TEXT("The index is written: %s"), *Reason.ToString()), Index.LoadFromFile(File, Reason)))
	{
		return true;
	}
	int32 Valid = 0;
	for (const FImpactManifestEntry& Entry : Manifest.GetEntries())
	{
		const FDatasetDescriptionEntry* Listed = Index.Find(Entry.FileName);
		if (!Entry.bValid)
		{
			TestNull(*FString::Printf(TEXT("Invalid clip %d is not listed"), Entry.ClipIndex), Listed);
			TestEqual(TEXT("and has no descriptions"), Entry.Descriptions.Num(), 0);
			continue;
		}
		++Valid;
		TestEqual(*FString::Printf(TEXT("Clip %d carries three phrasings in its manifest entry"), Entry.ClipIndex), Entry.Descriptions.Num(), 3);
		if (TestNotNull(*FString::Printf(TEXT("Clip %d is listed by name"), Entry.ClipIndex), Listed))
		{
			TestEqual(TEXT("as an impact"), Listed->Kind, FString(TEXT("impact")));
			TestEqual(TEXT("with its file under the batch"), Listed->File, FString(TEXT("ImpactDescriptionsBatch/")) + Entry.FileName + TEXT(".uasset"));
			TestEqual(TEXT("and its phrasings"), Listed->Descriptions, Entry.Descriptions);
		}
	}
	TestTrue(TEXT("The batch made valid clips"), Valid > 0);
	TestNull(TEXT("The batch's stale clip is dropped"), Index.Find(TEXT("IMP_999999_Old")));
	TestNotNull(TEXT("Another generator's animation is kept"), Index.Find(TEXT("CLB_000001_Traverse")));
	TestEqual(TEXT("Nothing else"), Index.Num(), Valid + 1);
	DeleteBatchOutput(Folder);
	IFileManager::Get().Delete(*File);
	return true;
}
