// Copyright Animotive. All Rights Reserved.

#include "ImpactorCatalog.h"

#include "ImpactRandom.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactorCatalog)

#define LOCTEXT_NAMESPACE "ImpactorCatalog"

bool FImpactorArchetype::Collapses(EImpactBodyRegion Region) const
{
	return bIncapacitatesHeadAndNeck && (Region == EImpactBodyRegion::Head || Region == EImpactBodyRegion::Neck);
}

float FImpactorSample::ImpulseKgCmPerSecond() const
{
	// Momentum in kg x m/s, scaled to the engine's centimetres.
	return MassKg * SpeedMetresPerSecond * 100.0f;
}

namespace
{
	FImpactorArchetype Make(
		const TCHAR* Id, const TCHAR* Noun, const TCHAR* Description, TArray<FString> Verbs,
		EImpactorShape Shape, float SizeCm, FFloatInterval MassKg, FFloatInterval SpeedMetresPerSecond,
		FFloatInterval ContactRadiusCm, EImpactDeliveryMode Delivery)
	{
		FImpactorArchetype Archetype;
		Archetype.Id = Id;
		Archetype.NounPhrase = Noun;
		Archetype.Description = Description;
		Archetype.Verbs = MoveTemp(Verbs);
		Archetype.Shape = Shape;
		Archetype.SizeCm = SizeCm;
		Archetype.MassKg = MassKg;
		Archetype.SpeedMetresPerSecond = SpeedMetresPerSecond;
		Archetype.ContactRadiusCm = ContactRadiusCm;
		Archetype.DefaultDelivery = Delivery;
		return Archetype;
	}

	bool IsPositiveRange(const FFloatInterval& Range)
	{
		return Range.Min > 0.0f && Range.Min <= Range.Max;
	}
}

bool FImpactorArchetype::CanStrike(EImpactBodyRegion Region) const
{
	return Regions.IsEmpty() || Regions.Contains(Region);
}

int32 FImpactorArchetype::ContactFrames(int32 FrameRate) const
{
	// Tolerance so a contact time that is a whole number of frames does not round up by float error.
	return FMath::Max(1, FMath::CeilToInt32(ContactSeconds * float(FMath::Max(0, FrameRate)) - 1e-3f));
}

