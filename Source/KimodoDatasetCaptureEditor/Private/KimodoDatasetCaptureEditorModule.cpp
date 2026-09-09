// Copyright Animotive. All Rights Reserved.

#include "KimodoDatasetCaptureEditorModule.h"

#include "DatasetInputTrackEditor.h"
#include "ISequencerModule.h"
#include "KimodoDatasetCaptureLog.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "KimodoDatasetCaptureEditor"

void FKimodoDatasetCaptureEditorModule::StartupModule()
{
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"));

	DatasetInputTrackEditorHandle = SequencerModule.RegisterTrackEditor(
		FOnCreateTrackEditor::CreateStatic(&FDatasetInputTrackEditor::CreateTrackEditor));

	UE_LOG(LogKimodoDatasetCapture, Verbose, TEXT("KimodoDatasetCaptureEditor module started; input track editor registered."));
}

void FKimodoDatasetCaptureEditorModule::ShutdownModule()
{
	// Unregister guarded on both the handle and the module still being loaded: during editor
	// shutdown Sequencer may already be gone, and LoadModuleChecked would fatally assert on a
	// path where there is nothing left to clean up anyway.
	if (DatasetInputTrackEditorHandle.IsValid() && FModuleManager::Get().IsModuleLoaded(TEXT("Sequencer")))
	{
		ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"));
		SequencerModule.UnRegisterTrackEditor(DatasetInputTrackEditorHandle);
	}

	DatasetInputTrackEditorHandle.Reset();

	UE_LOG(LogKimodoDatasetCapture, Verbose, TEXT("KimodoDatasetCaptureEditor module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FKimodoDatasetCaptureEditorModule, KimodoDatasetCaptureEditor)
