// Copyright Animotive. All Rights Reserved.

#include "ImpactBatchManifest.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "KimodoImpactCaptureLog.h"
#include "Misc/FileHelper.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactBatchManifest)

#define LOCTEXT_NAMESPACE "ImpactBatchManifest"

namespace
{
	const TCHAR* ClipIndexField = TEXT("clipIndex");

	/** UTF-8 always: an append in another encoding would corrupt the lines before it. */
	bool WriteText(const FString& Text, const FString& Filename, uint32 Flags)
	{
		return FFileHelper::SaveStringToFile(Text, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), Flags);
	}
}

bool FImpactBatchManifest::Load(const FString& InFilename, FText& OutReason)
{
	Filename = InFilename;
	Entries.Reset();
	IndexToEntry.Reset();
	DroppedLines = 0;

	if (!IFileManager::Get().FileExists(*Filename))
	{
		return true;
	}
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Filename))
	{
		OutReason = FText::Format(LOCTEXT("Unreadable", "Could not read the batch manifest {0}."), FText::FromString(Filename));
		return false;
	}

	// Every append ends with a line break, so text after the last one was cut short by a crash.
	const bool bCutShort = !Text.IsEmpty() && !Text.EndsWith(TEXT("\n"));
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, /*bCullEmpty*/ true);
	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		FImpactManifestEntry Entry;
		const bool bLastAndCut = bCutShort && LineIndex == Lines.Num() - 1;
		if (bLastAndCut || !FromLine(Lines[LineIndex], Entry) || IndexToEntry.Contains(Entry.ClipIndex))
		{
			++DroppedLines;
			continue;
		}
		IndexToEntry.Add(Entry.ClipIndex, Entries.Add(MoveTemp(Entry)));
	}

	if (DroppedLines > 0 || bCutShort)
	{
		// Rewrite without the dropped lines, through a temporary file, so a crash during the repair
		// still leaves one whole manifest on disk.
		UE_LOG(LogKimodoImpactCapture, Warning, TEXT("Batch manifest %s: dropped %d line(s) (cut short, unreadable or repeated); rewriting it."), *Filename, DroppedLines);
		if (!SaveAs(Filename, OutReason))
		{
			return false;
		}
	}
	return true;
}

bool FImpactBatchManifest::Append(const FImpactManifestEntry& Entry, FText& OutReason)
{
	if (Filename.IsEmpty())
	{
		OutReason = LOCTEXT("NotLoaded", "Load the batch manifest before appending to it.");
		return false;
	}
	if (Contains(Entry.ClipIndex))
	{
		OutReason = FText::Format(LOCTEXT("Repeat", "Clip {0} is already in the batch manifest."), FText::AsNumber(Entry.ClipIndex));
		return false;
	}
	if (!WriteText(ToLine(Entry) + TEXT("\n"), Filename, FILEWRITE_Append))
	{
		OutReason = FText::Format(LOCTEXT("AppendFailed", "Could not write to the batch manifest {0}."), FText::FromString(Filename));
		return false;
	}
	IndexToEntry.Add(Entry.ClipIndex, Entries.Add(Entry));
	return true;
}

bool FImpactBatchManifest::Contains(int32 ClipIndex) const
{
	return IndexToEntry.Contains(ClipIndex);
}

const FImpactManifestEntry* FImpactBatchManifest::Find(int32 ClipIndex) const
{
	const int32* Found = IndexToEntry.Find(ClipIndex);
	return Found ? &Entries[*Found] : nullptr;
}

TArray<int32> FImpactBatchManifest::Pending(int32 Start, int32 Count) const
{
	TArray<int32> Result;
	for (int32 Index = Start; Index < Start + Count; ++Index)
	{
		if (!Contains(Index))
		{
			Result.Add(Index);
		}
	}
	return Result;
}

const TCHAR* FImpactBatchManifest::FilePattern = TEXT("Manifest*.jsonl");

int32 FImpactBatchManifest::Merge(const FImpactBatchManifest& Other)
{
	int32 Repeats = 0;
	for (const FImpactManifestEntry& Entry : Other.Entries)
	{
		if (Contains(Entry.ClipIndex))
		{
			++Repeats;
			continue;
		}
		IndexToEntry.Add(Entry.ClipIndex, Entries.Add(Entry));
	}
	return Repeats;
}

bool FImpactBatchManifest::LoadAll(const FString& Folder, FImpactBatchManifest& Out, FText& OutReason)
{
	Out = FImpactBatchManifest();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Folder / FilePattern), /*Files*/ true, /*Directories*/ false);
	// Sorted, so which shard's copy of a repeated clip wins never depends on directory order.
	Files.Sort();
	for (const FString& File : Files)
	{
		FImpactBatchManifest Shard;
		if (!Shard.Load(Folder / File, OutReason))
		{
			return false;
		}
		const int32 Repeats = Out.Merge(Shard);
		if (Repeats > 0)
		{
			UE_LOG(LogKimodoImpactCapture, Warning, TEXT("Batch manifest %s repeats %d clip(s) already finished by another shard."), *File, Repeats);
		}
		Out.DroppedLines += Shard.DroppedLines;
	}
	return true;
}

bool FImpactBatchManifest::SaveAs(const FString& InFilename, FText& OutReason) const
{
	FString Text;
	for (const FImpactManifestEntry& Entry : Entries)
	{
		Text += ToLine(Entry);
		Text += TEXT("\n");
	}
	const FString Temporary = InFilename + TEXT(".tmp");
	if (!WriteText(Text, Temporary, FILEWRITE_None) || !IFileManager::Get().Move(*InFilename, *Temporary, /*bReplace*/ true))
	{
		OutReason = FText::Format(LOCTEXT("SaveFailed", "Could not write the batch manifest {0}."), FText::FromString(InFilename));
		return false;
	}
	return true;
}

FString FImpactBatchManifest::ToLine(const FImpactManifestEntry& Entry)
{
	// Condensed JSON escapes line breaks inside strings, so one entry is always one line.
	FString Line;
	FJsonObjectConverter::UStructToJsonObjectString(Entry, Line, 0, 0, 0, nullptr, /*bPrettyPrint*/ false);
	return Line;
}

bool FImpactBatchManifest::FromLine(const FString& Line, FImpactManifestEntry& OutEntry)
{
	TSharedPtr<FJsonObject> Object;
	if (Line.IsEmpty() || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Object) || !Object.IsValid()
		|| !Object->HasTypedField<EJson::Number>(ClipIndexField))
	{
		return false;
	}
	FImpactManifestEntry Entry;
	if (!FJsonObjectConverter::JsonObjectToUStruct(Object.ToSharedRef(), &Entry) || Entry.ClipIndex < 0)
	{
		return false;
	}
	OutEntry = MoveTemp(Entry);
	return true;
}

#undef LOCTEXT_NAMESPACE
