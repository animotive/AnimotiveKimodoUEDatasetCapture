// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasetInputSample.h"

#include "DatasetSidecar.generated.h"

/**
 * Sidecar schema version.
 *
 * Bump this whenever the shape or meaning of any field below changes. A reader must be able
 * to tell which version produced a file without understanding the rest of it, because the
 * legacy-build capture companion tool (see conductor/tech-stack.md) has to emit datasets
 * this same pipeline ingests.
 *
 * Version history:
 *   1 - initial: identity and timing, raw input stream, skeleton identity, labels.
 *
 * WIRE FORMAT NOTES. These are consequences of how FJsonObjectConverter serialises the
 * structs below, and they are part of the contract with the training pipeline, so they are
 * recorded here rather than left to be rediscovered from a file:
 *
 *   - Field names are lower camel case: "schemaVersion", "takeName", "frameNumber".
 *     Renaming a C++ member renames the JSON field and breaks the contract.
 *   - Booleans keep their Unreal "b" prefix, so validity is written as "bIsValid". Ugly on
 *     the wire, and left alone deliberately: a custom serialiser to prettify one field name
 *     would be a permanent maintenance cost, and the pipeline reads a documented name either
 *     way. Do not "fix" it without a schema version bump.
 *   - Enum-valued fields are written as their NAMES, e.g. "layers": ["RawInput"]. Renaming an
 *     enumerator therefore changes the file format even though no field changed. Add
 *     enumerators; never rename or reorder them.
 *   - Floats are written with full double precision ("0.98000001907348633" for 0.98f), which
 *     is verbose but exactly round-trippable. A round-trip test asserts that.
 *
 * A representative sample of the current schema is produced by the test suite at
 * Saved/KimodoDatasetCapture/SidecarSchemaSample.json, so it cannot drift from the code.
 */
inline constexpr int32 KDatasetSidecarSchemaVersion = 1;

/** Which capture layers a take actually contains. The opt-in layers may be absent. */
UENUM()
enum class EDatasetCaptureLayer : uint8
{
	/** Raw gamepad state. Always present in a valid take. */
	RawInput,

	/** Enhanced Input semantic actions. Absent in projects not using Enhanced Input. */
	SemanticInput,

	/** CharacterMovementComponent state. Absent in projects not using ACharacter. */
	CharacterState,
};

/** Identity of the take and the time base its frames are counted on. */
USTRUCT()
struct KIMODODATASETCAPTURE_API FDatasetSidecarIdentity
{
	GENERATED_BODY()

	/** Take name as Take Recorder assigned it. */
	UPROPERTY()
	FString TakeName;

	/** Package path of the recorded LevelSequence. */
	UPROPERTY()
	FString LevelSequencePath;

	/** Package path of the recorded AnimSequence, when Take Recorder produced one. */
	UPROPERTY()
	FString AnimSequencePath;

	/**
	 * Display frame rate of the take, as a rational number.
	 *
	 * Stored as an explicit numerator and denominator rather than a float, because 30000/1001
	 * is not representable as a float and a dataset that rounds its frame rate cannot be
	 * aligned back to its source.
	 */
	UPROPERTY()
	int32 FrameRateNumerator = 0;

	UPROPERTY()
	int32 FrameRateDenominator = 0;

	/** MovieScene tick resolution, also rational. Frame numbers are counted on this. */
	UPROPERTY()
	int32 TickResolutionNumerator = 0;

	UPROPERTY()
	int32 TickResolutionDenominator = 0;

	/** UTC capture start, ISO 8601. Provenance only; never used for frame alignment. */
	UPROPERTY()
	FString CaptureStartedUtc;

	/** Number of frames recorded, before any trim is applied. */
	UPROPERTY()
	int32 DurationFrames = 0;

	bool operator==(const FDatasetSidecarIdentity& Other) const;
	bool operator!=(const FDatasetSidecarIdentity& Other) const { return !(*this == Other); }
};

/** The per-frame raw input stream, keyed by frame number. */
USTRUCT()
struct KIMODODATASETCAPTURE_API FDatasetSidecarInputStream
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FDatasetInputSample> Samples;

	bool operator==(const FDatasetSidecarInputStream& Other) const;
	bool operator!=(const FDatasetSidecarInputStream& Other) const { return !(*this == Other); }
};

