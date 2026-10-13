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
		FString Repaired;
		for (const FImpactManifestEntry& Entry : Entries)
		{
			Repaired += ToLine(Entry);
			Repaired += TEXT("\n");
		}
		const FString Temporary = Filename + TEXT(".tmp");
		if (!WriteText(Repaired, Temporary, FILEWRITE_None) || !IFileManager::Get().Move(*Filename, *Temporary, /*bReplace*/ true))
		{
			OutReason = FText::Format(LOCTEXT("RepairFailed", "Could not repair the batch manifest {0}."), FText::FromString(Filename));
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
	return 0;
}

bool FImpactBatchManifest::LoadAll(const FString& Folder, FImpactBatchManifest& Out, FText& OutReason)
{
	Out = FImpactBatchManifest();
	return true;
}

bool FImpactBatchManifest::SaveAs(const FString& InFilename, FText& OutReason) const
{
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
