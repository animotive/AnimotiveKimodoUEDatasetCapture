// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasetSidecar.h"

/**
 * Reads and writes sidecar JSON.
 *
 * The sidecar is the contract shared with everything downstream, including the legacy-build
 * capture companion tool described in conductor/tech-stack.md. Two rules follow from that and
 * are enforced here rather than by convention:
 *
 *   1. The schema version is readable on its own, without parsing or understanding the rest
 *      of the file. TryReadSchemaVersion does that and nothing else.
 *   2. A file whose version this build does not understand is REJECTED, loudly, with the
 *      version reported. Reading an unknown schema optimistically is how a training set
 *      quietly acquires misinterpreted fields.
 */
class KIMODODATASETCAPTURE_API FDatasetSidecarSerializer
{
public:
	/** Serialises Sidecar to pretty-printed JSON. False if serialisation failed. */
	static bool ToJson(const FDatasetSidecar& Sidecar, FString& OutJson);

	/**
	 * Parses Json into OutSidecar.
	 *
	 * False when the text is not valid JSON, or when its schema version is one this build
	 * does not support. OutFailureReason receives plain language suitable for an operator.
	 */
	static bool FromJson(const FString& Json, FDatasetSidecar& OutSidecar, FString& OutFailureReason);

	/**
	 * Reads only the schema version.
	 *
	 * Succeeds even when the rest of the document is a version this build cannot interpret,
	 * which is the whole point: a reader must be able to say "this file is version 7, I speak
	 * version 1" instead of failing without explanation.
	 */
	static bool TryReadSchemaVersion(const FString& Json, int32& OutSchemaVersion);

	/** True when this build can interpret a sidecar of that version. */
	static bool IsSupportedSchemaVersion(int32 SchemaVersion);

	/** Writes Sidecar to FilePath as UTF-8 JSON, creating directories as needed. */
	static bool SaveToFile(const FDatasetSidecar& Sidecar, const FString& FilePath, FString& OutFailureReason);

	/** Reads a sidecar from FilePath. */
	static bool LoadFromFile(const FString& FilePath, FDatasetSidecar& OutSidecar, FString& OutFailureReason);
};
