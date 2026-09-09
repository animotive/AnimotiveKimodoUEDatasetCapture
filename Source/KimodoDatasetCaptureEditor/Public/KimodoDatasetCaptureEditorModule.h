// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Editor module. Owns the capture panel, the Take Recorder source registration, the
 * MovieScene input track editor, the Play In Editor overlay, and the export UI.
 *
 * Depends on KimodoDatasetCapture. The reverse dependency is forbidden and checked by
 * tools/check-module-deps.ps1.
 */
class FKimodoDatasetCaptureEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

private:
	/** Registration handle for the Sequencer track editor that draws the input track. */
	FDelegateHandle DatasetInputTrackEditorHandle;
};
