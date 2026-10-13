// Copyright Animotive. All Rights Reserved.

#include "ImpactDistributionReport.h"

#include "ImpactBatchManifest.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactDistributionReport)

#define LOCTEXT_NAMESPACE "ImpactDistributionReport"

namespace
{
	/** "Name count (share%), ..." largest first, ties by name, shares of Of. */
	FString Line(const TCHAR* Heading, const TMap<FString, int32>& Counts, int32 Of)
	{
		TArray<TPair<FString, int32>> Sorted = Counts.Array();
		Sorted.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B)
		{
			return A.Value != B.Value ? A.Value > B.Value : A.Key < B.Key;
		});
		TArray<FString> Parts;
		for (const TPair<FString, int32>& Pair : Sorted)
		{
			Parts.Add(Of > 0
				? FString::Printf(TEXT("%s %d (%d%%)"), *Pair.Key, Pair.Value, FMath::RoundToInt(100.0 * Pair.Value / Of))
				: FString::Printf(TEXT("%s %d"), *Pair.Key, Pair.Value));
		}
		return FString::Printf(TEXT("  %s: %s"), Heading, Parts.IsEmpty() ? TEXT("none") : *FString::Join(Parts, TEXT(", ")));
	}
}

FImpactDistributionReport FImpactDistributionReport::Build(const FImpactBatchManifest& Manifest)
{
	FImpactDistributionReport Report;
	int64 Frames = 0;
	for (const FImpactManifestEntry& Entry : Manifest.GetEntries())
	{
		++Report.Total;
		Report.Bytes += Entry.Bytes;
		if (!Entry.bValid)
		{
			++Report.Invalid;
			++Report.InvalidReasons.FindOrAdd(Entry.InvalidReason);
			continue;
		}
		++Report.Valid;
		Frames += Entry.Frames;
		++Report.Regions.FindOrAdd(Entry.Region);
		++Report.Intensities.FindOrAdd(Entry.Intensity);
		++Report.DeliveryModes.FindOrAdd(Entry.DeliveryMode);
		++Report.Outcomes.FindOrAdd(Entry.Outcome);
		++Report.SourceAnimations.FindOrAdd(Entry.SourceAnimation);
		++Report.Impactors.FindOrAdd(Entry.ImpactorId);
	}
	Report.MeanFrames = Report.Valid > 0 ? float(double(Frames) / Report.Valid) : 0.0f;
	return Report;
}

FString FImpactDistributionReport::ToJson() const
{
	FString Json;
	FJsonObjectConverter::UStructToJsonObjectString(*this, Json);
	return Json;
}

bool FImpactDistributionReport::Save(const FString& Filename, FText& OutReason) const
{
	if (!FFileHelper::SaveStringToFile(ToJson(), *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutReason = FText::Format(LOCTEXT("SaveFailed", "Could not write the distribution report {0}."), FText::FromString(Filename));
		return false;
	}
	return true;
}

FString FImpactDistributionReport::ToText() const
{
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Impact batch: %d clips, %d valid, %d invalid, mean %.0f frames, %.1f MB"),
		Total, Valid, Invalid, MeanFrames, double(Bytes) / (1024.0 * 1024.0)));
	Lines.Add(Line(TEXT("Region"), Regions, Valid));
	Lines.Add(Line(TEXT("Intensity"), Intensities, Valid));
	Lines.Add(Line(TEXT("Mode"), DeliveryModes, Valid));
	Lines.Add(Line(TEXT("Outcome"), Outcomes, Valid));
	Lines.Add(Line(TEXT("Source"), SourceAnimations, Valid));
	Lines.Add(Line(TEXT("Impactor"), Impactors, Valid));
	if (Invalid > 0)
	{
		Lines.Add(Line(TEXT("Invalid"), InvalidReasons, 0));
	}
	return FString::Join(Lines, TEXT("\n"));
}

#undef LOCTEXT_NAMESPACE
