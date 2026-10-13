// Copyright Animotive. All Rights Reserved.

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "ImpactBatchManifest.h"
#include "ImpactDistributionReport.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ImpactDistributionReportTests
{
	struct FRow
	{
		const TCHAR* Region;
		const TCHAR* Intensity;
		const TCHAR* Mode;
		const TCHAR* Outcome;
		const TCHAR* Source;
		const TCHAR* Impactor;
		bool bValid;
		const TCHAR* Reason;
		int32 Frames;
	};

	/** Seven clips: five valid, two invalid (one discarded with no asset). */
	const FRow Rows[] = {
		{ TEXT("Chest"), TEXT("Light"), TEXT("Instant"), TEXT("Flinch"), TEXT("Run"), TEXT("Punch"), true, TEXT(""), 100 },
		{ TEXT("Chest"), TEXT("Medium"), TEXT("Burst"), TEXT("Stagger"), TEXT("Idle"), TEXT("Punch"), true, TEXT(""), 140 },
		{ TEXT("FootLeft"), TEXT("Heavy"), TEXT("Projectile"), TEXT("Knockdown"), TEXT("Run"), TEXT("SmallBullet"), true, TEXT(""), 200 },
		{ TEXT("Pelvis"), TEXT("Extreme"), TEXT("Sustained"), TEXT("Ragdoll"), TEXT("Idle"), TEXT("CarBumper"), true, TEXT(""), 260 },
		{ TEXT("Head"), TEXT("Light"), TEXT("Instant"), TEXT("Flinch"), TEXT("Walk"), TEXT("SmallBullet"), true, TEXT(""), 100 },
		{ TEXT("Head"), TEXT("Extreme"), TEXT("Instant"), TEXT("Ragdoll"), TEXT("Run"), TEXT("CarBumper"), false, TEXT("Physics exploded"), 30 },
		{ TEXT("Chest"), TEXT("Heavy"), TEXT("Instant"), TEXT("Stagger"), TEXT("Run"), TEXT("Shove"), false, TEXT("Physics exploded"), 40 },
	};

	FImpactBatchManifest MakeManifest(FString& OutFile)
	{
		OutFile = FPaths::AutomationTransientDir() / TEXT("ImpactReport") / (FGuid::NewGuid().ToString() + TEXT(".jsonl"));
		FImpactBatchManifest Manifest;
		FText Reason;
		Manifest.Load(OutFile, Reason);
		int32 Index = 0;
		for (const FRow& Row : Rows)
		{
			FImpactManifestEntry E;
			E.ClipIndex = Index++;
			E.ClipId = FString::Printf(TEXT("imp_%016x"), Index);
			E.Region = Row.Region;
			E.Intensity = Row.Intensity;
			E.DeliveryMode = Row.Mode;
			E.Outcome = Row.Outcome;
			E.SourceAnimation = FString::Printf(TEXT("/Game/Anims/%s.%s"), Row.Source, Row.Source);
			E.ImpactorId = Row.Impactor;
			E.bValid = Row.bValid;
			E.InvalidReason = Row.Reason;
			E.Frames = Row.Frames;
			E.Bytes = Row.bValid ? 1000 : 0;
			Manifest.Append(E, Reason);
		}
		return Manifest;
	}

	int32 Sum(const TMap<FString, int32>& Counts)
	{
		int32 Total = 0;
		for (const TPair<FString, int32>& Pair : Counts)
		{
			Total += Pair.Value;
		}
		return Total;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactDistributionCountsTest,
	"KimodoDatasetCapture.Impact.Report.CountsMatchManifest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactDistributionCountsTest::RunTest(const FString& Parameters)
{
	using namespace ImpactDistributionReportTests;
	FString File;
	const FImpactBatchManifest Manifest = MakeManifest(File);
	const FImpactDistributionReport Report = FImpactDistributionReport::Build(Manifest);

	TestEqual(TEXT("Total is every clip"), Report.Total, 7);
	TestEqual(TEXT("Valid clips"), Report.Valid, 5);
	TestEqual(TEXT("Invalid clips"), Report.Invalid, 2);

	// Every category counts the valid clips exactly once.
	TestEqual(TEXT("Regions sum to the valid clips"), Sum(Report.Regions), 5);
	TestEqual(TEXT("Intensities sum to the valid clips"), Sum(Report.Intensities), 5);
	TestEqual(TEXT("Modes sum to the valid clips"), Sum(Report.DeliveryModes), 5);
	TestEqual(TEXT("Outcomes sum to the valid clips"), Sum(Report.Outcomes), 5);
	TestEqual(TEXT("Sources sum to the valid clips"), Sum(Report.SourceAnimations), 5);
	TestEqual(TEXT("Impactors sum to the valid clips"), Sum(Report.Impactors), 5);

	TestEqual(TEXT("Chest: two valid (the invalid shove is not counted)"), Report.Regions.FindRef(TEXT("Chest")), 2);
	TestEqual(TEXT("Head: one valid"), Report.Regions.FindRef(TEXT("Head")), 1);
	TestEqual(TEXT("Light"), Report.Intensities.FindRef(TEXT("Light")), 2);
	TestEqual(TEXT("Extreme: the invalid one is not counted"), Report.Intensities.FindRef(TEXT("Extreme")), 1);
	TestEqual(TEXT("Instant"), Report.DeliveryModes.FindRef(TEXT("Instant")), 2);
	TestEqual(TEXT("Projectile"), Report.DeliveryModes.FindRef(TEXT("Projectile")), 1);
	TestEqual(TEXT("Every outcome appears: flinch"), Report.Outcomes.FindRef(TEXT("Flinch")), 2);
	TestEqual(TEXT("stagger"), Report.Outcomes.FindRef(TEXT("Stagger")), 1);
	TestEqual(TEXT("knockdown"), Report.Outcomes.FindRef(TEXT("Knockdown")), 1);
	TestEqual(TEXT("ragdoll"), Report.Outcomes.FindRef(TEXT("Ragdoll")), 1);
	TestEqual(TEXT("Sources by asset path: run"), Report.SourceAnimations.FindRef(TEXT("/Game/Anims/Run.Run")), 2);
	TestEqual(TEXT("idle"), Report.SourceAnimations.FindRef(TEXT("/Game/Anims/Idle.Idle")), 2);
	TestEqual(TEXT("Impactors: small bullet"), Report.Impactors.FindRef(TEXT("SmallBullet")), 2);
	TestEqual(TEXT("Invalid clips by reason"), Report.InvalidReasons.FindRef(TEXT("Physics exploded")), 2);
	TestEqual(TEXT("Mean frames over valid clips"), Report.MeanFrames, 160.0f);
	TestEqual(TEXT("Bytes over every kept clip"), Report.Bytes, int64(5000));

	const FImpactDistributionReport Empty = FImpactDistributionReport::Build(FImpactBatchManifest());
	TestEqual(TEXT("An empty manifest reports nothing"), Empty.Total, 0);
	TestEqual(TEXT("with no mean"), Empty.MeanFrames, 0.0f);

	IFileManager::Get().Delete(*File);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactDistributionOutputTest,
	"KimodoDatasetCapture.Impact.Report.Written",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactDistributionOutputTest::RunTest(const FString& Parameters)
{
	using namespace ImpactDistributionReportTests;
	FString File;
	const FImpactDistributionReport Report = FImpactDistributionReport::Build(MakeManifest(File));

	const FString ReportFile = FPaths::GetPath(File) / FGuid::NewGuid().ToString() + TEXT("_Report.json");
	FText Reason;
	TestTrue(TEXT("The report saves"), Report.Save(ReportFile, Reason));
	FString Json;
	FFileHelper::LoadFileToString(Json, *ReportFile);
	TSharedPtr<FJsonObject> Object;
	if (TestTrue(TEXT("The saved report is JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) && Object.IsValid()))
	{
		TestEqual(TEXT("with the total"), int32(Object->GetNumberField(TEXT("total"))), 7);
		const TSharedPtr<FJsonObject>* Outcomes = nullptr;
		if (TestTrue(TEXT("and outcome counts"), Object->TryGetObjectField(TEXT("outcomes"), Outcomes)))
		{
			TestEqual(TEXT("ragdoll in the file"), int32((*Outcomes)->GetNumberField(TEXT("Ragdoll"))), 1);
		}
		TestTrue(TEXT("and region counts"), Object->HasField(TEXT("regions")));
		TestTrue(TEXT("and mode counts"), Object->HasField(TEXT("deliveryModes")));
		TestTrue(TEXT("and intensity counts"), Object->HasField(TEXT("intensities")));
		TestTrue(TEXT("and source counts"), Object->HasField(TEXT("sourceAnimations")));
	}

	// The log summary names every category, with shares, largest first.
	const FString Text = Report.ToText();
	for (const TCHAR* Heading : { TEXT("Region"), TEXT("Intensity"), TEXT("Mode"), TEXT("Outcome"), TEXT("Source"), TEXT("Impactor") })
	{
		TestTrue(*FString::Printf(TEXT("The summary has a %s line"), Heading), Text.Contains(Heading));
	}
	TestTrue(TEXT("The summary gives shares"), Text.Contains(TEXT("Flinch 2 (40%)")));
	TestTrue(TEXT("Largest first"), Text.Find(TEXT("Flinch 2")) < Text.Find(TEXT("Ragdoll 1")));
	TestTrue(TEXT("The summary states the invalid count"), Text.Contains(TEXT("2 invalid")));

	IFileManager::Get().Delete(*ReportFile);
	IFileManager::Get().Delete(*File);
	return true;
}