TArray<FImpactorArchetype> UImpactorCatalog::MakeStarterArchetypes()
{
	using enum EImpactorShape;
	using enum EImpactDeliveryMode;

	// Masses and speeds are real-world figures. For people and limbs, mass is the effective
	// mass behind the blow, not the whole body.
	TArray<FImpactorArchetype> Starter = {
		Make(TEXT("SmallBullet"), TEXT("a small bullet"), TEXT("A small-calibre pistol round."),
			{ TEXT("shot") }, Sphere, 0.9f, { 0.006f, 0.010f }, { 250.0f, 380.0f }, { 0.5f, 1.0f }, Instant),
		Make(TEXT("RifleRound"), TEXT("a rifle round"), TEXT("A high-velocity rifle bullet."),
			{ TEXT("shot") }, Sphere, 0.6f, { 0.004f, 0.012f }, { 700.0f, 950.0f }, { 0.5f, 1.0f }, Instant),
		Make(TEXT("ShotgunBlast"), TEXT("a shotgun blast"), TEXT("A spread of shotgun pellets arriving together."),
			{ TEXT("blasted"), TEXT("shot") }, Area, 15.0f, { 0.030f, 0.040f }, { 350.0f, 450.0f }, { 8.0f, 20.0f }, Burst),
		Make(TEXT("ThrownStone"), TEXT("a thrown stone"), TEXT("A fist-sized stone thrown by hand."),
			{ TEXT("hit"), TEXT("struck") }, Sphere, 6.0f, { 0.2f, 0.5f }, { 10.0f, 20.0f }, { 2.0f, 4.0f }, Projectile),
		Make(TEXT("Baseball"), TEXT("a baseball"), TEXT("A pitched or batted baseball."),
			{ TEXT("hit"), TEXT("struck") }, Sphere, 7.4f, { 0.142f, 0.149f }, { 25.0f, 45.0f }, { 3.0f, 4.0f }, Projectile),
		Make(TEXT("Football"), TEXT("a football"), TEXT("A kicked football."),
			{ TEXT("hit"), TEXT("struck") }, Sphere, 22.0f, { 0.41f, 0.45f }, { 15.0f, 30.0f }, { 8.0f, 11.0f }, Projectile),
		Make(TEXT("Punch"), TEXT("a punch"), TEXT("A closed-fist punch."),
			{ TEXT("punched"), TEXT("hit") }, Sphere, 10.0f, { 2.0f, 5.0f }, { 5.0f, 12.0f }, { 4.0f, 6.0f }, Instant),
		Make(TEXT("Kick"), TEXT("a kick"), TEXT("A full kick from another person."),
			{ TEXT("kicked") }, Capsule, 25.0f, { 5.0f, 10.0f }, { 6.0f, 14.0f }, { 6.0f, 10.0f }, Instant),
		Make(TEXT("Shove"), TEXT("a two-handed shove"), TEXT("Another person pushing with both hands."),
			{ TEXT("shoved"), TEXT("pushed") }, Area, 40.0f, { 20.0f, 40.0f }, { 1.5f, 3.0f }, { 15.0f, 25.0f }, Sustained),
		Make(TEXT("SportsTackle"), TEXT("a tackle"), TEXT("A full-body sports tackle from another person."),
			{ TEXT("tackled"), TEXT("hit") }, Area, 50.0f, { 70.0f, 110.0f }, { 4.0f, 8.0f }, { 20.0f, 35.0f }, Instant),
		Make(TEXT("SwingingBat"), TEXT("a swinging bat"), TEXT("A baseball bat swung at full strength."),
			{ TEXT("struck"), TEXT("hit") }, Capsule, 85.0f, { 0.8f, 1.0f }, { 25.0f, 40.0f }, { 3.0f, 6.0f }, Instant),
		Make(TEXT("CarBumper"), TEXT("a car"), TEXT("The front of a moving car."),
			{ TEXT("hit"), TEXT("struck"), TEXT("rammed") }, Area, 180.0f, { 900.0f, 1600.0f }, { 3.0f, 12.0f }, { 30.0f, 60.0f }, Instant),
		Make(TEXT("WreckingBall"), TEXT("a wrecking ball"), TEXT("A swinging demolition wrecking ball."),
			{ TEXT("struck"), TEXT("hit") }, Sphere, 90.0f, { 500.0f, 2000.0f }, { 2.0f, 6.0f }, { 30.0f, 45.0f }, Instant),
		Make(TEXT("WindPush"), TEXT("a gust of wind"), TEXT("A strong, sustained gust of wind."),
			{ TEXT("buffeted"), TEXT("pushed") }, Area, 200.0f, { 5.0f, 20.0f }, { 10.0f, 25.0f }, { 60.0f, 100.0f }, Sustained),
	};

	// Reaction gains. Physical transfer is the reduced-mass momentum; the gain turns it into the
	// reaction the simulation applies. Projectiles transfer almost nothing, yet people react hard
	// to them: a small bullet lands near 18k kg cm/s (medium), a rifle round near 32k (heavy).
	// Heavy impactors already transfer body-scale momentum and need little or none.
	const TMap<FName, float> Gains = {
		{ TEXT("SmallBullet"), 60.0f }, { TEXT("RifleRound"), 40.0f }, { TEXT("ShotgunBlast"), 12.0f },
		{ TEXT("ThrownStone"), 4.0f }, { TEXT("Baseball"), 4.0f }, { TEXT("Football"), 3.0f },
		{ TEXT("Punch"), 2.5f }, { TEXT("Kick"), 2.0f }, { TEXT("Shove"), 1.5f },
		{ TEXT("SportsTackle"), 1.5f }, { TEXT("SwingingBat"), 2.5f }, { TEXT("CarBumper"), 1.0f },
		{ TEXT("WreckingBall"), 1.5f }, { TEXT("WindPush"), 1.0f },
	};
	// Contact times: wide, heavy impactors keep pushing; everything else lands in one frame.
	const TMap<FName, float> ContactTimes = {
		{ TEXT("CarBumper"), 0.1f }, { TEXT("WreckingBall"), 0.08f }, { TEXT("SportsTackle"), 0.15f },
	};
	// Where each impactor can come from and what it can reach. Elevation is the direction of
	// travel (positive travels upward); a missing entry is anywhere. A car travels level and
	// reaches the hips and legs; drawn over the whole sphere it hit a thigh from below.
	using enum EImpactBodyRegion;
	const TArray<EImpactBodyRegion> Arms = { UpperArmLeft, UpperArmRight, LowerArmLeft, LowerArmRight };
	const TArray<EImpactBodyRegion> Legs = { ThighLeft, ThighRight, ShinLeft, ShinRight };
	auto Join = [](std::initializer_list<TArray<EImpactBodyRegion>> Parts)
	{
		TArray<EImpactBodyRegion> All;
		for (const TArray<EImpactBodyRegion>& Part : Parts)
		{
			All.Append(Part);
		}
		return All;
	};
	struct FReach
	{
		FFloatInterval Elevation;
		TArray<EImpactBodyRegion> Regions;
	};
	const TMap<FName, FReach> Reach = {
		{ TEXT("SmallBullet"), { { -60.0f, 30.0f }, {} } },
		{ TEXT("RifleRound"), { { -60.0f, 30.0f }, {} } },
		{ TEXT("ShotgunBlast"), { { -45.0f, 20.0f }, {} } },
		{ TEXT("ThrownStone"), { { -45.0f, 15.0f }, {} } },
		{ TEXT("Baseball"), { { -30.0f, 15.0f }, {} } },
		{ TEXT("Football"), { { -20.0f, 35.0f }, {} } },
		{ TEXT("Punch"), { { -20.0f, 20.0f }, Join({ { Head, Neck, Chest, Abdomen, HandLeft, HandRight }, Arms }) } },
		{ TEXT("Kick"), { { -10.0f, 40.0f }, Join({ { Head, Chest, Abdomen, Pelvis }, Legs }) } },
		{ TEXT("Shove"), { { -10.0f, 10.0f }, { Chest, Abdomen, UpperArmLeft, UpperArmRight } } },
		{ TEXT("SportsTackle"), { { -15.0f, 15.0f }, { Chest, Abdomen, Pelvis, ThighLeft, ThighRight } } },
		{ TEXT("SwingingBat"), { { -25.0f, 20.0f }, Join({ { Head, Chest, Abdomen, Pelvis }, Arms, Legs }) } },
		{ TEXT("CarBumper"), { { -5.0f, 10.0f }, Join({ { Abdomen, Pelvis }, Legs }) } },
		{ TEXT("WreckingBall"), { { -10.0f, 20.0f }, { Head, Neck, Chest, Abdomen, Pelvis, UpperArmLeft, UpperArmRight } } },
		{ TEXT("WindPush"), { { -10.0f, 10.0f }, {} } },
	};
	for (FImpactorArchetype& Archetype : Starter)
	{
		Archetype.ReactionGain = Gains.FindRef(Archetype.Id, 1.0f);
		Archetype.ContactSeconds = ContactTimes.FindRef(Archetype.Id, 0.0f);
		// Shot in the head or neck, a person goes down whatever the round's momentum.
		Archetype.bIncapacitatesHeadAndNeck = Archetype.Id == TEXT("SmallBullet") || Archetype.Id == TEXT("RifleRound") || Archetype.Id == TEXT("ShotgunBlast");
		if (const FReach* Found = Reach.Find(Archetype.Id))
		{
			Archetype.ElevationDegrees = Found->Elevation;
			Archetype.Regions = Found->Regions;
		}
	}
	return Starter;
}

