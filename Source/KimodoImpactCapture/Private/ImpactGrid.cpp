// Copyright Animotive. All Rights Reserved.

#include "ImpactGrid.h"

#include "ImpactBatchDefinition.h"
#include "ImpactorCatalog.h"
#include "Algo/BinarySearch.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactGrid)

bool FImpactGridCell::operator==(const FImpactGridCell& Other) const
{
	return Source == Other.Source && Archetype == Other.Archetype && Region == Other.Region && Azimuth == Other.Azimuth
		&& Elevation == Other.Elevation && Strength == Other.Strength && Phase == Other.Phase && Moment == Other.Moment;
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
		Out.bFlightMoments = Settings.bFlightMoments;
		return Out;
	}

	const EImpactSourceMoment FlightMoments[] = { EImpactSourceMoment::BeforeTakeOff, EImpactSourceMoment::Apex, EImpactSourceMoment::AfterLanding };
}

bool FImpactGrid::Build(const UImpactBatchDefinition& Batch, const UImpactorCatalog& Catalog, int32 NumSources, FText& OutReason)
{
	return Build(Batch, Catalog, NumSources, {}, OutReason);
}

bool FImpactGrid::Build(const UImpactBatchDefinition& Batch, const UImpactorCatalog& Catalog, int32 NumSources, TConstArrayView<bool> SourcePlaysOnce, FText& OutReason)
{
	Triples.Reset();
	Starts.Reset();
	Settings = Clamped(Batch.Grid);
	if (NumSources < 1)
	{
		OutReason = LOCTEXT("NoSources", "A full grid needs at least one source animation.");
		return false;
	}
	for (int32 Source = 0; Source < NumSources; ++Source)
	{
		const bool bMoments = Settings.bFlightMoments && SourcePlaysOnce.IsValidIndex(Source) && SourcePlaysOnce[Source];
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
					Triples.Add({ Source, Archetype, Region.Region, bMoments });
				}
			}
		}
	}
	int64 Cells = 0;
	for (const FTriple& Triple : Triples)
	{
		Starts.Add(Cells);
		Cells += int64(Settings.AzimuthSteps) * Settings.ElevationSteps * Settings.StrengthLevels * TimingSteps(Triple);
	}
	Starts.Add(Cells);
	if (Cells < 1)
	{
		Triples.Reset();
		Starts.Reset();
		OutReason = LOCTEXT("Empty", "No impactor in the catalog can strike the batch's regions from its elevations.");
		return false;
	}
	if (Cells > MAX_int32)
	{
		Triples.Reset();
		Starts.Reset();
		OutReason = FText::Format(LOCTEXT("TooBig", "The full grid has {0} cells, more than one batch can index; use fewer steps."), FText::AsNumber(Cells));
		return false;
	}
	return true;
}

int32 FImpactGrid::Num() const
{
	return Starts.IsEmpty() ? 0 : int32(Starts.Last());
}

int32 FImpactGrid::TimingSteps(const FTriple& Triple) const
{
	return Triple.bMoments ? UE_ARRAY_COUNT(FlightMoments) : Settings.PhasesPerSource;
}

FImpactGridCell FImpactGrid::Cell(int32 Index) const
{
	// Each triple owns a block: ((az * Elevation + el) * Strength + level) * Timing + timing, where
	// timing is a phase, or a flight moment for a play-once source. With no moments every block is
	// the same size and Index = ((((Triple * Azimuth) + az) * Elevation + el) * Strength + level) * Phase + phase.
	FImpactGridCell Cell;
	if (Triples.IsEmpty())
	{
		return Cell;
	}
	const int64 Clamped = FMath::Clamp(int64(Index), int64(0), Starts.Last() - 1);
	const int32 Triple = FMath::Clamp(Algo::UpperBound(Starts, Clamped) - 1, 0, Triples.Num() - 1);
	const int32 Timing = TimingSteps(Triples[Triple]);
	int64 Rest = Clamped - Starts[Triple];
	const int32 Step = int32(Rest % Timing);
	Rest /= Timing;
	Cell.Strength = int32(Rest % Settings.StrengthLevels);
	Rest /= Settings.StrengthLevels;
	Cell.Elevation = int32(Rest % Settings.ElevationSteps);
	Rest /= Settings.ElevationSteps;
	Cell.Azimuth = int32(Rest % Settings.AzimuthSteps);
	Cell.Source = Triples[Triple].Source;
	Cell.Archetype = Triples[Triple].Archetype;
	Cell.Region = Triples[Triple].Region;
	if (Triples[Triple].bMoments)
	{
		Cell.Moment = FlightMoments[Step];
	}
	else
	{
		Cell.Phase = Step;
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
	Params.SourceMoment = Cell.Moment;
	return Params;
}

#undef LOCTEXT_NAMESPACE
