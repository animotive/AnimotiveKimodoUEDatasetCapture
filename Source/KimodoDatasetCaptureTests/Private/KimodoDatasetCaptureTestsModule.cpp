// Copyright Animotive. All Rights Reserved.

#include "Modules/ModuleManager.h"

/**
 * Test module. Carries automation tests only; it registers nothing and hooks nothing, so
 * loading it has no effect beyond making its tests discoverable.
 */
IMPLEMENT_MODULE(FDefaultModuleImpl, KimodoDatasetCaptureTests)
