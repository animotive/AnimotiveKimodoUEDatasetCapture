// Copyright Animotive. All Rights Reserved.

#include "KimodoDatasetCaptureModule.h"

#include "KimodoDatasetCaptureLog.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "KimodoDatasetCapture"

DEFINE_LOG_CATEGORY(LogKimodoDatasetCapture);

namespace
{
	const FName KimodoDatasetCaptureModuleName(TEXT("KimodoDatasetCapture"));
}

void FKimodoDatasetCaptureModule::StartupModule()
{
	UE_LOG(LogKimodoDatasetCapture, Verbose, TEXT("KimodoDatasetCapture module started."));
}

void FKimodoDatasetCaptureModule::ShutdownModule()
{
	UE_LOG(LogKimodoDatasetCapture, Verbose, TEXT("KimodoDatasetCapture module shut down."));
}

FKimodoDatasetCaptureModule& FKimodoDatasetCaptureModule::Get()
{
	return FModuleManager::LoadModuleChecked<FKimodoDatasetCaptureModule>(KimodoDatasetCaptureModuleName);
}

bool FKimodoDatasetCaptureModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(KimodoDatasetCaptureModuleName);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FKimodoDatasetCaptureModule, KimodoDatasetCapture)
