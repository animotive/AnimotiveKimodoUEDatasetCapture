// Copyright Animotive. All Rights Reserved.

#include "ImpactRegionTable.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

namespace ImpactRegionTableTests
{
	/** The 22 bodies of PA_UEFN_Mannequin, so the tests run without GameAnimationSample content. */
	const TCHAR* UEFNBodies[] = {
		TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05"),
		TEXT("neck_01"), TEXT("head"),
		TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
		TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
		TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"),
		TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"),
	};

	UPhysicsAsset* MakePhysicsAsset(TArrayView<const TCHAR* const> Bodies)
	{
		UPhysicsAsset* Asset = NewObject<UPhysicsAsset>();
		for (const TCHAR* Body : Bodies)
		{
			USkeletalBodySetup* Setup = NewObject<USkeletalBodySetup>(Asset);
			Setup->BoneName = Body;
			Asset->SkeletalBodySetups.Add(Setup);
		}
		Asset->UpdateBodySetupIndexMap();
		return Asset;
	}

	UImpactRegionTable* MakeTable(TArray<FImpactRegionBodies> Entries)
	{
		UImpactRegionTable* Table = NewObject<UImpactRegionTable>();
		Table->Entries = MoveTemp(Entries);
		return Table;
	}

	TArray<EImpactBodyRegion> AllRegions()
	{
		TArray<EImpactBodyRegion> Regions;
		const UEnum* Enum = StaticEnum<EImpactBodyRegion>();
		for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index) // last entry is _MAX
		{
			Regions.Add(EImpactBodyRegion(Enum->GetValueByIndex(Index)));
		}
		return Regions;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactRegionTableStarterTest,
	"KimodoDatasetCapture.Impact.RegionTable.UEFNMannequin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactRegionTableStarterTest::RunTest(const FString& Parameters)
{
	using namespace ImpactRegionTableTests;
	UImpactRegionTable* Table = MakeTable(UImpactRegionTable::MakeUEFNMannequinEntries());
	UPhysicsAsset* Asset = MakePhysicsAsset(UEFNBodies);

	FText Reason;
	TestTrue(TEXT("UEFN table validates against the UEFN body set"), Table->Validate(*Asset, Reason));

	for (const EImpactBodyRegion Region : AllRegions())
	{
		const FString Name = StaticEnum<EImpactBodyRegion>()->GetNameStringByValue(int64(Region));
		TestTrue(*FString::Printf(TEXT("%s resolves to at least one body"), *Name), Table->BodiesFor(Region).Num() > 0);
	}

	// Sides are not crossed: a left region never names a right body and vice versa.
	for (const FImpactRegionBodies& Entry : Table->Entries)
	{
		const FString Name = StaticEnum<EImpactBodyRegion>()->GetNameStringByValue(int64(Entry.Region));
		for (const FName& Body : Entry.Bodies)
		{
			if (Name.EndsWith(TEXT("Left")))
			{
				TestFalse(*FString::Printf(TEXT("%s does not use right-side %s"), *Name, *Body.ToString()), Body.ToString().EndsWith(TEXT("_r")));
			}
			if (Name.EndsWith(TEXT("Right")))
			{
				TestFalse(*FString::Printf(TEXT("%s does not use left-side %s"), *Name, *Body.ToString()), Body.ToString().EndsWith(TEXT("_l")));
			}
		}
	}

	// The spike's contact body stays inside the chest region.
	TestTrue(TEXT("Chest includes spine_03"), Table->BodiesFor(EImpactBodyRegion::Chest).Contains(FName(TEXT("spine_03"))));
	TestTrue(TEXT("Foot is the foot body"), Table->BodiesFor(EImpactBodyRegion::FootLeft).Contains(FName(TEXT("foot_l"))));

	// The real asset, where present.
	if (UPhysicsAsset* Real = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Rigs/PA_UEFN_Mannequin.PA_UEFN_Mannequin")))
	{
		TestTrue(TEXT("UEFN table validates against the real PA_UEFN_Mannequin"), Table->Validate(*Real, Reason));
	}
	else
	{
		AddWarning(TEXT("PA_UEFN_Mannequin not found; real-asset check skipped. Run in GameAnimationSample."));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactRegionTableValidationTest,
	"KimodoDatasetCapture.Impact.RegionTable.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactRegionTableValidationTest::RunTest(const FString& Parameters)
{
	using namespace ImpactRegionTableTests;
	UPhysicsAsset* Asset = MakePhysicsAsset(UEFNBodies);
	auto Rejects = [this, Asset](const TCHAR* What, TArray<FImpactRegionBodies> Entries, const TCHAR* MustMention)
	{
		FText Reason;
		TestFalse(*FString::Printf(TEXT("%s is rejected"), What), MakeTable(MoveTemp(Entries))->Validate(*Asset, Reason));
		TestTrue(*FString::Printf(TEXT("%s reason names %s"), What, MustMention), Reason.ToString().Contains(MustMention));
	};

	TArray<FImpactRegionBodies> Good = UImpactRegionTable::MakeUEFNMannequinEntries();

	TArray<FImpactRegionBodies> UnknownBody = Good;
	if (UnknownBody.Num() > 0)
	{
		UnknownBody[0].Bodies.Add(TEXT("spine_99"));
	}
	Rejects(TEXT("Unknown body name"), UnknownBody, TEXT("spine_99"));

	TArray<FImpactRegionBodies> Missing = Good;
	Missing.RemoveAll([](const FImpactRegionBodies& Entry) { return Entry.Region == EImpactBodyRegion::Neck; });
	Rejects(TEXT("Region not mapped"), Missing, TEXT("Neck"));

	TArray<FImpactRegionBodies> Empty = Good;
	for (FImpactRegionBodies& Entry : Empty)
	{
		if (Entry.Region == EImpactBodyRegion::HandLeft)
		{
			Entry.Bodies.Empty();
		}
	}
	Rejects(TEXT("Region with no bodies"), Empty, TEXT("HandLeft"));

	TArray<FImpactRegionBodies> Duplicate = Good;
	Duplicate.Add({ EImpactBodyRegion::Head, { TEXT("head") } });
	Rejects(TEXT("Region mapped twice"), Duplicate, TEXT("Head"));

	FText Reason;
	TestFalse(TEXT("Empty table is rejected"), MakeTable({})->Validate(*Asset, Reason));
	TestFalse(TEXT("Empty table states a reason"), Reason.IsEmpty());

	// A physics asset missing a body the table needs (a rig without separate feet, say).
	const TCHAR* NoFeet[] = { TEXT("pelvis"), TEXT("spine_01"), TEXT("head") };
	TestFalse(TEXT("Table rejected against a rig lacking its bodies"), MakeTable(Good)->Validate(*MakePhysicsAsset(NoFeet), Reason));
	TestFalse(TEXT("Missing-body rejection states a reason"), Reason.IsEmpty());

	// Unmapped region resolves to nothing rather than a default body.
	TestEqual(TEXT("Unmapped region has no bodies"), MakeTable(Missing)->BodiesFor(EImpactBodyRegion::Neck).Num(), 0);
	return true;
}
