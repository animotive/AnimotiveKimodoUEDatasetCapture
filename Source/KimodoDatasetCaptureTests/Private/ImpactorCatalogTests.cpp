// Copyright Animotive. All Rights Reserved.

#include "ImpactorCatalog.h"
#include "ImpactRandom.h"
#include "Misc/AutomationTest.h"

namespace ImpactorCatalogTests
{
	FImpactorArchetype MakeValid(FName Id, float Weight = 1.0f)
	{
		FImpactorArchetype Archetype;
		Archetype.Id = Id;
		Archetype.NounPhrase = TEXT("a test object");
		Archetype.Description = TEXT("A test object.");
		Archetype.Verbs = { TEXT("hit") };
		Archetype.SizeCm = 5.0f;
		Archetype.MassKg = FFloatInterval(1.0f, 2.0f);
		Archetype.SpeedMetresPerSecond = FFloatInterval(3.0f, 4.0f);
		Archetype.ContactRadiusCm = FFloatInterval(1.0f, 5.0f);
		Archetype.Weight = Weight;
		return Archetype;
	}

	UImpactorCatalog* MakeCatalog(TArray<FImpactorArchetype> Archetypes)
	{
		UImpactorCatalog* Catalog = NewObject<UImpactorCatalog>();
		Catalog->Archetypes = MoveTemp(Archetypes);
		return Catalog;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactorCatalogStarterTest,
	"KimodoDatasetCapture.Impact.Catalog.StarterSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactorCatalogStarterTest::RunTest(const FString& Parameters)
{
	using namespace ImpactorCatalogTests;
	const TArray<FImpactorArchetype> Starter = UImpactorCatalog::MakeStarterArchetypes();

	// The starter set promised in the spec, by id.
	for (const TCHAR* Id : { TEXT("SmallBullet"), TEXT("RifleRound"), TEXT("ShotgunBlast"), TEXT("ThrownStone"),
		TEXT("Baseball"), TEXT("Football"), TEXT("Punch"), TEXT("Kick"), TEXT("Shove"), TEXT("SportsTackle"),
		TEXT("SwingingBat"), TEXT("CarBumper"), TEXT("WreckingBall"), TEXT("WindPush") })
	{
		TestTrue(*FString::Printf(TEXT("Starter set contains %s"), Id),
			Starter.ContainsByPredicate([Id](const FImpactorArchetype& A) { return A.Id == FName(Id); }));
	}

	// Every archetype carries what a caption and a sidecar need.
	for (const FImpactorArchetype& Archetype : Starter)
	{
		const FString Id = Archetype.Id.ToString();
		TestFalse(*(Id + TEXT(" has a noun phrase")), Archetype.NounPhrase.IsEmpty());
		TestFalse(*(Id + TEXT(" has a description")), Archetype.Description.IsEmpty());
		TestTrue(*(Id + TEXT(" has at least one verb")), Archetype.Verbs.Num() > 0);
		TestTrue(*(Id + TEXT(" has a positive size")), Archetype.SizeCm > 0.0f);
		TestTrue(*(Id + TEXT(" has a positive mass range")), Archetype.MassKg.Min > 0.0f && Archetype.MassKg.Min <= Archetype.MassKg.Max);
		TestTrue(*(Id + TEXT(" has a positive speed range")), Archetype.SpeedMetresPerSecond.Min > 0.0f && Archetype.SpeedMetresPerSecond.Min <= Archetype.SpeedMetresPerSecond.Max);
		TestTrue(*(Id + TEXT(" has a positive contact radius range")), Archetype.ContactRadiusCm.Min > 0.0f && Archetype.ContactRadiusCm.Min <= Archetype.ContactRadiusCm.Max);
	}

	// The starter set must itself be a valid catalog.
	FText Reason;
	TestTrue(TEXT("Starter catalog validates"), MakeCatalog(Starter)->Validate(Reason));

	// Physical sanity between archetypes: a car carries far more momentum than a bullet.
	const FImpactorArchetype* Bullet = Starter.FindByPredicate([](const FImpactorArchetype& A) { return A.Id == FName(TEXT("SmallBullet")); });
	const FImpactorArchetype* Car = Starter.FindByPredicate([](const FImpactorArchetype& A) { return A.Id == FName(TEXT("CarBumper")); });
	if (Bullet && Car)
	{
		TestTrue(TEXT("Largest bullet momentum is below smallest car momentum"),
			Bullet->MassKg.Max * Bullet->SpeedMetresPerSecond.Max < Car->MassKg.Min * Car->SpeedMetresPerSecond.Min);
		TestTrue(TEXT("A bullet's contact is point-like next to a car's"), Bullet->ContactRadiusCm.Max < Car->ContactRadiusCm.Min);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactorCatalogContactTimeTest,
	"KimodoDatasetCapture.Impact.Catalog.ContactTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactorCatalogContactTimeTest::RunTest(const FString& Parameters)
{
	FImpactorArchetype Archetype;
	TestEqual(TEXT("No contact time is a single frame"), Archetype.ContactFrames(60), 1);
	Archetype.ContactSeconds = 0.1f;
	TestEqual(TEXT("A tenth of a second at 60 fps is six frames"), Archetype.ContactFrames(60), 6);
	TestEqual(TEXT("At 30 fps, three frames"), Archetype.ContactFrames(30), 3);
	Archetype.ContactSeconds = 0.11f;
	TestEqual(TEXT("A partial frame rounds up"), Archetype.ContactFrames(60), 7);
	TestEqual(TEXT("A bad frame rate still gives one frame"), Archetype.ContactFrames(0), 1);

	// Heavy, wide impactors stay in contact; small fast ones are a single frame.
	const TArray<FImpactorArchetype> Starter = UImpactorCatalog::MakeStarterArchetypes();
	auto Frames = [&Starter](const TCHAR* Id)
	{
		const FImpactorArchetype* Found = Starter.FindByPredicate([Id](const FImpactorArchetype& A) { return A.Id == FName(Id); });
		return Found ? Found->ContactFrames(60) : -1;
	};
	TestTrue(TEXT("A car pushes for at least a tenth of a second"), Frames(TEXT("CarBumper")) >= 6);
	TestTrue(TEXT("A wrecking ball stays in contact over several frames"), Frames(TEXT("WreckingBall")) >= 4);
	TestTrue(TEXT("A tackle stays in contact over several frames"), Frames(TEXT("SportsTackle")) >= 6);
	TestEqual(TEXT("A bullet is a single frame"), Frames(TEXT("SmallBullet")), 1);
	TestEqual(TEXT("A punch is a single frame"), Frames(TEXT("Punch")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactorCatalogValidationTest,
	"KimodoDatasetCapture.Impact.Catalog.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactorCatalogValidationTest::RunTest(const FString& Parameters)
{
	using namespace ImpactorCatalogTests;
	auto Rejects = [this](const TCHAR* What, TArray<FImpactorArchetype> Archetypes)
	{
		FText Reason;
		const bool bValid = MakeCatalog(MoveTemp(Archetypes))->Validate(Reason);
		TestFalse(*FString::Printf(TEXT("%s is rejected"), What), bValid);
		TestFalse(*FString::Printf(TEXT("%s states a reason"), What), Reason.IsEmpty());
	};

	FText Reason;
	TestTrue(TEXT("A well-formed catalog validates"), MakeCatalog({ MakeValid(TEXT("A")), MakeValid(TEXT("B")) })->Validate(Reason));

	Rejects(TEXT("Empty catalog"), {});
	Rejects(TEXT("All weights zero"), { MakeValid(TEXT("A"), 0.0f), MakeValid(TEXT("B"), 0.0f) });

	FImpactorArchetype NoNoun = MakeValid(TEXT("A"));
	NoNoun.NounPhrase.Empty();
	Rejects(TEXT("Missing noun phrase"), { NoNoun });

	FImpactorArchetype NoDescription = MakeValid(TEXT("A"));
	NoDescription.Description.Empty();
	Rejects(TEXT("Missing description"), { NoDescription });

	FImpactorArchetype NoVerbs = MakeValid(TEXT("A"));
	NoVerbs.Verbs.Empty();
	Rejects(TEXT("Missing verbs"), { NoVerbs });

	FImpactorArchetype NoId = MakeValid(NAME_None);
	Rejects(TEXT("Missing id"), { NoId });

	Rejects(TEXT("Duplicate id"), { MakeValid(TEXT("A")), MakeValid(TEXT("A")) });

	FImpactorArchetype ZeroSize = MakeValid(TEXT("A"));
	ZeroSize.SizeCm = 0.0f;
	Rejects(TEXT("Zero size"), { ZeroSize });

	FImpactorArchetype InvertedMass = MakeValid(TEXT("A"));
	InvertedMass.MassKg = FFloatInterval(5.0f, 1.0f);
	Rejects(TEXT("Inverted mass range"), { InvertedMass });

	FImpactorArchetype ZeroSpeed = MakeValid(TEXT("A"));
	ZeroSpeed.SpeedMetresPerSecond = FFloatInterval(0.0f, 3.0f);
	Rejects(TEXT("Non-positive speed"), { ZeroSpeed });

	FImpactorArchetype NegativeRadius = MakeValid(TEXT("A"));
	NegativeRadius.ContactRadiusCm = FFloatInterval(-1.0f, 3.0f);
	Rejects(TEXT("Negative contact radius"), { NegativeRadius });

	FImpactorArchetype NegativeWeight = MakeValid(TEXT("A"), -1.0f);
	Rejects(TEXT("Negative weight"), { NegativeWeight, MakeValid(TEXT("B")) });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactorCatalogSamplingTest,
	"KimodoDatasetCapture.Impact.Catalog.Sampling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactorCatalogSamplingTest::RunTest(const FString& Parameters)
{
	using namespace ImpactorCatalogTests;

	// Weights: 1 to 3 means a quarter of draws pick the first.
	UImpactorCatalog* Catalog = MakeCatalog({ MakeValid(TEXT("Light"), 1.0f), MakeValid(TEXT("Heavy"), 3.0f), MakeValid(TEXT("Never"), 0.0f) });
	FImpactRandom Random(11, 0, EImpactRandomStream::Impactor);
	constexpr int32 Draws = 20000;
	int32 Light = 0, Never = 0, Null = 0;
	for (int32 Index = 0; Index < Draws; ++Index)
	{
		const FImpactorArchetype* Picked = Catalog->SampleArchetype(Random);
		Null += Picked ? 0 : 1;
		Light += (Picked && Picked->Id == FName(TEXT("Light"))) ? 1 : 0;
		Never += (Picked && Picked->Id == FName(TEXT("Never"))) ? 1 : 0;
	}
	TestEqual(TEXT("Sampling never returns null from a valid catalog"), Null, 0);
	TestEqual(TEXT("Zero-weight archetype is never picked"), Never, 0);
	TestEqual(TEXT("Weights respected (1 of 4)"), double(Light) / Draws, 0.25, 0.02);

	// Empty catalog samples nothing rather than crashing.
	TestNull(TEXT("Empty catalog samples null"), MakeCatalog({})->SampleArchetype(Random));

	// Instances stay within their archetype, and impulse is exactly mass x speed (kg x cm/s).
	const FImpactorArchetype Archetype = MakeValid(TEXT("Bounded"));
	bool bInRange = true, bImpulseIsMomentum = true;
	for (int32 Index = 0; Index < 2000; ++Index)
	{
		const FImpactorSample Sample = UImpactorCatalog::SampleInstance(Archetype, Random);
		bInRange &= Sample.Archetype == &Archetype;
		bInRange &= Sample.MassKg >= Archetype.MassKg.Min && Sample.MassKg <= Archetype.MassKg.Max;
		bInRange &= Sample.SpeedMetresPerSecond >= Archetype.SpeedMetresPerSecond.Min && Sample.SpeedMetresPerSecond <= Archetype.SpeedMetresPerSecond.Max;
		bInRange &= Sample.ContactRadiusCm >= Archetype.ContactRadiusCm.Min && Sample.ContactRadiusCm <= Archetype.ContactRadiusCm.Max;
		bImpulseIsMomentum &= FMath::IsNearlyEqual(Sample.ImpulseKgCmPerSecond(), Sample.MassKg * Sample.SpeedMetresPerSecond * 100.0f, 0.01f);
		const float Impulse = Sample.ImpulseKgCmPerSecond();
		bInRange &= Impulse >= Archetype.MassKg.Min * Archetype.SpeedMetresPerSecond.Min * 100.0f - 0.01f;
		bInRange &= Impulse <= Archetype.MassKg.Max * Archetype.SpeedMetresPerSecond.Max * 100.0f + 0.01f;
	}
	TestTrue(TEXT("Sampled instance stays within the archetype's ranges"), bInRange);
	TestTrue(TEXT("Impulse is mass x speed in kg x cm/s"), bImpulseIsMomentum);

	// A fixed range samples its single value exactly.
	FImpactorArchetype Fixed = MakeValid(TEXT("Fixed"));
	Fixed.MassKg = FFloatInterval(2.0f, 2.0f);
	Fixed.SpeedMetresPerSecond = FFloatInterval(10.0f, 10.0f);
	const FImpactorSample FixedSample = UImpactorCatalog::SampleInstance(Fixed, Random);
	TestEqual(TEXT("Fixed mass and speed give a fixed impulse"), FixedSample.ImpulseKgCmPerSecond(), 2000.0f, 0.01f);
	return true;
}
