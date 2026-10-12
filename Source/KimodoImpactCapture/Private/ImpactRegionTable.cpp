// Copyright Animotive. All Rights Reserved.

#include "ImpactRegionTable.h"

#include "PhysicsEngine/PhysicsAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactRegionTable)

#define LOCTEXT_NAMESPACE "ImpactRegionTable"

TArray<FImpactRegionBodies> UImpactRegionTable::MakeUEFNMannequinEntries()
{
	using enum EImpactBodyRegion;
	return {
		{ Head, { TEXT("head") } },
		{ Neck, { TEXT("neck_01") } },
		{ Chest, { TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05") } },
		{ Abdomen, { TEXT("spine_01"), TEXT("spine_02") } },
		{ Pelvis, { TEXT("pelvis") } },
		{ UpperArmLeft, { TEXT("upperarm_l") } },
		{ UpperArmRight, { TEXT("upperarm_r") } },
		{ LowerArmLeft, { TEXT("lowerarm_l") } },
		{ LowerArmRight, { TEXT("lowerarm_r") } },
		{ HandLeft, { TEXT("hand_l") } },
		{ HandRight, { TEXT("hand_r") } },
		{ ThighLeft, { TEXT("thigh_l") } },
		{ ThighRight, { TEXT("thigh_r") } },
		{ ShinLeft, { TEXT("calf_l") } },
		{ ShinRight, { TEXT("calf_r") } },
		{ FootLeft, { TEXT("foot_l") } },
		{ FootRight, { TEXT("foot_r") } },
	};
}

bool UImpactRegionTable::Validate(const UPhysicsAsset& PhysicsAsset, FText& OutReason) const
{
	if (Entries.IsEmpty())
	{
		OutReason = LOCTEXT("Empty", "The region table is empty. Map every body region to at least one physics body.");
		return false;
	}

	const UEnum* RegionEnum = StaticEnum<EImpactBodyRegion>();
	auto RegionName = [RegionEnum](EImpactBodyRegion Region) { return FText::FromString(RegionEnum->GetNameStringByValue(int64(Region))); };

	TSet<EImpactBodyRegion> Mapped;
	for (const FImpactRegionBodies& Entry : Entries)
	{
		if (Mapped.Contains(Entry.Region))
		{
			OutReason = FText::Format(LOCTEXT("Duplicate", "Region {0} is mapped more than once."), RegionName(Entry.Region));
			return false;
		}
		Mapped.Add(Entry.Region);

		if (Entry.Bodies.IsEmpty())
		{
			OutReason = FText::Format(LOCTEXT("NoBodies", "Region {0} has no physics bodies."), RegionName(Entry.Region));
			return false;
		}
		for (const FName& Body : Entry.Bodies)
		{
			if (PhysicsAsset.FindBodyIndex(Body) == INDEX_NONE)
			{
				OutReason = FText::Format(
					LOCTEXT("UnknownBody", "Region {0} names body {1}, which is not in physics asset {2}."),
					RegionName(Entry.Region), FText::FromName(Body), FText::FromString(PhysicsAsset.GetName()));
				return false;
			}
		}
	}

	// Every region a batch can choose must be mapped (the last enum entry is the generated _MAX).
	for (int32 Index = 0; Index < RegionEnum->NumEnums() - 1; ++Index)
	{
		const EImpactBodyRegion Region = EImpactBodyRegion(RegionEnum->GetValueByIndex(Index));
		if (!Mapped.Contains(Region))
		{
			OutReason = FText::Format(LOCTEXT("Unmapped", "Region {0} is not mapped to any physics body."), RegionName(Region));
			return false;
		}
	}
	return true;
}

TArray<FName> UImpactRegionTable::BodiesFor(EImpactBodyRegion Region) const
{
	for (const FImpactRegionBodies& Entry : Entries)
	{
		if (Entry.Region == Region)
		{
			return Entry.Bodies;
		}
	}
	return {};
}

#undef LOCTEXT_NAMESPACE
