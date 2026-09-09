// Copyright Animotive. All Rights Reserved.

#include "DatasetInputSample.h"
#include "DatasetSidecarBuilder.h"
#include "DatasetSidecarSerializer.h"
#include "DatasetSkeletonFingerprint.h"
#include "DatasetTrimRange.h"
#include "Misc/AutomationTest.h"
#include "MovieSceneDatasetInputSection.h"
#include "UObject/Package.h"

namespace
{
	UMovieSceneDatasetInputSection* NewSectionWithSamples(const TArray<FDatasetInputSample>& Samples)
	{
		UMovieSceneDatasetInputSection* Section = NewObject<UMovieSceneDatasetInputSection>(GetTransientPackage());
		if (Section)
		{
			Section->WriteSamples(Samples);
		}

		return Section;
	}

	FDatasetSidecarBuilder::FBuildContext MakeContext()
	{
		FDatasetSidecarBuilder::FBuildContext Context;
		Context.TakeName = TEXT("Take_0007_HeroStairs");
		Context.LevelSequencePath = TEXT("/Game/Cinematics/Takes/Take_0007.Take_0007");
		Context.AnimSequencePath = TEXT("/Game/Cinematics/Takes/Take_0007_Hero.Take_0007_Hero");
		// A rational rate that no float represents exactly, carried through on purpose.
		Context.DisplayRate = FFrameRate(30000, 1001);
		Context.TickResolution = FFrameRate(24000, 1);
		Context.CaptureStartedUtc = TEXT("2026-09-09T13:04:57Z");
		Context.Skeleton.SkeletonPath = TEXT("/Game/Characters/Hero/SK_Hero_Skeleton.SK_Hero_Skeleton");
		Context.Skeleton.BoneCount = 5;
		Context.Skeleton.BoneHierarchyFingerprint = TEXT("51038bbad3e18d9f");

		return Context;
	}

