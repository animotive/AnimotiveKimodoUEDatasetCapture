// Copyright Animotive. All Rights Reserved.

#include "ImpactParams.h"
#include "ImpactRandom.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"

namespace ImpactParamsTests
{
	constexpr float BodyMassKg = 75.0f;

	FImpactDistributions MakeDistributions()
	{
		FImpactDistributions Distributions;
		Distributions.Regions = { { EImpactBodyRegion::Chest, 1.0f }, { EImpactBodyRegion::FootLeft, 3.0f }, { EImpactBodyRegion::Head, 0.0f } };
		Distributions.ElevationDegrees = FFloatInterval(-20.0f, 40.0f);
		Distributions.AzimuthDegrees = FFloatInterval(135.0f, 225.0f);
		Distributions.SourcePhase = FFloatInterval(0.25f, 0.75f);
		return Distributions;
	}

	UImpactorCatalog* MakeStarterCatalog()
	{
		UImpactorCatalog* Catalog = NewObject<UImpactorCatalog>();
		Catalog->Archetypes = UImpactorCatalog::MakeStarterArchetypes();
		return Catalog;
	}

	const FImpactorArchetype& Find(const UImpactorCatalog& Catalog, const TCHAR* Id)
	{
		return *Catalog.Archetypes.FindByPredicate([Id](const FImpactorArchetype& A) { return A.Id == FName(Id); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactParamsIntensityTest,
	"KimodoDatasetCapture.Impact.Params.IntensityBuckets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactParamsIntensityTest::RunTest(const FString& Parameters)
{
	const FImpactIntensityThresholds Thresholds;
	TestEqual(TEXT("Zero is a tap"), Thresholds.Classify(0.0f), EImpactIntensity::Tap);
	TestEqual(TEXT("Just below Light is a tap"), Thresholds.Classify(Thresholds.Light - 1.0f), EImpactIntensity::Tap);
	// On a bound belongs to the higher bucket, at every bound.
	TestEqual(TEXT("On Light is light"), Thresholds.Classify(Thresholds.Light), EImpactIntensity::Light);
	TestEqual(TEXT("Just below Medium is light"), Thresholds.Classify(Thresholds.Medium - 1.0f), EImpactIntensity::Light);
	TestEqual(TEXT("On Medium is medium"), Thresholds.Classify(Thresholds.Medium), EImpactIntensity::Medium);
	TestEqual(TEXT("On Heavy is heavy"), Thresholds.Classify(Thresholds.Heavy), EImpactIntensity::Heavy);
	TestEqual(TEXT("Just below Knockdown is heavy"), Thresholds.Classify(Thresholds.Knockdown - 1.0f), EImpactIntensity::Heavy);
	TestEqual(TEXT("On Knockdown is knockdown"), Thresholds.Classify(Thresholds.Knockdown), EImpactIntensity::Knockdown);
	TestEqual(TEXT("On Extreme is extreme"), Thresholds.Classify(Thresholds.Extreme), EImpactIntensity::Extreme);
	TestEqual(TEXT("Far above Extreme is extreme"), Thresholds.Classify(1.0e9f), EImpactIntensity::Extreme);
	// A negative impulse is a caller bug; it must not land in a strong bucket.
	TestEqual(TEXT("Negative is a tap"), Thresholds.Classify(-100.0f), EImpactIntensity::Tap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactParamsTransferTest,
	"KimodoDatasetCapture.Impact.Params.MomentumTransfer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactParamsTransferTest::RunTest(const FString& Parameters)
{
	using namespace ImpactParamsTests;

	// Exact formula: reduced mass x speed x (1 + e), in kg x cm/s.
	const float Mu = (10.0f * BodyMassKg) / (10.0f + BodyMassKg);
	TestEqual(TEXT("Reduced-mass formula"), FImpactParamsSampler::TransferredImpulse(10.0f, 5.0f, BodyMassKg, 0.2f), Mu * 5.0f * 1.2f * 100.0f, 0.5f);

	// Light impactor: transfers about its own momentum.
	const float Bullet = FImpactParamsSampler::TransferredImpulse(0.008f, 350.0f, BodyMassKg, 0.0f);
	TestEqual(TEXT("A bullet transfers its own momentum"), Bullet, 0.008f * 350.0f * 100.0f, 1.0f);

	// Heavy impactor: transfers about the body's mass times its speed, not its own momentum.
	const float Car = FImpactParamsSampler::TransferredImpulse(1200.0f, 10.0f, BodyMassKg, 0.0f);
	TestTrue(TEXT("A car transfers no more than the body's mass times its speed"), Car <= BodyMassKg * 10.0f * 100.0f);
	TestTrue(TEXT("A car transfers most of the body's mass times its speed"), Car > 0.9f * BodyMassKg * 10.0f * 100.0f);

	// Restitution scales between plastic (1x) and elastic (2x).
	TestEqual(TEXT("Elastic doubles plastic"),
		FImpactParamsSampler::TransferredImpulse(5.0f, 4.0f, BodyMassKg, 1.0f),
		2.0f * FImpactParamsSampler::TransferredImpulse(5.0f, 4.0f, BodyMassKg, 0.0f), 0.5f);

	// Degenerate inputs transfer nothing rather than dividing by zero.
	TestEqual(TEXT("Zero speed transfers nothing"), FImpactParamsSampler::TransferredImpulse(5.0f, 0.0f, BodyMassKg, 0.2f), 0.0f);
	TestEqual(TEXT("Zero impactor mass transfers nothing"), FImpactParamsSampler::TransferredImpulse(0.0f, 5.0f, BodyMassKg, 0.2f), 0.0f);
	TestEqual(TEXT("Zero body mass transfers nothing"), FImpactParamsSampler::TransferredImpulse(5.0f, 5.0f, 0.0f, 0.2f), 0.0f);

	// Starter reaction gains: a bullet reacts far beyond its momentum; a car does not need to.
	UImpactorCatalog* Catalog = MakeStarterCatalog();
	TestTrue(TEXT("Bullet reaction gain amplifies"), Find(*Catalog, TEXT("SmallBullet")).ReactionGain > 10.0f);
	TestEqual(TEXT("Car reaction gain is physical"), Find(*Catalog, TEXT("CarBumper")).ReactionGain, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactParamsSamplingTest,
	"KimodoDatasetCapture.Impact.Params.Sampling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactParamsSamplingTest::RunTest(const FString& Parameters)
{
	using namespace ImpactParamsTests;
	UImpactorCatalog* Catalog = MakeStarterCatalog();
	const FImpactDistributions Distributions = MakeDistributions();

	constexpr int32 Clips = 4000;
	bool bAllSampled = true, bPhaseInRange = true, bDirectionValid = true, bImpulseConsistent = true, bIntensityConsistent = true;
	int32 Foot = 0, Head = 0;
	for (int32 Clip = 0; Clip < Clips; ++Clip)
	{
		FImpactParams Params;
		FText Reason;
		const bool bSampled = FImpactParamsSampler::Sample(*Catalog, Distributions, BodyMassKg, 77, Clip, Params, Reason);
		bAllSampled &= bSampled && Params.Impactor.Archetype != nullptr;
		if (!bSampled || !Params.Impactor.Archetype)
		{
			continue;
		}

		bPhaseInRange &= Params.SourcePhase >= Distributions.SourcePhase.Min && Params.SourcePhase <= Distributions.SourcePhase.Max;

		const FVector Direction = Params.DirectionLocal;
		const float Elevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(float(Direction.Z), -1.0f, 1.0f)));
		float Azimuth = FMath::RadiansToDegrees(FMath::Atan2(float(Direction.Y), float(Direction.X)));
		Azimuth = Azimuth < 0.0f ? Azimuth + 360.0f : Azimuth;
		bDirectionValid &= FMath::IsNearlyEqual(Direction.Size(), 1.0, 1e-4);
		bDirectionValid &= Elevation >= Distributions.ElevationDegrees.Min - 0.01f && Elevation <= Distributions.ElevationDegrees.Max + 0.01f;
		bDirectionValid &= Azimuth >= Distributions.AzimuthDegrees.Min - 0.01f && Azimuth <= Distributions.AzimuthDegrees.Max + 0.01f;

		const float Expected = FImpactParamsSampler::TransferredImpulse(Params.Impactor.MassKg, Params.Impactor.SpeedMetresPerSecond, BodyMassKg, Distributions.Restitution);
		bImpulseConsistent &= FMath::IsNearlyEqual(Params.TransferredImpulseKgCmPerSecond, Expected, FMath::Max(1.0f, Expected * 1e-4f));
		bImpulseConsistent &= Params.ReactionGain == Params.Impactor.Archetype->ReactionGain;
		bImpulseConsistent &= FMath::IsNearlyEqual(Params.ReactionImpulseKgCmPerSecond, Params.TransferredImpulseKgCmPerSecond * Params.ReactionGain, FMath::Max(1.0f, Params.ReactionImpulseKgCmPerSecond * 1e-4f));
		bIntensityConsistent &= Params.Intensity == Distributions.Intensity.Classify(Params.ReactionImpulseKgCmPerSecond);

		Foot += Params.Region == EImpactBodyRegion::FootLeft ? 1 : 0;
		Head += Params.Region == EImpactBodyRegion::Head ? 1 : 0;
	}
	TestTrue(TEXT("Every clip samples"), bAllSampled);
	TestTrue(TEXT("Source phase within its range"), bPhaseInRange);
	TestTrue(TEXT("Direction is unit and within elevation and azimuth ranges"), bDirectionValid);
	TestTrue(TEXT("Transferred and reaction impulse consistent with the impactor"), bImpulseConsistent);
	TestTrue(TEXT("Intensity derived from the reaction impulse"), bIntensityConsistent);
	TestEqual(TEXT("Zero-weight region never hit"), Head, 0);
	TestEqual(TEXT("Region weights respected (3 of 4)"), double(Foot) / Clips, 0.75, 0.03);

	// Area-uniform elevation: within -20..40 degrees, the share above 0 is
	// (sin 40 - sin 0) / (sin 40 - sin -20) = 0.6428 / 0.9848.
	FImpactParams Params;
	FText Reason;
	int32 Upward = 0;
	for (int32 Clip = 0; Clip < Clips; ++Clip)
	{
		FImpactParamsSampler::Sample(*Catalog, Distributions, BodyMassKg, 78, Clip, Params, Reason);
		Upward += Params.DirectionLocal.Z > 0.0 ? 1 : 0;
	}
	TestEqual(TEXT("Directions are area-uniform in elevation"), double(Upward) / Clips, 0.6428 / 0.9848, 0.03);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactParamsDeterminismTest,
	"KimodoDatasetCapture.Impact.Params.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactParamsDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace ImpactParamsTests;
	UImpactorCatalog* Catalog = MakeStarterCatalog();
	const FImpactDistributions Distributions = MakeDistributions();

	FImpactParams A, B;
	FText Reason;
	FImpactParamsSampler::Sample(*Catalog, Distributions, BodyMassKg, 5, 17, A, Reason);
	FImpactParamsSampler::Sample(*Catalog, Distributions, BodyMassKg, 5, 17, B, Reason);
	TestTrue(TEXT("Same clip, same impactor"), A.Impactor.Archetype == B.Impactor.Archetype && A.Impactor.MassKg == B.Impactor.MassKg);
	TestTrue(TEXT("Same clip, same direction"), A.DirectionLocal == B.DirectionLocal);
	TestTrue(TEXT("Same clip, same region and phase"), A.Region == B.Region && A.SourcePhase == B.SourcePhase);

	// Changing the catalog's weights changes which impactor is drawn but must not move the
	// direction, region or timing: each family has its own stream.
	UImpactorCatalog* Reweighted = MakeStarterCatalog();
	for (FImpactorArchetype& Archetype : Reweighted->Archetypes)
	{
		Archetype.Weight = Archetype.Id == FName(TEXT("CarBumper")) ? 50.0f : 1.0f;
	}
	FImpactParams C;
	FImpactParamsSampler::Sample(*Reweighted, Distributions, BodyMassKg, 5, 17, C, Reason);
	TestTrue(TEXT("Direction independent of impactor weights"), A.DirectionLocal == C.DirectionLocal);
	TestTrue(TEXT("Region and phase independent of impactor weights"), A.Region == C.Region && A.SourcePhase == C.SourcePhase);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactParamsInvalidTest,
	"KimodoDatasetCapture.Impact.Params.InvalidInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactParamsInvalidTest::RunTest(const FString& Parameters)
{
	using namespace ImpactParamsTests;
	UImpactorCatalog* Catalog = MakeStarterCatalog();
	auto Rejects = [this, Catalog](const TCHAR* What, const UImpactorCatalog& UseCatalog, const FImpactDistributions& Distributions, float Mass)
	{
		FImpactParams Params;
		FText Reason;
		TestFalse(*FString::Printf(TEXT("%s is rejected"), What), FImpactParamsSampler::Sample(UseCatalog, Distributions, Mass, 1, 0, Params, Reason));
		TestFalse(*FString::Printf(TEXT("%s states a reason"), What), Reason.IsEmpty());
	};

	Rejects(TEXT("Empty catalog"), *NewObject<UImpactorCatalog>(), MakeDistributions(), BodyMassKg);
	Rejects(TEXT("Zero character mass"), *Catalog, MakeDistributions(), 0.0f);

	FImpactDistributions NoRegions = MakeDistributions();
	NoRegions.Regions.Empty();
	Rejects(TEXT("No regions"), *Catalog, NoRegions, BodyMassKg);

	FImpactDistributions ZeroRegions = MakeDistributions();
	for (FImpactRegionWeight& Region : ZeroRegions.Regions)
	{
		Region.Weight = 0.0f;
	}
	Rejects(TEXT("All region weights zero"), *Catalog, ZeroRegions, BodyMassKg);

	FImpactDistributions BadElevation = MakeDistributions();
	BadElevation.ElevationDegrees = FFloatInterval(30.0f, -30.0f);
	Rejects(TEXT("Inverted elevation"), *Catalog, BadElevation, BodyMassKg);

	FImpactDistributions OutOfRangeElevation = MakeDistributions();
	OutOfRangeElevation.ElevationDegrees = FFloatInterval(-120.0f, 30.0f);
	Rejects(TEXT("Elevation beyond the poles"), *Catalog, OutOfRangeElevation, BodyMassKg);

	FImpactDistributions BadPhase = MakeDistributions();
	BadPhase.SourcePhase = FFloatInterval(0.5f, 1.5f);
	Rejects(TEXT("Phase outside 0..1"), *Catalog, BadPhase, BodyMassKg);
	return true;
}
