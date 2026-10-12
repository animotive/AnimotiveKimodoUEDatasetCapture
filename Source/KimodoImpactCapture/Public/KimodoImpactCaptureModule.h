// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Synthetic impact generation module. Holds the batch definition, deterministic impact
 * parameter sampling, the Physics Control driven reaction character, outcome detection, the
 * fixed-step pose recorder and bake, and the batch runner. Declared UncookedOnly, like the
 * capture module, so none of it can reach a cooked client build.
 *
 * Depends on the capture module for the sidecar schema. The capture module must never
 * depend on this one, and this module must not depend on any editor-only module.
 */
class KIMODOIMPACTCAPTURE_API FKimodoImpactCaptureModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	/** Convenience accessor for code that needs the loaded module instance. */
	static FKimodoImpactCaptureModule& Get();

	/** True when the module is loaded and safe to call into. */
	static bool IsAvailable();
};
