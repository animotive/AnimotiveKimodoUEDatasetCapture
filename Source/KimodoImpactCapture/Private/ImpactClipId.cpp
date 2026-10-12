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
