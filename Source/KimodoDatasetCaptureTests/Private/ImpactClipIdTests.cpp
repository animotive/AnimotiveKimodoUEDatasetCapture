// Copyright Animotive. All Rights Reserved.

#include "ImpactClipId.h"
#include "ImpactParams.h"
#include "ImpactSchedule.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"

namespace ImpactClipIdTests
{
	struct FFixture
	{
		FImpactorArchetype Archetype;
		FImpactorArchetype OtherArchetype;
		FImpactParams Params;
		FImpactSchedule Schedule;
		FName Source = TEXT("/Game/Anim/M_Neutral_Run_Loop_F");

		FFixture()
		{
			Archetype.Id = TEXT("Baseball");
			OtherArchetype.Id = TEXT("ThrownStone");
			Params.Impactor.Archetype = &Archetype;
			Params.Impactor.MassKg = 0.145f;
			Params.Impactor.SpeedMetresPerSecond = 33.0f;
			Params.Impactor.ContactRadiusCm = 3.5f;
			Params.TransferredImpulseKgCmPerSecond = 570.0f;
			Params.ReactionGain = 4.0f;
			Params.ReactionImpulseKgCmPerSecond = 2280.0f;
			Params.Region = EImpactBodyRegion::Chest;
			Params.DirectionLocal = FVector(-1.0, 0.0, 0.0);
			Params.SourcePhase = 0.4f;
			Params.Intensity = EImpactIntensity::Tap;
			Schedule.Mode = EImpactDeliveryMode::Burst;
			Schedule.Hits = { { 0, 0, 1140.0f, FVector(-1, 0, 0) }, { 4, 0, 1140.0f, FVector(-1, 0.05, 0).GetSafeNormal() } };
		}

		FString Id() const { return FImpactClipId::Make(Source, Params, Schedule); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactClipIdHashTest,
	"KimodoDatasetCapture.Impact.ClipId.Hash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactClipIdHashTest::RunTest(const FString& Parameters)
{
	// Reference FNV-1a 64 values: the empty input is the offset basis.
	const uint8 Abc[] = { 'a', 'b', 'c' };
	TestEqual(TEXT("FNV-1a 64 of \"abc\""), FImpactClipId::Fnv1a64(Abc), 0xe71fa2190541574bull);
	TestEqual(TEXT("FNV-1a 64 of nothing is the offset basis"), FImpactClipId::Fnv1a64({}), 0xcbf29ce484222325ull);

	using namespace ImpactClipIdTests;
	const FFixture Fixture;
	const FString Id = Fixture.Id();
	TestTrue(TEXT("Id starts with imp_"), Id.StartsWith(TEXT("imp_"), ESearchCase::CaseSensitive));
	TestEqual(TEXT("Id is imp_ plus 16 digits"), Id.Len(), 20);
	bool bPathSafe = true;
	for (const TCHAR Char : Id.Mid(4))
	{
		bPathSafe &= (Char >= '0' && Char <= '9') || (Char >= 'a' && Char <= 'f');
	}
	TestTrue(TEXT("Id digits are lowercase hex"), bPathSafe);
	TestTrue(TEXT("Canonical string is versioned"), FImpactClipId::CanonicalString(Fixture.Source, Fixture.Params, Fixture.Schedule).StartsWith(TEXT("v1|")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactClipIdSensitivityTest,
	"KimodoDatasetCapture.Impact.ClipId.Sensitivity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactClipIdSensitivityTest::RunTest(const FString& Parameters)
{
	using namespace ImpactClipIdTests;
	const FFixture Base;
	const FString BaseId = Base.Id();
	TestFalse(TEXT("Base id is not empty"), BaseId.IsEmpty());
	TestEqual(TEXT("Identical parameters give the same id"), FFixture().Id(), BaseId);

	auto Differs = [this, &BaseId](const TCHAR* What, const FFixture& Changed)
	{
		TestNotEqual(*FString::Printf(TEXT("Changing %s changes the id"), What), Changed.Id(), BaseId);
	};

	{ FFixture F; F.Source = TEXT("/Game/Anim/M_Neutral_Stand_Idle_Loop"); Differs(TEXT("the source animation"), F); }
	{ FFixture F; F.Params.Impactor.Archetype = &F.OtherArchetype; Differs(TEXT("the impactor archetype"), F); }
	{ FFixture F; F.Params.Impactor.MassKg = 0.146f; Differs(TEXT("impactor mass"), F); }
	{ FFixture F; F.Params.Impactor.SpeedMetresPerSecond = 33.001f; Differs(TEXT("impactor speed (tiny change)"), F); }
	{ FFixture F; F.Params.Impactor.ContactRadiusCm = 3.6f; Differs(TEXT("contact radius"), F); }
	{ FFixture F; F.Params.ReactionGain = 4.5f; Differs(TEXT("reaction gain"), F); }
	{ FFixture F; F.Params.Region = EImpactBodyRegion::Head; Differs(TEXT("body region"), F); }
	{ FFixture F; F.Params.DirectionLocal = FVector(-1.0, 0.001, 0.0).GetSafeNormal(); Differs(TEXT("direction"), F); }
	{ FFixture F; F.Params.SourcePhase = 0.41f; Differs(TEXT("source phase"), F); }
	{ FFixture F; F.Schedule.Mode = EImpactDeliveryMode::Instant; Differs(TEXT("delivery mode"), F); }
	{ FFixture F; F.Schedule.Hits[1].FrameOffset = 5; Differs(TEXT("a hit's frame"), F); }
	{ FFixture F; F.Schedule.Hits[1].ImpulseKgCmPerSecond = 1141.0f; Differs(TEXT("a hit's impulse"), F); }
	{ FFixture F; F.Schedule.Hits.Pop(); Differs(TEXT("the number of hits"), F); }
	{
		FFixture F;
		FImpactProjectileSpec Spec;
		Spec.LaunchDistanceCm = 200.0f;
		F.Schedule.Projectile = Spec;
		Differs(TEXT("adding a projectile"), F);
	}

	// Swapping two hits is a different clip, not the same set reordered.
	{
		FFixture F;
		F.Schedule.Hits[0].ImpulseKgCmPerSecond = 1000.0f;
		F.Schedule.Hits[1].ImpulseKgCmPerSecond = 1280.0f;
		FFixture Swapped = F;
		Swapped.Schedule.Hits[0].ImpulseKgCmPerSecond = 1280.0f;
		Swapped.Schedule.Hits[1].ImpulseKgCmPerSecond = 1000.0f;
		TestNotEqual(TEXT("Hit order is part of the identity"), F.Id(), Swapped.Id());
	}

	// No impactor is still a stable id, not a crash.
	{
		FFixture F;
		F.Params.Impactor.Archetype = nullptr;
		TestEqual(TEXT("Missing impactor still yields a stable id"), F.Id(), F.Id());
	}
	return true;
}
