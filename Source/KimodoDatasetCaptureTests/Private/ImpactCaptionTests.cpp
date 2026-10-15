// Copyright Animotive. All Rights Reserved.

#include "ImpactCaption.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace ImpactCaptionTests
{
	const FImpactorArchetype* Find(const TArray<FImpactorArchetype>& Archetypes, const TCHAR* Id)
	{
		return Archetypes.FindByPredicate([Id](const FImpactorArchetype& A) { return A.Id == FName(Id); });
	}

	FImpactSourceMotion Running()
	{
		FImpactSourceMotion Motion;
		Motion.Participle = TEXT("running");
		Motion.Type = TEXT("run");
		Motion.bTravels = true;
		return Motion;
	}

	FImpactSourceMotion Idle()
	{
		FImpactSourceMotion Motion;
		Motion.Participle = TEXT("standing idle");
		Motion.Type = TEXT("idle");
		return Motion;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactCaptionPhrasesTest,
	"KimodoDatasetCapture.Impact.Caption.Phrases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactCaptionPhrasesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Left foot"), FImpactCaption::RegionPhrase(EImpactBodyRegion::FootLeft), FString(TEXT("the left foot")));
	TestEqual(TEXT("Head"), FImpactCaption::RegionPhrase(EImpactBodyRegion::Head), FString(TEXT("the head")));
	TestEqual(TEXT("Right forearm"), FImpactCaption::RegionPhrase(EImpactBodyRegion::LowerArmRight), FString(TEXT("the right forearm")));
	for (int32 Region = 0; Region <= int32(EImpactBodyRegion::FootRight); ++Region)
	{
		TestTrue(*FString::Printf(TEXT("Region %d has a phrase"), Region), FImpactCaption::RegionPhrase(EImpactBodyRegion(Region)).StartsWith(TEXT("the ")));
	}

	// Direction names the side the impactor came from, not where it is going.
	TestEqual(TEXT("Travelling backwards: from the front"), FImpactCaption::DirectionPhrase(FVector(-1, 0, 0)), FString(TEXT("from the front")));
	TestEqual(TEXT("Travelling forwards: from behind"), FImpactCaption::DirectionPhrase(FVector(1, 0, 0)), FString(TEXT("from behind")));
	TestEqual(TEXT("Travelling right: from the left"), FImpactCaption::DirectionPhrase(FVector(0, 1, 0)), FString(TEXT("from the left")));
	TestEqual(TEXT("Travelling left: from the right"), FImpactCaption::DirectionPhrase(FVector(0, -1, 0)), FString(TEXT("from the right")));
	TestEqual(TEXT("Travelling down: from above"), FImpactCaption::DirectionPhrase(FVector(0.2, 0, -1)), FString(TEXT("from above")));
	TestEqual(TEXT("The strongest axis decides"), FImpactCaption::DirectionPhrase(FVector(-0.8, 0.5, 0.1)), FString(TEXT("from the front")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactCaptionMotionTest,
	"KimodoDatasetCapture.Impact.Caption.SourceMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactCaptionMotionTest::RunTest(const FString& Parameters)
{
	// Worded from the animation's name when the table does not know it.
	const FImpactSourceMotion Run = FImpactCaption::MotionFromName(TEXT("M_Neutral_Run_Loop_F"));
	TestEqual(TEXT("Run is running"), Run.Participle, FString(TEXT("running")));
	TestEqual(TEXT("Run's tag"), Run.Type, FString(TEXT("run")));
	TestTrue(TEXT("A run travels"), Run.bTravels);
	TestEqual(TEXT("Walk is walking"), FImpactCaption::MotionFromName(TEXT("M_Neutral_Walk_Loop_F")).Participle, FString(TEXT("walking")));
	const FImpactSourceMotion Idle = FImpactCaption::MotionFromName(TEXT("M_Neutral_Stand_Idle_Loop"));
	TestEqual(TEXT("Stand idle is standing idle"), Idle.Participle, FString(TEXT("standing idle")));
	TestEqual(TEXT("Idle's tag"), Idle.Type, FString(TEXT("idle")));
	TestFalse(TEXT("Idle does not travel"), Idle.bTravels);
	TestEqual(TEXT("Sprint is sprinting"), FImpactCaption::MotionFromName(TEXT("M_Neutral_Sprint_Loop_F")).Participle, FString(TEXT("sprinting")));
	TestEqual(TEXT("Crouch is crouching"), FImpactCaption::MotionFromName(TEXT("M_Neutral_Crouch_Idle_Loop")).Participle, FString(TEXT("crouching")));
	const FImpactSourceMotion Unknown = FImpactCaption::MotionFromName(TEXT("ABC_Xyzzy_07"));
	TestEqual(TEXT("An unknown name is still worded"), Unknown.Participle, FString(TEXT("moving")));
	TestEqual(TEXT("An unknown name still has a tag"), Unknown.Type, FString(TEXT("other")));

	// The table wins when it knows the animation.
	TStrongObjectPtr<UImpactSourceMotionTable> Table(NewObject<UImpactSourceMotionTable>());
	FImpactSourceMotionEntry Entry;
	Entry.Animation = TEXT("M_Neutral_Run_Loop_F");
	Entry.Motion.Participle = TEXT("jogging forwards");
	Entry.Motion.Type = TEXT("jog");
	Entry.Motion.bTravels = true;
	Table->Entries.Add(Entry);
	TestEqual(TEXT("A table entry is used"), FImpactCaption::ResolveMotion(Table.Get(), TEXT("M_Neutral_Run_Loop_F")).Participle, FString(TEXT("jogging forwards")));
	TestEqual(TEXT("A missing entry falls back to the name"), FImpactCaption::ResolveMotion(Table.Get(), TEXT("M_Neutral_Walk_Loop_F")).Participle, FString(TEXT("walking")));
	TestEqual(TEXT("No table falls back to the name"), FImpactCaption::ResolveMotion(nullptr, TEXT("M_Neutral_Run_Loop_F")).Participle, FString(TEXT("running")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactCaptionDescribeTest,
	"KimodoDatasetCapture.Impact.Caption.Describe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactCaptionDescribeTest::RunTest(const FString& Parameters)
{
	using namespace ImpactCaptionTests;
	const TArray<FImpactorArchetype> Archetypes = UImpactorCatalog::MakeStarterArchetypes();

	// The foot-shot trip: every slot named.
	FImpactCaptionInput Trip;
	Trip.Impactor = Find(Archetypes, TEXT("SmallBullet"));
	Trip.Motion = Running();
	Trip.Region = EImpactBodyRegion::FootLeft;
	Trip.DirectionLocal = FVector(-1, 0, 0);
	Trip.Intensity = EImpactIntensity::Medium;
	Trip.Outcome = EImpactOutcome::Knockdown;
	Trip.bTrip = true;
	const FString Caption = FImpactCaption::Describe(Trip, 3, 7);
	AddInfo(Caption);
	TestTrue(TEXT("Names the subject"), Caption.Contains(TEXT("person")));
	TestTrue(TEXT("Names the impactor"), Caption.Contains(TEXT("a small bullet")));
	TestTrue(TEXT("Uses the impactor's verb"), Caption.Contains(TEXT("shot")));
	TestTrue(TEXT("Names the region"), Caption.Contains(TEXT("the left foot")));
	TestTrue(TEXT("Names the direction"), Caption.Contains(TEXT("from the front")));
	TestTrue(TEXT("Names the source motion"), Caption.Contains(TEXT("running")));
	TestTrue(TEXT("Says it trips"), Caption.Contains(TEXT("trip")));
	TestEqual(TEXT("Same clip, same caption"), FImpactCaption::Describe(Trip, 3, 7), Caption);

	// Phrasings vary between clips.
	TSet<FString> Phrasings;
	for (int32 Clip = 0; Clip < 60; ++Clip)
	{
		Phrasings.Add(FImpactCaption::Describe(Trip, 3, Clip));
	}
	TestTrue(*FString::Printf(TEXT("Several phrasings across clips (%d)"), Phrasings.Num()), Phrasings.Num() >= 4);

	// A verb that repeats the impactor ("punched by a punch") is not used with it.
	FImpactCaptionInput Punch = Trip;
	Punch.Impactor = Find(Archetypes, TEXT("Punch"));
	Punch.Motion = Idle();
	Punch.Region = EImpactBodyRegion::Head;
	Punch.Outcome = EImpactOutcome::Stagger;
	Punch.bTrip = false;
	Punch.Steps = 2;
	bool bNeverRedundant = true;
	for (int32 Clip = 0; Clip < 60; ++Clip)
	{
		bNeverRedundant &= !FImpactCaption::Describe(Punch, 3, Clip).Contains(TEXT("punched"));
	}
	TestTrue(TEXT("Never 'punched ... by a punch'"), bNeverRedundant);

	// Nobody is hit "with a car": only things a person could wield or throw take "with".
	FImpactCaptionInput Car = Punch;
	Car.Impactor = Find(Archetypes, TEXT("CarBumper"));
	bool bNeverWithACar = true;
	for (int32 Clip = 0; Clip < 60; ++Clip)
	{
		bNeverWithACar &= !FImpactCaption::Describe(Car, 3, Clip).Contains(TEXT("with a car"));
	}
	TestTrue(TEXT("Never 'with a car'"), bNeverWithACar);

	// Every starter impactor x region x outcome x motion reads as a finished sentence.
	bool bAllWritten = true;
	FString FirstBad;
	for (const FImpactorArchetype& Impactor : Archetypes)
	{
		for (int32 Region = 0; Region <= int32(EImpactBodyRegion::FootRight); ++Region)
		{
			for (int32 Outcome = 0; Outcome <= int32(EImpactOutcome::Ragdoll); ++Outcome)
			{
				for (const bool bRun : { false, true })
				{
					FImpactCaptionInput Input;
					Input.Impactor = &Impactor;
					Input.Motion = bRun ? Running() : Idle();
					Input.Region = EImpactBodyRegion(Region);
					Input.DirectionLocal = FVector(0, 1, 0);
					Input.Outcome = EImpactOutcome(Outcome);
					Input.bTrip = bRun && Outcome == int32(EImpactOutcome::Knockdown);
					Input.Steps = Outcome == int32(EImpactOutcome::Stagger) ? 1 : 0;
					const FString Text = FImpactCaption::Describe(Input, 1, Region * 10 + Outcome);
					const bool bOk = Text.Len() > 20 && FChar::IsUpper(Text[0]) && Text.EndsWith(TEXT("."))
						&& !Text.Contains(TEXT("{")) && !Text.Contains(TEXT("}")) && !Text.Contains(TEXT("  "))
						&& !Text.Contains(TEXT(" ,")) && !Text.Contains(TEXT(" .")) && Text.Contains(Impactor.NounPhrase)
						&& Text.Contains(FImpactCaption::RegionPhrase(EImpactBodyRegion(Region)));
					if (!bOk && FirstBad.IsEmpty())
					{
						FirstBad = Text.IsEmpty() ? FString::Printf(TEXT("(empty for %s)"), *Impactor.Id.ToString()) : Text;
					}
					bAllWritten &= bOk;
				}
			}
		}
	}
	TestTrue(*FString::Printf(TEXT("Every combination reads as a finished sentence naming impactor and region. First bad: %s"), *FirstBad), bAllWritten);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactCaptionTagsTest,
	"KimodoDatasetCapture.Impact.Caption.Tags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactCaptionTagsTest::RunTest(const FString& Parameters)
{
	using namespace ImpactCaptionTests;
	const TArray<FImpactorArchetype> Archetypes = UImpactorCatalog::MakeStarterArchetypes();
	FImpactCaptionInput Input;
	Input.Impactor = Find(Archetypes, TEXT("CarBumper"));
	Input.Motion = Idle();
	Input.Region = EImpactBodyRegion::Pelvis;
	Input.Intensity = EImpactIntensity::Knockdown;
	Input.Outcome = EImpactOutcome::Ragdoll;
	const TArray<FString> Tags = FImpactCaption::Tags(Input);
	const TArray<FString> Expected = { TEXT("impact"), TEXT("CarBumper"), TEXT("Pelvis"), TEXT("Ragdoll"), TEXT("Knockdown"), TEXT("idle") };
	TestEqual(TEXT("Tags are impact, impactor, region, outcome, intensity, motion"), Tags, Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactCaptionDescriptionsTest,
	"KimodoDatasetCapture.Impact.Caption.Descriptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactCaptionDescriptionsTest::RunTest(const FString& Parameters)
{
	using namespace ImpactCaptionTests;
	const TArray<FImpactorArchetype> Archetypes = UImpactorCatalog::MakeStarterArchetypes();
	FImpactCaptionInput Input;
	Input.Impactor = Find(Archetypes, TEXT("Punch"));
	Input.Motion = Idle();
	Input.Region = EImpactBodyRegion::Chest;
	Input.Outcome = EImpactOutcome::Stagger;
	Input.Steps = 2;
	for (int32 Clip = 0; Clip < 20; ++Clip)
	{
		const TArray<FString> Descriptions = FImpactCaption::Descriptions(Input, 3, Clip);
		if (!TestEqual(*FString::Printf(TEXT("Clip %d: three phrasings"), Clip), Descriptions.Num(), 3))
		{
			continue;
		}
		TestEqual(TEXT("The first is the clip's caption"), Descriptions[0], FImpactCaption::Describe(Input, 3, Clip));
		TestEqual(TEXT("All different"), TSet<FString>(Descriptions).Num(), 3);
		for (const FString& Description : Descriptions)
		{
			TestTrue(*FString::Printf(TEXT("A full sentence about a person: %s"), *Description),
				Description.Contains(TEXT("person")) && Description.EndsWith(TEXT(".")) && FChar::IsUpper(Description[0]));
		}
		TestEqual(TEXT("Same clip, same phrasings"), FImpactCaption::Descriptions(Input, 3, Clip), Descriptions);
	}
	TestEqual(TEXT("As many as asked for"), FImpactCaption::Descriptions(Input, 3, 0, 2).Num(), 2);
	TestEqual(TEXT("No impactor, nothing to say"), FImpactCaption::Descriptions(FImpactCaptionInput(), 3, 0).Num(), 0);
	return true;
}
