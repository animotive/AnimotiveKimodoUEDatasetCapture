// Copyright Animotive. All Rights Reserved.

#include "ImpactClipId.h"

#include "ImpactParams.h"
#include "ImpactSchedule.h"

namespace
{
	/** Exact bit pattern of a float: no rounding can make two different values print alike. */
	FString Bits(float Value)
	{
		return FString::Printf(TEXT("%08x"), FMath::AsUInt(Value));
	}

	FString Bits(const FVector& Value)
	{
		return Bits(float(Value.X)) + TEXT(",") + Bits(float(Value.Y)) + TEXT(",") + Bits(float(Value.Z));
	}
}

uint64 FImpactClipId::Fnv1a64(TConstArrayView<uint8> Bytes)
{
	uint64 Hash = 0xcbf29ce484222325ull;
	for (const uint8 Byte : Bytes)
	{
		Hash ^= Byte;
		Hash *= 0x100000001b3ull;
	}
	return Hash;
}

FString FImpactClipId::CanonicalString(FName SourceAnimation, const FImpactParams& Params, const FImpactSchedule& Schedule)
{
	TStringBuilder<512> Out;
	// Format version first: changing the layout must never let an old id alias a new clip.
	Out << TEXT("v1|") << SourceAnimation.ToString();

	const FName Impactor = Params.Impactor.Archetype ? Params.Impactor.Archetype->Id : NAME_None;
	Out << TEXT("|imp:") << Impactor.ToString()
		<< TEXT("|m:") << Bits(Params.Impactor.MassKg)
		<< TEXT("|v:") << Bits(Params.Impactor.SpeedMetresPerSecond)
		<< TEXT("|r:") << Bits(Params.Impactor.ContactRadiusCm)
		<< TEXT("|t:") << Bits(Params.TransferredImpulseKgCmPerSecond)
		<< TEXT("|g:") << Bits(Params.ReactionGain)
		<< TEXT("|j:") << Bits(Params.ReactionImpulseKgCmPerSecond)
		<< TEXT("|reg:") << int32(Params.Region)
		<< TEXT("|dir:") << Bits(Params.DirectionLocal)
		<< TEXT("|ph:") << Bits(Params.SourcePhase)
		<< TEXT("|int:") << int32(Params.Intensity)
		<< TEXT("|mode:") << int32(Schedule.Mode)
		<< TEXT("|hits:") << Schedule.Hits.Num();

	// Order matters: swapping two hits is a different clip.
	for (const FImpactHit& Hit : Schedule.Hits)
	{
		Out << TEXT("|h:") << Hit.FrameOffset << TEXT(",") << Hit.DurationFrames
			<< TEXT(",") << Bits(Hit.ImpulseKgCmPerSecond) << TEXT(",") << Bits(Hit.DirectionLocal);
	}

	if (Schedule.Projectile.IsSet())
	{
		const FImpactProjectileSpec& Spec = Schedule.Projectile.GetValue();
		Out << TEXT("|proj:") << int32(Spec.Shape)
			<< TEXT(",") << Bits(Spec.SizeCm) << TEXT(",") << Bits(Spec.MassKg)
			<< TEXT(",") << Bits(Spec.SpeedMetresPerSecond) << TEXT(",") << Bits(Spec.LaunchDistanceCm)
			<< TEXT(",") << Bits(Spec.ExtraReactionImpulseKgCmPerSecond);
	}
	else
	{
		Out << TEXT("|proj:none");
	}
	return Out.ToString();
}

FString FImpactClipId::Make(FName SourceAnimation, const FImpactParams& Params, const FImpactSchedule& Schedule)
{
	const FTCHARToUTF8 Utf8(*CanonicalString(SourceAnimation, Params, Schedule));
	const uint64 Hash = Fnv1a64(TConstArrayView<uint8>(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length()));
	return FString::Printf(TEXT("imp_%016llx"), Hash);
}

namespace
{
	/** Letters and digits only: safe in package, object and file names. */
	FString PathSafe(const FString& Part)
	{
		FString Safe;
		for (const TCHAR Char : Part)
		{
			if (FChar::IsAlnum(Char))
			{
				Safe.AppendChar(Char);
			}
		}
		return Safe;
	}

	FString Capitalised(const FString& Word)
	{
		const FString Safe = PathSafe(Word);
		return Safe.IsEmpty() ? Safe : FString::Chr(FChar::ToUpper(Safe[0])) + Safe.Mid(1);
	}

	/** S1, L4, C1: a take or seed suffix, not part of the motion. */
	bool IsTakeSuffix(const FString& Token)
	{
		return Token.Len() >= 2 && FChar::IsAlpha(Token[0])
			&& Token.Mid(1).ToUpper() == Token.Mid(1) && Token.Mid(1).IsNumeric();
	}
}

FString FImpactClipName::Make(int32 Index, const FString& SourceLabel, const FString& Impactor, const FString& Region, const FString& Outcome)
{
	FString Name = IndexPrefix(Index);
	Name.LeftChopInline(1);
	for (const FString* Part : { &SourceLabel, &Impactor, &Region, &Outcome })
	{
		const FString Safe = PathSafe(*Part);
		if (!Safe.IsEmpty())
		{
			Name += TEXT("_") + Safe;
		}
	}
	return Name;
}

FString FImpactClipName::IndexPrefix(int32 Index)
{
	return FString::Printf(TEXT("IMP_%06d_"), Index);
}

FString FImpactClipName::SourceLabel(const FString& AssetName)
{
	FString Name = AssetName;
	const int32 Retargeted = Name.Find(TEXT("_KimodoRetargeted"), ESearchCase::IgnoreCase);
	if (Retargeted != INDEX_NONE)
	{
		Name.LeftInline(Retargeted);
	}
	TArray<FString> Tokens;
	Name.ParseIntoArray(Tokens, TEXT("_"));

	FString Label;
	for (int32 Index = 0; Index < Tokens.Num(); ++Index)
	{
		const FString& Token = Tokens[Index];
		const bool bPrefix = Index == 0 && Token.Equals(TEXT("M"), ESearchCase::IgnoreCase);
		const bool bBoilerplate = Token.Equals(TEXT("Kimodo"), ESearchCase::IgnoreCase) || Token.Equals(TEXT("Loop"), ESearchCase::IgnoreCase)
			|| Token.Equals(TEXT("F"), ESearchCase::IgnoreCase);
		if (!bPrefix && !bBoilerplate && !IsTakeSuffix(Token))
		{
			Label += Capitalised(Token);
		}
	}
	if (Label.IsEmpty())
	{
		for (const FString& Token : Tokens)
		{
			Label += Capitalised(Token);
		}
	}
	return Label;
}
