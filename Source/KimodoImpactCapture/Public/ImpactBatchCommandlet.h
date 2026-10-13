// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "ImpactBatchCommandlet.generated.h"

/**
 * Headless host for an impact batch: one bare world, stepped as fast as the CPU allows.
 *
 *   UnrealEditor-Cmd <Project>.uproject -run=ImpactBatch -Batch=/Game/Path/MyBatch [-Start=0] [-Count=1000] [-IgnoreDisk]
 *
 * Shard across processes with -Start/-Count; each shard keeps its own manifest and a rerun resumes.
 *
 * Exit codes:
 *   0  the range is finished (invalid clips are recorded in the manifest, not failures);
 *   1  the run could not start (bad arguments, batch, assets, or not enough disk without -IgnoreDisk);
 *   2  the run stopped partway (a clip could not be baked or recorded); rerun to resume.
 */
UCLASS()
class KIMODOIMPACTCAPTURE_API UImpactBatchCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UImpactBatchCommandlet();

	virtual int32 Main(const FString& Params) override;
};
