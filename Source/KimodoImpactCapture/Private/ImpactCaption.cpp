// Copyright Animotive. All Rights Reserved.

#include "ImpactCaption.h"

#include "ImpactorCatalog.h"
#include "ImpactRandom.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactCaption)

namespace
{
	/** Picks one of Options from the caption stream. */
	const TCHAR* Pick(FImpactRandom& Random, std::initializer_list<const TCHAR*> Options)
	{
		const int32 Index = Random.RangeInt(0, int32(Options.size()) - 1);
		return *(Options.begin() + Index);
	}

	/** Dominant axis of travel: 0 = X, 1 = Y, 2 = Z. */
	int32 DominantAxis(const FVector& Direction)
	{
		const FVector Abs = Direction.GetAbs();
		return Abs.X >= Abs.Y && Abs.X >= Abs.Z ? 0 : (Abs.Y >= Abs.Z ? 1 : 2);
	}

	/** Which way the body is pushed, for a stagger: away from where the impactor came from. */
	const TCHAR* AwayPhrase(const FVector& Direction)
	{
		switch (DominantAxis(Direction))
		{
		case 0: return Direction.X < 0.0 ? TEXT("backwards") : TEXT("forwards");
		case 1: return TEXT("sideways");
		default: return TEXT("off balance");
		}
	}

	/** True when Verb only repeats the impactor's noun: "punched" with "a punch". */
	bool RepeatsNoun(const FString& Verb, const FString& Noun)
	{
		FString Head = Noun;
		int32 Space = INDEX_NONE;
		if (Head.FindLastChar(TEXT(' '), Space))
		{
			Head.RightChopInline(Space + 1);
		}
		return Head.Len() >= 3 && Verb.Len() >= 3 && Verb.Left(4).Equals(Head.Left(4), ESearchCase::IgnoreCase);
	}

	FString OutcomePhrase(const FImpactCaptionInput& Input, FImpactRandom& Random)
	{
		const TCHAR* Away = AwayPhrase(Input.DirectionLocal);
		switch (Input.Outcome)
		{
		case EImpactOutcome::Flinch:
			if (Input.Motion.bTravels)
			{
				const int32 Choice = Random.RangeInt(0, 2);
				return Choice == 0 ? FString::Printf(TEXT("flinches but keeps %s"), *Input.Motion.Participle)
					: Choice == 1 ? FString(TEXT("flinches but keeps going"))
					: FString(TEXT("barely breaks stride"));
			}
			return Pick(Random, { TEXT("flinches"), TEXT("recoils slightly"), TEXT("jolts but holds their position") });
		case EImpactOutcome::Stagger:
			if (Input.Steps >= 2)
			{
				const int32 Choice = Random.RangeInt(0, 2);
				return Choice == 0 ? FString::Printf(TEXT("staggers %s a few steps"), Away)
					: Choice == 1 ? FString::Printf(TEXT("stumbles %s but stays on their feet"), Away)
					: FString::Printf(TEXT("is knocked %s and catches their balance"), Away);
			}
			if (Input.Steps == 1)
			{
				const int32 Choice = Random.RangeInt(0, 2);
				return Choice == 0 ? FString::Printf(TEXT("takes a step %s to stay upright"), Away)
					: Choice == 1 ? FString::Printf(TEXT("stumbles a step %s"), Away)
					: FString::Printf(TEXT("staggers %s but recovers"), Away);
			}
			return Pick(Random, { TEXT("sways but keeps their footing"), TEXT("rocks but stays upright") });
		case EImpactOutcome::Knockdown:
			if (Input.bTrip)
			{
				return Pick(Random, { TEXT("trips, falling forwards"), TEXT("trips and falls forwards"), TEXT("trips, going down hard") });
			}
			return Pick(Random, { TEXT("falls to the ground"), TEXT("is knocked down"), TEXT("loses their balance and falls") });
		case EImpactOutcome::Collapse:
			return Pick(Random, { TEXT("collapses"), TEXT("crumples to the ground"), TEXT("drops where they stand"), TEXT("collapses to the ground") });
		case EImpactOutcome::Ragdoll:
		default:
			return Pick(Random, { TEXT("is thrown to the ground"), TEXT("is knocked off their feet"), TEXT("is flung to the ground") });
		}
	}

