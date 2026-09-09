// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/**
 * Log category for everything this plugin emits, capture module and editor module alike.
 *
 * Verbosity contract (see conductor/product-guidelines.md):
 *  - Error   : a failure the operator must act on. Always also surfaced in the panel.
 *  - Warning : capture succeeded but degraded, e.g. an optional input layer was unavailable.
 *  - Verbose : per-frame or per-sample detail. Never emitted from the capture hot path
 *              at Log verbosity, because a 10-second take is hundreds of frames.
 */
KIMODODATASETCAPTURE_API DECLARE_LOG_CATEGORY_EXTERN(LogKimodoDatasetCapture, Log, All);
