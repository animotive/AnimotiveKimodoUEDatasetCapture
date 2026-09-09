// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Capture module. Holds per-frame input sampling, the capture data model, and the sidecar
 * JSON schema. Loaded in the editor and during Play In Editor; never present in a cooked
 * client build, because the module is declared UncookedOnly.
 *
 * This module owns no editor UI and must not depend on any editor-only module.
 */
class KIMODODATASETCAPTURE_API FKimodoDatasetCaptureModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	/** Convenience accessor for code that needs the loaded module instance. */
	static FKimodoDatasetCaptureModule& Get();

	/** True when the module is loaded and safe to call into. */
	static bool IsAvailable();
};
