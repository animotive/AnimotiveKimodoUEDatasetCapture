// Copyright Animotive. All Rights Reserved.

#include "ImpactBatchDefinition.h"

#include "Animation/AnimSequence.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/PackageName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactBatchDefinition)

#define LOCTEXT_NAMESPACE "ImpactBatchDefinition"

bool UImpactBatchDefinition::ValidateSettings(FText& OutReason) const
{
	auto Fail = [&OutReason](const FText& Reason)
	{
		OutReason = Reason;
		return false;
	};

	if (RequiredSkeleton.IsNull())
	{
		return Fail(LOCTEXT("NoSkeleton", "Set the required skeleton: clips bake onto it, so every source animation must share it."));
	}
	if (AnimationFolders.IsEmpty() && ExplicitAnimations.IsEmpty())
	{
		return Fail(LOCTEXT("NoSources", "Add an animation folder or an explicit animation to draw source motion from."));
	}
	if (Mesh.IsNull())
	{
		return Fail(LOCTEXT("NoMesh", "Set the skeletal mesh the reaction character uses."));
	}
	if (PhysicsAsset.IsNull())
	{
		return Fail(LOCTEXT("NoPhysics", "Set the physics asset the reaction character simulates with."));
	}
	if (RegionTable.IsNull())
	{
		return Fail(LOCTEXT("NoRegions", "Set the region table that maps body regions to physics bodies."));
	}
	if (Catalog.IsNull())
	{
		return Fail(LOCTEXT("NoCatalog", "Set the impactor catalog to draw impactors from."));
	}
	if (ClipCount < 1)
	{
		return Fail(LOCTEXT("NoClips", "The batch must produce at least one clip."));
	}
	if (FrameRate < 1)
	{
		return Fail(LOCTEXT("NoFrameRate", "The frame rate must be at least one frame per second."));
	}
	if (PreRollFrames < 0)
	{
		return Fail(LOCTEXT("NegativePreRoll", "Pre-roll cannot be negative."));
	}
	if (MaxReactionFrames < 1)
	{
		return Fail(LOCTEXT("NoReaction", "The reaction ceiling must be at least one frame."));
	}
	if (SettleFrames < 1)
	{
		return Fail(LOCTEXT("NoSettle", "Settle must last at least one frame."));
	}
	if (!FPackageName::IsValidLongPackageName(OutputRoot))
	{
		return Fail(FText::Format(LOCTEXT("BadOutput", "Output root {0} is not a content path. Use one like /Game/KimodoImpactDataset."), FText::FromString(OutputRoot)));
	}
	return true;
}

TArray<FSoftObjectPath> FImpactSourceResolver::Filter(const UImpactBatchDefinition& Batch, TConstArrayView<FAssetData> Candidates)
{
	TArray<FSoftObjectPath> Result;
	if (Batch.RequiredSkeleton.IsNull())
	{
		// Nothing can be baked safely without knowing the skeleton.
		return Result;
	}
	const FSoftObjectPath Skeleton = Batch.RequiredSkeleton.ToSoftObjectPath();
	const FTopLevelAssetPath SequenceClass = UAnimSequence::StaticClass()->GetClassPathName();

	TSet<FSoftObjectPath> Seen;
	for (const FAssetData& Asset : Candidates)
	{
		// Sequences only: blend spaces and montages do not bake as a single source motion.
		if (Asset.AssetClassPath != SequenceClass)
		{
			continue;
		}
		FString SkeletonTag;
		if (!Asset.GetTagValue(TEXT("Skeleton"), SkeletonTag)
			|| FSoftObjectPath(FPackageName::ExportTextPathToObjectPath(SkeletonTag)) != Skeleton)
		{
			continue;
		}

		const FString Name = Asset.AssetName.ToString();
		const bool bIncluded = Batch.NameIncludes.IsEmpty()
			|| Batch.NameIncludes.ContainsByPredicate([&Name](const FString& Include) { return Name.Contains(Include, ESearchCase::IgnoreCase); });
		const bool bExcluded = Batch.NameExcludes.ContainsByPredicate([&Name](const FString& Exclude) { return Name.Contains(Exclude, ESearchCase::IgnoreCase); });
		if (!bIncluded || bExcluded)
		{
			continue;
		}

		const FSoftObjectPath Path = Asset.GetSoftObjectPath();
		if (!Seen.Contains(Path))
		{
			Seen.Add(Path);
			Result.Add(Path);
		}
	}

	// Sorted by path so clip N always draws from the same list, however the registry enumerates.
	Result.Sort([](const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.ToString() < B.ToString(); });
	return Result;
}

bool FImpactSourceResolver::Resolve(const UImpactBatchDefinition& Batch, TArray<FSoftObjectPath>& OutAnimations, FText& OutReason)
{
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();

	TArray<FString> Folders;
	for (const FDirectoryPath& Folder : Batch.AnimationFolders)
	{
		if (!Folder.Path.IsEmpty())
		{
			Folders.Add(Folder.Path);
		}
	}

	TArray<FAssetData> Candidates;
	if (!Folders.IsEmpty())
	{
		// Headless runs start before the registry finishes its background scan.
		Registry.ScanPathsSynchronous(Folders, false);

		FARFilter Query;
		for (const FString& Folder : Folders)
		{
			Query.PackagePaths.Add(FName(*Folder));
		}
		Query.bRecursivePaths = true;
		Query.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
		Registry.GetAssets(Query, Candidates);
	}
	for (const TSoftObjectPtr<UAnimSequence>& Explicit : Batch.ExplicitAnimations)
	{
		const FAssetData Asset = Registry.GetAssetByObjectPath(Explicit.ToSoftObjectPath());
		if (Asset.IsValid())
		{
			Candidates.Add(Asset);
		}
	}

	OutAnimations = Filter(Batch, Candidates);
	if (OutAnimations.IsEmpty())
	{
		OutReason = FText::Format(
			LOCTEXT("NoAnimations", "No animation sequence on skeleton {0} passed the filters. Searched: {1}; explicit animations: {2}."),
			FText::FromString(Batch.RequiredSkeleton.ToSoftObjectPath().ToString()),
			FText::FromString(Folders.IsEmpty() ? TEXT("(no folders)") : FString::Join(Folders, TEXT(", "))),
			FText::AsNumber(Batch.ExplicitAnimations.Num()));
		return false;
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