	struct FMotionWord
	{
		const TCHAR* Token;
		const TCHAR* Participle;
		const TCHAR* Type;
	};
}

FImpactSourceMotion FImpactCaption::ResolveMotion(const UImpactSourceMotionTable* Table, FName Animation)
{
	if (Table)
	{
		for (const FImpactSourceMotionEntry& Entry : Table->Entries)
		{
			if (Entry.Animation == Animation && !Entry.Motion.Participle.IsEmpty())
			{
				return Entry.Motion;
			}
		}
	}
	return MotionFromName(Animation);
}

FImpactSourceMotion FImpactCaption::MotionFromName(FName Animation)
{
	TArray<FString> Tokens;
	Animation.ToString().ParseIntoArray(Tokens, TEXT("_"));
	auto Has = [&Tokens](const TCHAR* Token)
	{
		return Tokens.ContainsByPredicate([Token](const FString& T) { return T.Equals(Token, ESearchCase::IgnoreCase); });
	};

	// First match wins, most specific motion first.
	static const FMotionWord Words[] = {
		{ TEXT("Sprint"), TEXT("sprinting"), TEXT("sprint") },
		{ TEXT("Run"), TEXT("running"), TEXT("run") },
		{ TEXT("Jog"), TEXT("jogging"), TEXT("jog") },
		{ TEXT("Crouch"), TEXT("crouching"), TEXT("crouch") },
		{ TEXT("Walk"), TEXT("walking"), TEXT("walk") },
		{ TEXT("Jump"), TEXT("jumping"), TEXT("jump") },
		{ TEXT("Idle"), TEXT("standing idle"), TEXT("idle") },
		{ TEXT("Stand"), TEXT("standing idle"), TEXT("idle") },
	};
	FImpactSourceMotion Motion;
	Motion.Participle = TEXT("moving");
	Motion.Type = TEXT("other");
	for (const FMotionWord& Word : Words)
	{
		if (Has(Word.Token))
		{
			Motion.Participle = Word.Participle;
			Motion.Type = Word.Type;
			break;
		}
	}
	Motion.bTravels = Has(TEXT("Sprint")) || Has(TEXT("Run")) || Has(TEXT("Jog")) || Has(TEXT("Walk"));
	return Motion;
}

FString FImpactCaption::RegionPhrase(EImpactBodyRegion Region)
{
	switch (Region)
	{
	case EImpactBodyRegion::Head: return TEXT("the head");
	case EImpactBodyRegion::Neck: return TEXT("the neck");
	case EImpactBodyRegion::Chest: return TEXT("the chest");
	case EImpactBodyRegion::Abdomen: return TEXT("the stomach");
	case EImpactBodyRegion::Pelvis: return TEXT("the hips");
	case EImpactBodyRegion::UpperArmLeft: return TEXT("the left upper arm");
	case EImpactBodyRegion::UpperArmRight: return TEXT("the right upper arm");
	case EImpactBodyRegion::LowerArmLeft: return TEXT("the left forearm");
	case EImpactBodyRegion::LowerArmRight: return TEXT("the right forearm");
	case EImpactBodyRegion::HandLeft: return TEXT("the left hand");
	case EImpactBodyRegion::HandRight: return TEXT("the right hand");
	case EImpactBodyRegion::ThighLeft: return TEXT("the left thigh");
	case EImpactBodyRegion::ThighRight: return TEXT("the right thigh");
	case EImpactBodyRegion::ShinLeft: return TEXT("the left shin");
	case EImpactBodyRegion::ShinRight: return TEXT("the right shin");
	case EImpactBodyRegion::FootLeft: return TEXT("the left foot");
	case EImpactBodyRegion::FootRight: return TEXT("the right foot");
	default: return TEXT("the body");
	}
}

