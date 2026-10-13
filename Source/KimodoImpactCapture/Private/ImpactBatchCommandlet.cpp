// Copyright Animotive. All Rights Reserved.

#include "ImpactBatchCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/PlatformTime.h"
#include "ImpactBatchDefinition.h"
#include "ImpactBatchRunner.h"
#include "KimodoImpactCaptureLog.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactBatchCommandlet)

namespace
{
	enum EExitCode : int32
	{
		Finished = 0,
		CouldNotStart = 1,
		StoppedPartway = 2,
	};
}

UImpactBatchCommandlet::UImpactBatchCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	// The exit code is what the run decided. Without this, any unrelated error logged anywhere in
	// the process (plugins' startup noise included) turns a finished batch into exit code 1.
	UseCommandletResultAsExitCode = true;
}

int32 UImpactBatchCommandlet::Main(const FString& Params)
{
	FString BatchPath;
	if (!FParse::Value(*Params, TEXT("Batch="), BatchPath))
	{
		UE_LOG(LogKimodoImpactCapture, Error, TEXT("ImpactBatch: -Batch=<object path of a UImpactBatchDefinition> is required."));
		return CouldNotStart;
	}
	FImpactBatchRunConfig Config;
	FParse::Value(*Params, TEXT("Start="), Config.Start);
	FParse::Value(*Params, TEXT("Count="), Config.Count);
	const bool bIgnoreDisk = FParse::Param(*Params, TEXT("IgnoreDisk"));

	// A commandlet gets no background registry scan; source folders are scanned by the resolver,
	// the batch's own path is scanned here so it can be found by name.
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({ FPackageName::GetLongPackagePath(FSoftObjectPath(BatchPath).GetLongPackageName()) }, false);

	UImpactBatchDefinition* Batch = LoadObject<UImpactBatchDefinition>(nullptr, *BatchPath);
	if (!Batch)
	{
		UE_LOG(LogKimodoImpactCapture, Error, TEXT("ImpactBatch: could not load the batch %s."), *BatchPath);
		return CouldNotStart;
	}

	FImpactBatchRunner Runner;
	FText Reason;
	if (!Runner.Begin(*Batch, Config, Reason))
	{
		UE_LOG(LogKimodoImpactCapture, Error, TEXT("ImpactBatch: %s"), *Reason.ToString());
		return CouldNotStart;
	}
	if (!Runner.GetDiskWarning().IsEmpty())
	{
		if (!bIgnoreDisk)
		{
			UE_LOG(LogKimodoImpactCapture, Error, TEXT("ImpactBatch: %s Pass -IgnoreDisk to run anyway."), *Runner.GetDiskWarning().ToString());
			return CouldNotStart;
		}
		UE_LOG(LogKimodoImpactCapture, Warning, TEXT("ImpactBatch: %s Running anyway (-IgnoreDisk)."), *Runner.GetDiskWarning().ToString());
	}

	const int32 Total = Runner.GetStats().Pending;
	const double Started = FPlatformTime::Seconds();
	double LastReport = Started;
	while (Runner.Step(Reason))
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - LastReport >= 30.0 || Runner.IsDone())
		{
			LastReport = Now;
			const FImpactBatchRunStats& Stats = Runner.GetStats();
			const double PerClip = (Now - Started) / FMath::Max(1, Stats.Done);
			UE_LOG(LogKimodoImpactCapture, Display, TEXT("ImpactBatch: %d/%d clips, %d invalid, %.2f s/clip, about %.0f min left."),
				Stats.Done, Total, Stats.Invalid, PerClip, PerClip * Stats.Pending / 60.0);
		}
	}
	const bool bStopped = !Reason.IsEmpty();
	if (bStopped)
	{
		UE_LOG(LogKimodoImpactCapture, Error, TEXT("ImpactBatch: stopped partway: %s Rerun to resume."), *Reason.ToString());
	}

	FText ReportReason;
	if (!Runner.Finish(ReportReason))
	{
		UE_LOG(LogKimodoImpactCapture, Error, TEXT("ImpactBatch: %s"), *ReportReason.ToString());
	}
	const FImpactBatchRunStats& Stats = Runner.GetStats();
	UE_LOG(LogKimodoImpactCapture, Display, TEXT("ImpactBatch: %d clips made (%d invalid), %d were already done, in %.1f min."),
		Stats.Done, Stats.Invalid, Stats.Skipped, (FPlatformTime::Seconds() - Started) / 60.0);
	return bStopped ? StoppedPartway : Finished;
}
