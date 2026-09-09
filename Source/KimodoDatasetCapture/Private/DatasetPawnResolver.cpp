// Copyright Animotive. All Rights Reserved.

#include "DatasetPawnResolver.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

// RED PHASE: neutral bodies so the tests fail at runtime rather than failing to compile.

bool FDatasetPawnTarget::IsStillValid() const
{
	return false;
}

EDatasetPawnResolution FDatasetPawnResolver::Evaluate(const FDatasetPawnFacts& Facts)
{
	return EDatasetPawnResolution::Resolved;
}

bool FDatasetPawnResolver::IsFailure(EDatasetPawnResolution Result)
{
	return false;
}

FText FDatasetPawnResolver::DescribeFailure(EDatasetPawnResolution Result)
{
	return FText::GetEmpty();
}

EDatasetPawnResolution FDatasetPawnResolver::ResolveFromWorld(UWorld* World, FDatasetPawnTarget& OutTarget)
{
	return EDatasetPawnResolution::Resolved;
}
