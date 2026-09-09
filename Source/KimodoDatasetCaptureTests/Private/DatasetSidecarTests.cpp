// Copyright Animotive. All Rights Reserved.

#include "DatasetSidecar.h"
#include "Misc/AutomationTest.h"

namespace
{
	FDatasetInputSample MakeSampleAtFrame(int32 FrameNumber, float LeftStickX = 0.0f)
	{
		FDatasetInputSample Sample;
		Sample.FrameNumber = FrameNumber;
		Sample.LeftStickX = LeftStickX;
		return Sample;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarDefaultsTest,
	"KimodoDatasetCapture.Sidecar.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarDefaultsTest::RunTest(const FString& Parameters)
{
	const FDatasetSidecar Sidecar;

	// A sidecar that forgot its version is unusable to any reader, so the default must
	// already carry the current one rather than relying on the writer to set it.
	TestEqual(TEXT("Default carries the current schema version"), Sidecar.SchemaVersion, KDatasetSidecarSchemaVersion);
	TestEqual(TEXT("Current schema version is 1"), KDatasetSidecarSchemaVersion, 1);

	// All four blocks exist and start empty.
	TestEqual(TEXT("Identity take name empty"), Sidecar.Identity.TakeName, FString());
	TestEqual(TEXT("Identity duration zero"), Sidecar.Identity.DurationFrames, 0);
	TestEqual(TEXT("Input stream empty"), Sidecar.GetSampleCount(), 0);
	TestEqual(TEXT("Skeleton bone count zero"), Sidecar.Skeleton.BoneCount, 0);
	TestEqual(TEXT("Labels caption empty"), Sidecar.Labels.Caption, FString());
	TestEqual(TEXT("Labels tags empty"), Sidecar.Labels.Tags.Num(), 0);

	// A freshly constructed clip is valid until capture says otherwise: invalidity is a
	// reported outcome, not the default state.
	TestTrue(TEXT("Defaults to valid"), Sidecar.Labels.bIsValid);
	TestEqual(TEXT("No invalid reason by default"), Sidecar.Labels.InvalidReason, FString());

	TestFalse(TEXT("First frame unset when stream empty"), Sidecar.GetFirstFrame().IsSet());
	TestFalse(TEXT("Last frame unset when stream empty"), Sidecar.GetLastFrame().IsSet());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarFrameKeyedStreamTest,
	"KimodoDatasetCapture.Sidecar.FrameKeyedStream",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarFrameKeyedStreamTest::RunTest(const FString& Parameters)
{
	FDatasetSidecar Sidecar;

	// Frames need not start at zero: a take's first frame is wherever the sequence starts.
	TestTrue(TEXT("First sample added"), Sidecar.AddSample(MakeSampleAtFrame(100, 0.1f)));
	TestTrue(TEXT("Second sample added"), Sidecar.AddSample(MakeSampleAtFrame(101, 0.2f)));
	TestTrue(TEXT("Third sample added"), Sidecar.AddSample(MakeSampleAtFrame(102, 0.3f)));
	TestEqual(TEXT("Three samples stored"), Sidecar.GetSampleCount(), 3);

	FDatasetInputSample Found;
	TestTrue(TEXT("Middle frame found by frame number"), Sidecar.TryGetSampleAtFrame(101, Found));
	TestEqual(TEXT("Found sample carries that frame"), Found.FrameNumber, 101);
	TestEqual(TEXT("Found sample carries its own data"), Found.LeftStickX, 0.2f);

	TestTrue(TEXT("First frame found"), Sidecar.TryGetSampleAtFrame(100, Found));
	TestTrue(TEXT("Last frame found"), Sidecar.TryGetSampleAtFrame(102, Found));

	// Lookup is by frame number, not by array index. Asking for index-like values that are
	// not recorded frames must miss, or every downstream frame lookup is silently wrong.
	TestFalse(TEXT("Index zero is not a recorded frame"), Sidecar.TryGetSampleAtFrame(0, Found));
	TestFalse(TEXT("Frame before the range misses"), Sidecar.TryGetSampleAtFrame(99, Found));
	TestFalse(TEXT("Frame after the range misses"), Sidecar.TryGetSampleAtFrame(103, Found));

	TestTrue(TEXT("First frame is set"), Sidecar.GetFirstFrame().IsSet());
	// Get with a sentinel rather than GetValue: an unset optional must fail the assertion,
	// not check()-crash the test process and take the rest of the suite with it.
	TestEqual(TEXT("First frame is 100"), Sidecar.GetFirstFrame().Get(-1), 100);
	TestEqual(TEXT("Last frame is 102"), Sidecar.GetLastFrame().Get(-1), 102);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarRejectsOutOfOrderTest,
	"KimodoDatasetCapture.Sidecar.RejectsOutOfOrderSamples",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarRejectsOutOfOrderTest::RunTest(const FString& Parameters)
{
	FDatasetSidecar Sidecar;
	TestTrue(TEXT("Frame 10 accepted"), Sidecar.AddSample(MakeSampleAtFrame(10)));
	TestTrue(TEXT("Frame 11 accepted"), Sidecar.AddSample(MakeSampleAtFrame(11)));

	// Out-of-order and duplicate frames would misalign every lookup after them, so they are
	// rejected where they enter rather than diagnosed downstream.
	TestFalse(TEXT("Earlier frame rejected"), Sidecar.AddSample(MakeSampleAtFrame(10)));
	TestFalse(TEXT("Duplicate frame rejected"), Sidecar.AddSample(MakeSampleAtFrame(11)));
	TestEqual(TEXT("Rejected samples changed nothing"), Sidecar.GetSampleCount(), 2);

	// A gap is legitimate: a dropped frame is still ordered.
	TestTrue(TEXT("Frame after a gap accepted"), Sidecar.AddSample(MakeSampleAtFrame(20)));
	TestEqual(TEXT("Gap sample stored"), Sidecar.GetSampleCount(), 3);
	TestEqual(TEXT("Last frame is 20"), Sidecar.GetLastFrame().Get(-1), 20);

	FDatasetInputSample Found;
	TestFalse(TEXT("Frame inside the gap misses"), Sidecar.TryGetSampleAtFrame(15, Found));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarLayersTest,
	"KimodoDatasetCapture.Sidecar.Layers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarLayersTest::RunTest(const FString& Parameters)
{
	FDatasetSidecar Sidecar;

	TestFalse(TEXT("No layers by default"), Sidecar.HasLayer(EDatasetCaptureLayer::RawInput));

	Sidecar.AddLayer(EDatasetCaptureLayer::RawInput);
	TestTrue(TEXT("Raw input layer recorded"), Sidecar.HasLayer(EDatasetCaptureLayer::RawInput));
	TestFalse(TEXT("Optional layer still absent"), Sidecar.HasLayer(EDatasetCaptureLayer::SemanticInput));

	// A project without Enhanced Input yields raw input only, and that is a valid dataset.
	Sidecar.AddLayer(EDatasetCaptureLayer::RawInput);
	TestEqual(TEXT("Adding a layer twice is a no-op"), Sidecar.Layers.Num(), 1);

	Sidecar.AddLayer(EDatasetCaptureLayer::CharacterState);
	TestTrue(TEXT("Character state layer recorded"), Sidecar.HasLayer(EDatasetCaptureLayer::CharacterState));
	TestEqual(TEXT("Two distinct layers"), Sidecar.Layers.Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarEqualityTest,
	"KimodoDatasetCapture.Sidecar.Equality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarEqualityTest::RunTest(const FString& Parameters)
{
	FDatasetSidecar A;
	A.Identity.TakeName = TEXT("Take_001");
	A.Identity.FrameRateNumerator = 30000;
	A.Identity.FrameRateDenominator = 1001;
	A.Identity.DurationFrames = 600;
	A.Skeleton.SkeletonPath = TEXT("/Game/Chars/SK_Hero_Skeleton");
	A.Skeleton.BoneCount = 77;
	A.Skeleton.BoneHierarchyFingerprint = TEXT("deadbeefcafef00d");
	A.Labels.Caption = TEXT("a person walks up a small staircase");
	A.Labels.Tags.Add(TEXT("stairs-up"));
	A.Labels.TrimOutFrame = 599;
	// Populated through members rather than AddLayer/AddSample: this test is about
	// equality, and should not fail or pass because of the mutators' behaviour.
	A.Layers.Add(EDatasetCaptureLayer::RawInput);
	A.InputStream.Samples.Add(MakeSampleAtFrame(0, 0.5f));

	FDatasetSidecar B = A;
	TestTrue(TEXT("Copy equals original"), A == B);

	// Every block must participate in equality, or the round-trip test in the next task
	// could pass while a whole block was dropped in transit.
	B = A;
	B.SchemaVersion = 99;
	TestTrue(TEXT("Schema version participates"), A != B);

	B = A;
	B.Identity.FrameRateDenominator = 1;
	TestTrue(TEXT("Identity participates"), A != B);

	B = A;
	B.InputStream.Samples[0].LeftStickX = 0.6f;
	TestTrue(TEXT("Input stream participates"), A != B);

	B = A;
	B.Skeleton.BoneHierarchyFingerprint = TEXT("0000000000000000");
	TestTrue(TEXT("Skeleton participates"), A != B);

	B = A;
	B.Labels.Caption = TEXT("a person walks up a large staircase");
	TestTrue(TEXT("Labels caption participates"), A != B);

	B = A;
	B.Labels.Tags.Add(TEXT("jump"));
	TestTrue(TEXT("Labels tags participate"), A != B);

	B = A;
	B.Labels.bIsValid = false;
	TestTrue(TEXT("Validity participates"), A != B);

	B = A;
	B.Layers.Empty();
	TestTrue(TEXT("Layers participate"), A != B);

	return true;
}
