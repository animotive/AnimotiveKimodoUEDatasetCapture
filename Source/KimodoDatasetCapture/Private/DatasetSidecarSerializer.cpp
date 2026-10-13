// Copyright Animotive. All Rights Reserved.

#include "DatasetSidecarSerializer.h"

#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "KimodoDatasetCaptureLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "DatasetSidecarSerializer"

namespace
{
	const FString SchemaVersionField = TEXT("SchemaVersion");

	/** Parses Json into a JSON object. Null when the text is not a JSON object. */
	TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
	{
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Json);

		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return nullptr;
		}

		return Root;
	}
}

bool FDatasetSidecarSerializer::ToJson(const FDatasetSidecar& Sidecar, FString& OutJson)
{
	OutJson.Reset();

	const TSharedPtr<FJsonObject> Root = FJsonObjectConverter::UStructToJsonObject(Sidecar, 0, 0);
	if (!Root.IsValid())
	{
		UE_LOG(LogKimodoDatasetCapture, Error, TEXT("Failed to serialise sidecar for take '%s'."), *Sidecar.Identity.TakeName);
		return false;
	}

	// Blocks that do not apply are absent, not empty: a synthetic clip has no raw input to
	// zero-fill, and a recorded take has no impact.
	if (Sidecar.IsSyntheticImpact())
	{
		Root->RemoveField(TEXT("inputStream"));
		Root->RemoveField(TEXT("layers"));
	}
	else
	{
		Root->RemoveField(TEXT("impact"));
	}

	// Pretty-printed on purpose. A sidecar is read by people diagnosing a dataset at least
	// as often as by tools, and the size difference is irrelevant next to the take itself.
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		UE_LOG(LogKimodoDatasetCapture, Error, TEXT("Failed to write sidecar JSON for take '%s'."), *Sidecar.Identity.TakeName);
		return false;
	}

	return true;
}

bool FDatasetSidecarSerializer::FromJson(const FString& Json, FDatasetSidecar& OutSidecar, FText& OutFailureReason)
{
	OutFailureReason = FText::GetEmpty();

	const TSharedPtr<FJsonObject> Root = ParseJsonObject(Json);
	if (!Root.IsValid())
	{
		OutFailureReason = LOCTEXT("NotJson", "This file is not valid JSON.");
		return false;
	}

	// Version is checked BEFORE any field is read, so an unknown schema can never be
	// partially interpreted into a sidecar this build thinks it understands.
	int32 SchemaVersion = 0;
	if (!Root->TryGetNumberField(SchemaVersionField, SchemaVersion))
	{
		OutFailureReason = LOCTEXT("NoSchemaVersion", "This file has no schema version, so it is not a dataset sidecar.");
		return false;
	}

	if (!IsSupportedSchemaVersion(SchemaVersion))
	{
		OutFailureReason = FText::Format(
			LOCTEXT("UnsupportedSchemaVersion", "This sidecar is schema version {0}. This plugin reads version {1}. Update the plugin to read it."),
			FText::AsNumber(SchemaVersion, &FNumberFormattingOptions::DefaultNoGrouping()),
			FText::AsNumber(KDatasetSidecarSchemaVersion, &FNumberFormattingOptions::DefaultNoGrouping()));
		return false;
	}

	FDatasetSidecar Parsed;
	if (!FJsonObjectConverter::JsonObjectToUStruct(Root.ToSharedRef(), &Parsed, 0, 0))
	{
		OutFailureReason = LOCTEXT("BadLayout", "This sidecar could not be read. Its contents do not match the expected layout.");
		return false;
	}

	// Only assigned once everything succeeded: a caller that ignores the return value still
	// never receives a half-populated sidecar.
	OutSidecar = MoveTemp(Parsed);
	return true;
}

bool FDatasetSidecarSerializer::TryReadSchemaVersion(const FString& Json, int32& OutSchemaVersion)
{
	OutSchemaVersion = 0;

	const TSharedPtr<FJsonObject> Root = ParseJsonObject(Json);
	if (!Root.IsValid())
	{
		return false;
	}

	// Reads the version and nothing else, so this succeeds for documents whose remaining
	// blocks are from a future schema. That is the forward-compatibility contract: a reader
	// can always say which version it is looking at.
	return Root->TryGetNumberField(SchemaVersionField, OutSchemaVersion);
}

bool FDatasetSidecarSerializer::IsSupportedSchemaVersion(int32 SchemaVersion)
{
	// An explicit list, never "less than or equal to": a range check would silently accept a
	// version this build has no reader for.
	//
	// v2 reads unchanged because v3 is purely additive: a v2 file lacks clipSource and impact,
	// which default to a recorded take with no impact. v1 is rejected rather than upgraded
	// because nothing had been captured at v1: there is no data to migrate.
	return SchemaVersion == 2 || SchemaVersion == KDatasetSidecarSchemaVersion;
}

bool FDatasetSidecarSerializer::SaveToFile(const FDatasetSidecar& Sidecar, const FString& FilePath, FText& OutFailureReason)
{
	OutFailureReason = FText::GetEmpty();

	FString Json;
	if (!ToJson(Sidecar, Json))
	{
		OutFailureReason = LOCTEXT("PrepareFailed", "The sidecar could not be prepared for writing.");
		return false;
	}

	if (!FFileHelper::SaveStringToFile(Json, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutFailureReason = FText::Format(
			LOCTEXT("WriteFailed", "Could not write the sidecar to '{0}'."), FText::FromString(FilePath));
		UE_LOG(LogKimodoDatasetCapture, Error, TEXT("%s"), *OutFailureReason.ToString());
		return false;
	}

	UE_LOG(LogKimodoDatasetCapture, Verbose, TEXT("Wrote sidecar to '%s'."), *FilePath);
	return true;
}

bool FDatasetSidecarSerializer::LoadFromFile(const FString& FilePath, FDatasetSidecar& OutSidecar, FText& OutFailureReason)
{
	OutFailureReason = FText::GetEmpty();

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *FilePath))
	{
		OutFailureReason = FText::Format(
			LOCTEXT("ReadFailed", "Could not read a sidecar at '{0}'."), FText::FromString(FilePath));
		return false;
	}

	return FromJson(Json, OutSidecar, OutFailureReason);
}

#undef LOCTEXT_NAMESPACE
