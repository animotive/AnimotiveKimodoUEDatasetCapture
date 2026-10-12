// Copyright Animotive. All Rights Reserved.

#include "ImpactParams.h"

#include "ImpactRandom.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactParams)

#define LOCTEXT_NAMESPACE "ImpactParams"

EImpactIntensity FImpactIntensityThresholds::Classify(float ReactionImpulseKgCmPerSecond) const
{
	const float Impulse = ReactionImpulseKgCmPerSecond;
	if (Impulse >= Extreme)
	{
		return EImpactIntensity::Extreme;
	}
	if (Impulse >= Knockdown)
	{
		return EImpactIntensity::Knockdown;
	}
	if (Impulse >= Heavy)
	{
		return EImpactIntensity::Heavy;
	}
	if (Impulse >= Medium)
	{
		return EImpactIntensity::Medium;
	}
	if (Impulse >= Light)
	{
		return EImpactIntensity::Light;
	}
	return EImpactIntensity::Tap;
}

float FImpactParamsSampler::TransferredImpulse(float ImpactorMassKg, float SpeedMetresPerSecond, float BodyMassKg, float Restitution)
{
	if (ImpactorMassKg <= 0.0f || SpeedMetresPerSecond <= 0.0f || BodyMassKg <= 0.0f)
	{
		return 0.0f;
	}
	const float ReducedMass = (ImpactorMassKg * BodyMassKg) / (ImpactorMassKg + BodyMassKg);
	return ReducedMass * SpeedMetresPerSecond * (1.0f + FMath::Clamp(Restitution, 0.0f, 1.0f)) * 100.0f;
}

namespace
{
	bool ValidateDistributions(const FImpactDistributions& Distributions, FText& OutReason)
	{
		float RegionWeight = 0.0f;
		for (const FImpactRegionWeight& Region : Distributions.Regions)
		{
			if (Region.Weight < 0.0f)
			{
				OutReason = LOCTEXT("NegativeRegionWeight", "A body region has a negative weight.");
				return false;
			}
			RegionWeight += Region.Weight;
		}
		if (Distributions.Regions.IsEmpty() || RegionWeight <= 0.0f)
		{
			OutReason = LOCTEXT("NoRegions", "No body region can be hit. Give at least one region a positive weight.");
			return false;
		}

		const FFloatInterval& Elevation = Distributions.ElevationDegrees;
		if (Elevation.Min > Elevation.Max || Elevation.Min < -90.0f || Elevation.Max > 90.0f)
		{
			OutReason = LOCTEXT("BadElevation", "Elevation must lie within -90 to 90 degrees with min no greater than max.");
			return false;
		}
		if (Distributions.AzimuthDegrees.Min > Distributions.AzimuthDegrees.Max)
		{
			OutReason = LOCTEXT("BadAzimuth", "Azimuth min must be no greater than max.");
			return false;
		}
		const FFloatInterval& Phase = Distributions.SourcePhase;
		if (Phase.Min > Phase.Max || Phase.Min < 0.0f || Phase.Max > 1.0f)
		{
			OutReason = LOCTEXT("BadPhase", "Source phase must lie within 0 to 1 with min no greater than max.");
			return false;
		}
		return true;
	}

	EImpactBodyRegion SampleRegion(const TArray<FImpactRegionWeight>& Regions, FImpactRandom& Random)
	{
		double Total = 0.0;
		for (const FImpactRegionWeight& Region : Regions)
		{
			Total += Region.Weight;
		}
		const double Pick = Random.Range(0.0, Total);
		double Cumulative = 0.0;
		EImpactBodyRegion Last = Regions[0].Region;
		for (const FImpactRegionWeight& Region : Regions)
		{
			if (Region.Weight <= 0.0f)
			{
				continue;
			}
			Last = Region.Region;
			Cumulative += Region.Weight;
			if (Pick < Cumulative)
			{
				return Region.Region;
			}
		}
		return Last;
	}

	FVector SampleDirection(const FImpactDistributions& Distributions, FImpactRandom& Random)
	{
		// Uniform height between the sines of the elevation bounds is area-uniform on that band.
		const double Z = Random.Range(
			FMath::Sin(FMath::DegreesToRadians(double(Distributions.ElevationDegrees.Min))),
			FMath::Sin(FMath::DegreesToRadians(double(Distributions.ElevationDegrees.Max))));
		const double Azimuth = FMath::DegreesToRadians(Random.Range(Distributions.AzimuthDegrees.Min, Distributions.AzimuthDegrees.Max));
		const double Radius = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
		return FVector(Radius * FMath::Cos(Azimuth), Radius * FMath::Sin(Azimuth), Z);
	}
}

bool FImpactParamsSampler::Sample(
	const UImpactorCatalog& Catalog, const FImpactDistributions& Distributions, float CharacterMassKg,
	int32 BatchSeed, int32 ClipIndex, FImpactParams& OutParams, FText& OutReason)
{
	if (!Catalog.Validate(OutReason))
	{
		return false;
	}
	if (!(CharacterMassKg > 0.0f))
	{
		OutReason = LOCTEXT("NoMass", "The character's mass must be greater than zero.");
		return false;
	}
	if (!ValidateDistributions(Distributions, OutReason))
	{
		return false;
	}

	FImpactParams Params;

	// Each family draws from its own stream: changing one never moves another.
	FImpactRandom ImpactorRandom(BatchSeed, ClipIndex, EImpactRandomStream::Impactor);
	const FImpactorArchetype* Archetype = Catalog.SampleArchetype(ImpactorRandom);
	check(Archetype); // Validate() guarantees a positive weight.
	Params.Impactor = UImpactorCatalog::SampleInstance(*Archetype, ImpactorRandom);
	Params.TransferredImpulseKgCmPerSecond = TransferredImpulse(
		Params.Impactor.MassKg, Params.Impactor.SpeedMetresPerSecond, CharacterMassKg, Distributions.Restitution);
	Params.ReactionGain = Archetype->ReactionGain;
	Params.ReactionImpulseKgCmPerSecond = Params.TransferredImpulseKgCmPerSecond * Params.ReactionGain;
	Params.Intensity = Distributions.Intensity.Classify(Params.ReactionImpulseKgCmPerSecond);

	FImpactRandom RegionRandom(BatchSeed, ClipIndex, EImpactRandomStream::BodyRegion);
	Params.Region = SampleRegion(Distributions.Regions, RegionRandom);

	FImpactRandom DirectionRandom(BatchSeed, ClipIndex, EImpactRandomStream::Direction);
	Params.DirectionLocal = SampleDirection(Distributions, DirectionRandom);

	FImpactRandom TimingRandom(BatchSeed, ClipIndex, EImpactRandomStream::Timing);
	Params.SourcePhase = float(TimingRandom.Range(Distributions.SourcePhase.Min, Distributions.SourcePhase.Max));

	OutParams = Params;
	return true;
}

#undef LOCTEXT_NAMESPACE
