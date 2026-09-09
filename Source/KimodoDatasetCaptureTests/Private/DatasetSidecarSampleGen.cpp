// Copyright Animotive. All Rights Reserved.

#include "DatasetSidecar.h"
#include "DatasetSidecarSerializer.h"
#include "DatasetSkeletonFingerprint.h"
#include "DatasetTrimRange.h"
#include "HAL/FileManager.h"
#include "KimodoDatasetCaptureLog.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

/**
 * Writes a representative schema v1 sidecar to
 *   Saved/KimodoDatasetCapture/SidecarSchemaSample.json
 * and leaves it there.
 *
 * The sidecar JSON is the contract shared with the Animotive training pipeline and with the
 * legacy-build capture tool described in conductor/tech-stack.md. Asserting round-trips in
 * memory proves the code agrees with itself; it does not let a person read the format their
 * pipeline has to ingest. This test produces that artefact as a side effect of the normal
 * suite, so the sample can never drift from the schema the code actually writes.
 *
 * It is a test, not a commentary: it asserts that what it wrote reads back identical.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarSchemaSampleTest,
	"KimodoDatasetCapture.SidecarSerializer.SchemaSample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarSchemaSampleTest::RunTest(const FString& Parameters)
{
	FDatasetSidecar Sidecar;

	Sidecar.Identity.TakeName = TEXT("Take_0007_HeroStairs");
	Sidecar.Identity.LevelSequencePath = TEXT("/Game/Cinematics/Takes/2026-09-09/Take_0007.Take_0007");
	Sidecar.Identity.AnimSequencePath = TEXT("/Game/Cinematics/Takes/2026-09-09/Take_0007_Hero.Take_0007_Hero");
	Sidecar.Identity.FrameRateNumerator = 60;
	Sidecar.Identity.FrameRateDenominator = 1;
	Sidecar.Identity.TickResolutionNumerator = 24000;
	Sidecar.Identity.TickResolutionDenominator = 1;
	Sidecar.Identity.CaptureStartedUtc = TEXT("2026-09-09T13:04:57Z");
	Sidecar.Identity.DurationFrames = 4;

	Sidecar.Skeleton.SkeletonPath = TEXT("/Game/Characters/Hero/SK_Hero_Skeleton.SK_Hero_Skeleton");
	Sidecar.Skeleton.BoneCount = 5;
	Sidecar.Skeleton.BoneHierarchyFingerprint = FDatasetSkeletonFingerprint::Compute(
		TArray<FDatasetBoneEntry>
		{
			FDatasetBoneEntry(TEXT("root"), INDEX_NONE),
			FDatasetBoneEntry(TEXT("pelvis"), 0),
			FDatasetBoneEntry(TEXT("spine_01"), 1),
			FDatasetBoneEntry(TEXT("thigh_l"), 1),
			FDatasetBoneEntry(TEXT("thigh_r"), 1),
		});

	Sidecar.AddLayer(EDatasetCaptureLayer::RawInput);
	Sidecar.AddLayer(EDatasetCaptureLayer::CharacterState);

	// Walking forward, then a jump on frame 202, still held on 203.
	{
		FDatasetInputSample Sample;
		Sample.FrameNumber = 200;
		Sample.LeftStickY = 0.98f;
		Sidecar.AddSample(Sample);
	}
	{
		FDatasetInputSample Sample;
		Sample.FrameNumber = 201;
		Sample.LeftStickY = 1.0f;
		Sidecar.AddSample(Sample);
	}
	{
		FDatasetInputSample Sample;
		Sample.FrameNumber = 202;
		Sample.LeftStickY = 1.0f;
		Sample.SetHeld(EDatasetGamepadButton::FaceBottom, true);
		Sidecar.AddSample(Sample);
	}
	{
		FDatasetInputSample Sample;
		Sample.FrameNumber = 203;
		Sample.LeftStickY = 0.87f;
		Sample.SetHeld(EDatasetGamepadButton::FaceBottom, true);
		Sidecar.AddSample(Sample);
	}

	Sidecar.Labels.Caption = TEXT("a person walks up a small staircase and jumps the last step");
	Sidecar.Labels.Tags.Add(TEXT("locomotion"));
	Sidecar.Labels.Tags.Add(TEXT("stairs-up"));
	Sidecar.Labels.Tags.Add(TEXT("jump"));
	FDatasetTrimRange::FullRange(200, 203).ApplyToLabels(Sidecar.Labels);

	// The sample must be internally consistent, or it documents something untrue.
	TestEqual(TEXT("Sample has four frames"), Sidecar.GetSampleCount(), 4);
	TestEqual(TEXT("Duration matches the stream"), Sidecar.Identity.DurationFrames, Sidecar.GetSampleCount());
	TestTrue(
		TEXT("Trim covers the whole recorded range"),
		FDatasetTrimRange::FromLabels(Sidecar.Labels) == FDatasetTrimRange::FullRange(200, 203));

	const FString FilePath = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("KimodoDatasetCapture"), TEXT("SidecarSchemaSample.json"));

	FText FailureReason;
	TestTrue(TEXT("Sample written"), FDatasetSidecarSerializer::SaveToFile(Sidecar, FilePath, FailureReason));
	TestTrue(TEXT("Sample file exists"), IFileManager::Get().FileExists(*FilePath));

	FDatasetSidecar Reloaded;
	TestTrue(TEXT("Sample reads back"), FDatasetSidecarSerializer::LoadFromFile(FilePath, Reloaded, FailureReason));
	TestTrue(TEXT("Sample round-trips identically"), Reloaded == Sidecar);

	UE_LOG(LogKimodoDatasetCapture, Display, TEXT("Sidecar schema v%d sample written to '%s'."), KDatasetSidecarSchemaVersion, *FilePath);

	// Deliberately NOT deleted: the artefact is the point.
	return true;
}
