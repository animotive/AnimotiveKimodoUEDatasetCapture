// Copyright Animotive. All Rights Reserved.

#include "ImpactRegionTable.h"
#include "ImpactorCatalog.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactStarterFillCatalogTest,
	"KimodoDatasetCapture.Impact.StarterFill.Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactStarterFillCatalogTest::RunTest(const FString& Parameters)
{
	const int32 StarterCount = UImpactorCatalog::MakeStarterArchetypes().Num();

	// Empty catalog: the full starter set.
	UImpactorCatalog* Empty = NewObject<UImpactorCatalog>();
	TestEqual(TEXT("Empty catalog gains every starter archetype"), Empty->FillWithStarterArchetypes(), StarterCount);
	TestEqual(TEXT("Empty catalog now holds the starter set"), Empty->Archetypes.Num(), StarterCount);
	FText Reason;
	TestTrue(TEXT("Filled catalog validates"), Empty->Validate(Reason));

	// Pressing it again adds nothing.
	TestEqual(TEXT("Second fill adds nothing"), Empty->FillWithStarterArchetypes(), 0);
	TestEqual(TEXT("Second fill leaves the count unchanged"), Empty->Archetypes.Num(), StarterCount);

	// A project's own impactor and its edit to a starter entry both survive.
	UImpactorCatalog* Custom = NewObject<UImpactorCatalog>();
	FImpactorArchetype Chair = UImpactorCatalog::MakeStarterArchetypes()[0];
	Chair.Id = TEXT("ThrownChair");
	Chair.NounPhrase = TEXT("a thrown chair");
	FImpactorArchetype EditedBullet = UImpactorCatalog::MakeStarterArchetypes()[0];
	EditedBullet.Weight = 7.0f;
	Custom->Archetypes = { Chair, EditedBullet };
	TestEqual(TEXT("Fill adds only the missing starter archetypes"), Custom->FillWithStarterArchetypes(), StarterCount - 1);
	TestEqual(TEXT("Custom plus starter set"), Custom->Archetypes.Num(), StarterCount + 1);
	TestEqual(TEXT("Custom impactor kept first"), Custom->Archetypes[0].Id, FName(TEXT("ThrownChair")));
	TestEqual(TEXT("Edited starter entry not overwritten"), Custom->Archetypes[1].Weight, 7.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactStarterFillRegionsTest,
	"KimodoDatasetCapture.Impact.StarterFill.RegionTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactStarterFillRegionsTest::RunTest(const FString& Parameters)
{
	const int32 EntryCount = UImpactRegionTable::MakeUEFNMannequinEntries().Num();

	UImpactRegionTable* Empty = NewObject<UImpactRegionTable>();
	TestEqual(TEXT("Empty table gains every region"), Empty->FillWithUEFNMannequinEntries(), EntryCount);
	TestEqual(TEXT("Second fill adds nothing"), Empty->FillWithUEFNMannequinEntries(), 0);

	// A project's remapping of one region survives.
	UImpactRegionTable* Custom = NewObject<UImpactRegionTable>();
	Custom->Entries = { { EImpactBodyRegion::Chest, { TEXT("spine_04") } } };
	TestEqual(TEXT("Fill adds only unmapped regions"), Custom->FillWithUEFNMannequinEntries(), EntryCount - 1);
	TestEqual(TEXT("Custom chest mapping kept"), Custom->BodiesFor(EImpactBodyRegion::Chest), TArray<FName>({ TEXT("spine_04") }));
	return true;
}
