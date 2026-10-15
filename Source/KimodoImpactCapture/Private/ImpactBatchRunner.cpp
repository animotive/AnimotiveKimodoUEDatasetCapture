// Copyright Animotive. All Rights Reserved.

#include "ImpactBatchRunner.h"

#include "Animation/AnimSequence.h"
#include "DatasetDescriptions.h"
#include "DatasetSidecarSerializer.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "ImpactBatchDefinition.h"
#include "ImpactCaption.h"
#include "ImpactClipBaker.h"
#include "ImpactClipId.h"
#include "ImpactClipSimulator.h"
#include "ImpactDiskEstimate.h"
#include "ImpactLegIK.h"
#include "ImpactPoseBuffer.h"
#include "ImpactPoseRebuild.h"
#include "ImpactRandom.h"
#include "ImpactRegionTable.h"
#include "ImpactSidecarBuilder.h"
#include "ImpactSimWorld.h"
#include "ImpactorCatalog.h"
#include "KimodoImpactCaptureLog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "ImpactBatchRunner"

namespace
{
	template <typename TEnum>
	FString EnumName(TEnum Value)
	{
		return StaticEnum<TEnum>()->GetNameStringByValue(int64(Value));
	}

	template <typename T>
	bool LoadHeld(const TSoftObjectPtr<T>& Soft, TStrongObjectPtr<T>& Out, const TCHAR* What, FText& OutReason)
	{
		Out.Reset(Soft.LoadSynchronous());
		if (!Out.IsValid())
		{
			OutReason = FText::Format(LOCTEXT("LoadFailed", "Could not load the batch's {0}: {1}"), FText::FromString(What),
				FText::FromString(Soft.ToSoftObjectPath().ToString()));
			return false;
		}
		return true;
	}

	/** Moves a package off its name and lets it be collected, so the name can be baked again. */
	void ReleasePackageName(UPackage* Package)
	{
		Package->Rename(*FString::Printf(TEXT("/Temp/ImpactReleased_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)), nullptr,
			REN_DontCreateRedirectors | REN_NonTransactional);
		Package->MarkAsGarbage();
	}

	/**
	 * Removes whatever a run that stopped before recording clip Index left of it. Its name may
	 * differ (the outcome is part of it), but it always starts with the clip's index prefix; a new
	 * package will not save over a file it was not loaded from.
	 */
	void DeleteLeftovers(const FString& Folder, int32 Index)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Folder / FImpactClipName::IndexPrefix(Index) + TEXT("*")), true, false);
		for (const FString& File : Files)
		{
			IFileManager::Get().Delete(*(Folder / File), false, true);
		}
	}

	int64 FileSize(const FString& Filename)
	{
		return FMath::Max<int64>(0, IFileManager::Get().FileSize(*Filename));
	}
}

bool FImpactBatchAssets::Load(const UImpactBatchDefinition& Batch, FImpactBatchAssets& Out, FText& OutReason)
{
	Out = FImpactBatchAssets();
	if (!LoadHeld(Batch.Mesh, Out.Mesh, TEXT("mesh"), OutReason)
		|| !LoadHeld(Batch.PhysicsAsset, Out.PhysicsAsset, TEXT("physics asset"), OutReason)
		|| !LoadHeld(Batch.RegionTable, Out.RegionTable, TEXT("region table"), OutReason)
		|| !LoadHeld(Batch.Catalog, Out.Catalog, TEXT("impactor catalog"), OutReason))
	{
		return false;
	}
	if (!Batch.MotionTable.IsNull() && !LoadHeld(Batch.MotionTable, Out.MotionTable, TEXT("source motion table"), OutReason))
	{
		return false;
	}

	TArray<FSoftObjectPath> Paths;
	if (!FImpactSourceResolver::Resolve(Batch, Paths, OutReason))
	{
		return false;
	}
	for (const FSoftObjectPath& Path : Paths)
	{
		UAnimSequence* Sequence = Cast<UAnimSequence>(Path.TryLoad());
		if (!Sequence)
		{
			OutReason = FText::Format(LOCTEXT("SourceFailed", "Could not load the source animation {0}."), FText::FromString(Path.ToString()));
			return false;
		}
		Out.Sources.Emplace(Sequence);
		Out.SourcePlaysOnce.Add(PlaysOnce(Batch, Sequence->GetName()));
	}
	return !Batch.bFullGrid || Out.Grid.Build(Batch, *Out.Catalog, Out.Sources.Num(), Out.SourcePlaysOnce, OutReason);
}

