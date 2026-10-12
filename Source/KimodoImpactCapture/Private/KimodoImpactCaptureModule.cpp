// Copyright Animotive. All Rights Reserved.

#include "KimodoImpactCaptureModule.h"

#include "KimodoImpactCaptureLog.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "KimodoImpactCapture"

DEFINE_LOG_CATEGORY(LogKimodoImpactCapture);

namespace
{
	const FName KimodoImpactCaptureModuleName(TEXT("KimodoImpactCapture"));
}

void FKimodoImpactCaptureModule::StartupModule()
{
	UE_LOG(LogKimodoImpactCapture, Verbose, TEXT("KimodoImpactCapture module started."));
}

void FKimodoImpactCaptureModule::ShutdownModule()
{
	UE_LOG(LogKimodoImpactCapture, Verbose, TEXT("KimodoImpactCapture module shut down."));
}

FKimodoImpactCaptureModule& FKimodoImpactCaptureModule::Get()
{
	return FModuleManager::LoadModuleChecked<FKimodoImpactCaptureModule>(KimodoImpactCaptureModuleName);
}

bool FKimodoImpactCaptureModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(KimodoImpactCaptureModuleName);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FKimodoImpactCaptureModule, KimodoImpactCapture)
