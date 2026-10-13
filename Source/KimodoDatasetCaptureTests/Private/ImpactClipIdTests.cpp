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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactClipNameTest,
	"KimodoDatasetCapture.Impact.ClipId.ReadableName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactClipNameTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Index, source, impactor, region, direction, intensity, outcome"),
		FImpactClipName::Make(7, TEXT("Sprint"), TEXT("SmallBullet"), TEXT("FootLeft"), TEXT("FromFront"), TEXT("Medium"), TEXT("Knockdown")),
		FString(TEXT("IMP_000007_Sprint_SmallBullet_FootLeft_FromFront_Medium_Knockdown")));
	TestEqual(TEXT("Large indices keep their digits"), FImpactClipName::Make(1234567, TEXT("Run"), TEXT("Car"), TEXT("Chest"), TEXT("FromLeft"), TEXT("Extreme"), TEXT("Ragdoll")),
		FString(TEXT("IMP_1234567_Run_Car_Chest_FromLeft_Extreme_Ragdoll")));
	TestEqual(TEXT("A clip that never ran has no outcome part"), FImpactClipName::Make(3, TEXT("Walk"), TEXT("Punch"), TEXT("Head"), TEXT("FromBack"), TEXT("Light"), FString()),
		FString(TEXT("IMP_000003_Walk_Punch_Head_FromBack_Light")));
	TestEqual(TEXT("Unsafe characters are dropped"), FImpactClipName::Make(1, TEXT("Run Fast"), TEXT("Bat/Club"), TEXT("Chest."), TEXT("From Front"), TEXT("Tap"), TEXT("Flinch")),
		FString(TEXT("IMP_000001_RunFast_BatClub_Chest_FromFront_Tap_Flinch")));
	TestTrue(TEXT("Every name of a clip starts with its index prefix"),
		FImpactClipName::Make(42, TEXT("Jog"), TEXT("Shove"), TEXT("Pelvis"), TEXT("FromFront"), TEXT("Light"), TEXT("Stagger")).StartsWith(FImpactClipName::IndexPrefix(42)));
	TestEqual(TEXT("The prefix ends at the index"), FImpactClipName::IndexPrefix(42), FString(TEXT("IMP_000042_")));
	TestFalse(TEXT("Clip 4's prefix is not clip 42's"), FImpactClipName::Make(42, TEXT("Jog"), TEXT("Shove"), TEXT("Pelvis"), TEXT("FromFront"), TEXT("Light"), TEXT("Stagger"))
		.StartsWith(FImpactClipName::IndexPrefix(4)));
	TestTrue(TEXT("A readable name is a valid asset name"), FName::IsValidXName(FImpactClipName::Make(9, TEXT("CrouchIdle"), TEXT("Baseball"), TEXT("HandRight"), TEXT("FromFrontLeftHigh"), TEXT("Tap"), TEXT("Flinch")),
		INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS));

	// Where the hit arrives from: the opposite of the way it travels. Character space: X forward, Y right.
	TestEqual(TEXT("Travelling backwards: from the front"), FImpactClipName::DirectionLabel(FVector(-1, 0, 0)), FString(TEXT("FromFront")));
	TestEqual(TEXT("Travelling forwards: from the back"), FImpactClipName::DirectionLabel(FVector(1, 0, 0)), FString(TEXT("FromBack")));
	TestEqual(TEXT("Travelling right: from the left"), FImpactClipName::DirectionLabel(FVector(0, 1, 0)), FString(TEXT("FromLeft")));
	TestEqual(TEXT("Travelling left: from the right"), FImpactClipName::DirectionLabel(FVector(0, -1, 0)), FString(TEXT("FromRight")));
	TestEqual(TEXT("Back and to the right: from the front left"), FImpactClipName::DirectionLabel(FVector(-1, 1, 0).GetSafeNormal()), FString(TEXT("FromFrontLeft")));
	TestEqual(TEXT("Forward and to the left: from the back right"), FImpactClipName::DirectionLabel(FVector(1, -1, 0).GetSafeNormal()), FString(TEXT("FromBackRight")));
	TestEqual(TEXT("Steeply downwards from the front: high"), FImpactClipName::DirectionLabel(FVector(-1, 0, -1).GetSafeNormal()), FString(TEXT("FromFrontHigh")));
	TestEqual(TEXT("Steeply upwards from behind: low"), FImpactClipName::DirectionLabel(FVector(1, 0, 1).GetSafeNormal()), FString(TEXT("FromBackLow")));
	TestEqual(TEXT("Twenty degrees down is still level"), FImpactClipName::DirectionLabel(FVector(-0.94, 0, -0.34).GetSafeNormal()), FString(TEXT("FromFront")));
	TestEqual(TEXT("Straight down: from above"), FImpactClipName::DirectionLabel(FVector(0, 0, -1)), FString(TEXT("FromAbove")));
	TestEqual(TEXT("Straight up: from below"), FImpactClipName::DirectionLabel(FVector(0, 0, 1)), FString(TEXT("FromBelow")));

	TestEqual(TEXT("Crouch idle"), FImpactClipName::SourceLabel(TEXT("M_Kimodo_Crouch_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin")), FString(TEXT("CrouchIdle")));
	TestEqual(TEXT("Run, seed suffix dropped"), FImpactClipName::SourceLabel(TEXT("M_Kimodo_Run_Loop_F_S1_KimodoRetargeted_SK_UEFN_Mannequin")), FString(TEXT("Run")));
	TestEqual(TEXT("Sprint, take suffix dropped"), FImpactClipName::SourceLabel(TEXT("M_Kimodo_Sprint_Loop_F_L4_KimodoRetargeted_SK_UEFN_Mannequin")), FString(TEXT("Sprint")));
	TestEqual(TEXT("Stand idle"), FImpactClipName::SourceLabel(TEXT("M_Kimodo_Stand_Idle_Loop_KimodoRetargeted_SK_UEFN_Mannequin")), FString(TEXT("StandIdle")));
	TestEqual(TEXT("Crouch walk"), FImpactClipName::SourceLabel(TEXT("M_Kimodo_Crouch_Walk_Loop_F_KimodoRetargeted_SK_UEFN_Mannequin")), FString(TEXT("CrouchWalk")));
	TestEqual(TEXT("Jump in place"), FImpactClipName::SourceLabel(TEXT("M_Kimodo_Jump_In_Place_KimodoRetargeted_SK_UEFN_Mannequin")), FString(TEXT("JumpInPlace")));
	TestEqual(TEXT("An Epic clip"), FImpactClipName::SourceLabel(TEXT("M_Neutral_Run_Loop_F")), FString(TEXT("NeutralRun")));
	TestEqual(TEXT("Lowercase words are capitalised"), FImpactClipName::SourceLabel(TEXT("walk_slow")), FString(TEXT("WalkSlow")));
	TestEqual(TEXT("A name that is all boilerplate falls back to the asset name"), FImpactClipName::SourceLabel(TEXT("M_Loop_F")), FString(TEXT("MLoopF")));
	return true;
}

