// Copyright Animotive. All Rights Reserved.

#include "HAL/FileManager.h"
#include "ImpactBatchManifest.h"
#include "ImpactShard.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace ImpactShardTests
{
	FImpactManifestEntry Entry(int32 Index)
	{
		FImpactManifestEntry E;
		E.ClipIndex = Index;
		E.ClipId = FString::Printf(TEXT("imp_%016x"), Index + 1);
		E.Outcome = TEXT("Stagger");
		return E;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactShardSplitTest,
	"KimodoDatasetCapture.Impact.Shard.Split",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactShardSplitTest::RunTest(const FString& Parameters)
{
	struct FCase { int32 Clips; int32 Shards; };
	const FCase Cases[] = { { 10, 3 }, { 1000, 7 }, { 12, 4 }, { 1, 1 }, { 5, 1 }, { 3, 8 }, { 100000, 16 } };
	for (const FCase& Case : Cases)
	{
		const TArray<FImpactShardRange> Ranges = FImpactSharding::Split(Case.Clips, Case.Shards);
		const FString What = FString::Printf(TEXT("%d clips in %d shards"), Case.Clips, Case.Shards);
		TestEqual(*(What + TEXT(": one range per shard, never more than one per clip")), Ranges.Num(), FMath::Min(Case.Clips, Case.Shards));

		// Contiguous from zero to the end: no gap, no overlap.
		int32 Next = 0;
		int32 Smallest = TNumericLimits<int32>::Max();
		int32 Largest = 0;
		for (const FImpactShardRange& Range : Ranges)
		{
			TestEqual(*(What + TEXT(": each range starts where the last ended")), Range.Start, Next);
			TestTrue(*(What + TEXT(": no range is empty")), Range.Count > 0);
			Next = Range.End();
			Smallest = FMath::Min(Smallest, Range.Count);
			Largest = FMath::Max(Largest, Range.Count);
		}
		TestEqual(*(What + TEXT(": the ranges end at the last clip")), Next, Case.Clips);
		TestTrue(*(What + TEXT(": sizes differ by at most one")), Largest - Smallest <= 1);
	}
	TestEqual(TEXT("10 in 3 is 4, 3, 3"), FImpactSharding::Split(10, 3),
		TArray<FImpactShardRange>({ { 0, 4 }, { 4, 3 }, { 7, 3 } }));
	TestEqual(TEXT("No clips, no ranges"), FImpactSharding::Split(0, 4).Num(), 0);
	TestEqual(TEXT("Fewer than one shard is one shard"), FImpactSharding::Split(6, 0), TArray<FImpactShardRange>({ { 0, 6 } }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactShardResolveTest,
	"KimodoDatasetCapture.Impact.Shard.Resolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactShardResolveTest::RunTest(const FString& Parameters)
{
	FImpactShardRange Range;
	FText Reason;
	TestTrue(TEXT("A range inside the batch"), FImpactSharding::Resolve(100, 20, 30, Range, Reason));
	TestEqual(TEXT("is kept as asked"), Range, FImpactShardRange{ 20, 30 });

	TestTrue(TEXT("No count"), FImpactSharding::Resolve(100, 20, -1, Range, Reason));
	TestEqual(TEXT("runs to the end of the batch"), Range, FImpactShardRange{ 20, 80 });

	TestTrue(TEXT("A count past the end"), FImpactSharding::Resolve(100, 90, 50, Range, Reason));
	TestEqual(TEXT("stops at the end"), Range, FImpactShardRange{ 90, 10 });

	TestTrue(TEXT("The whole batch"), FImpactSharding::Resolve(100, 0, -1, Range, Reason));
	TestEqual(TEXT("is every clip"), Range, FImpactShardRange{ 0, 100 });

	TestTrue(TEXT("A zero count is allowed"), FImpactSharding::Resolve(100, 5, 0, Range, Reason));
	TestEqual(TEXT("and runs nothing"), Range.Count, 0);

	Reason = FText::GetEmpty();
	TestFalse(TEXT("A start past the last clip is refused"), FImpactSharding::Resolve(100, 100, 10, Range, Reason));
	TestFalse(TEXT("with a reason"), Reason.IsEmpty());
	TestFalse(TEXT("A negative start is refused"), FImpactSharding::Resolve(100, -1, 10, Range, Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactShardManifestMergeTest,
	"KimodoDatasetCapture.Impact.Shard.ManifestsMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactShardManifestMergeTest::RunTest(const FString& Parameters)
{
	using namespace ImpactShardTests;
	const FString Folder = FPaths::AutomationTransientDir() / TEXT("ImpactShards") / FGuid::NewGuid().ToString();
	FText Reason;

	const TArray<FImpactShardRange> Ranges = FImpactSharding::Split(10, 3);
	TSet<FString> Files;
	for (const FImpactShardRange& Range : Ranges)
	{
		const FString File = FImpactSharding::ManifestFilename(Folder, Range);
		TestTrue(TEXT("A shard's manifest is inside the batch folder"), FPaths::IsUnderDirectory(File, Folder));
		TestTrue(TEXT("A shard's manifest is found by the manifest pattern"), FPaths::GetCleanFilename(File).MatchesWildcard(FImpactBatchManifest::FilePattern));
		Files.Add(File);
	}
	TestEqual(TEXT("Every shard writes its own file"), Files.Num(), Ranges.Num());

	// Shards 0 and 2 finished; shard 1 was killed after two clips. Shard 2 also redid clip 4 (an
	// overlapping hand-run range): the repeat counts once.
	for (const FImpactShardRange& Range : Ranges)
	{
		FImpactBatchManifest Shard;
		TestTrue(TEXT("Shard manifest opens"), Shard.Load(FImpactSharding::ManifestFilename(Folder, Range), Reason));
		const int32 Stop = Range.Start == 4 ? Range.Start + 2 : Range.End();
		for (int32 Index = Range.Start; Index < Stop; ++Index)
		{
			Shard.Append(Entry(Index), Reason);
		}
		if (Range.Start == 7)
		{
			Shard.Append(Entry(4), Reason);
		}
	}

	FImpactBatchManifest All;
	TestTrue(TEXT("Every shard's manifest loads as one"), FImpactBatchManifest::LoadAll(Folder, All, Reason));
	TestEqual(TEXT("The merged manifest has each finished clip once"), All.Num(), 9);
	TestEqual(TEXT("Only the killed shard's unfinished clip is pending"), All.Pending(0, 10), TArray<int32>({ 6 }));
	TestTrue(TEXT("The merged view has no file of its own"), All.GetFilename().IsEmpty());
	FText AppendReason;
	TestFalse(TEXT("so it cannot be appended to"), All.Append(Entry(6), AppendReason));

	FImpactBatchManifest Extra;
	FImpactBatchManifest Twice = All;
	Extra.Merge(All);
	TestEqual(TEXT("Merging into an empty manifest takes every entry"), Extra.Num(), 9);
	TestEqual(TEXT("Merging the same entries again skips them all"), Twice.Merge(All), 9);
	TestEqual(TEXT("and adds nothing"), Twice.Num(), 9);

	// The merged manifest written as one file reads back the same.
	const FString Merged = Folder / TEXT("Merged") / TEXT("Manifest.jsonl");
	TestTrue(TEXT("The merged manifest saves"), All.SaveAs(Merged, Reason));
	FImpactBatchManifest Reread;
	Reread.Load(Merged, Reason);
	TestEqual(TEXT("and reads back with every clip"), Reread.Num(), 9);
	TestTrue(TEXT("including the last shard's"), Reread.Contains(9));

	FImpactBatchManifest Empty;
	TestTrue(TEXT("A batch folder that does not exist yet has no clips"), FImpactBatchManifest::LoadAll(Folder / TEXT("Missing"), Empty, Reason));
	TestEqual(TEXT("none"), Empty.Num(), 0);

	IFileManager::Get().DeleteDirectory(*Folder, false, true);
	return true;
}
