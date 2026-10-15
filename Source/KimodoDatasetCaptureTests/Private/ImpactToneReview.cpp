// Copyright Animotive. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "ImpactBatchDefinition.h"
#include "ImpactBatchRunner.h"
#include "ImpactClipBaker.h"
#include "ImpactClipSimulator.h"
#include "ImpactPoseBuffer.h"
#include "ImpactSimWorld.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

/**
 * Ragdoll tone review (user, 2026-10-15: "the ragdolls might be too loose, they read as ragdoll
 * instead of realistic"). Re-simulates chosen clips of the 10k grid at the batch's own released
 * tone and at two firmer ones, and bakes each for side-by-side viewing. Not part of the suite:
 * it lives outside the KimodoDatasetCapture test root and reads its clips from
 * Saved/KimodoImpactReviewTone/Clips.txt, one "<clip index> <name>" per line.
 */
namespace ImpactToneReview
{
	const TCHAR* Folder = TEXT("KimodoImpactReviewTone");
	const TCHAR* Batch = TEXT("/Game/KimodoImpactSample/IB_KimodoGrid10k.IB_KimodoGrid10k");

	struct FToneVariant
	{
		const TCHAR* Suffix;
		/** Released tone at release; negative keeps the clip's own (0.10-0.18). */
		float ToneStart;
		/** Tone faded to, as a share of ToneStart. */
		float EndRatio;
	};
	const FToneVariant Variants[] = {
		{ TEXT("ToneNow"), -1.0f, 0.0f },
		{ TEXT("Tone025"), 0.25f, 0.5f },
		{ TEXT("Tone035"), 0.35f, 0.5f },
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactToneReview,
	"KimodoReview.RagdollTone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactToneReview::RunTest(const FString& Parameters)
{
	using namespace ImpactToneReview;
	TArray<FString> Lines;
	const FString ClipsFile = FPaths::ProjectSavedDir() / Folder / TEXT("Clips.txt");
	if (!TestTrue(*FString::Printf(TEXT("Clip list %s read"), *ClipsFile), FFileHelper::LoadFileToStringArray(Lines, *ClipsFile)))
	{
		return true;
	}
	UImpactBatchDefinition* Definition = LoadObject<UImpactBatchDefinition>(nullptr, Batch);
	FImpactBatchAssets Assets;
	FText Reason;
	if (!TestNotNull(TEXT("Batch loaded"), Definition) || !TestTrue(*FString::Printf(TEXT("Batch assets: %s"), *Reason.ToString()),
		FImpactBatchAssets::Load(*Definition, Assets, Reason)))
	{
		return true;
	}

	TArray<FString> Summary;
	for (const FString& Line : Lines)
	{
		FString IndexText, Name;
		if (!Line.TrimStartAndEnd().Split(TEXT(" "), &IndexText, &Name))
		{
			continue;
		}
		const int32 Index = FCString::Atoi(*IndexText);
		for (const FToneVariant& Variant : Variants)
		{
			FImpactClipSetup Setup;
			FString ClipId;
			if (!TestTrue(*FString::Printf(TEXT("Clip %d set up: %s"), Index, *Reason.ToString()), FImpactClipFactory::Make(*Definition, Assets, Index, Setup, ClipId, Reason)))
			{
				break;
			}
			if (Variant.ToneStart >= 0.0f)
			{
				Setup.Strength.ReleasedToneStart = Variant.ToneStart;
				Setup.Strength.ReleasedToneEnd = Variant.ToneStart * Variant.EndRatio;
			}
			FImpactSimWorld World;
			FImpactClipRecording Recording;
			if (!TestTrue(*FString::Printf(TEXT("Clip %d simulates: %s"), Index, *Reason.ToString()), FImpactClipSimulator::Run(World, Setup, Recording, Reason)))
			{
				continue;
			}
			FImpactPoseBuffer Poses;
			FImpactClipPoser::Build(Setup.Mesh->GetRefSkeleton(), Recording, Setup.MeshForward, Poses);

			const FString AssetName = FString::Printf(TEXT("%s_%s"), *Name, Variant.Suffix);
			const FString PackageName = FString::Printf(TEXT("/Game/%s/%s"), Folder, *AssetName);
			if (FPackageName::DoesPackageExist(PackageName))
			{
				AddError(FString::Printf(TEXT("%s already exists in Content; delete it to re-bake."), *PackageName));
				continue;
			}
			UPackage* Package = CreatePackage(*PackageName);
			UAnimSequence* Sequence = FImpactClipBaker::Bake(Package, *AssetName, Setup.Mesh->GetSkeleton(), Setup.Mesh->GetRefSkeleton(), Poses, Setup.FrameRate, Reason);
			const FString File = FPaths::ProjectSavedDir() / Folder / (AssetName + FPackageName::GetAssetPackageExtension());
			TestTrue(*FString::Printf(TEXT("%s baked: %s"), *AssetName, *Reason.ToString()), Sequence && FImpactClipBaker::Save(Sequence, File, Reason));
			Package->MarkAsGarbage();

			const FString Invalid = Recording.End == EImpactClipEnd::Invalidated ? TEXT(", INVALID: ") + Recording.InvalidReason.ToString() : FString();
			Summary.Add(FString::Printf(TEXT("%s: tone %.3f -> %.3f, outcome %s, %d frames%s"), *AssetName, Setup.Strength.ReleasedToneStart,
				Setup.Strength.ReleasedToneEnd, *UEnum::GetValueAsString(Recording.Outcome.Outcome), Recording.BodyPoses.Num(),
				*Invalid));
		}
	}
	FFileHelper::SaveStringToFile(FString::Join(Summary, TEXT("\n")), *(FPaths::ProjectSavedDir() / Folder / TEXT("Summary.txt")));
	return true;
}
