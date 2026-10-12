// Copyright Animotive. All Rights Reserved.

#include "ImpactRandom.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactRandomGoldenValuesTest,
	"KimodoDatasetCapture.Impact.Random.GoldenValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactRandomGoldenValuesTest::RunTest(const FString& Parameters)
{
	// Pinned outputs. If these change, every clip any batch ever generated changes with them,
	// and "regenerate clip N from its seed" silently stops being true. Values computed from the
	// reference SplitMix64 definition, independently of this implementation.
	FImpactRandom Random(1234, 0, EImpactRandomStream::Impactor);
	TestEqual(TEXT("First value"), Random.NextUInt64(), 0x8592a247b7f5dbe9ull);
	TestEqual(TEXT("Second value"), Random.NextUInt64(), 0xb223dd10f9926917ull);
	TestEqual(TEXT("Third value"), Random.NextUInt64(), 0x85468b409b86c7ccull);

	FImpactRandom Unit(1234, 0, EImpactRandomStream::Impactor);
	TestEqual(TEXT("First unit value"), Unit.NextUnit(), 0.52176870585386526, 1e-15);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactRandomIndependenceTest,
	"KimodoDatasetCapture.Impact.Random.Independence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactRandomIndependenceTest::RunTest(const FString& Parameters)
{
	constexpr int32 Draws = 64;
	auto Sequence = [](int32 Seed, int32 Clip, EImpactRandomStream Stream)
	{
		FImpactRandom Random(Seed, Clip, Stream);
		TArray<uint64> Values;
		for (int32 Index = 0; Index < Draws; ++Index)
		{
			Values.Add(Random.NextUInt64());
		}
		return Values;
	};
	auto Matches = [](const TArray<uint64>& A, const TArray<uint64>& B)
	{
		int32 Same = 0;
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			Same += A[Index] == B[Index] ? 1 : 0;
		}
		return Same;
	};

	// Same inputs, same stream: identical, however many times it is rebuilt.
	TestTrue(TEXT("Same seed, clip and stream repeat exactly"),
		Sequence(7, 42, EImpactRandomStream::Direction) == Sequence(7, 42, EImpactRandomStream::Direction));

	// Adjacent clips must not share values; adjacent seeds are the worst case for weak mixing.
	TestEqual(TEXT("Adjacent clips share no values"),
		Matches(Sequence(7, 42, EImpactRandomStream::Direction), Sequence(7, 43, EImpactRandomStream::Direction)), 0);
	TestEqual(TEXT("Adjacent batch seeds share no values"),
		Matches(Sequence(7, 42, EImpactRandomStream::Direction), Sequence(8, 42, EImpactRandomStream::Direction)), 0);
	TestEqual(TEXT("Different streams of one clip share no values"),
		Matches(Sequence(7, 42, EImpactRandomStream::Direction), Sequence(7, 42, EImpactRandomStream::Impactor)), 0);

	// Clip index and batch seed are not interchangeable: (1, 2) and (2, 1) are different clips.
	TestEqual(TEXT("Seed and clip index do not commute"),
		Matches(Sequence(1, 2, EImpactRandomStream::Timing), Sequence(2, 1, EImpactRandomStream::Timing)), 0);

	// Order independence: draining one stream first changes nothing about another.
	FImpactRandom Impactor(7, 42, EImpactRandomStream::Impactor);
	for (int32 Index = 0; Index < 1000; ++Index)
	{
		Impactor.NextUInt64();
	}
	TestTrue(TEXT("Direction stream unaffected by draws on another stream"),
		Sequence(7, 42, EImpactRandomStream::Direction) == Sequence(7, 42, EImpactRandomStream::Direction));

	// Negative seeds and indices are legal inputs and still distinct from their positive twins.
	TestEqual(TEXT("Negative seed differs from positive"),
		Matches(Sequence(-5, 0, EImpactRandomStream::Caption), Sequence(5, 0, EImpactRandomStream::Caption)), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactRandomRangesTest,
	"KimodoDatasetCapture.Impact.Random.Ranges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactRandomRangesTest::RunTest(const FString& Parameters)
{
	FImpactRandom Random(99, 3, EImpactRandomStream::Timing);
	constexpr int32 Draws = 10000;

	double UnitSum = 0.0;
	bool bUnitInRange = true;
	bool bRangeInRange = true;
	for (int32 Index = 0; Index < Draws; ++Index)
	{
		const double Unit = Random.NextUnit();
		bUnitInRange &= Unit >= 0.0 && Unit < 1.0;
		UnitSum += Unit;

		const double Value = Random.Range(-2.5, 7.5);
		bRangeInRange &= Value >= -2.5 && Value < 7.5;
	}
	TestTrue(TEXT("NextUnit stays in [0, 1)"), bUnitInRange);
	TestTrue(TEXT("Range stays in [Min, Max)"), bRangeInRange);
	// Uniform mean is 0.5; 10k draws put the standard error near 0.003.
	TestEqual(TEXT("NextUnit is uniform on average"), UnitSum / Draws, 0.5, 0.02);

	// RangeInt is inclusive on both ends and reaches both.
	bool bSawMin = false, bSawMax = false, bIntInRange = true;
	for (int32 Index = 0; Index < Draws; ++Index)
	{
		const int32 Value = Random.RangeInt(3, 6);
		bIntInRange &= Value >= 3 && Value <= 6;
		bSawMin |= Value == 3;
		bSawMax |= Value == 6;
	}
	TestTrue(TEXT("RangeInt stays in [Min, Max]"), bIntInRange);
	TestTrue(TEXT("RangeInt reaches Min"), bSawMin);
	TestTrue(TEXT("RangeInt reaches Max"), bSawMax);

	// Degenerate ranges return Min rather than garbage.
	TestEqual(TEXT("Empty real range returns Min"), Random.Range(4.0, 4.0), 4.0);
	TestEqual(TEXT("Inverted real range returns Min"), Random.Range(5.0, 1.0), 5.0);
	TestEqual(TEXT("Single-value int range returns it"), Random.RangeInt(9, 9), 9);
	TestEqual(TEXT("Inverted int range returns Min"), Random.RangeInt(9, 2), 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImpactRandomUnitVectorTest,
	"KimodoDatasetCapture.Impact.Random.UnitVector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImpactRandomUnitVectorTest::RunTest(const FString& Parameters)
{
	FImpactRandom Random(5, 5, EImpactRandomStream::Direction);
	constexpr int32 Draws = 2000;
	bool bUnitLength = true;
	uint8 Octants = 0;
	FVector Sum = FVector::ZeroVector;
	for (int32 Index = 0; Index < Draws; ++Index)
	{
		const FVector Direction = Random.UnitVector();
		bUnitLength &= FMath::IsNearlyEqual(Direction.Size(), 1.0, 1e-6);
		const int32 Octant = (Direction.X >= 0 ? 1 : 0) | (Direction.Y >= 0 ? 2 : 0) | (Direction.Z >= 0 ? 4 : 0);
		Octants |= uint8(1 << Octant);
		Sum += Direction;
	}
	TestTrue(TEXT("Every direction is unit length"), bUnitLength);
	TestEqual(TEXT("All eight octants reached"), int32(Octants), 0xFF);
	// A uniform sphere has no preferred direction; the mean vector shrinks toward zero.
	TestTrue(TEXT("No directional bias"), (Sum / Draws).Size() < 0.1);
	return true;
}
