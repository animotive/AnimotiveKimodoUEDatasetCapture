// Copyright Animotive. All Rights Reserved.

#include "DatasetPawnResolver.h"
#include "Misc/AutomationTest.h"

namespace
{
	FDatasetPawnFacts MakeFacts(bool bWorld, bool bController, bool bPawn, bool bMesh)
	{
		FDatasetPawnFacts Facts;
		Facts.bHasWorld = bWorld;
		Facts.bHasPlayerController = bController;
		Facts.bHasPawn = bPawn;
		Facts.bHasSkeletalMesh = bMesh;

		return Facts;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetPawnResolverDecisionTest,
	"KimodoDatasetCapture.PawnResolver.Decision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetPawnResolverDecisionTest::RunTest(const FString& Parameters)
{
	// Everything present.
	TestEqual(
		TEXT("All facts present resolves"),
		static_cast<uint8>(FDatasetPawnResolver::Evaluate(MakeFacts(true, true, true, true))),
		static_cast<uint8>(EDatasetPawnResolution::Resolved));

	// Each missing link reports the FIRST thing missing, not the last. An operator who has
	// not pressed Play should be told that, rather than being told their pawn has no mesh.
	TestEqual(
		TEXT("No world reports no world"),
		static_cast<uint8>(FDatasetPawnResolver::Evaluate(MakeFacts(false, false, false, false))),
		static_cast<uint8>(EDatasetPawnResolution::NoWorld));
	TestEqual(
		TEXT("World but no controller reports no controller"),
		static_cast<uint8>(FDatasetPawnResolver::Evaluate(MakeFacts(true, false, false, false))),
		static_cast<uint8>(EDatasetPawnResolution::NoPlayerController));
	TestEqual(
		TEXT("Controller but no pawn reports no pawn"),
		static_cast<uint8>(FDatasetPawnResolver::Evaluate(MakeFacts(true, true, false, false))),
		static_cast<uint8>(EDatasetPawnResolution::NoPawn));
	TestEqual(
		TEXT("Pawn but no skeletal mesh reports no mesh"),
		static_cast<uint8>(FDatasetPawnResolver::Evaluate(MakeFacts(true, true, true, false))),
		static_cast<uint8>(EDatasetPawnResolution::NoSkeletalMesh));

	// Contradictory facts must still report the earliest gap rather than trusting the later
	// ones. A pawn without a world cannot be recorded whatever else is true.
	TestEqual(
		TEXT("A mesh without a world still reports no world"),
		static_cast<uint8>(FDatasetPawnResolver::Evaluate(MakeFacts(false, true, true, true))),
		static_cast<uint8>(EDatasetPawnResolution::NoWorld));
	TestEqual(
		TEXT("A pawn without a controller still reports no controller"),
		static_cast<uint8>(FDatasetPawnResolver::Evaluate(MakeFacts(true, false, true, true))),
		static_cast<uint8>(EDatasetPawnResolution::NoPlayerController));

	// Only Resolved is a success.
	TestFalse(TEXT("Resolved is not a failure"), FDatasetPawnResolver::IsFailure(EDatasetPawnResolution::Resolved));
	TestTrue(TEXT("NoWorld is a failure"), FDatasetPawnResolver::IsFailure(EDatasetPawnResolution::NoWorld));
	TestTrue(TEXT("NoPlayerController is a failure"), FDatasetPawnResolver::IsFailure(EDatasetPawnResolution::NoPlayerController));
	TestTrue(TEXT("NoPawn is a failure"), FDatasetPawnResolver::IsFailure(EDatasetPawnResolution::NoPawn));
	TestTrue(TEXT("NoSkeletalMesh is a failure"), FDatasetPawnResolver::IsFailure(EDatasetPawnResolution::NoSkeletalMesh));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetPawnResolverMessagesTest,
	"KimodoDatasetCapture.PawnResolver.FailureMessages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetPawnResolverMessagesTest::RunTest(const FString& Parameters)
{
	// Success says nothing. A "success message" would be noise on take forty.
	TestTrue(
		TEXT("Resolved has no failure text"),
		FDatasetPawnResolver::DescribeFailure(EDatasetPawnResolution::Resolved).IsEmpty());

	const EDatasetPawnResolution Failures[] =
	{
		EDatasetPawnResolution::NoWorld,
		EDatasetPawnResolution::NoPlayerController,
		EDatasetPawnResolution::NoPawn,
		EDatasetPawnResolution::NoSkeletalMesh,
	};

	for (const EDatasetPawnResolution Failure : Failures)
	{
		const FText Message = FDatasetPawnResolver::DescribeFailure(Failure);
		const FString MessageString = Message.ToString();

		if (Message.IsEmpty())
		{
			AddError(FString::Printf(TEXT("Failure %d has no message; the panel would show nothing."), static_cast<int32>(Failure)));
			continue;
		}

		// The reader is a client animator, so engine-internal vocabulary is a defect here,
		// not a style preference. See the voice rules in product-guidelines.md.
		const TCHAR* Jargon[] = { TEXT("APawn"), TEXT("USkeletalMeshComponent"), TEXT("UWorld"), TEXT("APlayerController"), TEXT("nullptr") };
		for (const TCHAR* Term : Jargon)
		{
			if (MessageString.Contains(Term))
			{
				AddError(FString::Printf(TEXT("Message '%s' contains engine jargon '%s'."), *MessageString, Term));
			}
		}

		// Every failure message must tell the operator what to do next, not just what broke.
		// Checked by requiring a second sentence rather than by matching exact wording, so
		// the assertion survives rewording.
		const int32 SentenceCount = MessageString.Len() - MessageString.Replace(TEXT("."), TEXT("")).Len();
		if (SentenceCount < 2)
		{
			AddError(FString::Printf(
				TEXT("Message '%s' states the problem but not the next action."), *MessageString));
		}
	}

	// The two most common cases in a real session, checked for specific guidance.
	TestTrue(
		TEXT("No world tells the operator to start play"),
		FDatasetPawnResolver::DescribeFailure(EDatasetPawnResolution::NoWorld).ToString().Contains(TEXT("play"), ESearchCase::IgnoreCase));
	TestTrue(
		TEXT("No skeletal mesh mentions animation"),
		FDatasetPawnResolver::DescribeFailure(EDatasetPawnResolution::NoSkeletalMesh).ToString().Contains(TEXT("animation"), ESearchCase::IgnoreCase));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetPawnResolverNoWorldTest,
	"KimodoDatasetCapture.PawnResolver.ResolveWithoutAWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetPawnResolverNoWorldTest::RunTest(const FString& Parameters)
{
	// The one engine-facing path reachable headlessly: arming with nothing playing. This is
	// also the most common operator mistake, so it must not crash and must not resolve.
	FDatasetPawnTarget Target;
	const EDatasetPawnResolution Result = FDatasetPawnResolver::ResolveFromWorld(nullptr, Target);

	TestEqual(
		TEXT("Resolving without a world reports no world"),
		static_cast<uint8>(Result),
		static_cast<uint8>(EDatasetPawnResolution::NoWorld));
	TestTrue(TEXT("Failure leaves no target"), FDatasetPawnResolver::IsFailure(Result));
	TestFalse(TEXT("Target is not usable"), Target.IsStillValid());
	TestFalse(TEXT("No pawn was written"), Target.Pawn.IsValid());
	TestFalse(TEXT("No mesh was written"), Target.SkeletalMesh.IsValid());

	// An empty target must report itself unusable rather than crashing on a stale pointer,
	// because a pawn destroyed mid-take leaves exactly this state.
	FDatasetPawnTarget Empty;
	TestFalse(TEXT("Default target is not usable"), Empty.IsStillValid());

	return true;
}
