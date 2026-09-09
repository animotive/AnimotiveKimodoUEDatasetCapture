// Copyright Animotive. All Rights Reserved.

#include "DatasetPawnResolver.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "KimodoDatasetCaptureLog.h"

#define LOCTEXT_NAMESPACE "DatasetPawnResolver"

bool FDatasetPawnTarget::IsStillValid() const
{
	// Weak pointers, checked every time rather than cached. PIE ends and actors die
	// mid-take, and a stale raw pointer here would crash the editor during a capture.
	// The player input is not required: a pawn can be recorded while the operator's hands
	// are off the pad, and losing input is a degraded capture rather than an impossible one.
	return Pawn.IsValid() && SkeletalMesh.IsValid();
}

EDatasetPawnResolution FDatasetPawnResolver::Evaluate(const FDatasetPawnFacts& Facts)
{
	// Checked in dependency order, returning the FIRST gap. An operator who has not pressed
	// Play must hear about that, not about their pawn lacking a mesh, so the order of these
	// four tests is the behaviour rather than an implementation detail.
	if (!Facts.bHasWorld)
	{
		return EDatasetPawnResolution::NoWorld;
	}

	if (!Facts.bHasPlayerController)
	{
		return EDatasetPawnResolution::NoPlayerController;
	}

	if (!Facts.bHasPawn)
	{
		return EDatasetPawnResolution::NoPawn;
	}

	if (!Facts.bHasSkeletalMesh)
	{
		return EDatasetPawnResolution::NoSkeletalMesh;
	}

	return EDatasetPawnResolution::Resolved;
}

bool FDatasetPawnResolver::IsFailure(EDatasetPawnResolution Result)
{
	return Result != EDatasetPawnResolution::Resolved;
}

FText FDatasetPawnResolver::DescribeFailure(EDatasetPawnResolution Result)
{
	// Each message names what happened and what to do next, in words a client animator can
	// act on. No engine types appear, by design and by test.
	switch (Result)
	{
	case EDatasetPawnResolution::NoWorld:
		return LOCTEXT("NoWorld", "Nothing is playing. Press play, then arm the capture.");

	case EDatasetPawnResolution::NoPlayerController:
		return LOCTEXT("NoPlayerController", "No player was found in the running game. Make sure a player is spawned before arming the capture.");

	case EDatasetPawnResolution::NoPawn:
		return LOCTEXT("NoPawn", "The player has no character to record. Take control of a character, then arm the capture.");

	case EDatasetPawnResolution::NoSkeletalMesh:
		return LOCTEXT("NoSkeletalMesh", "The player's character has no skeletal mesh, so there is no animation to record. Pick a character with a skeletal mesh.");

	case EDatasetPawnResolution::Resolved:
	default:
		return FText::GetEmpty();
	}
}

EDatasetPawnResolution FDatasetPawnResolver::ResolveFromWorld(UWorld* World, FDatasetPawnTarget& OutTarget)
{
	// Reset first, so a failed resolve can never leave a caller holding the previous take's
	// target and recording the wrong character.
	OutTarget = FDatasetPawnTarget();

	FDatasetPawnFacts Facts;
	Facts.bHasWorld = World != nullptr;

	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	Facts.bHasPlayerController = PlayerController != nullptr;

	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	Facts.bHasPawn = Pawn != nullptr;

	// FindComponentByClass, not a cast to ACharacter. A client's player pawn need not derive
	// from ACharacter, and requiring it would be exactly the integration tax the product
	// guide rules out.
	USkeletalMeshComponent* SkeletalMesh = Pawn ? Pawn->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	Facts.bHasSkeletalMesh = SkeletalMesh != nullptr;

	const EDatasetPawnResolution Result = Evaluate(Facts);
	if (IsFailure(Result))
	{
		UE_LOG(
			LogKimodoDatasetCapture,
			Verbose,
			TEXT("Pawn resolution failed: %s"),
			*DescribeFailure(Result).ToString());
		return Result;
	}

	OutTarget.Pawn = Pawn;
	OutTarget.SkeletalMesh = SkeletalMesh;

	// Player input is optional. It is the source of the raw input layer, but a take with a
	// pawn and no readable input is a degraded capture, not an impossible one, so its absence
	// does not fail resolution.
	OutTarget.PlayerInput = PlayerController->PlayerInput;

	return EDatasetPawnResolution::Resolved;
}

#undef LOCTEXT_NAMESPACE
