// Copyright Animotive. All Rights Reserved.

#include "HAL/FileManager.h"
#include "ImpactBatchManifest.h"
#include "ImpactDiskEstimate.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactDiskEstimateSizeTest,
	"KimodoDatasetCapture.Impact.Disk.Estimate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactDiskEstimateSizeTest::RunTest(const FString& Parameters)
{
	// The spike's measured clip: 3 s at 60 Hz, 88 bones, ~1.6 MB.
	const int64 Spike = FImpactDiskEstimate::BytesPerClip(88, 180);
	TestTrue(*FString::Printf(TEXT("A spike-sized clip estimates near the measured 1.6 MB (got %lld)"), Spike),
		Spike > 1400000 && Spike < 1900000);
	TestTrue(TEXT("Longer clips take more"), FImpactDiskEstimate::BytesPerClip(88, 290) > Spike);
	TestTrue(TEXT("More bones take more"), FImpactDiskEstimate::BytesPerClip(120, 180) > Spike);
	TestEqual(TEXT("10,000 clips"), FImpactDiskEstimate::Estimate(10000, 1600000), int64(16000000000));
	TestEqual(TEXT("Nothing pending needs nothing"), FImpactDiskEstimate::Estimate(0, 1600000), int64(0));

	// Once clips are on disk, the estimate uses what they actually took.
	const FString File = FPaths::AutomationTransientDir() / TEXT("ImpactDisk") / (FGuid::NewGuid().ToString() + TEXT(".jsonl"));
	FImpactBatchManifest Manifest;
	FText Reason;
	Manifest.Load(File, Reason);
	TestEqual(TEXT("With no clips yet, the upper bound"), FImpactDiskEstimate::PerClip(Manifest, 88, 180), Spike);
	const int64 Sizes[] = { 900000, 1100000, 0 };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FImpactManifestEntry Entry;
		Entry.ClipIndex = Index;
		Entry.Bytes = Sizes[Index];
		// The third was invalid and discarded: it took no disk and says nothing about clip size.
		Entry.bValid = Sizes[Index] > 0;
		Manifest.Append(Entry, Reason);
	}
	TestEqual(TEXT("The mean of the clips kept so far"), FImpactDiskEstimate::PerClip(Manifest, 88, 180), int64(1000000));

	IFileManager::Get().Delete(*File);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactDiskEstimateWarnTest,
	"KimodoDatasetCapture.Impact.Disk.WarnsWhenFull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactDiskEstimateWarnTest::RunTest(const FString& Parameters)
{
	FText Warning;
	TestTrue(TEXT("Warns when the batch will not fit"), FImpactDiskEstimate::ShouldWarn(16000000000, 10000000000, Warning));
	TestFalse(TEXT("with a warning to show"), Warning.IsEmpty());
	TestTrue(TEXT("that gives the needed size"), Warning.ToString().Contains(TEXT("14.9 GB")));
	TestTrue(TEXT("and the free size"), Warning.ToString().Contains(TEXT("9.3 GB")));

	Warning = FText::GetEmpty();
	TestFalse(TEXT("Silent when it fits"), FImpactDiskEstimate::ShouldWarn(1000000000, 10000000000, Warning));
	TestTrue(TEXT("with no warning"), Warning.IsEmpty());
	TestFalse(TEXT("Silent at exactly the free space"), FImpactDiskEstimate::ShouldWarn(10000000000, 10000000000, Warning));
	TestFalse(TEXT("Unknown free space never warns"), FImpactDiskEstimate::ShouldWarn(16000000000, -1, Warning));

	const int64 Free = FImpactDiskEstimate::FreeBytes(FPaths::ProjectSavedDir());
	TestTrue(TEXT("The project drive's free space can be read"), Free > 0);
	return true;
}
