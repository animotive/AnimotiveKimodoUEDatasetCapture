// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAnimSequence;
class USkeletalMesh;
struct FReferenceSkeleton;

struct KIMODOIMPACTCAPTURE_API FImpactSourceClockConfig
{
	/** A source whose root travels faster than this is a moving source (a run). */
	float MovingSpeedThresholdCmPerSecond = 100.0f;
	/** Playback drop per unit of source speed lost along the direction of travel. */
	float DropGain = 2.5f;
	/** Playback never drops further than this. */
	float MaxDrop = 0.85f;
	/** Frames after the hit before the drop is measured and applied. */
	int32 MeasureDelayFrames = 2;
	/** Frames over which playback returns to full speed. */
	int32 RecoverFrames = 48;
};

/**
 * Source animation time for one clip. Moving or standing is decided once, from the source's own
 * root speed: deciding it per frame from the slowed-down speed once reclassified a slowed run as
 * idle mid-stumble. A hit against the direction of travel slows playback, and with it root
 * motion, in proportion to the momentum it removed.
 */
class KIMODOIMPACTCAPTURE_API FImpactSourceClock
{
public:
	FImpactSourceClock(double LoopLengthSeconds, const FVector& RootTravelPerLoop, const FImpactSourceClockConfig& InConfig);

	bool IsMoving() const;
	/** Root speed of the unwarped source, cm/s. */
	float NominalSpeedCmPerSecond() const;

	/**
	 * Records the hit's effect on playback. LostSpeedAlongTravel is the source-relative speed the
	 * body lost along the direction of travel (positive is slowed down). Standing sources ignore it.
	 */
	void ApplyHit(int32 HitFrame, float LostSpeedAlongTravelCmPerSecond);

	float GetDrop() const { return Drop; }

	/** Playback rate on a frame: 1 until the drop applies, then recovering to 1. */
	float RateAt(int32 Frame) const;

	/** Advances source time by one frame at that frame's rate; returns the new time. */
	double Advance(int32 Frame, float DeltaSeconds);

	double GetTime() const { return Time; }

	/** Completed loops at Time (never negative). */
	static int32 LoopIndex(double InTime, double LoopLength);

	/** Time within the current loop, in [0, LoopLength). */
	static double LoopLocalTime(double InTime, double LoopLength);

	/** Root offset after Loops whole loops, each re-based on where the last ended. */
	static FTransform LoopOffset(const FTransform& LoopDelta, int32 Loops);

	/**
	 * Where in the source a clip starts, from its phase (0-1). A looping source starts anywhere in
	 * its loop. A play-once source starts so the first hit, LeadInSeconds later, lands between
	 * its start and MinTailSeconds before its end: drawn over the whole length, a jump's clip
	 * could start after the landing and play only the standing tail. May be negative: the
	 * source then holds its first frame through part of the lead-in.
	 */
	static double StartSeconds(double Phase, double Length, bool bPlayOnce, double LeadInSeconds, double MinTailSeconds = 0.5);

private:
	FImpactSourceClockConfig Config;
	double LoopLength = 1.0;
	FVector RootTravel = FVector::ZeroVector;
	int32 HitFrame = INDEX_NONE;
	float Drop = 0.0f;
	double Time = 0.0;
};

/**
 * Samples a sequence onto a mesh's reference skeleton in component space, with root motion
 * carried across loops. Bones the sequence has no track for keep the mesh's reference pose.
 */
class KIMODOIMPACTCAPTURE_API FImpactPoseSampler
{
public:
	/** bPlayOnce: past its end the sequence holds its last frame rather than looping. */
	FImpactPoseSampler(const USkeletalMesh& Mesh, const UAnimSequence& Sequence, bool bPlayOnce = false);

	double GetLength() const { return Length; }
	const FTransform& GetLoopDelta() const { return LoopDelta; }

	/** Component-space pose within one loop, Time clamped to [0, Length]. */
	void SampleLoop(double InTime, TArray<FTransform>& OutComponentSpace) const;

	/** Component-space pose at any Time: root motion accumulated across loops, or held at the end when playing once. */
	void Sample(double InTime, TArray<FTransform>& OutComponentSpace) const;

private:
	const FReferenceSkeleton* MeshRef = nullptr;
	const UAnimSequence* Sequence = nullptr;
	TArray<int32> SkeletonIndex;
	double Length = 0.0;
	FTransform LoopDelta = FTransform::Identity;
	bool bPlayOnce = false;
};

/**
 * Whether the source is in the air: both feet clear of their reference height for a sustained
 * stretch. A person in the air cannot resist a push, so a hard hit mid-jump releases the body
 * rather than letting it follow the jump's own landing. Sustained, because a running stride has
 * a moment with both feet up too.
 */
struct KIMODOIMPACTCAPTURE_API FImpactSourceFlight
{
	/** True when every sample of both feet is more than ThresholdCm clear. False with no samples. */
	static bool IsInFlight(TConstArrayView<float> LeftClearanceCm, TConstArrayView<float> RightClearanceCm, float ThresholdCm);

	/**
	 * True when the source has both feet more than ThresholdCm above their reference-pose height
	 * from Time for WindowSeconds, sampled at 60 Hz. The 12 cm default is the jump R&D's flight rule.
	 */
	static bool IsInFlight(const FImpactPoseSampler& Sampler, const FReferenceSkeleton& Ref, int32 LeftFoot, int32 RightFoot, double Time,
		double WindowSeconds = 0.15, float ThresholdCm = 12.0f);
};