FString FImpactCaption::DirectionPhrase(const FVector& DirectionLocal)
{
	// The impactor travels along DirectionLocal, so it arrives from the opposite side.
	switch (DominantAxis(DirectionLocal))
	{
	case 0: return DirectionLocal.X < 0.0 ? TEXT("from the front") : TEXT("from behind");
	case 1: return DirectionLocal.Y > 0.0 ? TEXT("from the left") : TEXT("from the right");
	default: return DirectionLocal.Z < 0.0 ? TEXT("from above") : TEXT("from below");
	}
}

FString FImpactCaption::Describe(const FImpactCaptionInput& Input, int32 BatchSeed, int32 ClipIndex)
{
	if (!Input.Impactor)
	{
		return FString();
	}
	FImpactRandom Random(BatchSeed, ClipIndex, EImpactRandomStream::Caption);

	const FString& Noun = Input.Impactor->NounPhrase;
	TArray<FString> Verbs;
	for (const FString& Verb : Input.Impactor->Verbs)
	{
		if (!Verb.IsEmpty() && !RepeatsNoun(Verb, Noun))
		{
			Verbs.Add(Verb);
		}
	}
	const FString Verb = Verbs.IsEmpty() ? FString(TEXT("hit")) : Verbs[Random.RangeInt(0, Verbs.Num() - 1)];
	// "shot with a small bullet" and "hit with a bat" read well; anything with a car does not:
	// "with" only for something a person could wield or throw.
	const bool bWithVerb = Verb == TEXT("shot") || Verb == TEXT("blasted") || Verb == TEXT("hit") || Verb == TEXT("struck");
	const bool bWithReads = bWithVerb && Input.Impactor->SizeCm <= 100.0f;
	const TCHAR* Preposition = bWithReads && Random.RangeInt(0, 1) == 1 ? TEXT("with") : TEXT("by");

	const FString Motion = Input.Motion.Participle.IsEmpty() ? FString(TEXT("moving")) : Input.Motion.Participle;
	const FString Region = RegionPhrase(Input.Region);
	const FString Direction = DirectionPhrase(Input.DirectionLocal);
	const FString Outcome = OutcomePhrase(Input, Random);

	switch (Random.RangeInt(0, 2))
	{
	case 0:
		return FString::Printf(TEXT("A person %s is %s in %s %s %s %s and %s."),
			*Motion, *Verb, *Region, Preposition, *Noun, *Direction, *Outcome);
	case 1:
		return FString::Printf(TEXT("While %s, a person is %s in %s %s %s %s and %s."),
			*Motion, *Verb, *Region, Preposition, *Noun, *Direction, *Outcome);
	default:
		return FString::Printf(TEXT("A person is %s in %s %s %s %s while %s, and %s."),
			*Verb, *Region, Preposition, *Noun, *Direction, *Motion, *Outcome);
	}
}

TArray<FString> FImpactCaption::Descriptions(const FImpactCaptionInput& Input, int32 BatchSeed, int32 ClipIndex, int32 Count)
{
	TArray<FString> Out;
	if (!Input.Impactor)
	{
		return Out;
	}
	// The caption first, then the same clip described from other caption streams until enough
	// phrasings differ; a clip whose wording runs out gets fewer.
	for (int32 Variant = 0; Variant < 64 && Out.Num() < Count; ++Variant)
	{
		Out.AddUnique(Describe(Input, BatchSeed + Variant * 7919, ClipIndex));
	}
	return Out;
}

TArray<FString> FImpactCaption::Tags(const FImpactCaptionInput& Input)
{
	return {
		TEXT("impact"),
		Input.Impactor ? Input.Impactor->Id.ToString() : FString(TEXT("unknown")),
		StaticEnum<EImpactBodyRegion>()->GetNameStringByValue(int64(Input.Region)),
		StaticEnum<EImpactOutcome>()->GetNameStringByValue(int64(Input.Outcome)),
		StaticEnum<EImpactIntensity>()->GetNameStringByValue(int64(Input.Intensity)),
		Input.Motion.Type.IsEmpty() ? FString(TEXT("other")) : Input.Motion.Type,
	};
}