int32 UImpactorCatalog::FillWithStarterArchetypes()
{
	// Undoable in the editor; a no-op outside a transaction.
	Modify();
	int32 Added = 0;
	for (FImpactorArchetype& Starter : MakeStarterArchetypes())
	{
		const bool bPresent = Archetypes.ContainsByPredicate([&Starter](const FImpactorArchetype& A) { return A.Id == Starter.Id; });
		if (!bPresent)
		{
			Archetypes.Add(MoveTemp(Starter));
			++Added;
		}
	}
	return Added;
}

bool UImpactorCatalog::Validate(FText& OutReason) const
{
	if (Archetypes.IsEmpty())
	{
		OutReason = LOCTEXT("Empty", "The impactor catalog has no archetypes. Add at least one, or start from the starter set.");
		return false;
	}

	TSet<FName> Seen;
	bool bAnyWeight = false;
	for (const FImpactorArchetype& Archetype : Archetypes)
	{
		const FText Name = Archetype.Id.IsNone() ? LOCTEXT("Unnamed", "(unnamed)") : FText::FromName(Archetype.Id);
		auto Fail = [&OutReason, &Name](const FText& Problem)
		{
			OutReason = FText::Format(LOCTEXT("ArchetypeProblem", "Impactor {0}: {1}"), Name, Problem);
			return false;
		};

		if (Archetype.Id.IsNone())
		{
			return Fail(LOCTEXT("NoId", "it has no id."));
		}
		if (Seen.Contains(Archetype.Id))
		{
			return Fail(LOCTEXT("DuplicateId", "another archetype already uses this id."));
		}
		Seen.Add(Archetype.Id);

		if (Archetype.NounPhrase.TrimStartAndEnd().IsEmpty())
		{
			return Fail(LOCTEXT("NoNoun", "it has no noun phrase, so captions cannot name it."));
		}
		if (Archetype.Description.TrimStartAndEnd().IsEmpty())
		{
			return Fail(LOCTEXT("NoDescription", "it has no description."));
		}
		if (Archetype.Verbs.IsEmpty())
		{
			return Fail(LOCTEXT("NoVerbs", "it has no verbs, so captions cannot say what it did."));
		}
		if (!(Archetype.SizeCm > 0.0f))
		{
			return Fail(LOCTEXT("BadSize", "its size must be greater than zero."));
		}
		if (!IsPositiveRange(Archetype.MassKg))
		{
			return Fail(LOCTEXT("BadMass", "its mass range must be above zero with min no greater than max."));
		}
		if (!IsPositiveRange(Archetype.SpeedMetresPerSecond))
		{
			return Fail(LOCTEXT("BadSpeed", "its speed range must be above zero with min no greater than max."));
		}
		if (!IsPositiveRange(Archetype.ContactRadiusCm))
		{
			return Fail(LOCTEXT("BadRadius", "its contact radius range must be above zero with min no greater than max."));
		}
		if (Archetype.Weight < 0.0f)
		{
			return Fail(LOCTEXT("NegativeWeight", "its weight cannot be negative."));
		}
		bAnyWeight |= Archetype.Weight > 0.0f;
	}

	if (!bAnyWeight)
	{
		OutReason = LOCTEXT("NoWeight", "Every impactor has zero weight, so none can be chosen. Give at least one a positive weight.");
		return false;
	}
	return true;
}