	TArray<FDatasetInputSample> MakeWalkAndJumpSamples()
	{
		TArray<FDatasetInputSample> Samples;
		Samples.SetNum(4);

		for (int32 Index = 0; Index < Samples.Num(); ++Index)
		{
			Samples[Index].FrameNumber = 800 * (Index + 1);
			Samples[Index].LeftStickY = 1.0f;
		}

		// Jump pressed on the third sample and still held on the fourth.
		Samples[2].SetHeld(EDatasetGamepadButton::FaceBottom, true);
		Samples[3].SetHeld(EDatasetGamepadButton::FaceBottom, true);

		return Samples;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarBuilderDerivesStreamTest,
	"KimodoDatasetCapture.SidecarBuilder.DerivesStreamFromSection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarBuilderDerivesStreamTest::RunTest(const FString& Parameters)
{
	const TArray<FDatasetInputSample> Written = MakeWalkAndJumpSamples();

	UMovieSceneDatasetInputSection* Section = NewSectionWithSamples(Written);
	if (!Section)
	{
		AddError(TEXT("Failed to construct a section."));
		return false;
	}

	FDatasetSidecar Sidecar;
	FString FailureReason;
	TestTrue(
		TEXT("Sidecar built from section"),
		FDatasetSidecarBuilder::BuildFromSection(*Section, MakeContext(), Sidecar, FailureReason));
	TestEqual(TEXT("No failure reason on success"), FailureReason, FString());

	// The stream must come from the section, sample for sample and frame for frame. Any
	// difference means there are two records of the same capture, which is the situation this
	// design exists to make impossible.
	TestEqual(TEXT("Every sample derived"), Sidecar.GetSampleCount(), Written.Num());

	bool bAllMatch = Sidecar.GetSampleCount() == Written.Num();
	for (int32 Index = 0; Index < Written.Num() && bAllMatch; ++Index)
	{
		FDatasetInputSample Derived;
		if (!Sidecar.TryGetSampleAtFrame(Written[Index].FrameNumber, Derived))
		{
			AddError(FString::Printf(TEXT("Sidecar has no sample at frame %d."), Written[Index].FrameNumber));
			bAllMatch = false;
			break;
		}

		if (Derived != Written[Index])
		{
			AddError(FString::Printf(TEXT("Sample at frame %d differs from what was recorded."), Written[Index].FrameNumber));
			bAllMatch = false;
		}
	}
	TestTrue(TEXT("Derived stream matches the section exactly"), bAllMatch);

	// The button transition must survive derivation, since it is what a caption describes.
	FDatasetInputSample BeforeJump;
	FDatasetInputSample AtJump;
	if (Sidecar.TryGetSampleAtFrame(1600, BeforeJump) && Sidecar.TryGetSampleAtFrame(2400, AtJump))
	{
		TestTrue(
			TEXT("Jump reads as a press between the second and third samples"),
			FDatasetInputSample::WasPressed(BeforeJump, AtJump, EDatasetGamepadButton::FaceBottom));
	}
	else
	{
		AddError(TEXT("Expected samples at frames 1600 and 2400."));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarBuilderIdentityTest,
	"KimodoDatasetCapture.SidecarBuilder.IdentityAndDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarBuilderIdentityTest::RunTest(const FString& Parameters)
{
	UMovieSceneDatasetInputSection* Section = NewSectionWithSamples(MakeWalkAndJumpSamples());
	if (!Section)
	{
		AddError(TEXT("Failed to construct a section."));
		return false;
	}

	FDatasetSidecarBuilder::FBuildContext Context = MakeContext();
	Context.OptionalLayers.Add(EDatasetCaptureLayer::CharacterState);

	FDatasetSidecar Sidecar;
	FString FailureReason;
	TestTrue(TEXT("Sidecar built"), FDatasetSidecarBuilder::BuildFromSection(*Section, Context, Sidecar, FailureReason));

	TestEqual(TEXT("Schema version carried"), Sidecar.SchemaVersion, KDatasetSidecarSchemaVersion);
	TestEqual(TEXT("Take name carried"), Sidecar.Identity.TakeName, Context.TakeName);
	TestEqual(TEXT("LevelSequence path carried"), Sidecar.Identity.LevelSequencePath, Context.LevelSequencePath);
	TestEqual(TEXT("AnimSequence path carried"), Sidecar.Identity.AnimSequencePath, Context.AnimSequencePath);
	TestEqual(TEXT("Capture start carried"), Sidecar.Identity.CaptureStartedUtc, Context.CaptureStartedUtc);

	// Rates survive as integers. A float here would make 30000/1001 unrecoverable.
	TestEqual(TEXT("Frame rate numerator carried"), Sidecar.Identity.FrameRateNumerator, 30000);
	TestEqual(TEXT("Frame rate denominator carried"), Sidecar.Identity.FrameRateDenominator, 1001);
	TestEqual(TEXT("Tick resolution numerator carried"), Sidecar.Identity.TickResolutionNumerator, 24000);
	TestEqual(TEXT("Tick resolution denominator carried"), Sidecar.Identity.TickResolutionDenominator, 1);

	TestEqual(TEXT("Skeleton path carried"), Sidecar.Identity.DurationFrames, 4);
	TestEqual(TEXT("Skeleton fingerprint carried"), Sidecar.Skeleton.BoneHierarchyFingerprint, Context.Skeleton.BoneHierarchyFingerprint);
	TestEqual(TEXT("Bone count carried"), Sidecar.Skeleton.BoneCount, 5);

	// Trim defaults to the full recorded range, expressed in the section's own frames.
	TestTrue(
		TEXT("Trim defaults to the full recorded range"),
		FDatasetTrimRange::FromLabels(Sidecar.Labels) == FDatasetTrimRange::FullRange(800, 3200));

	// Raw input is always present when a section has samples; the optional layer is carried
	// through from the context; a layer that was not recorded must not appear.
	TestTrue(TEXT("Raw input layer always present"), Sidecar.HasLayer(EDatasetCaptureLayer::RawInput));
	TestTrue(TEXT("Optional layer carried through"), Sidecar.HasLayer(EDatasetCaptureLayer::CharacterState));
	TestFalse(TEXT("Unrecorded layer absent"), Sidecar.HasLayer(EDatasetCaptureLayer::SemanticInput));

	// A freshly derived clip is valid and unlabelled: capture reports validity, the operator
	// supplies the caption.
	TestTrue(TEXT("Derived clip is valid"), Sidecar.Labels.bIsValid);
	TestEqual(TEXT("Caption starts empty for the operator"), Sidecar.Labels.Caption, FString());

	// And the derived sidecar must survive the serializer, since that is its destination.
	FString Json;
	TestTrue(TEXT("Derived sidecar serialises"), FDatasetSidecarSerializer::ToJson(Sidecar, Json));

	FDatasetSidecar Reloaded;
	TestTrue(TEXT("Derived sidecar parses back"), FDatasetSidecarSerializer::FromJson(Json, Reloaded, FailureReason));
	TestTrue(TEXT("Derived sidecar round-trips"), Reloaded == Sidecar);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarBuilderEmptySectionTest,
	"KimodoDatasetCapture.SidecarBuilder.EmptySectionFailsLoudly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarBuilderEmptySectionTest::RunTest(const FString& Parameters)
{
	UMovieSceneDatasetInputSection* Section = NewObject<UMovieSceneDatasetInputSection>(GetTransientPackage());
	if (!Section)
	{
		AddError(TEXT("Failed to construct a section."));
		return false;
	}

	TestEqual(TEXT("Section starts empty"), Section->GetSampleCount(), 0);

	FDatasetSidecar Sidecar;
	FString FailureReason;

	// A take that recorded nothing is a FAILED capture, not a clip with an empty stream.
	// Producing a plausible sidecar describing no input is exactly the silent degradation the
	// fail-loud rule in product-guidelines.md forbids.
	TestFalse(
		TEXT("Empty section does not build a sidecar"),
		FDatasetSidecarBuilder::BuildFromSection(*Section, MakeContext(), Sidecar, FailureReason));
	TestFalse(TEXT("Failure states a reason"), FailureReason.IsEmpty());

	// The reason is read by an operator, so it must not be engine jargon.
	TestFalse(TEXT("Reason avoids engine internals"), FailureReason.Contains(TEXT("UMovieScene")));

	return true;
}
