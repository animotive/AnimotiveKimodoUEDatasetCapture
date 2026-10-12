// Copyright Animotive. All Rights Reserved.

#include "ImpactClipId.h"
#include "ImpactParams.h"
#include "ImpactSchedule.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

/**
 * Writes 500 sampled clips from the starter catalog to Saved/KimodoImpactCapture/ImpactSampleBatch.csv,
 * so a person can read what a batch actually draws before any simulation runs. Same convention as
 * the sidecar schema sample: the documented behaviour cannot drift from the code.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactSampleBatchGen,
	"KimodoDatasetCapture.Impact.SampleBatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactSampleBatchGen::RunTest(const FString& Parameters)
{
	UImpactorCatalog* Catalog = NewObject<UImpactorCatalog>();
	Catalog->Archetypes = UImpactorCatalog::MakeStarterArchetypes();

	FImpactDistributions Impact;
	const UEnum* RegionEnum = StaticEnum<EImpactBodyRegion>();
	for (int32 Index = 0; Index < RegionEnum->NumEnums() - 1; ++Index)
	{
		Impact.Regions.Add({ EImpactBodyRegion(RegionEnum->GetValueByIndex(Index)), 1.0f });
	}
	const FImpactDeliveryDistributions Delivery;
	constexpr float CharacterMassKg = 75.0f;
	constexpr int32 Clips = 500;

	TArray<FString> Rows = { TEXT("clip,id,impactor,noun,mass_kg,speed_mps,transfer_kgcms,gain,reaction_kgcms,intensity,region,azimuth_deg,elevation_deg,phase,mode,hits") };
	TSet<FString> Ids;
	for (int32 Clip = 0; Clip < Clips; ++Clip)
	{
		FImpactParams Params;
		FImpactSchedule Schedule;
		FText Reason;
		if (!FImpactParamsSampler::Sample(*Catalog, Impact, CharacterMassKg, 1, Clip, Params, Reason)
			|| !FImpactScheduleSampler::Sample(Params, Delivery, 1, Clip, Schedule, Reason))
		{
			AddError(FString::Printf(TEXT("Clip %d failed to sample: %s"), Clip, *Reason.ToString()));
			continue;
		}
		const FString Id = FImpactClipId::Make(TEXT("/Game/SampleSource"), Params, Schedule);
		Ids.Add(Id);
		const FVector D = Params.DirectionLocal;
		float Azimuth = FMath::RadiansToDegrees(FMath::Atan2(float(D.Y), float(D.X)));
		Azimuth = Azimuth < 0.0f ? Azimuth + 360.0f : Azimuth;
		Rows.Add(FString::Printf(TEXT("%d,%s,%s,\"%s\",%.4f,%.2f,%.0f,%.1f,%.0f,%s,%s,%.1f,%.1f,%.3f,%s,%d"),
			Clip, *Id, *Params.Impactor.Archetype->Id.ToString(), *Params.Impactor.Archetype->NounPhrase,
			Params.Impactor.MassKg, Params.Impactor.SpeedMetresPerSecond, Params.TransferredImpulseKgCmPerSecond,
			Params.ReactionGain, Params.ReactionImpulseKgCmPerSecond,
			*StaticEnum<EImpactIntensity>()->GetNameStringByValue(int64(Params.Intensity)),
			*RegionEnum->GetNameStringByValue(int64(Params.Region)),
			Azimuth, FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(float(D.Z), -1.0f, 1.0f))), Params.SourcePhase,
			*StaticEnum<EImpactDeliveryMode>()->GetNameStringByValue(int64(Schedule.Mode)),
			Schedule.Projectile.IsSet() ? 0 : Schedule.Hits.Num()));
	}
	TestEqual(TEXT("Every sampled clip has a distinct id"), Ids.Num(), Clips);

	const FString File = FPaths::ProjectSavedDir() / TEXT("KimodoImpactCapture") / TEXT("ImpactSampleBatch.csv");
	TestTrue(TEXT("Sample batch written"), FFileHelper::SaveStringToFile(FString::Join(Rows, TEXT("\n")), *File));
	AddInfo(FString::Printf(TEXT("Sample batch: %s"), *FPaths::ConvertRelativePathToFull(File)));
	return true;
}
