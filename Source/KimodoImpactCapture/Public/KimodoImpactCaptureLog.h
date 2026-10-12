// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/**
 * Log category for synthetic impact generation.
 *
 * Verbosity contract, matching LogKimodoDatasetCapture:
 *  - Error   : a batch or clip failure. An invalid clip is logged once with its reason.
 *  - Warning : a batch ran but degraded, e.g. a source animation was skipped.
 *  - Log     : batch-level progress only. Never per clip at Log verbosity: a batch is
 *              thousands of clips.
 *  - Verbose : per-clip and per-frame detail.
 */
KIMODOIMPACTCAPTURE_API DECLARE_LOG_CATEGORY_EXTERN(LogKimodoImpactCapture, Log, All);
