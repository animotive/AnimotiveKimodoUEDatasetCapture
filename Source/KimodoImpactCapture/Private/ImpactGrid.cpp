// Copyright Animotive. All Rights Reserved.

#include "ImpactGrid.h"

#include "ImpactBatchDefinition.h"
#include "ImpactorCatalog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactGrid)

bool FImpactGridCell::operator==(const FImpactGridCell& Other) const
{
	return Source == Other.Source && Archetype == Other.Archetype && Region == Other.Region && Azimuth == Other.Azimuth
		&& Elevation == Other.Elevation && Strength == Other.Strength && Phase == Other.Phase;
}

#define LOCTEXT_NAMESPACE "ImpactGrid"

namespace
{
	FFloatInterval ElevationBand(const UImpactBatchDefinition& Batch, const FImpactorArchetype& Archetype)
	{
		return FFloatInterval(FMath::Max(Batch.Impact.ElevationDegrees.Min, Archetype.ElevationDegrees.Min),
			FMath::Min(Batch.Impact.ElevationDegrees.Max, Archetype.ElevationDegrees.Max));
	}

	/** Step Index of Steps across [Min, Max], ends included; one step is the middle. */
	float Step(float Min, float Max, int32 Index, int32 Steps)
	{
		return Steps > 1 ? FMath::Lerp(Min, Max, float(Index) / float(Steps - 1)) : 0.5f * (Min + Max);
	}

	FImpactGridSettings Clamped(const FImpactGridSettings& Settings)
	{
		FImpactGridSettings Out;
		Out.AzimuthSteps = FMath::Max(1, Settings.AzimuthSteps);
		Out.ElevationSteps = FMath::Max(1, Settings.ElevationSteps);
		Out.StrengthLevels = FMath::Max(1, Settings.StrengthLevels);
		Out.PhasesPerSource = FMath::Max(1, Settings.PhasesPerSource);
		return Out;
	}
}

bool FImpactGrid::Build(const UImpactBatchDefinition& Batch, const UImpactorCatalog& Catalog, int32 NumSources, FText& OutReason)
{
	Triples.Reset();
	Settings = Clamped(Batch.Grid);
	if (NumSources < 1)
	{
		OutReason = LOCTEXT("NoSources", "A full grid needs at least one source animation.");
		return false;
	}
	for (int32 Source = 0; Source < NumSources; ++Source)
	{
		for (int32 Archetype = 0; Archetype < Catalog.Archetypes.Num(); ++Archetype)
		{
			const FImpactorArchetype& Candidate = Catalog.Archetypes[Archetype];
			const FFloatInterval Band = ElevationBand(Batch, Candidate);
			if (Candidate.Weight <= 0.0f || Band.Min > Band.Max)
			{
				continue;
			}
			for (const FImpactRegionWeight& Region : Batch.Impact.Regions)
			{
				if (Region.Weight > 0.0f && Candidate.CanStrike(Region.Region))
				{
					Triples.Add({ Source, Archetype, Region.Region });
				}
			}
		}
	}
	const int64 Cells = int64(Triples.Num()) * Settings.AzimuthSteps * Settings.ElevationSteps * Settings.StrengthLevels * Settings.PhasesPerSource;
	if (Cells < 1)
	{
		OutReason = LOCTEXT("Empty", "No impactor in the catalog can strike the batch's regions from its elevations.");
		return false;
	}
	if (Cells > MAX_int32)
	{
		Triples.Reset();
		OutReason = FText::Format(LOCTEXT("TooBig", "The full grid has {0} cells, more than one batch can index; use fewer steps."), FText::AsNumber(Cells));
		return false;
	}
	return true;
}

int32 FImpactGrid::Num() const
{
	return Triples.Num() * Settings.AzimuthSteps * Settings.ElevationSteps * Settings.StrengthLevels * Settings.PhasesPerSource;
}

FImpactGridCell FImpactGrid::Cell(int32 Index) const
{
	// Index = ((((Triple * Azimuth) + az) * Elevation + el) * Strength + level) * Phase + phase.
	FImpactGridCell Cell;
	int32 Rest = FMath::Clamp(Index, 0, FMath::Max(0, Num() - 1));
	Cell.Phase = Rest % Settings.PhasesPerSource;
	Rest /= Settings.PhasesPerSource;
	Cell.Strength = Rest % Settings.StrengthLevels;
	Rest /= Settings.StrengthLevels;
	Cell.Elevation = Rest % Settings.ElevationSteps;
	Rest /= Settings.ElevationSteps;
	Cell.Azimuth = Rest % Settings.AzimuthSteps;
	Rest /= Settings.AzimuthSteps;
	if (Triples.IsValidIndex(Rest))
	{
		Cell.Source = Triples[Rest].Source;
		Cell.Archetype = Triples[Rest].Archetype;
		Cell.Region = Triples[Rest].Region;
	}
	return Cell;
}

FImpactParams FImpactGrid::Params(const FImpactGridCell& Cell, const UImpactBatchDefinition& Batch, const UImpactorCatalog& Catalog)
{
	FImpactParams Params;
	if (!Catalog.Archetypes.IsValidIndex(Cell.Archetype))
	{
		return Params;
	}
	const FImpactGridSettings Steps = Clamped(Batch.Grid);
	const FImpactorArchetype& Archetype = Catalog.Archetypes[Cell.Archetype];

	Params.Impactor.Archetype = &Archetype;
	Params.Impactor.MassKg = Step(Archetype.MassKg.Min, Archetype.MassKg.Max, Cell.Strength, Steps.StrengthLevels);
	Params.Impactor.SpeedMetresPerSecond = Step(Archetype.SpeedMetresPerSecond.Min, Archetype.SpeedMetresPerSecond.Max, Cell.Strength, Steps.StrengthLevels);
	Params.Impactor.ContactRadiusCm = 0.5f * (Archetype.ContactRadiusCm.Min + Archetype.ContactRadiusCm.Max);
	Params.TransferredImpulseKgCmPerSecond = FImpactParamsSampler::TransferredImpulse(
		Params.Impactor.MassKg, Params.Impactor.SpeedMetresPerSecond, Batch.CharacterMassKg, Batch.Impact.Restitution);
	Params.ReactionGain = Archetype.ReactionGain;
	Params.ReactionImpulseKgCmPerSecond = Params.TransferredImpulseKgCmPerSecond * Params.ReactionGain;
	Params.Intensity = Batch.Impact.Intensity.Classify(Params.ReactionImpulseKgCmPerSecond);
	Params.Region = Cell.Region;

	const FFloatInterval Band = ElevationBand(Batch, Archetype);
	const double Elevation = FMath::DegreesToRadians(double(Step(Band.Min, Band.Max, Cell.Elevation, Steps.ElevationSteps)));
	// Azimuth steps share the range without repeating its end (0 and 360 are one direction).
	const FFloatInterval& Azimuths = Batch.Impact.AzimuthDegrees;
	const double Azimuth = FMath::DegreesToRadians(double(Azimuths.Min + (Azimuths.Max - Azimuths.Min) * float(Cell.Azimuth) / float(Steps.AzimuthSteps)));
	Params.DirectionLocal = FVector(FMath::Cos(Elevation) * FMath::Cos(Azimuth), FMath::Cos(Elevation) * FMath::Sin(Azimuth), FMath::Sin(Elevation));

	const FFloatInterval& Phases = Batch.Impact.SourcePhase;
	Params.SourcePhase = FMath::Lerp(Phases.Min, Phases.Max, (float(Cell.Phase) + 0.5f) / float(Steps.PhasesPerSource));
	return Params;
}

#undef LOCTEXT_NAMESPACE
