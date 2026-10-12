// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ImpactParams.h"

#include "ImpactRegionTable.generated.h"

class UPhysicsAsset;

/** The physics bodies that make up one body region on one skeleton. */
USTRUCT()
struct KIMODOIMPACTCAPTURE_API FImpactRegionBodies
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Region")
	EImpactBodyRegion Region = EImpactBodyRegion::Chest;

	/** Physics body (bone) names, in the physics asset. */
	UPROPERTY(EditAnywhere, Category = "Region")
	TArray<FName> Bodies;
};

/**
 * Maps body regions to physics bodies for one skeleton. Data, so a project with a different rig
 * supplies its own table instead of changing code.
 */
UCLASS(BlueprintType)
class KIMODOIMPACTCAPTURE_API UImpactRegionTable : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Regions")
	TArray<FImpactRegionBodies> Entries;

	/**
	 * Adds a UEFN mannequin entry for every region the table does not already map. Never replaces
	 * an existing mapping. Shown as a button on the asset's Details panel. Returns how many were added.
	 */
	UFUNCTION(CallInEditor, Category = "Regions")
	int32 FillWithUEFNMannequinEntries();

	/** Table for the UEFN mannequin, which GameAnimationSample's animations are authored on. */
	static TArray<FImpactRegionBodies> MakeUEFNMannequinEntries();

	/**
	 * True when every region is mapped exactly once to at least one body, and every named body
	 * exists in PhysicsAsset. On failure, OutReason names the region or body at fault.
	 */
	bool Validate(const UPhysicsAsset& PhysicsAsset, FText& OutReason) const;

	/** Bodies for a region, or empty when the region is not mapped. */
	TArray<FName> BodiesFor(EImpactBodyRegion Region) const;
};
