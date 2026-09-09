// Copyright Animotive. All Rights Reserved.

#include "DatasetSidecarSerializer.h"

// RED PHASE: neutral bodies so the tests fail at runtime rather than failing to compile.

bool FDatasetSidecarSerializer::ToJson(const FDatasetSidecar& Sidecar, FString& OutJson)
{
	return false;
}

bool FDatasetSidecarSerializer::FromJson(const FString& Json, FDatasetSidecar& OutSidecar, FString& OutFailureReason)
{
	return false;
}

bool FDatasetSidecarSerializer::TryReadSchemaVersion(const FString& Json, int32& OutSchemaVersion)
{
	return false;
}

bool FDatasetSidecarSerializer::IsSupportedSchemaVersion(int32 SchemaVersion)
{
	return false;
}

bool FDatasetSidecarSerializer::SaveToFile(const FDatasetSidecar& Sidecar, const FString& FilePath, FString& OutFailureReason)
{
	return false;
}

bool FDatasetSidecarSerializer::LoadFromFile(const FString& FilePath, FDatasetSidecar& OutSidecar, FString& OutFailureReason)
{
	return false;
}
