// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImpactParams.h"

#include "ImpactGrid.generated.h"

class UImpactBatchDefinition;
class UImpactorCatalog;

/** How finely a full-grid batch steps each axis. */
USTRUCT()
struct KIMODOIMPACTCAPTURE_API FImpactGridSettings
{
	GENERATED_BODY()

	/** Directions around the body, evenly over the batch's azimuth range. */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = "1"))
	int32 AzimuthSteps = 8;

	/** Elevations, evenly over each impactor's range within the batch's (one step: its middle). */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = "1"))
	int32 ElevationSteps = 3;

	/** Impactor strengths, from its lightest and slowest to its heaviest and fastest (one step: the middle). */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = "1"))
	int32 StrengthLevels = 3;

	/** Points in each source the hit lands at, evenly over the batch's source-phase range. */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = "1"))
	int32 PhasesPerSource = 1;

	/**
	 * A play-once source (a jump) is hit at its three flight moments - before take-off, at the apex,
	 * after landing - in place of the phase steps.
	 */
	UPROPERTY(EditAnywhere, Category = "Grid")
	bool bFlightMoments = true;
};

/** One clip's place in a full grid. */
struct KIMODOIMPACTCAPTURE_API FImpactGridCell
{
	int32 Source = 0;
	/** Index into the catalog's archetypes. */
	int32 Archetype = 0;
	EImpactBodyRegion Region = EImpactBodyRegion::Chest;
	int32 Azimuth = 0;
	int32 Elevation = 0;
	int32 Strength = 0;
	int32 Phase = 0;
	/** Phase for a looping source; a flight moment for a play-once source when the grid uses them. */
	EImpactSourceMoment Moment = EImpactSourceMoment::Phase;

	bool operator==(const FImpactGridCell& Other) const;
};

/**
 * Every combination of a batch's sources, the impactors it can draw, the regions each can strike,
 * and the grid's directions, strengths and phases, laid out so a clip index names one cell.
 */
class KIMODOIMPACTCAPTURE_API FImpactGrid
{
public:
	/** Lays out the grid. False with a reason when it would be empty. */
	bool Build(const UImpactBatchDefinition& Batch, const UImpactorCatalog& Catalog, int32 NumSources, FText& OutReason);

	/** As above; SourcePlaysOnce (one per source, or empty) marks the sources hit at their flight moments. */
	bool Build(const UImpactBatchDefinition& Batch, const UImpactorCatalog& Catalog, int32 NumSources, TConstArrayView<bool> SourcePlaysOnce, FText& OutReason);

	int32 Num() const;

	/** The cell clip Index stands for; Index must be in [0, Num()). */
	FImpactGridCell Cell(int32 Index) const;

	/**
	 * The impact a cell describes: the archetype at the cell's strength level, travelling at its
	 * azimuth and elevation, striking its region, at its phase.
	 */
	static FImpactParams Params(const FImpactGridCell& Cell, const UImpactBatchDefinition& Batch, const UImpactorCatalog& Catalog);

private:
	struct FTriple
	{
		int32 Source = 0;
		int32 Archetype = 0;
		EImpactBodyRegion Region = EImpactBodyRegion::Chest;
	};
	TArray<FTriple> Triples;
	FImpactGridSettings Settings;
};
