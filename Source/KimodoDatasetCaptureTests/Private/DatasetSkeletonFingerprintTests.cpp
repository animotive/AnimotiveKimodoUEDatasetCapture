// Copyright Animotive. All Rights Reserved.

#include "DatasetSkeletonFingerprint.h"
#include "Misc/AutomationTest.h"
#include "ReferenceSkeleton.h"

namespace
{
	/** A small humanoid-ish chain, enough to exercise ordering and parenting. */
	TArray<FDatasetBoneEntry> MakeReferenceBones()
	{
		return TArray<FDatasetBoneEntry>
		{
			FDatasetBoneEntry(TEXT("root"), INDEX_NONE),
			FDatasetBoneEntry(TEXT("pelvis"), 0),
			FDatasetBoneEntry(TEXT("spine_01"), 1),
			FDatasetBoneEntry(TEXT("thigh_l"), 1),
			FDatasetBoneEntry(TEXT("thigh_r"), 1),
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSkeletonFingerprintStabilityTest,
	"KimodoDatasetCapture.SkeletonFingerprint.Stability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSkeletonFingerprintStabilityTest::RunTest(const FString& Parameters)
{
	const TArray<FDatasetBoneEntry> Bones = MakeReferenceBones();

	const FString First = FDatasetSkeletonFingerprint::Compute(Bones);
	const FString Second = FDatasetSkeletonFingerprint::Compute(Bones);

	TestFalse(TEXT("Fingerprint is not empty"), First.IsEmpty());
	TestEqual(TEXT("Fingerprint is 16 hex characters"), First.Len(), 16);
	TestEqual(TEXT("Same input yields the same fingerprint"), Second, First);

	// A separately built but identical bone list must match, or the fingerprint is keyed on
	// memory rather than content and is useless across sessions.
	const TArray<FDatasetBoneEntry> RebuiltBones = MakeReferenceBones();
	TestEqual(TEXT("Rebuilt identical list matches"), FDatasetSkeletonFingerprint::Compute(RebuiltBones), First);

	// Lowercase hex only, so string comparison downstream needs no normalisation.
	TestEqual(TEXT("Fingerprint is lowercase"), First, First.ToLower());
	for (const TCHAR Character : First)
	{
		const bool bIsHexDigit = (Character >= TEXT('0') && Character <= TEXT('9'))
			|| (Character >= TEXT('a') && Character <= TEXT('f'));
		if (!bIsHexDigit)
		{
			AddError(FString::Printf(TEXT("Fingerprint contains a non-hex character: '%c'"), Character));
			break;
		}
	}

	// An empty skeleton still fingerprints, deterministically. It must not collide with a
	// real skeleton, and must not be an empty string that reads as "not computed".
	const TArray<FDatasetBoneEntry> NoBones;
	const FString EmptyFingerprint = FDatasetSkeletonFingerprint::Compute(NoBones);
	TestEqual(TEXT("Empty skeleton fingerprint is 16 characters"), EmptyFingerprint.Len(), 16);
	TestEqual(TEXT("Empty skeleton fingerprint is stable"), FDatasetSkeletonFingerprint::Compute(NoBones), EmptyFingerprint);
	TestNotEqual(TEXT("Empty skeleton differs from a real one"), EmptyFingerprint, First);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSkeletonFingerprintSensitivityTest,
	"KimodoDatasetCapture.SkeletonFingerprint.Sensitivity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSkeletonFingerprintSensitivityTest::RunTest(const FString& Parameters)
{
	const FString Baseline = FDatasetSkeletonFingerprint::Compute(MakeReferenceBones());

	// A renamed bone is a different rig.
	{
		TArray<FDatasetBoneEntry> Bones = MakeReferenceBones();
		Bones[2].Name = TEXT("spine_001");
		TestNotEqual(TEXT("Renamed bone changes the fingerprint"), FDatasetSkeletonFingerprint::Compute(Bones), Baseline);
	}

	// Re-parenting changes how a pose is interpreted, so it must change the fingerprint.
	{
		TArray<FDatasetBoneEntry> Bones = MakeReferenceBones();
		Bones[3].ParentIndex = 2;
		TestNotEqual(TEXT("Re-parented bone changes the fingerprint"), FDatasetSkeletonFingerprint::Compute(Bones), Baseline);
	}

	// Same bones, different order: bone order determines how a pose array is read, so this
	// is a different rig even though the name set is identical.
	{
		TArray<FDatasetBoneEntry> Bones = MakeReferenceBones();
		Swap(Bones[3], Bones[4]);
		TestNotEqual(TEXT("Reordered bones change the fingerprint"), FDatasetSkeletonFingerprint::Compute(Bones), Baseline);
	}

	// An added bone is a different rig.
	{
		TArray<FDatasetBoneEntry> Bones = MakeReferenceBones();
		Bones.Add(FDatasetBoneEntry(TEXT("spine_02"), 2));
		TestNotEqual(TEXT("Added bone changes the fingerprint"), FDatasetSkeletonFingerprint::Compute(Bones), Baseline);
	}

	// A removed bone is a different rig.
	{
		TArray<FDatasetBoneEntry> Bones = MakeReferenceBones();
		Bones.RemoveAt(Bones.Num() - 1);
		TestNotEqual(TEXT("Removed bone changes the fingerprint"), FDatasetSkeletonFingerprint::Compute(Bones), Baseline);
	}

	// Case, however, is NOT a difference: FName comparison is case-insensitive, so two
	// skeletons differing only in bone name case are the same skeleton to the engine, and
	// must be the same skeleton to the dataset.
	{
		TArray<FDatasetBoneEntry> Bones = MakeReferenceBones();
		Bones[1].Name = TEXT("Pelvis");
		Bones[3].Name = TEXT("Thigh_L");
		TestEqual(TEXT("Bone name case does not change the fingerprint"), FDatasetSkeletonFingerprint::Compute(Bones), Baseline);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSkeletonFingerprintReferenceSkeletonTest,
	"KimodoDatasetCapture.SkeletonFingerprint.ReferenceSkeleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSkeletonFingerprintReferenceSkeletonTest::RunTest(const FString& Parameters)
{
	// Built in memory rather than loaded from an asset, so this runs headless with no content.
	FReferenceSkeleton ReferenceSkeleton;
	{
		FReferenceSkeletonModifier Modifier(ReferenceSkeleton, nullptr);
		for (const FDatasetBoneEntry& Bone : MakeReferenceBones())
		{
			Modifier.Add(FMeshBoneInfo(Bone.Name, Bone.Name.ToString(), Bone.ParentIndex), FTransform::Identity);
		}
	}

	TestEqual(TEXT("Reference skeleton has five bones"), ReferenceSkeleton.GetNum(), 5);

	const TArray<FDatasetBoneEntry> Extracted = FDatasetSkeletonFingerprint::ExtractBones(ReferenceSkeleton);
	TestEqual(TEXT("Extracted every bone"), Extracted.Num(), 5);

	if (Extracted.Num() == 5)
	{
		TestEqual(TEXT("First bone name extracted"), Extracted[0].Name, FName(TEXT("root")));
		TestEqual(TEXT("Root has no parent"), Extracted[0].ParentIndex, static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("Child parent index extracted"), Extracted[2].ParentIndex, 1);
	}

	// The engine overload must agree with the data overload, or a sidecar written from a real
	// skeleton would not match one written from extracted data.
	TestEqual(
		TEXT("Reference skeleton fingerprint matches the data fingerprint"),
		FDatasetSkeletonFingerprint::Compute(ReferenceSkeleton),
		FDatasetSkeletonFingerprint::Compute(MakeReferenceBones()));

	return true;
}
