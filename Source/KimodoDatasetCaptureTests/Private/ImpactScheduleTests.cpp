// Copyright Animotive. All Rights Reserved.

#include "ImpactParams.h"
#include "ImpactSchedule.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"

namespace ImpactScheduleTests
{
	/** A fixed impact, so delivery is tested apart from impactor sampling. */
	struct FFixture
	{
		FImpactorArchetype Archetype;
		FImpactParams Params;

		explicit FFixture(EImpactDeliveryMode DefaultDelivery, float ReactionGain = 1.0f)
		{
			Archetype.Id = TEXT("Test");
			Archetype.Shape = EImpactorShape::Capsule;
			Archetype.SizeCm = 12.0f;
			Archetype.DefaultDelivery = DefaultDelivery;
			Archetype.ReactionGain = ReactionGain;
			Params.Impactor.Archetype = &Archetype;
			Params.Impactor.MassKg = 3.0f;
			Params.Impactor.SpeedMetresPerSecond = 20.0f;
			Params.TransferredImpulseKgCmPerSecond = 6000.0f;
			Params.ReactionGain = ReactionGain;
			Params.ReactionImpulseKgCmPerSecond = 6000.0f * ReactionGain;
			Params.DirectionLocal = FVector(-1.0, 0.0, 0.0);
		}
	};

