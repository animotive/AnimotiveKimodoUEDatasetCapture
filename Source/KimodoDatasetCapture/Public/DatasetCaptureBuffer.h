// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasetInputSample.h"

/**
 * Fixed-capacity store for one take's samples.
 *
 * Reserved once before recording starts and never grown during it. The per-frame capture path
 * must not allocate: the data describes the game's own motion, so a hitch caused by capture
 * perturbs the thing being measured.
 *
 * When the buffer is full it REFUSES further samples rather than growing. That refusal is the
 * signal the recorder uses to stop at the configured maximum duration, so the constraint and
 * the auto-stop are the same mechanism rather than two that could disagree.
 *
 * This is not a UObject and holds no engine state, so a whole take's worth of capture can be
 * exercised in a headless test.
 */
class KIMODODATASETCAPTURE_API FDatasetCaptureBuffer
{
public:
	/**
	 * Reserves room for MaxSamples and clears any existing content.
	 *
	 * Call once, before recording. Calling it during a take would be the very allocation this
	 * class exists to avoid.
	 */
	void Reserve(int32 MaxSamples);

	/**
	 * Appends a sample.
	 *
	 * False, changing nothing, when the buffer is full or when Sample.FrameNumber does not
	 * advance past the last sample's. Both are refusals rather than accommodations: growing
	 * would allocate mid-capture, and accepting an out-of-order frame would misalign every
	 * lookup after it.
	 */
	bool Add(const FDatasetInputSample& Sample);

	/** Samples recorded so far. */
	int32 Num() const;

	/** Samples this buffer can hold. */
	int32 Capacity() const;

	/** True when no further sample can be added. */
	bool IsFull() const;

	/** Clears the samples, keeping the capacity so the next take needs no allocation. */
	void Reset();

	/** The recorded samples, in frame order. */
	TArrayView<const FDatasetInputSample> GetSamples() const;

	/**
	 * How many times the underlying storage moved since the last Reserve.
	 *
	 * Instrumentation, not bookkeeping. It exists so a test can ASSERT that a full take
	 * allocates nothing, instead of the claim living only in a comment. Any value above zero
	 * during a capture is a defect.
	 */
	int32 GetReallocationCount() const;

private:
	TArray<FDatasetInputSample> Samples;

	/** Storage address observed after the last Reserve or Add, to detect a move. */
	const FDatasetInputSample* LastKnownData = nullptr;

	int32 ReallocationCount = 0;

	int32 MaxSampleCount = 0;
};
