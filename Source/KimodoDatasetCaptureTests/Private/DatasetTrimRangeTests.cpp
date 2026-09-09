// Copyright Animotive. All Rights Reserved.

#include "DatasetSidecar.h"
#include "DatasetTrimRange.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetTrimRangeConstructionTest,
	"KimodoDatasetCapture.TrimRange.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetTrimRangeConstructionTest::RunTest(const FString& Parameters)
{
	// A take does not start at frame zero, so the full range is built from the recorded
	// first and last frames rather than from a count.
	const FDatasetTrimRange Full = FDatasetTrimRange::FullRange(100, 102);
	TestEqual(TEXT("Full range in-point"), Full.InFrame, 100);
	TestEqual(TEXT("Full range out-point"), Full.OutFrame, 102);
	TestTrue(TEXT("Full range is valid"), Full.IsValid());

	// Inclusive on both ends: 100..102 is three frames, not two.
	TestEqual(TEXT("Full range covers three frames"), Full.NumFrames(), 3);

	// The duration form must agree with the explicit form, or the two ways of describing the
	// same clip would disagree by one frame.
	TestTrue(TEXT("FromDuration agrees with FullRange"), FDatasetTrimRange::FromDuration(100, 3) == Full);

	// A single-frame clip is legitimate and covers exactly one frame.
	const FDatasetTrimRange Single = FDatasetTrimRange::FullRange(50, 50);
	TestTrue(TEXT("Single-frame range is valid"), Single.IsValid());
	TestEqual(TEXT("Single-frame range covers one frame"), Single.NumFrames(), 1);
	TestTrue(TEXT("FromDuration of one agrees"), FDatasetTrimRange::FromDuration(50, 1) == Single);

	// A capture that recorded nothing must not present itself as having recorded a frame.
	const FDatasetTrimRange Empty = FDatasetTrimRange::FromDuration(100, 0);
	TestFalse(TEXT("Zero duration is invalid"), Empty.IsValid());
	TestEqual(TEXT("Zero duration covers no frames"), Empty.NumFrames(), 0);

	// A negative duration is a caller bug; it must not wrap into a huge valid range.
	const FDatasetTrimRange Negative = FDatasetTrimRange::FromDuration(100, -5);
	TestFalse(TEXT("Negative duration is invalid"), Negative.IsValid());
	TestEqual(TEXT("Negative duration covers no frames"), Negative.NumFrames(), 0);

	// An inverted range is invalid rather than silently reordered: an out-point before the
	// in-point means the caller's frames are wrong, and reordering would hide that.
	const FDatasetTrimRange Inverted(10, 5);
	TestFalse(TEXT("Inverted range is invalid"), Inverted.IsValid());
	TestEqual(TEXT("Inverted range covers no frames"), Inverted.NumFrames(), 0);

	// Negative frame numbers are legal: MovieScene frame numbers can be negative.
	const FDatasetTrimRange AcrossZero = FDatasetTrimRange::FullRange(-2, 2);
	TestTrue(TEXT("Range across zero is valid"), AcrossZero.IsValid());
	TestEqual(TEXT("Range across zero covers five frames"), AcrossZero.NumFrames(), 5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetTrimRangeContainsTest,
	"KimodoDatasetCapture.TrimRange.ContainsFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetTrimRangeContainsTest::RunTest(const FString& Parameters)
{
	const FDatasetTrimRange Range = FDatasetTrimRange::FullRange(100, 102);

	// Both boundary frames are inside. These two assertions are the whole reason this test
	// exists: an exclusive end would silently drop the last frame of every exported clip.
	TestTrue(TEXT("In-point is contained"), Range.ContainsFrame(100));
	TestTrue(TEXT("Out-point is contained"), Range.ContainsFrame(102));
	TestTrue(TEXT("Interior frame is contained"), Range.ContainsFrame(101));

	TestFalse(TEXT("Frame before the in-point is excluded"), Range.ContainsFrame(99));
	TestFalse(TEXT("Frame after the out-point is excluded"), Range.ContainsFrame(103));

	// A single-frame range contains exactly that frame.
	const FDatasetTrimRange Single = FDatasetTrimRange::FullRange(50, 50);
	TestTrue(TEXT("Single-frame range contains its frame"), Single.ContainsFrame(50));
	TestFalse(TEXT("Single-frame range excludes the next frame"), Single.ContainsFrame(51));

	// An invalid range contains nothing at all, including the frames it names.
	const FDatasetTrimRange Invalid(10, 5);
	TestFalse(TEXT("Invalid range contains neither end"), Invalid.ContainsFrame(10));
	TestFalse(TEXT("Invalid range contains nothing between"), Invalid.ContainsFrame(7));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetTrimRangeClampTest,
	"KimodoDatasetCapture.TrimRange.Clamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetTrimRangeClampTest::RunTest(const FString& Parameters)
{
	const FDatasetTrimRange Recorded = FDatasetTrimRange::FullRange(100, 200);

	// A range already inside the bounds is untouched.
	const FDatasetTrimRange Inside(120, 180);
	TestTrue(TEXT("Interior range is unchanged by clamping"), Inside.ClampedTo(Recorded) == Inside);

	// A range landing exactly on both bounds is untouched: clamping must not shrink a
	// legitimate full-range trim by a frame at either end.
	TestTrue(TEXT("Range on the bounds is unchanged"), Recorded.ClampedTo(Recorded) == Recorded);

	// Overhang at each end is pulled back to the bound.
	TestTrue(
		TEXT("Overhang before the start clamps to the start"),
		FDatasetTrimRange(50, 150).ClampedTo(Recorded) == FDatasetTrimRange(100, 150));
	TestTrue(
		TEXT("Overhang past the end clamps to the end"),
		FDatasetTrimRange(150, 500).ClampedTo(Recorded) == FDatasetTrimRange(150, 200));
	TestTrue(
		TEXT("Overhang at both ends clamps to the whole range"),
		FDatasetTrimRange(0, 1000).ClampedTo(Recorded) == Recorded);

	// A range sharing no frame with the bounds yields an invalid range rather than a
	// nearest-fit guess. The caller asked for frames that were never recorded.
	TestFalse(TEXT("Range entirely before the bounds clamps to invalid"), FDatasetTrimRange(0, 50).ClampedTo(Recorded).IsValid());
	TestFalse(TEXT("Range entirely after the bounds clamps to invalid"), FDatasetTrimRange(300, 400).ClampedTo(Recorded).IsValid());

	// Clamping an already invalid range keeps it invalid; it must not become valid.
	TestFalse(TEXT("Invalid range stays invalid after clamping"), FDatasetTrimRange(180, 120).ClampedTo(Recorded).IsValid());

	// A one-frame overlap at each boundary survives, since those frames were recorded.
	TestTrue(
		TEXT("Single-frame overlap at the start survives"),
		FDatasetTrimRange(50, 100).ClampedTo(Recorded) == FDatasetTrimRange(100, 100));
	TestTrue(
		TEXT("Single-frame overlap at the end survives"),
		FDatasetTrimRange(200, 400).ClampedTo(Recorded) == FDatasetTrimRange(200, 200));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetTrimRangeLabelsTest,
	"KimodoDatasetCapture.TrimRange.Labels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetTrimRangeLabelsTest::RunTest(const FString& Parameters)
{
	FDatasetSidecarLabels Labels;
	Labels.Caption = TEXT("a person walks up a large staircase");
	Labels.TrimInFrame = 110;
	Labels.TrimOutFrame = 190;

	const FDatasetTrimRange FromLabels = FDatasetTrimRange::FromLabels(Labels);
	TestEqual(TEXT("In-point read from labels"), FromLabels.InFrame, 110);
	TestEqual(TEXT("Out-point read from labels"), FromLabels.OutFrame, 190);

	// Writing back must change only the trim fields: a trim edit that quietly cleared an
	// operator's caption would lose the part of the clip that matters most.
	const FDatasetTrimRange Tightened(120, 180);
	Tightened.ApplyToLabels(Labels);
	TestEqual(TEXT("In-point written to labels"), Labels.TrimInFrame, 120);
	TestEqual(TEXT("Out-point written to labels"), Labels.TrimOutFrame, 180);
	TestEqual(
		TEXT("Caption untouched by a trim edit"),
		Labels.Caption,
		FString(TEXT("a person walks up a large staircase")));
	TestTrue(TEXT("Validity untouched by a trim edit"), Labels.bIsValid);

	// Round-trip through the label block preserves the range exactly.
	TestTrue(TEXT("Range round-trips through labels"), FDatasetTrimRange::FromLabels(Labels) == Tightened);

	return true;
}
