// Copyright Animotive. All Rights Reserved.

#include "HAL/FileManager.h"
#include "ImpactBatchManifest.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace ImpactBatchManifestTests
{
	/** A fresh manifest path in the automation transient folder; the file does not exist yet. */
	FString TempManifest()
	{
		return FPaths::AutomationTransientDir() / TEXT("ImpactManifest") / (FGuid::NewGuid().ToString() + TEXT(".jsonl"));
	}

	FImpactManifestEntry Entry(int32 Index)
	{
		FImpactManifestEntry E;
		E.ClipIndex = Index;
		E.ClipId = FString::Printf(TEXT("imp_%016x"), Index + 1);
		E.AnimSequencePath = FString::Printf(TEXT("/Game/Out/Batch/%s.%s"), *E.ClipId, *E.ClipId);
		E.SourceAnimation = TEXT("/Game/Anims/M_Neutral_Run_Loop_F.M_Neutral_Run_Loop_F");
		E.ImpactorId = TEXT("SmallBullet");
		E.Region = TEXT("FootLeft");
		E.Intensity = TEXT("Heavy");
		E.DeliveryMode = TEXT("Instant");
		E.Outcome = TEXT("Knockdown");
		E.Frames = 120 + Index;
		E.Bytes = 1600000;
		return E;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactManifestLineTest,
	"KimodoDatasetCapture.Impact.Manifest.Line",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactManifestLineTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBatchManifestTests;
	FImpactManifestEntry Original = Entry(7);
	Original.bValid = false;
	Original.InvalidReason = TEXT("Physics exploded: \"pelvis\" moved 9000 cm\nin one frame");
	const FString Line = FImpactBatchManifest::ToLine(Original);
	TestFalse(TEXT("A line holds no line break, even when a field does"), Line.Contains(TEXT("\n")) || Line.Contains(TEXT("\r")));
	TestTrue(TEXT("A line is not empty"), !Line.IsEmpty());

	FImpactManifestEntry Read;
	if (!TestTrue(TEXT("The line reads back"), FImpactBatchManifest::FromLine(Line, Read)))
	{
		return true;
	}
	TestEqual(TEXT("Index"), Read.ClipIndex, Original.ClipIndex);
	TestEqual(TEXT("Id"), Read.ClipId, Original.ClipId);
	TestEqual(TEXT("Asset"), Read.AnimSequencePath, Original.AnimSequencePath);
	TestEqual(TEXT("Source"), Read.SourceAnimation, Original.SourceAnimation);
	TestEqual(TEXT("Impactor"), Read.ImpactorId, Original.ImpactorId);
	TestEqual(TEXT("Region"), Read.Region, Original.Region);
	TestEqual(TEXT("Intensity"), Read.Intensity, Original.Intensity);
	TestEqual(TEXT("Mode"), Read.DeliveryMode, Original.DeliveryMode);
	TestEqual(TEXT("Outcome"), Read.Outcome, Original.Outcome);
	TestFalse(TEXT("Validity"), Read.bValid);
	TestEqual(TEXT("Reason"), Read.InvalidReason, Original.InvalidReason);
	TestEqual(TEXT("Frames"), Read.Frames, Original.Frames);
	TestEqual(TEXT("Bytes"), Read.Bytes, Original.Bytes);

	FImpactManifestEntry Ignored;
	TestFalse(TEXT("Half a line does not read"), FImpactBatchManifest::FromLine(Line.Left(Line.Len() / 2), Ignored));
	TestFalse(TEXT("An empty line does not read"), FImpactBatchManifest::FromLine(FString(), Ignored));
	TestFalse(TEXT("A line without a clip index does not read"), FImpactBatchManifest::FromLine(TEXT("{\"clipId\":\"imp_1\"}"), Ignored));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactManifestPersistTest,
	"KimodoDatasetCapture.Impact.Manifest.PersistedPerClip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactManifestPersistTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBatchManifestTests;
	const FString File = TempManifest();
	FText Reason;

	FImpactBatchManifest Writer;
	TestTrue(TEXT("A missing file loads as an empty manifest"), Writer.Load(File, Reason));
	TestEqual(TEXT("Empty"), Writer.Num(), 0);

	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestTrue(*FString::Printf(TEXT("Clip %d appended"), Index), Writer.Append(Entry(Index), Reason));

		// Another reader sees the clip at once: nothing waits for the batch to end.
		FImpactBatchManifest Reader;
		TestTrue(TEXT("Reloads"), Reader.Load(File, Reason));
		TestEqual(*FString::Printf(TEXT("After clip %d the file holds every clip so far"), Index), Reader.Num(), Index + 1);
		TestTrue(*FString::Printf(TEXT("Clip %d is on disk"), Index), Reader.Contains(Index));
	}

	FImpactBatchManifest Reloaded;
	Reloaded.Load(File, Reason);
	const FImpactManifestEntry* Found = Reloaded.Find(1);
	if (TestNotNull(TEXT("Clip 1 found after reload"), Found))
	{
		TestEqual(TEXT("Clip 1 keeps its id"), Found->ClipId, Entry(1).ClipId);
	}
	TestNull(TEXT("A clip never run is not found"), Reloaded.Find(5));
	TestEqual(TEXT("Nothing dropped"), Reloaded.GetDroppedLines(), 0);

	IFileManager::Get().Delete(*File);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactManifestPendingTest,
	"KimodoDatasetCapture.Impact.Manifest.ResumeSkipsCompleted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactManifestPendingTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBatchManifestTests;
	const FString File = TempManifest();
	FText Reason;

	FImpactBatchManifest First;
	First.Load(File, Reason);
	First.Append(Entry(0), Reason);
	First.Append(Entry(2), Reason);
	First.Append(Entry(5), Reason);

	// A clip that was baked but never recorded: its files exist, its entry does not.
	const FString Stray = FPaths::GetPath(File) / Entry(3).ClipId + TEXT(".json");
	FFileHelper::SaveStringToFile(TEXT("{}"), *Stray);

	FImpactBatchManifest Resumed;
	TestTrue(TEXT("Reloads"), Resumed.Load(File, Reason));
	TestEqual(TEXT("Pending skips completed clips and keeps the order"), Resumed.Pending(0, 6), TArray<int32>({ 1, 3, 4 }));
	TestEqual(TEXT("A partly written clip without an entry is made again"), Resumed.Pending(3, 1), TArray<int32>({ 3 }));
	TestEqual(TEXT("A range that is all done has nothing pending"), Resumed.Pending(5, 1), TArray<int32>());
	TestEqual(TEXT("Pending covers only the range asked for"), Resumed.Pending(4, 3), TArray<int32>({ 4, 6 }));
	TestEqual(TEXT("An empty range has nothing pending"), Resumed.Pending(0, 0), TArray<int32>());

	IFileManager::Get().Delete(*Stray);
	IFileManager::Get().Delete(*File);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactManifestTruncationTest,
	"KimodoDatasetCapture.Impact.Manifest.SurvivesTruncation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactManifestTruncationTest::RunTest(const FString& Parameters)
{
	using namespace ImpactBatchManifestTests;
	const FString File = TempManifest();
	FText Reason;

	{
		FImpactBatchManifest Writer;
		Writer.Load(File, Reason);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Writer.Append(Entry(Index), Reason);
		}
	}

	// A crash mid-append: the file ends partway through the third entry.
	FString Text;
	FFileHelper::LoadFileToString(Text, *File);
	const FString Third = FImpactBatchManifest::ToLine(Entry(2));
	const int32 ThirdStart = Text.Find(Third);
	if (!TestTrue(TEXT("The third entry is in the file"), ThirdStart != INDEX_NONE))
	{
		return true;
	}
	FFileHelper::SaveStringToFile(Text.Left(ThirdStart + Third.Len() / 2), *File);

	FImpactBatchManifest Resumed;
	TestTrue(TEXT("A cut-short file still loads"), Resumed.Load(File, Reason));
	TestEqual(TEXT("The complete entries survive"), Resumed.Num(), 2);
	TestFalse(TEXT("The cut-short entry is not counted as done"), Resumed.Contains(2));
	TestEqual(TEXT("The cut-short line is reported"), Resumed.GetDroppedLines(), 1);
	TestEqual(TEXT("The interrupted clip is pending again"), Resumed.Pending(0, 3), TArray<int32>({ 2 }));

	// The run goes on: the remade clip's entry must land on a line of its own.
	TestTrue(TEXT("Appends after the repair"), Resumed.Append(Entry(2), Reason));
	TestTrue(TEXT("And again"), Resumed.Append(Entry(3), Reason));
	FImpactBatchManifest Final;
	Final.Load(File, Reason);
	TestEqual(TEXT("Every entry reads back after the repair"), Final.Num(), 4);
	TestEqual(TEXT("No line is lost or garbled"), Final.GetDroppedLines(), 0);
	const FImpactManifestEntry* Remade = Final.Find(2);
	if (TestNotNull(TEXT("The remade clip is recorded"), Remade))
	{
		TestEqual(TEXT("With its full entry"), Remade->Frames, Entry(2).Frames);
	}

	// A repeated index (two shards that overlapped, a hand-edited file) counts once.
	FFileHelper::SaveStringToFile(FImpactBatchManifest::ToLine(Entry(1)) + TEXT("\n"), *File,
		FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
	TestFalse(TEXT("Append refuses a clip already recorded"), Final.Append(Entry(3), Reason));
	FImpactBatchManifest Deduped;
	Deduped.Load(File, Reason);
	TestEqual(TEXT("A repeated clip index counts once"), Deduped.Num(), 4);
	TestEqual(TEXT("The repeat is reported"), Deduped.GetDroppedLines(), 1);

	IFileManager::Get().Delete(*File);
	return true;
}