int32 FImpactBatchAssets::ClipCount(const UImpactBatchDefinition& Batch) const
{
	return Batch.bFullGrid ? Grid.Num() : Batch.ClipCount;
}

bool FImpactBatchAssets::PlaysOnce(const UImpactBatchDefinition& Batch, const FString& Name)
{
	return Batch.PlayOnceNames.ContainsByPredicate([&Name](const FString& Once)
	{
		return !Once.IsEmpty() && Name.Contains(Once, ESearchCase::IgnoreCase);
	});
}

bool FImpactClipFactory::Make(const UImpactBatchDefinition& Batch, const FImpactBatchAssets& Assets, int32 ClipIndex,
	FImpactClipSetup& OutSetup, FString& OutClipId, FText& OutReason)
{
	if (Assets.Sources.IsEmpty() || !Assets.Catalog.IsValid())
	{
		OutReason = LOCTEXT("NoAssets", "The batch's assets are not loaded.");
		return false;
	}
	FImpactClipSetup Setup;
	Setup.Mesh = Assets.Mesh.Get();
	Setup.PhysicsAsset = Assets.PhysicsAsset.Get();
	Setup.RegionTable = Assets.RegionTable.Get();
	Setup.BatchSeed = Batch.Seed;
	Setup.ClipIndex = ClipIndex;
	Setup.FrameRate = Batch.FrameRate;
	Setup.PreRollFrames = Batch.PreRollFrames;
	Setup.MaxReactionFrames = Batch.MaxReactionFrames;
	Setup.SettleFrames = Batch.SettleFrames;

	int32 Source = 0;
	if (Batch.bFullGrid)
	{
		// A full grid names the clip's source and impact outright; the rest still comes from the seed.
		if (ClipIndex < 0 || ClipIndex >= Assets.Grid.Num())
		{
			OutReason = LOCTEXT("OutsideGrid", "The clip index is outside the batch's full grid.");
			return false;
		}
		const FImpactGridCell Cell = Assets.Grid.Cell(ClipIndex);
		Source = Cell.Source;
		Setup.Params = FImpactGrid::Params(Cell, Batch, *Assets.Catalog);
	}
	else
	{
		FImpactRandom SourceRandom(Batch.Seed, ClipIndex, EImpactRandomStream::SourceAnimation);
		Source = SourceRandom.RangeInt(0, Assets.Sources.Num() - 1);
		if (!FImpactParamsSampler::Sample(*Assets.Catalog, Batch.Impact, Batch.CharacterMassKg, Batch.Seed, ClipIndex, Setup.Params, OutReason))
		{
			return false;
		}
	}
	Setup.Source = Assets.Sources[Source].Get();
	Setup.bSourcePlaysOnce = Assets.SourcePlaysOnce.IsValidIndex(Source) && Assets.SourcePlaysOnce[Source];
	if (!FImpactScheduleSampler::Sample(Setup.Params, Batch.Delivery, Batch.Seed, ClipIndex, Setup.Schedule, OutReason))
	{
		return false;
	}
	FImpactRandom StrengthRandom(Batch.Seed, ClipIndex, EImpactRandomStream::StrengthProfile);
	Setup.Strength = FImpactStrengthProfile::Sample(Batch.Strength, StrengthRandom);

	OutClipId = FImpactClipId::Make(Setup.Source->GetFName(), Setup.Params, Setup.Schedule);
	OutSetup = MoveTemp(Setup);
	return true;
}