	FImpactSchedule Sample(const FFixture& Fixture, const FImpactDeliveryDistributions& Distributions, int32 Clip = 0)
	{
		FImpactSchedule Schedule;
		FText Reason;
		FImpactScheduleSampler::Sample(Fixture.Params, Distributions, 3, Clip, Schedule, Reason);
		return Schedule;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactScheduleInstantSustainedTest,
	"KimodoDatasetCapture.Impact.Schedule.InstantAndSustained",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactScheduleInstantSustainedTest::RunTest(const FString& Parameters)
{
	using namespace ImpactScheduleTests;
	const FImpactDeliveryDistributions Distributions;

	const FFixture Instant(EImpactDeliveryMode::Instant);
	const FImpactSchedule A = Sample(Instant, Distributions);
	TestEqual(TEXT("Instant uses the archetype default"), A.Mode, EImpactDeliveryMode::Instant);
	TestEqual(TEXT("Instant is one hit"), A.Hits.Num(), 1);
	if (A.Hits.Num() == 1)
	{
		TestEqual(TEXT("Instant hit is at frame zero"), A.Hits[0].FrameOffset, 0);
		TestEqual(TEXT("Instant hit has no duration"), A.Hits[0].DurationFrames, 0);
		TestEqual(TEXT("Instant hit carries the full reaction impulse"), A.Hits[0].ImpulseKgCmPerSecond, Instant.Params.ReactionImpulseKgCmPerSecond, 0.01f);
		TestTrue(TEXT("Instant hit keeps the impact's direction"), A.Hits[0].DirectionLocal.Equals(Instant.Params.DirectionLocal, 1e-6));
	}
	TestFalse(TEXT("Instant spawns no projectile"), A.Projectile.IsSet());

	const FFixture Sustained(EImpactDeliveryMode::Sustained);
	bool bInRange = true;
	for (int32 Clip = 0; Clip < 200; ++Clip)
	{
		const FImpactSchedule S = Sample(Sustained, Distributions, Clip);
		bInRange &= S.Mode == EImpactDeliveryMode::Sustained && S.Hits.Num() == 1;
		if (S.Hits.Num() == 1)
		{
			bInRange &= S.Hits[0].FrameOffset == 0;
			bInRange &= S.Hits[0].DurationFrames >= Distributions.SustainedFrames.Min && S.Hits[0].DurationFrames <= Distributions.SustainedFrames.Max;
			bInRange &= FMath::IsNearlyEqual(S.TotalScriptedImpulse(), Sustained.Params.ReactionImpulseKgCmPerSecond, 0.5f);
		}
	}
	TestTrue(TEXT("Sustained is one hit, duration in range, full impulse"), bInRange);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactScheduleBurstTest,
	"KimodoDatasetCapture.Impact.Schedule.Burst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactScheduleBurstTest::RunTest(const FString& Parameters)
{
	using namespace ImpactScheduleTests;
	const FImpactDeliveryDistributions Distributions;
	const FFixture Burst(EImpactDeliveryMode::Burst);

	bool bCount = true, bOrdered = true, bSpacing = true, bSum = true, bDirections = true;
	bool bSawMinCount = false, bSawMaxCount = false;
	const float MaxAngle = FMath::DegreesToRadians(Distributions.BurstDirectionJitterDegrees) + 1e-4f;
	for (int32 Clip = 0; Clip < 400; ++Clip)
	{
		const FImpactSchedule S = Sample(Burst, Distributions, Clip);
		const int32 Count = S.Hits.Num();
		bCount &= S.Mode == EImpactDeliveryMode::Burst && Count >= Distributions.BurstHits.Min && Count <= Distributions.BurstHits.Max;
		bSawMinCount |= Count == Distributions.BurstHits.Min;
		bSawMaxCount |= Count == Distributions.BurstHits.Max;
		if (Count == 0)
		{
			continue;
		}
		bOrdered &= S.Hits[0].FrameOffset == 0;
		for (int32 Index = 1; Index < Count; ++Index)
		{
			const int32 Gap = S.Hits[Index].FrameOffset - S.Hits[Index - 1].FrameOffset;
			bOrdered &= Gap > 0;
			bSpacing &= Gap >= Distributions.BurstSpacingFrames.Min && Gap <= Distributions.BurstSpacingFrames.Max;
		}
		for (const FImpactHit& Hit : S.Hits)
		{
			bDirections &= FMath::IsNearlyEqual(Hit.DirectionLocal.Size(), 1.0, 1e-4);
			bDirections &= FMath::Acos(FMath::Clamp(float(FVector::DotProduct(Hit.DirectionLocal, Burst.Params.DirectionLocal)), -1.0f, 1.0f)) <= MaxAngle;
			bDirections &= Hit.DurationFrames == 0;
		}
		// Splitting must not add or lose momentum.
		bSum &= FMath::IsNearlyEqual(S.TotalScriptedImpulse(), Burst.Params.ReactionImpulseKgCmPerSecond, 0.5f);
	}
	TestTrue(TEXT("Burst hit count within range"), bCount);
	TestTrue(TEXT("Burst reaches its smallest count"), bSawMinCount);
	TestTrue(TEXT("Burst reaches its largest count"), bSawMaxCount);
	TestTrue(TEXT("Burst hits ordered from frame zero"), bOrdered);
	TestTrue(TEXT("Burst spacing within range"), bSpacing);
	TestTrue(TEXT("Burst impulses sum to the reaction impulse"), bSum);
	TestTrue(TEXT("Burst directions within the jitter cone"), bDirections);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactScheduleProjectileTest,
	"KimodoDatasetCapture.Impact.Schedule.Projectile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactScheduleProjectileTest::RunTest(const FString& Parameters)
{
	using namespace ImpactScheduleTests;
	const FImpactDeliveryDistributions Distributions;

	const FFixture Projectile(EImpactDeliveryMode::Projectile, 4.0f);
	const FImpactSchedule S = Sample(Projectile, Distributions);
	TestEqual(TEXT("Projectile mode"), S.Mode, EImpactDeliveryMode::Projectile);
	TestEqual(TEXT("Projectile has no scripted hits: the collision delivers it"), S.Hits.Num(), 0);
	TestTrue(TEXT("Projectile spec present"), S.Projectile.IsSet());
	if (S.Projectile.IsSet())
	{
		const FImpactProjectileSpec& Spec = S.Projectile.GetValue();
		TestEqual(TEXT("Shape from the archetype"), Spec.Shape, EImpactorShape::Capsule);
		TestEqual(TEXT("Size from the archetype"), Spec.SizeCm, 12.0f);
		TestEqual(TEXT("Mass from the sample"), Spec.MassKg, 3.0f);
		TestEqual(TEXT("Speed from the sample"), Spec.SpeedMetresPerSecond, 20.0f);
		TestTrue(TEXT("Launch distance in range"),
			Spec.LaunchDistanceCm >= Distributions.ProjectileLaunchDistanceCm.Min && Spec.LaunchDistanceCm <= Distributions.ProjectileLaunchDistanceCm.Max);
		TestEqual(TEXT("Extra reaction is reaction minus transfer"), Spec.ExtraReactionImpulseKgCmPerSecond,
			Projectile.Params.ReactionImpulseKgCmPerSecond - Projectile.Params.TransferredImpulseKgCmPerSecond, 0.5f);
	}

	const FFixture Physical(EImpactDeliveryMode::Projectile, 1.0f);
	const FImpactSchedule P = Sample(Physical, Distributions);
	TestTrue(TEXT("Gain of one adds no extra reaction"), P.Projectile.IsSet() && P.Projectile->ExtraReactionImpulseKgCmPerSecond == 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactScheduleModeOverrideTest,
	"KimodoDatasetCapture.Impact.Schedule.ModeOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactScheduleModeOverrideTest::RunTest(const FString& Parameters)
{
	using namespace ImpactScheduleTests;
	FImpactDeliveryDistributions Distributions;
	Distributions.ModeOverride = { { EImpactDeliveryMode::Burst, 1.0f }, { EImpactDeliveryMode::Sustained, 1.0f }, { EImpactDeliveryMode::Projectile, 0.0f } };

	const FFixture Fixture(EImpactDeliveryMode::Instant);
	constexpr int32 Clips = 4000;
	int32 Burst = 0, Projectile = 0, Instant = 0;
	for (int32 Clip = 0; Clip < Clips; ++Clip)
	{
		const FImpactSchedule S = Sample(Fixture, Distributions, Clip);
		Burst += S.Mode == EImpactDeliveryMode::Burst ? 1 : 0;
		Projectile += S.Mode == EImpactDeliveryMode::Projectile ? 1 : 0;
		Instant += S.Mode == EImpactDeliveryMode::Instant ? 1 : 0;
	}
	TestEqual(TEXT("Override replaces the archetype default"), Instant, 0);
	TestEqual(TEXT("Zero-weight mode never chosen"), Projectile, 0);
	TestEqual(TEXT("Override weights respected (1 of 2)"), double(Burst) / Clips, 0.5, 0.03);

	// Deterministic per clip.
	const FImpactSchedule First = Sample(Fixture, Distributions, 9);
	const FImpactSchedule Again = Sample(Fixture, Distributions, 9);
	TestTrue(TEXT("Same clip, same schedule"), First.Mode == Again.Mode && First.Hits.Num() == Again.Hits.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactScheduleInvalidTest,
	"KimodoDatasetCapture.Impact.Schedule.InvalidInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactScheduleInvalidTest::RunTest(const FString& Parameters)
{
	using namespace ImpactScheduleTests;
	const FFixture Fixture(EImpactDeliveryMode::Burst);
	auto Rejects = [this, &Fixture](const TCHAR* What, const FImpactDeliveryDistributions& Distributions)
	{
		FImpactSchedule Schedule;
		FText Reason;
		TestFalse(*FString::Printf(TEXT("%s is rejected"), What), FImpactScheduleSampler::Sample(Fixture.Params, Distributions, 1, 0, Schedule, Reason));
		TestFalse(*FString::Printf(TEXT("%s states a reason"), What), Reason.IsEmpty());
	};

	FImpactDeliveryDistributions OneHitBurst;
	OneHitBurst.BurstHits = FInt32Interval(1, 3);
	Rejects(TEXT("Burst of fewer than two hits"), OneHitBurst);

	FImpactDeliveryDistributions ZeroSpacing;
	ZeroSpacing.BurstSpacingFrames = FInt32Interval(0, 4);
	Rejects(TEXT("Burst spacing below one frame"), ZeroSpacing);

	FImpactDeliveryDistributions ZeroSustained;
	ZeroSustained.SustainedFrames = FInt32Interval(0, 10);
	Rejects(TEXT("Sustained duration below one frame"), ZeroSustained);

	FImpactDeliveryDistributions InvertedBurst;
	InvertedBurst.BurstHits = FInt32Interval(5, 2);
	Rejects(TEXT("Inverted burst count"), InvertedBurst);

	FImpactDeliveryDistributions NoLaunch;
	NoLaunch.ProjectileLaunchDistanceCm = FFloatInterval(0.0f, 100.0f);
	Rejects(TEXT("Non-positive launch distance"), NoLaunch);

	FImpactDeliveryDistributions ZeroOverride;
	ZeroOverride.ModeOverride = { { EImpactDeliveryMode::Burst, 0.0f } };
	Rejects(TEXT("Override with every weight zero"), ZeroOverride);

	FImpactParams NoImpactor = Fixture.Params;
	NoImpactor.Impactor.Archetype = nullptr;
	FImpactSchedule Schedule;
	FText Reason;
	TestFalse(TEXT("Params without an impactor are rejected"), FImpactScheduleSampler::Sample(NoImpactor, FImpactDeliveryDistributions(), 1, 0, Schedule, Reason));
	TestFalse(TEXT("Params without an impactor state a reason"), Reason.IsEmpty());
	return true;
}
