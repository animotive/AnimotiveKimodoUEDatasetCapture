// Copyright Animotive. All Rights Reserved.

#include "DatasetSidecarSerializer.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
	/** A fully populated sidecar, so a round-trip exercises every block. */
	FDatasetSidecar MakePopulatedSidecar()
	{
		FDatasetSidecar Sidecar;

		Sidecar.Identity.TakeName = TEXT("Take_0042_Subscene");
		Sidecar.Identity.LevelSequencePath = TEXT("/Game/Cinematics/Takes/Take_0042.Take_0042");
		Sidecar.Identity.AnimSequencePath = TEXT("/Game/Cinematics/Takes/Take_0042_Hero.Take_0042_Hero");
		// Deliberately a rational rate that no float represents exactly.
		Sidecar.Identity.FrameRateNumerator = 30000;
		Sidecar.Identity.FrameRateDenominator = 1001;
		Sidecar.Identity.TickResolutionNumerator = 24000;
		Sidecar.Identity.TickResolutionDenominator = 1;
		Sidecar.Identity.CaptureStartedUtc = TEXT("2026-09-09T13:04:57Z");
		Sidecar.Identity.DurationFrames = 3;

		Sidecar.Skeleton.SkeletonPath = TEXT("/Game/Characters/Hero/SK_Hero_Skeleton.SK_Hero_Skeleton");
		Sidecar.Skeleton.BoneCount = 77;
		Sidecar.Skeleton.BoneHierarchyFingerprint = TEXT("9e107d9d372bb682");

		Sidecar.Labels.Caption = TEXT("a person walks up a small staircase and presses interact");
		Sidecar.Labels.Tags.Add(TEXT("locomotion"));
		Sidecar.Labels.Tags.Add(TEXT("stairs-up"));
		Sidecar.Labels.Tags.Add(TEXT("interact"));
		Sidecar.Labels.TrimInFrame = 100;
		Sidecar.Labels.TrimOutFrame = 102;

		Sidecar.AddLayer(EDatasetCaptureLayer::RawGamepadInput);
		Sidecar.AddLayer(EDatasetCaptureLayer::CharacterState);

		FDatasetInputSample First;
		First.FrameNumber = 100;
		First.LeftStickX = 0.5f;
		First.LeftStickY = -0.25f;
		First.SetHeld(EDatasetGamepadButton::FaceBottom, true);
		Sidecar.AddSample(First);

		FDatasetInputSample Second;
		Second.FrameNumber = 101;
		// An awkward value that is not exactly representable in a short decimal form, to
		// catch a writer that emits too few digits to survive the trip.
		Second.RightStickX = 0.123456789f;
		Second.RightTrigger = 1.0f;
		Second.SetHeld(EDatasetGamepadButton::FaceLeft, true);
		Second.SetHeld(EDatasetGamepadButton::ShoulderRight, true);
		Sidecar.AddSample(Second);

		FDatasetInputSample Third;
		Third.FrameNumber = 102;
		Third.LeftTrigger = 0.75f;
		Sidecar.AddSample(Third);

		return Sidecar;
	}

	FString MakeTempSidecarPath(const FString& Leaf)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("Tmp"), TEXT("KimodoDatasetCapture"), Leaf);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarRoundTripTest,
	"KimodoDatasetCapture.SidecarSerializer.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarRoundTripTest::RunTest(const FString& Parameters)
{
	const FDatasetSidecar Original = MakePopulatedSidecar();

	FString Json;
	TestTrue(TEXT("Serialised to JSON"), FDatasetSidecarSerializer::ToJson(Original, Json));
	TestFalse(TEXT("JSON is not empty"), Json.IsEmpty());

	FDatasetSidecar Restored;
	FText FailureReason;
	TestTrue(TEXT("Parsed back from JSON"), FDatasetSidecarSerializer::FromJson(Json, Restored, FailureReason));
	TestTrue(TEXT("No failure reason on success"), FailureReason.IsEmpty());

	// Whole-object equality. Block-by-block assertions follow only to make a failure
	// readable, since a single inequality says nothing about which field was lost.
	TestTrue(TEXT("Round-trip preserves the whole sidecar"), Original == Restored);
	TestTrue(TEXT("Round-trip preserves identity"), Original.Identity == Restored.Identity);
	TestTrue(TEXT("Round-trip preserves input stream"), Original.InputStream == Restored.InputStream);
	TestTrue(TEXT("Round-trip preserves skeleton"), Original.Skeleton == Restored.Skeleton);
	TestTrue(TEXT("Round-trip preserves labels"), Original.Labels == Restored.Labels);
	TestEqual(TEXT("Round-trip preserves layer count"), Restored.Layers.Num(), Original.Layers.Num());

	// The rational frame rate must survive as integers, not as a rounded float.
	TestEqual(TEXT("Frame rate numerator preserved"), Restored.Identity.FrameRateNumerator, 30000);
	TestEqual(TEXT("Frame rate denominator preserved"), Restored.Identity.FrameRateDenominator, 1001);

	// Frame keying must survive: the stream still answers by frame number, not index.
	FDatasetInputSample Found;
	TestTrue(TEXT("Frame 101 still found after round-trip"), Restored.TryGetSampleAtFrame(101, Found));
	TestTrue(TEXT("Buttons preserved"), Found.IsHeld(EDatasetGamepadButton::FaceLeft));
	TestTrue(TEXT("Second button preserved"), Found.IsHeld(EDatasetGamepadButton::ShoulderRight));
	TestFalse(TEXT("Unheld button stays unheld"), Found.IsHeld(EDatasetGamepadButton::FaceBottom));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarRoundTripEdgeCasesTest,
	"KimodoDatasetCapture.SidecarSerializer.RoundTripEdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarRoundTripEdgeCasesTest::RunTest(const FString& Parameters)
{
	FString Json;
	FText FailureReason;

	// Empty tag list: tags are optional, and an empty array must not become a missing field
	// that reads back as something else.
	{
		FDatasetSidecar Original = MakePopulatedSidecar();
		Original.Labels.Tags.Empty();

		FDatasetSidecar Restored;
		TestTrue(TEXT("Empty tags serialised"), FDatasetSidecarSerializer::ToJson(Original, Json));
		TestTrue(TEXT("Empty tags parsed"), FDatasetSidecarSerializer::FromJson(Json, Restored, FailureReason));
		TestTrue(TEXT("Empty tags round-trip"), Original == Restored);
		TestEqual(TEXT("Tags still empty"), Restored.Labels.Tags.Num(), 0);
	}

	// An invalid clip must carry its reason across the trip, because that reason is what an
	// operator and the export filter both act on.
	{
		FDatasetSidecar Original = MakePopulatedSidecar();
		Original.Labels.bIsValid = false;
		Original.Labels.InvalidReason = TEXT("Player pawn was destroyed during the take.");

		FDatasetSidecar Restored;
		TestTrue(TEXT("Invalid clip serialised"), FDatasetSidecarSerializer::ToJson(Original, Json));
		TestTrue(TEXT("Invalid clip parsed"), FDatasetSidecarSerializer::FromJson(Json, Restored, FailureReason));
		TestTrue(TEXT("Invalid clip round-trips"), Original == Restored);
		TestFalse(TEXT("Validity preserved as false"), Restored.Labels.bIsValid);
		TestEqual(
			TEXT("Invalid reason preserved"),
			Restored.Labels.InvalidReason,
			FString(TEXT("Player pawn was destroyed during the take.")));
	}

	// Full-range trim defaults, including a trim in-point of zero, which is the value most
	// likely to be dropped by a writer that omits default-valued fields.
	{
		FDatasetSidecar Original = MakePopulatedSidecar();
		Original.Labels.TrimInFrame = 0;
		Original.Labels.TrimOutFrame = 0;

		FDatasetSidecar Restored;
		TestTrue(TEXT("Zero trim serialised"), FDatasetSidecarSerializer::ToJson(Original, Json));
		TestTrue(TEXT("Zero trim parsed"), FDatasetSidecarSerializer::FromJson(Json, Restored, FailureReason));
		TestEqual(TEXT("Trim in preserved as zero"), Restored.Labels.TrimInFrame, 0);
		TestEqual(TEXT("Trim out preserved as zero"), Restored.Labels.TrimOutFrame, 0);
		TestTrue(TEXT("Zero trim round-trips"), Original == Restored);
	}

	// A sidecar with no samples at all: a capture that failed before its first frame.
	{
		FDatasetSidecar Original;
		Original.Labels.bIsValid = false;
		Original.Labels.InvalidReason = TEXT("No player pawn found.");

		FDatasetSidecar Restored;
		TestTrue(TEXT("Empty stream serialised"), FDatasetSidecarSerializer::ToJson(Original, Json));
		TestTrue(TEXT("Empty stream parsed"), FDatasetSidecarSerializer::FromJson(Json, Restored, FailureReason));
		TestEqual(TEXT("Still no samples"), Restored.GetSampleCount(), 0);
		TestTrue(TEXT("Empty stream round-trips"), Original == Restored);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarSchemaVersionTest,
	"KimodoDatasetCapture.SidecarSerializer.SchemaVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarSchemaVersionTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Current version is supported"), FDatasetSidecarSerializer::IsSupportedSchemaVersion(KDatasetSidecarSchemaVersion));
	TestFalse(TEXT("A future version is not supported"), FDatasetSidecarSerializer::IsSupportedSchemaVersion(KDatasetSidecarSchemaVersion + 1));
	TestFalse(TEXT("Zero is not a version"), FDatasetSidecarSerializer::IsSupportedSchemaVersion(0));

	// Version readable from a well-formed file.
	{
		FString Json;
		TestTrue(TEXT("Serialised"), FDatasetSidecarSerializer::ToJson(MakePopulatedSidecar(), Json));

		int32 Version = -1;
		TestTrue(TEXT("Version read from full document"), FDatasetSidecarSerializer::TryReadSchemaVersion(Json, Version));
		TestEqual(TEXT("Version is the current one"), Version, KDatasetSidecarSchemaVersion);
	}

	// Version readable from a document this build cannot otherwise interpret. This is the
	// forward-compatibility contract: report "file is version 7, I speak 1", never fail mute.
	{
		const FString FutureJson = TEXT("{\"SchemaVersion\": 7, \"SomeFutureBlock\": {\"Whatever\": [1,2,3]}}");

		int32 Version = -1;
		TestTrue(TEXT("Version read from unknown-version document"), FDatasetSidecarSerializer::TryReadSchemaVersion(FutureJson, Version));
		TestEqual(TEXT("Reported version is 7"), Version, 7);

		FDatasetSidecar Restored;
		FText FailureReason;
		TestFalse(
			TEXT("Unsupported version is rejected, not read optimistically"),
			FDatasetSidecarSerializer::FromJson(FutureJson, Restored, FailureReason));
		TestFalse(TEXT("Rejection states a reason"), FailureReason.IsEmpty());
		TestTrue(TEXT("Reason names the offending version"), FailureReason.ToString().Contains(TEXT("7")));
	}

	// Malformed input fails with a reason rather than yielding a half-populated sidecar.
	{
		FDatasetSidecar Restored;
		FText FailureReason;
		TestFalse(
			TEXT("Malformed JSON rejected"),
			FDatasetSidecarSerializer::FromJson(TEXT("{not json at all"), Restored, FailureReason));
		TestFalse(TEXT("Malformed JSON states a reason"), FailureReason.IsEmpty());

		int32 Version = -1;
		TestFalse(TEXT("No version from malformed JSON"), FDatasetSidecarSerializer::TryReadSchemaVersion(TEXT("{not json at all"), Version));
	}

	// A document with no version field is not a sidecar.
	{
		FDatasetSidecar Restored;
		FText FailureReason;
		TestFalse(
			TEXT("Missing version rejected"),
			FDatasetSidecarSerializer::FromJson(TEXT("{\"Identity\": {\"TakeName\": \"x\"}}"), Restored, FailureReason));
		TestFalse(TEXT("Missing version states a reason"), FailureReason.IsEmpty());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarFileRoundTripTest,
	"KimodoDatasetCapture.SidecarSerializer.FileRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarFileRoundTripTest::RunTest(const FString& Parameters)
{
	const FDatasetSidecar Original = MakePopulatedSidecar();
	const FString FilePath = MakeTempSidecarPath(TEXT("FileRoundTrip.json"));

	// Start from a known-absent file so a stale one cannot make this test pass.
	IFileManager::Get().Delete(*FilePath, false, true, true);
	TestFalse(TEXT("Target file absent before save"), IFileManager::Get().FileExists(*FilePath));

	FText FailureReason;
	TestTrue(TEXT("Saved to file"), FDatasetSidecarSerializer::SaveToFile(Original, FilePath, FailureReason));
	TestTrue(TEXT("File exists after save"), IFileManager::Get().FileExists(*FilePath));

	FDatasetSidecar Loaded;
	TestTrue(TEXT("Loaded from file"), FDatasetSidecarSerializer::LoadFromFile(FilePath, Loaded, FailureReason));
	TestTrue(TEXT("File round-trip preserves the sidecar"), Original == Loaded);

	// A missing file must report a reason, not be indistinguishable from an empty sidecar.
	{
		FDatasetSidecar Missing;
		FText MissingReason;
		TestFalse(
			TEXT("Loading an absent file fails"),
			FDatasetSidecarSerializer::LoadFromFile(MakeTempSidecarPath(TEXT("DoesNotExist.json")), Missing, MissingReason));
		TestFalse(TEXT("Absent file states a reason"), MissingReason.IsEmpty());
	}

	IFileManager::Get().Delete(*FilePath, false, true, true);
	return true;
}
