// Copyright Animotive. All Rights Reserved.

#include "KimodoDatasetCaptureEditorModule.h"

#include "KimodoDatasetCaptureLog.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "KimodoDatasetCaptureEditor"

void FKimodoDatasetCaptureEditorModule::StartupModule()
{
	UE_LOG(LogKimodoDatasetCapture, Verbose, TEXT("KimodoDatasetCaptureEditor module started."));
}

void FKimodoDatasetCaptureEditorModule::ShutdownModule()
{
	UE_LOG(LogKimodoDatasetCapture, Verbose, TEXT("KimodoDatasetCaptureEditor module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FKimodoDatasetCaptureEditorModule, KimodoDatasetCaptureEditor)
