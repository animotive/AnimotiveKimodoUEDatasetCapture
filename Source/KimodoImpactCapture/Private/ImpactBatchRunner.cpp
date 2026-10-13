// Copyright Animotive. All Rights Reserved.

#include "ImpactBatchRunner.h"

#include "ImpactClipSimulator.h"
#include "ImpactPoseBuffer.h"
#include "ImpactSimWorld.h"

bool FImpactBatchAssets::Load(const UImpactBatchDefinition& Batch, FImpactBatchAssets& Out, FText& OutReason)
{
	return false;
}

bool FImpactBatchAssets::PlaysOnce(const UImpactBatchDefinition& Batch, const FString& Name)
{
	return false;
}

bool FImpactClipFactory::Make(const UImpactBatchDefinition& Batch, const FImpactBatchAssets& Assets, int32 ClipIndex,
	FImpactClipSetup& OutSetup, FString& OutClipId, FText& OutReason)
{
	return false;
}

void FImpactClipPoser::Build(const FReferenceSkeleton& Ref, const FImpactClipRecording& Recording, const FVector& MeshForward, FImpactPoseBuffer& OutPoses)
{
}

FImpactBatchRunner::FImpactBatchRunner() = default;
FImpactBatchRunner::~FImpactBatchRunner() = default;

bool FImpactBatchRunner::Begin(UImpactBatchDefinition& InBatch, const FImpactBatchRunConfig& InConfig, FText& OutReason)
{
	return false;
}

bool FImpactBatchRunner::Step(FText& OutReason)
{
	return false;
}

bool FImpactBatchRunner::IsDone() const
{
	return true;
}

bool FImpactBatchRunner::Finish(FText& OutReason)
{
	return false;
}

FString FImpactBatchRunner::BatchPackagePath(const UImpactBatchDefinition& InBatch)
{
	return InBatch.OutputRoot;
}

FString FImpactBatchRunner::ReportFilename(const FString& InBatchFolder)
{
	return FString();
}