void FImpactClipPoser::Build(const FReferenceSkeleton& Ref, const FImpactClipRecording& Recording, const FVector& MeshForward, FImpactPoseBuffer& OutPoses)
{
	const int32 Bones = Ref.GetNum();
	OutPoses.Reserve(Recording.Frames.Num(), Bones);
	const FImpactPoseRebuild Rebuild(Ref, Recording.BodyBoneIndices);

	FImpactLegChain Legs[2];
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const TCHAR* Suffix = Side == 0 ? TEXT("_l") : TEXT("_r");
		Legs[Side].Thigh = Ref.FindBoneIndex(*(FString(TEXT("thigh")) + Suffix));
		Legs[Side].Calf = Ref.FindBoneIndex(*(FString(TEXT("calf")) + Suffix));
		Legs[Side].Foot = Ref.FindBoneIndex(*(FString(TEXT("foot")) + Suffix));
	}

	TArray<FTransform> BodyComponent;
	TArray<FTransform> Component;
	BodyComponent.Reserve(Recording.BodyBoneIndices.Num());
	Component.SetNum(Bones);
	for (int32 FrameIndex = 0; FrameIndex < Recording.Frames.Num(); ++FrameIndex)
	{
		const FImpactRecordedFrame& Frame = Recording.Frames[FrameIndex];
		const TArrayView<const FTransform> Bodies = Recording.BodyPoses.GetFrame(FrameIndex);
		const TArrayView<const FTransform> Source = Recording.SourcePoses.GetFrame(FrameIndex);
		BodyComponent.Reset();
		for (const FTransform& Body : Bodies)
		{
			BodyComponent.Add(Body.GetRelativeTransform(Recording.Reference));
		}
		Rebuild.Rebuild(BodyComponent, Source, Frame.Root, Component);

		// Planner-placed feet land exactly; on a moving source, feet the source has on the ground
		// go to its ground height.
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const FImpactLegChain& Leg = Legs[Side];
			if (!Leg.IsValid())
			{
				continue;
			}
			const TOptional<FVector> Effector = FImpactLegIK::ChooseEffector(Frame.bReleased, Frame.bFootPlaced[Side],
				Recording.Reference.InverseTransformPosition(Frame.FootTarget[Side]), Recording.bMovingSource,
				Source[Leg.Foot].GetLocation(), Component[Leg.Foot].GetLocation());
			if (Effector.IsSet())
			{
				FImpactLegIK::Solve(Ref, Leg, Effector.GetValue(), MeshForward, Source, Component);
			}
		}

		TArrayView<FTransform> Out = OutPoses.AddFrame();
		for (int32 Bone = 0; Bone < Bones; ++Bone)
		{
			Out[Bone] = Component[Bone];
		}
	}
}

FImpactBatchRunner::FImpactBatchRunner() = default;
FImpactBatchRunner::~FImpactBatchRunner() = default;

FString FImpactBatchRunner::BatchPackagePath(const UImpactBatchDefinition& InBatch)
{
	FString Root = InBatch.OutputRoot;
	while (Root.EndsWith(TEXT("/")))
	{
		Root.LeftChopInline(1);
	}
	return Root / InBatch.GetName();
}

FString FImpactBatchRunner::DescriptionsFilename(const UImpactBatchDefinition& InBatch)
{
	return FPaths::GetPath(FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(BatchPackagePath(InBatch))))
		/ TEXT("descriptions.json");
}

bool FImpactBatchRunner::WriteDescriptions(const FImpactBatchManifest& All, FText& OutReason) const
{
	if (!Batch.IsValid())
	{
		return true;
	}
	// One index for everything under the output root (and any other generator writing there):
	// this batch's entries are replaced as a whole, everyone else's kept.
	const FString File = DescriptionsFilename(*Batch);
	FDatasetDescriptions Index;
	if (IFileManager::Get().FileExists(*File) && !Index.LoadFromFile(File, OutReason))
	{
		return false;
	}
	const FString Prefix = Batch->GetName() + TEXT("/");
	Index.RemoveFilesUnder(Prefix);
	for (const FImpactManifestEntry& Entry : All.GetEntries())
	{
		if (!Entry.bValid || Entry.FileName.IsEmpty() || Entry.Descriptions.IsEmpty())
		{
			continue;
		}
		FDatasetDescriptionEntry Listed;
		Listed.Name = Entry.FileName;
		// The baked clip until FBX export exists.
		Listed.File = Prefix + Entry.FileName + FPackageName::GetAssetPackageExtension();
		Listed.Kind = TEXT("impact");
		Listed.Descriptions = Entry.Descriptions;
		if (!Index.Add(Listed, OutReason))
		{
			return false;
		}
	}
	return Index.SaveToFile(File, OutReason);
}

FString FImpactBatchRunner::ReportFilename(const FString& InBatchFolder)
{
	return InBatchFolder / TEXT("Report.json");
}