const FImpactorArchetype* UImpactorCatalog::SampleArchetype(FImpactRandom& Random) const
{
	double Total = 0.0;
	for (const FImpactorArchetype& Archetype : Archetypes)
	{
		Total += FMath::Max(Archetype.Weight, 0.0f);
	}
	if (Total <= 0.0)
	{
		return nullptr;
	}

	const double Pick = Random.Range(0.0, Total);
	double Cumulative = 0.0;
	const FImpactorArchetype* LastPositive = nullptr;
	for (const FImpactorArchetype& Archetype : Archetypes)
	{
		if (Archetype.Weight <= 0.0f)
		{
			continue;
		}
		LastPositive = &Archetype;
		Cumulative += Archetype.Weight;
		if (Pick < Cumulative)
		{
			return &Archetype;
		}
	}
	// Floating-point slack at the top end lands on the last weighted archetype.
	return LastPositive;
}

FImpactorSample UImpactorCatalog::SampleInstance(const FImpactorArchetype& Archetype, FImpactRandom& Random)
{
	FImpactorSample Sample;
	Sample.Archetype = &Archetype;
	Sample.MassKg = float(Random.Range(Archetype.MassKg.Min, Archetype.MassKg.Max));
	Sample.SpeedMetresPerSecond = float(Random.Range(Archetype.SpeedMetresPerSecond.Min, Archetype.SpeedMetresPerSecond.Max));
	Sample.ContactRadiusCm = float(Random.Range(Archetype.ContactRadiusCm.Min, Archetype.ContactRadiusCm.Max));
	return Sample;
}

#undef LOCTEXT_NAMESPACE
