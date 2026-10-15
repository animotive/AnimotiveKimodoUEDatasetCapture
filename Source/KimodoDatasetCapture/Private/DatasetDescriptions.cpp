// Copyright Animotive. All Rights Reserved.

#include "DatasetDescriptions.h"

#define LOCTEXT_NAMESPACE "DatasetDescriptions"

const TCHAR* FDatasetDescriptions::DefaultFilename = TEXT("descriptions.json");

bool FDatasetDescriptionEntry::operator==(const FDatasetDescriptionEntry& Other) const
{
	return Name == Other.Name && File == Other.File && Kind == Other.Kind && Descriptions == Other.Descriptions;
}

bool FDatasetDescriptions::Add(const FDatasetDescriptionEntry& Entry, FText& OutReason)
{
	return false;
}

int32 FDatasetDescriptions::RemoveFilesUnder(const FString& Prefix)
{
	return 0;
}

int32 FDatasetDescriptions::Num() const
{
	return 0;
}

const FDatasetDescriptionEntry* FDatasetDescriptions::Find(const FString& Name) const
{
	return nullptr;
}

bool FDatasetDescriptions::ToJson(FString& OutJson) const
{
	return false;
}

bool FDatasetDescriptions::FromJson(const FString& Json, FText& OutReason)
{
	return false;
}

bool FDatasetDescriptions::SaveToFile(const FString& FilePath, FText& OutReason) const
{
	return false;
}

bool FDatasetDescriptions::LoadFromFile(const FString& FilePath, FText& OutReason)
{
	return false;
}

#undef LOCTEXT_NAMESPACE