bool FImpactBatchRunner::Begin(UImpactBatchDefinition& InBatch, const FImpactBatchRunConfig& InConfig, FText& OutReason)
{
	Batch.Reset(&InBatch);
	Config = InConfig;
	Stats = FImpactBatchRunStats();
	DiskWarning = FText::GetEmpty();
	Pending.Reset();
	NextPending = 0;
	SinceGarbage = 0;

	if (!InBatch.ValidateSettings(OutReason)
		|| !FImpactBatchAssets::Load(InBatch, Assets, OutReason)
		|| !FImpactSharding::Resolve(Assets.ClipCount(InBatch), Config.Start, Config.Count, Range, OutReason))
	{
		return false;
	}
	BatchFolder = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(BatchPackagePath(InBatch)));

	// Every shard's manifest says what is done, whichever range this process runs.
	FImpactBatchManifest Done;
	if (!FImpactBatchManifest::LoadAll(BatchFolder, Done, OutReason)
		|| !Manifest.Load(FImpactSharding::ManifestFilename(BatchFolder, Range), OutReason))
	{
		return false;
	}
	Pending = Done.Pending(Range.Start, Range.Count);
	Stats.Pending = Pending.Num();
	Stats.Skipped = Range.Count - Pending.Num();

	const int32 MaxFrames = InBatch.PreRollFrames + InBatch.MaxReactionFrames + InBatch.SettleFrames;
	const int64 Needed = FImpactDiskEstimate::Estimate(Pending.Num(),
		FImpactDiskEstimate::PerClip(Done, Assets.Mesh->GetRefSkeleton().GetNum(), MaxFrames));
	FString Existing = BatchFolder;
	while (!Existing.IsEmpty() && !IFileManager::Get().DirectoryExists(*Existing))
	{
		Existing = FPaths::GetPath(Existing);
	}
	FImpactDiskEstimate::ShouldWarn(Needed, FImpactDiskEstimate::FreeBytes(Existing), DiskWarning);

	World = MakeUnique<FImpactSimWorld>();
	UE_LOG(LogKimodoImpactCapture, Display, TEXT("Impact batch %s (%d clips%s): clips %d-%d, %d pending, %d already done."),
		*InBatch.GetName(), Assets.ClipCount(InBatch), InBatch.bFullGrid ? TEXT(", full grid") : TEXT(""), Range.Start, Range.End() - 1, Stats.Pending, Stats.Skipped);
	return true;
}

bool FImpactBatchRunner::IsDone() const
{
	return NextPending >= Pending.Num();
}