/** Which rig the motion was recorded on. */
USTRUCT()
struct KIMODODATASETCAPTURE_API FDatasetSidecarSkeleton
{
	GENERATED_BODY()

	/** Package path of the USkeleton the recorded character used. */
	UPROPERTY()
	FString SkeletonPath;

	UPROPERTY()
	int32 BoneCount = 0;

	/**
	 * Stable hash of the ordered bone name and parent index pairs, as hex.
	 *
	 * Lets a downstream pipeline decide whether two clips share a rig without opening Unreal,
	 * and catches the case where a skeleton keeps its asset path while its hierarchy changes.
	 */
	UPROPERTY()
	FString BoneHierarchyFingerprint;

	bool operator==(const FDatasetSidecarSkeleton& Other) const;
	bool operator!=(const FDatasetSidecarSkeleton& Other) const { return !(*this == Other); }
};

/** Operator-supplied labels, plus the trim range and validity of the clip. */
USTRUCT()
struct KIMODODATASETCAPTURE_API FDatasetSidecarLabels
{
	GENERATED_BODY()

	/** Natural-language caption. This is what the model trains on; never empty on a save. */
	UPROPERTY()
	FString Caption;

	/** Tags drawn from the project-defined vocabulary. May be empty. */
	UPROPERTY()
	TArray<FString> Tags;

	/** Trim range in frames, inclusive. Defaults to the full recorded range. */
	UPROPERTY()
	int32 TrimInFrame = 0;

	UPROPERTY()
	int32 TrimOutFrame = 0;

	/** False when capture failed. An invalid clip never reaches an export bundle. */
	UPROPERTY()
	bool bIsValid = true;

	/** Plain-language reason, set only when bIsValid is false. */
	UPROPERTY()
	FString InvalidReason;

	bool operator==(const FDatasetSidecarLabels& Other) const;
	bool operator!=(const FDatasetSidecarLabels& Other) const { return !(*this == Other); }
};

/**
 * One captured clip's sidecar, written next to the take.
 *
 * Derived from the MovieScene input section at save time rather than accumulated alongside
 * it, so there is no second source of truth able to drift from the recorded take.
 */
USTRUCT()
struct KIMODODATASETCAPTURE_API FDatasetSidecar
{
	GENERATED_BODY()

	/** Always written, and readable without parsing anything else in the file. */
	UPROPERTY()
	int32 SchemaVersion = KDatasetSidecarSchemaVersion;

	UPROPERTY()
	FDatasetSidecarIdentity Identity;

	UPROPERTY()
	FDatasetSidecarInputStream InputStream;

	UPROPERTY()
	FDatasetSidecarSkeleton Skeleton;

	UPROPERTY()
	FDatasetSidecarLabels Labels;

	/** Capture layers present in this clip. RawInput is always present in a valid clip. */
	UPROPERTY()
	TArray<EDatasetCaptureLayer> Layers;

	/** Number of samples in the raw input stream. */
	int32 GetSampleCount() const;

	/**
	 * Appends a sample. Fails, changing nothing, if FrameNumber is not strictly greater than
	 * the last sample's — an out-of-order stream would misalign every downstream lookup, so
	 * it is rejected at the point of entry rather than discovered later.
	 */
	bool AddSample(const FDatasetInputSample& Sample);

	/** Frame-keyed lookup. False when no sample carries that exact frame number. */
	bool TryGetSampleAtFrame(int32 FrameNumber, FDatasetInputSample& OutSample) const;

	/** First and last recorded frame numbers. Unset when the stream is empty. */
	TOptional<int32> GetFirstFrame() const;
	TOptional<int32> GetLastFrame() const;

	/** True when Layer is recorded in this clip. */
	bool HasLayer(EDatasetCaptureLayer Layer) const;

	/** Records Layer as present. Adding a layer twice is a no-op. */
	void AddLayer(EDatasetCaptureLayer Layer);

	bool operator==(const FDatasetSidecar& Other) const;
	bool operator!=(const FDatasetSidecar& Other) const { return !(*this == Other); }
};
