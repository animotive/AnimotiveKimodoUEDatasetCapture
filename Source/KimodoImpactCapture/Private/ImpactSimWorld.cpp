// Copyright Animotive. All Rights Reserved.

#include "ImpactSimWorld.h"

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UObjectGlobals.h"

FImpactSimWorld::FImpactSimWorld(float InGroundHeight)
	: GroundHeight(InGroundHeight)
{
	World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("KimodoImpactSimWorld"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	if (!World->HasBegunPlay())
	{
		// No game mode in a bare world: without this, components never register their ticks.
		World->GetWorldSettings()->NotifyBeginPlay();
	}

	AActor* Floor = World->SpawnActor<AActor>();
	UBoxComponent* FloorBox = NewObject<UBoxComponent>(Floor);
	constexpr double HalfThickness = 10.0;
	FloorBox->SetBoxExtent(FVector(20000.0, 20000.0, HalfThickness));
	FloorBox->SetCollisionProfileName(TEXT("BlockAll"));
	Floor->SetRootComponent(FloorBox);
	FloorBox->RegisterComponent();
	FloorBox->SetWorldLocation(FVector(0.0, 0.0, GroundHeight - HalfThickness));
}

FImpactSimWorld::~FImpactSimWorld()
{
	if (!World)
	{
		return;
	}
	World->EndPlay(EEndPlayReason::Quit);
	World->BeginTearingDown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	World = nullptr;
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

void FImpactSimWorld::Step(float DeltaSeconds, int32 Substeps)
{
	// Substeps here, not through the engine's global substepping, so the editor's own physics
	// settings are untouched.
	const int32 Count = FMath::Max(1, Substeps);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Each substep is its own engine frame: the tick task manager runs a tick function at most
		// once per GFrameCounter.
		++GFrameCounter;
		World->Tick(LEVELTICK_All, DeltaSeconds / float(Count));
	}
}
