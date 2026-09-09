// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"

class APawn;
class UPlayerInput;
class USkeletalMeshComponent;
class UWorld;

/** Outcome of looking for something to record. */
enum class EDatasetPawnResolution : uint8
{
	/** A player pawn with a skeletal mesh was found. */
	Resolved,

	/** No world to look in. Nothing is playing. */
	NoWorld,

	/** A world, but no local player controller in it. */
	NoPlayerController,

	/** A player controller with no pawn. Usually a spectator, or a pawn not yet spawned. */
	NoPawn,

	/** A pawn with no skeletal mesh. There is no animation to record. */
	NoSkeletalMesh,
};

/**
 * What the resolver observed. Separated from the engine lookup so the decision can be tested
 * headlessly for every combination, including the ones that need a running game to reach.
 */
struct FDatasetPawnFacts
{
	bool bHasWorld = false;
	bool bHasPlayerController = false;
	bool bHasPawn = false;
	bool bHasSkeletalMesh = false;
};

/** What was found, when something was. */
struct KIMODODATASETCAPTURE_API FDatasetPawnTarget
{
	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMesh;
	TWeakObjectPtr<UPlayerInput> PlayerInput;

	/** True when everything needed to record is still alive. */
	bool IsStillValid() const;
};

/**
 * Finds the pawn to record, through public engine surfaces only.
 *
 * Nothing here requires the client's project to cooperate: no base class, no interface to
 * implement, no component to add. It asks the world for the local player controller, the
 * controller for its pawn, and the pawn for a skeletal mesh. A project that cannot answer
 * gets a stated reason, not a silent empty take.
 */
class KIMODODATASETCAPTURE_API FDatasetPawnResolver
{
public:
	/** The decision, given what was observed. Pure, and exhaustively tested. */
	static EDatasetPawnResolution Evaluate(const FDatasetPawnFacts& Facts);

	/** True when Result means nothing can be recorded. */
	static bool IsFailure(EDatasetPawnResolution Result);

	/**
	 * Operator-facing explanation of a failure, stating what happened and what to do next.
	 * Empty for Resolved.
	 */
	static FText DescribeFailure(EDatasetPawnResolution Result);

	/** Looks in World and fills OutTarget when the result is Resolved. */
	static EDatasetPawnResolution ResolveFromWorld(UWorld* World, FDatasetPawnTarget& OutTarget);
};