bool FImpactBatchRunner::Step(FText& OutReason)
{
	OutReason = FText::GetEmpty();
	if (IsDone() || !Batch.IsValid() || !World)
	{
		return false;
	}
	const UImpactBatchDefinition& Def = *Batch;
	const int32 ClipIndex = Pending[NextPending++];

	FImpactClipSetup Setup;
	FString ClipId;
	if (!FImpactClipFactory::Make(Def, Assets, ClipIndex, Setup, ClipId, OutReason))
	{
		return false;
	}

	FImpactManifestEntry Entry;
	Entry.ClipIndex = ClipIndex;
	Entry.ClipId = ClipId;
	Entry.SourceAnimation = Setup.Source->GetPathName();
	Entry.ImpactorId = Setup.Params.Impactor.Archetype ? Setup.Params.Impactor.Archetype->Id.ToString() : FString();
	Entry.Region = EnumName(Setup.Params.Region);
	Entry.Intensity = EnumName(Setup.Params.Intensity);
	Entry.DeliveryMode = EnumName(Setup.Schedule.Mode);

	// Each clip in a world of its own: in one shared world a clip inherited what earlier clips
	// left in the physics scene, and came out differently from the same clip run alone.
	World.Reset();
	World = MakeUnique<FImpactSimWorld>();

	FImpactClipRecording Recording;
	FText SimReason;
	if (!FImpactClipSimulator::Run(*World, Setup, Recording, SimReason))
	{
		// A clip that cannot be set up is recorded, so a resume does not retry it forever.
		Entry.bValid = false;
		Entry.InvalidReason = SimReason.ToString();
	}
	else
	{
		Entry.Frames = Recording.Frames.Num();
		Entry.Outcome = EnumName(Recording.Outcome.Outcome);
		Entry.bValid = Recording.End != EImpactClipEnd::Invalidated;
		Entry.InvalidReason = Entry.bValid ? FString() : Recording.InvalidReason.ToString();

		// Readable files, IMP_<index>_<Source>_<Impactor>_<Region>_<From>_<Intensity>_<Outcome>; the hash id stays the
		// clip's identity in its sidecar and the manifest.
		const FString FileName = FImpactClipName::Make(ClipIndex, FImpactClipName::SourceLabel(Setup.Source->GetName()),
			Entry.ImpactorId, Entry.Region, FImpactClipName::DirectionLabel(Setup.Params.DirectionLocal), Entry.Intensity, Entry.Outcome);
		Entry.FileName = FileName;
		DeleteLeftovers(BatchFolder, ClipIndex);

		FString AnimSequencePath;
		if (Entry.bValid || !Def.bDiscardInvalid)
		{
			FImpactPoseBuffer Poses;
			FImpactClipPoser::Build(Setup.Mesh->GetRefSkeleton(), Recording, Setup.MeshForward, Poses);

			const FString PackageName = FImpactClipBaker::PackageName(Def.OutputRoot, Def.GetName(), FileName);
			if (UPackage* Stale = FindPackage(nullptr, *PackageName))
			{
				// A clip made again in the same process (or loaded by an editor): a dead package
				// under the name would be handed back by CreatePackage and refuse to save.
				ReleasePackageName(Stale);
			}
			const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
			UPackage* Package = CreatePackage(*PackageName);
			UAnimSequence* Sequence = FImpactClipBaker::Bake(Package, FName(*FileName), Setup.Mesh->GetSkeleton(),
				Setup.Mesh->GetRefSkeleton(), Poses, Setup.FrameRate, OutReason);
			const bool bSaved = Sequence && FImpactClipBaker::Save(Sequence, Filename, OutReason);
			if (Sequence)
			{
				Sequence->ClearFlags(RF_Public | RF_Standalone);
			}
			ReleasePackageName(Package);
			if (!bSaved)
			{
				return false;
			}
			AnimSequencePath = PackageName + TEXT(".") + FileName;
			Entry.AnimSequencePath = AnimSequencePath;
			Entry.Bytes += FileSize(Filename);
		}

		FImpactSidecarContext Context;
		Context.ClipId = ClipId;
		Context.AnimSequencePath = AnimSequencePath;
		Context.Motion = FImpactCaption::ResolveMotion(Assets.MotionTable.Get(), Setup.Source->GetFName());
		const FString SidecarFile = BatchFolder / FileName + TEXT(".json");
		if (!FDatasetSidecarSerializer::SaveToFile(FImpactSidecarBuilder::Build(Setup, Recording, Context), SidecarFile, OutReason))
		{
			return false;
		}
		Entry.Bytes += FileSize(SidecarFile);
		if (Entry.bValid)
		{
			Entry.Descriptions = FImpactCaption::Descriptions(FImpactSidecarBuilder::CaptionInput(Setup, Recording, Context.Motion), Setup.BatchSeed, ClipIndex);
		}
	}

	if (!Manifest.Append(Entry, OutReason))
	{
		return false;
	}
	++Stats.Done;
	--Stats.Pending;
	Stats.Invalid += Entry.bValid ? 0 : 1;
	if (!Entry.Outcome.IsEmpty())
	{
		++Stats.Outcomes.FindOrAdd(Entry.Outcome);
	}
	UE_LOG(LogKimodoImpactCapture, Log, TEXT("Clip %d %s: %s%s"), ClipIndex, *ClipId, Entry.Outcome.IsEmpty() ? TEXT("not run") : *Entry.Outcome,
		Entry.bValid ? TEXT("") : *(TEXT(", invalid: ") + Entry.InvalidReason));

	if (++SinceGarbage >= FMath::Max(1, Config.CollectGarbageEveryClips))
	{
		SinceGarbage = 0;
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
	return true;
}

bool FImpactBatchRunner::Finish(FText& OutReason)
{
	FImpactBatchManifest All;
	if (!FImpactBatchManifest::LoadAll(BatchFolder, All, OutReason))
	{
		return false;
	}
	Report = FImpactDistributionReport::Build(All);
	UE_LOG(LogKimodoImpactCapture, Display, TEXT("%s"), *Report.ToText());
	if (!WriteDescriptions(All, OutReason))
	{
		return false;
	}
	World.Reset();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	return Report.Save(ReportFilename(BatchFolder), OutReason);
}

#undef LOCTEXT_NAMESPACE
