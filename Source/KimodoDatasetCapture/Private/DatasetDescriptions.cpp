// Copyright Animotive. All Rights Reserved.

#include "DatasetDescriptions.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "KimodoDatasetCaptureLog.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "DatasetDescriptions"

const TCHAR* FDatasetDescriptions::DefaultFilename = TEXT("descriptions.json");

namespace
{
	/** A JSON string literal. Non-ASCII text is written as is (the file is UTF-8), as the reference file does. */
	FString Quoted(const FString& Text)
	{
		FString Out;
		Out.Reserve(Text.Len() + 2);
		Out += TEXT('"');
		for (const TCHAR Char : Text)
		{
			switch (Char)
			{
			case TEXT('"'): Out += TEXT("\\\""); break;
			case TEXT('\\'): Out += TEXT("\\\\"); break;
			case TEXT('\n'): Out += TEXT("\\n"); break;
			case TEXT('\r'): Out += TEXT("\\r"); break;
			case TEXT('\t'): Out += TEXT("\\t"); break;
			default:
				if (Char < 0x20)
				{
					Out += FString::Printf(TEXT("\\u%04x"), int32(Char));
				}
				else
				{
					Out += Char;
				}
			}
		}
		Out += TEXT('"');
		return Out;
	}
}

bool FDatasetDescriptionEntry::operator==(const FDatasetDescriptionEntry& Other) const
{
	return Name == Other.Name && File == Other.File && Kind == Other.Kind && Descriptions == Other.Descriptions;
}

bool FDatasetDescriptions::Add(const FDatasetDescriptionEntry& Entry, FText& OutReason)
{
	if (Entry.Name.IsEmpty() || Entry.File.IsEmpty() || Entry.Kind.IsEmpty())
	{
		OutReason = FText::Format(LOCTEXT("Incomplete", "The description entry '{0}' needs a name, a file and a kind."), FText::FromString(Entry.Name));
		return false;
	}
	if (Entry.Descriptions.IsEmpty() || Entry.Descriptions.ContainsByPredicate([](const FString& Text) { return Text.TrimStartAndEnd().IsEmpty(); }))
	{
		OutReason = FText::Format(LOCTEXT("NoDescriptions", "The description entry '{0}' needs at least one description, and no empty ones."), FText::FromString(Entry.Name));
		return false;
	}
	Entries.Add(Entry.Name, Entry);
	return true;
}

int32 FDatasetDescriptions::RemoveFilesUnder(const FString& Prefix)
{
	TArray<FString> Gone;
	for (const TPair<FString, FDatasetDescriptionEntry>& Pair : Entries)
	{
		if (Pair.Value.File.StartsWith(Prefix, ESearchCase::CaseSensitive))
		{
			Gone.Add(Pair.Key);
		}
	}
	for (const FString& Name : Gone)
	{
		Entries.Remove(Name);
	}
	return Gone.Num();
}

int32 FDatasetDescriptions::Num() const
{
	return Entries.Num();
}

const FDatasetDescriptionEntry* FDatasetDescriptions::Find(const FString& Name) const
{
	return Entries.Find(Name);
}

bool FDatasetDescriptions::ToJson(FString& OutJson) const
{
	TArray<FString> Names;
	Entries.GetKeys(Names);
	Names.Sort([](const FString& A, const FString& B) { return A.Compare(B, ESearchCase::CaseSensitive) < 0; });

	// Written by hand: the reference file's two-space layout and field order (file, kind, descriptions).
	OutJson = TEXT("{");
	for (int32 Index = 0; Index < Names.Num(); ++Index)
	{
		const FDatasetDescriptionEntry& Entry = Entries[Names[Index]];
		OutJson += Index == 0 ? TEXT("\n") : TEXT(",\n");
		OutJson += FString::Printf(TEXT("  %s: {\n    \"file\": %s,\n    \"kind\": %s,\n    \"descriptions\": ["),
			*Quoted(Entry.Name), *Quoted(Entry.File), *Quoted(Entry.Kind));
		for (int32 Line = 0; Line < Entry.Descriptions.Num(); ++Line)
		{
			OutJson += FString::Printf(TEXT("%s\n      %s"), Line == 0 ? TEXT("") : TEXT(","), *Quoted(Entry.Descriptions[Line]));
		}
		OutJson += TEXT("\n    ]\n  }");
	}
	OutJson += Names.IsEmpty() ? TEXT("}\n") : TEXT("\n}\n");
	return true;
}

bool FDatasetDescriptions::FromJson(const FString& Json, FText& OutReason)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		OutReason = LOCTEXT("NotObject", "The descriptions file is not a JSON object of animations.");
		return false;
	}
	TArray<FDatasetDescriptionEntry> Read;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
	{
		const TSharedPtr<FJsonObject>* Value = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
		FDatasetDescriptionEntry Entry;
		Entry.Name = Pair.Key;
		if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Value) || !(*Value)->TryGetStringField(TEXT("file"), Entry.File)
			|| !(*Value)->TryGetStringField(TEXT("kind"), Entry.Kind) || !(*Value)->TryGetArrayField(TEXT("descriptions"), Lines))
		{
			OutReason = FText::Format(LOCTEXT("BadEntry", "The descriptions entry '{0}' needs file, kind and descriptions."), FText::FromString(Pair.Key));
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Line : *Lines)
		{
			FString Text;
			if (!Line.IsValid() || !Line->TryGetString(Text))
			{
				OutReason = FText::Format(LOCTEXT("BadLine", "A description of '{0}' is not a string."), FText::FromString(Pair.Key));
				return false;
			}
			Entry.Descriptions.Add(MoveTemp(Text));
		}
		Read.Add(MoveTemp(Entry));
	}
	// All or nothing: a bad entry leaves this index as it was.
	FDatasetDescriptions Checked = *this;
	for (const FDatasetDescriptionEntry& Entry : Read)
	{
		if (!Checked.Add(Entry, OutReason))
		{
			return false;
		}
	}
	*this = MoveTemp(Checked);
	return true;
}

bool FDatasetDescriptions::SaveToFile(const FString& FilePath, FText& OutReason) const
{
	FString Json;
	ToJson(Json);
	const FString Temporary = FilePath + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(Json, *Temporary, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		|| !IFileManager::Get().Move(*FilePath, *Temporary, true, true))
	{
		IFileManager::Get().Delete(*Temporary);
		OutReason = FText::Format(LOCTEXT("WriteFailed", "Could not write the descriptions to '{0}'."), FText::FromString(FilePath));
		UE_LOG(LogKimodoDatasetCapture, Error, TEXT("%s"), *OutReason.ToString());
		return false;
	}
	return true;
}

bool FDatasetDescriptions::LoadFromFile(const FString& FilePath, FText& OutReason)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *FilePath))
	{
		OutReason = FText::Format(LOCTEXT("ReadFailed", "Could not read the descriptions from '{0}'."), FText::FromString(FilePath));
		return false;
	}
	return FromJson(Json, OutReason);
}

#undef LOCTEXT_NAMESPACE
