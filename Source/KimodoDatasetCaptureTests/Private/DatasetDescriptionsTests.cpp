// Copyright Animotive. All Rights Reserved.

#include "DatasetDescriptions.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace DatasetDescriptionsTests
{
	FDatasetDescriptionEntry Entry(const TCHAR* Name, const TCHAR* Kind, TArray<FString> Descriptions)
	{
		FDatasetDescriptionEntry Out;
		Out.Name = Name;
		Out.File = FString(Name) + TEXT(".fbx");
		Out.Kind = Kind;
		Out.Descriptions = MoveTemp(Descriptions);
		return Out;
	}

	/** Two entries exactly as the user's reference descriptions.json writes them. */
	const TCHAR* Reference = TEXT(R"({
  "aug_0000_standalone__Crouch_Bwd_Loop": {
    "file": "aug_0000_standalone__Crouch_Bwd_Loop.glb",
    "kind": "standalone",
    "descriptions": [
      "A person moves backward while crouching.",
      "A person retreats in a low crouched stance."
    ]
  },
  "aug_0006_standalone__Idle_Rock": {
    "file": "aug_0006_standalone__Idle_Rock.glb",
    "kind": "standalone",
    "descriptions": [
      "A person performs the rock hand gesture in rock–paper–scissors.",
      "A person reveals a closed fist to represent rock in a hand game."
    ]
  }
})");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetDescriptionsFormatTest,
	"KimodoDatasetCapture.Descriptions.Format",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetDescriptionsFormatTest::RunTest(const FString& Parameters)
{
	using namespace DatasetDescriptionsTests;
	TestEqual(TEXT("The file is descriptions.json"), FString(FDatasetDescriptions::DefaultFilename), FString(TEXT("descriptions.json")));

	// Reads the reference format.
	FDatasetDescriptions Read;
	FText Reason;
	if (!TestTrue(*FString::Printf(TEXT("The reference format parses: %s"), *Reason.ToString()), Read.FromJson(Reference, Reason)))
	{
		return true;
	}
	TestEqual(TEXT("Both entries"), Read.Num(), 2);
	const FDatasetDescriptionEntry* Rock = Read.Find(TEXT("aug_0006_standalone__Idle_Rock"));
	if (TestNotNull(TEXT("Found by name"), Rock))
	{
		TestEqual(TEXT("file"), Rock->File, FString(TEXT("aug_0006_standalone__Idle_Rock.glb")));
		TestEqual(TEXT("kind"), Rock->Kind, FString(TEXT("standalone")));
		TestEqual(TEXT("Every description"), Rock->Descriptions.Num(), 2);
		TestTrue(TEXT("Unicode survives"), Rock->Descriptions[0].Contains(TEXT("rock–paper–scissors")));
	}

	// Writes the same shape: an object keyed by name, in name order, each value file, kind, descriptions.
	FDatasetDescriptions Written;
	TestTrue(TEXT("An impact clip is added"), Written.Add(Entry(TEXT("IMP_000002_StandIdle_Punch"), TEXT("impact"),
		{ TEXT("A person standing idle is punched in the chest and staggers back."), TEXT("A person is punched in the chest while standing and steps back.") }), Reason));
	TestTrue(TEXT("A climb clip is added"), Written.Add(Entry(TEXT("CLB_000001_Traverse"), TEXT("climb"),
		{ TEXT("A person climbs sideways along a wall."), TEXT("A person traverses a climbing wall to the left.") }), Reason));
	FString Json;
	if (!TestTrue(TEXT("Written"), Written.ToJson(Json)))
	{
		return true;
	}
	AddInfo(Json);
	TSharedPtr<FJsonObject> Root;
	if (!TestTrue(TEXT("The output is a JSON object"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) && Root.IsValid()))
	{
		return true;
	}
	TestEqual(TEXT("One key per animation"), Root->Values.Num(), 2);
	TestTrue(TEXT("Keys in name order"), Json.Find(TEXT("CLB_000001_Traverse")) < Json.Find(TEXT("IMP_000002_StandIdle_Punch")));
	const TSharedPtr<FJsonObject>* Punch = nullptr;
	if (TestTrue(TEXT("Keyed by name"), Root->TryGetObjectField(TEXT("IMP_000002_StandIdle_Punch"), Punch)))
	{
		TestEqual(TEXT("Exactly file, kind and descriptions"), (*Punch)->Values.Num(), 3);
		TestEqual(TEXT("file"), (*Punch)->GetStringField(TEXT("file")), FString(TEXT("IMP_000002_StandIdle_Punch.fbx")));
		TestEqual(TEXT("kind"), (*Punch)->GetStringField(TEXT("kind")), FString(TEXT("impact")));
		TestEqual(TEXT("descriptions"), (*Punch)->GetArrayField(TEXT("descriptions")).Num(), 2);
	}
	const int32 Start = Json.Find(TEXT("IMP_000002_StandIdle_Punch"));
	TestTrue(TEXT("Fields in the reference's order"), Json.Find(TEXT("\"file\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start)
		< Json.Find(TEXT("\"kind\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start)
		&& Json.Find(TEXT("\"kind\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start)
		< Json.Find(TEXT("\"descriptions\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start));

	FDatasetDescriptions RoundTrip;
	TestTrue(TEXT("What it writes, it reads"), RoundTrip.FromJson(Json, Reason));
	TestTrue(TEXT("Unchanged"), RoundTrip.Num() == 2 && *RoundTrip.Find(TEXT("CLB_000001_Traverse")) == *Written.Find(TEXT("CLB_000001_Traverse")));

	// Incomplete entries are refused.
	FDatasetDescriptions Strict;
	Reason = FText::GetEmpty();
	TestFalse(TEXT("No name"), Strict.Add(Entry(TEXT(""), TEXT("impact"), { TEXT("A person falls.") }), Reason));
	TestFalse(TEXT("with a reason"), Reason.IsEmpty());
	TestFalse(TEXT("No kind"), Strict.Add(Entry(TEXT("A"), TEXT(""), { TEXT("A person falls.") }), Reason));
	TestFalse(TEXT("No descriptions"), Strict.Add(Entry(TEXT("A"), TEXT("impact"), {}), Reason));
	TestFalse(TEXT("An empty description"), Strict.Add(Entry(TEXT("A"), TEXT("impact"), { TEXT("A person falls."), TEXT("") }), Reason));
	TestEqual(TEXT("None of them added"), Strict.Num(), 0);
	TestFalse(TEXT("Not an object of entries"), Strict.FromJson(TEXT("[1, 2]"), Reason));
	TestFalse(TEXT("An entry without descriptions"), Strict.FromJson(TEXT(R"({"A": {"file": "A.fbx", "kind": "impact"}})"), Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetDescriptionsMergeTest,
	"KimodoDatasetCapture.Descriptions.GeneratorsShareOneFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetDescriptionsMergeTest::RunTest(const FString& Parameters)
{
	using namespace DatasetDescriptionsTests;
	const FString Folder = FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("DatasetDescriptions");
	const FString File = Folder / FDatasetDescriptions::DefaultFilename;
	IFileManager::Get().DeleteDirectory(*Folder, false, true);
	FText Reason;

	FDatasetDescriptions Missing;
	TestFalse(TEXT("No file yet: nothing to load"), Missing.LoadFromFile(File, Reason));
	TestEqual(TEXT("and nothing loaded"), Missing.Num(), 0);

	// The climb generator writes first.
	{
		FDatasetDescriptions Climb;
		Climb.Add(Entry(TEXT("CLB_000001_Traverse"), TEXT("climb"), { TEXT("A person climbs sideways along a wall."), TEXT("A person traverses a wall.") }), Reason);
		TestTrue(*FString::Printf(TEXT("Saved: %s"), *Reason.ToString()), Climb.SaveToFile(File, Reason));
	}
	// The impact generator merges in, replacing its own earlier entry.
	{
		FDatasetDescriptions Impact;
		TestTrue(TEXT("The existing file loads"), Impact.LoadFromFile(File, Reason));
		Impact.Add(Entry(TEXT("IMP_000001_Run_Kick"), TEXT("impact"), { TEXT("A person running is kicked and falls."), TEXT("A runner is kicked to the ground.") }), Reason);
		Impact.Add(Entry(TEXT("IMP_000001_Run_Kick"), TEXT("impact"), { TEXT("A person running is kicked in the head and falls."), TEXT("A runner is kicked down.") }), Reason);
		TestTrue(TEXT("Saved again"), Impact.SaveToFile(File, Reason));
	}
	FDatasetDescriptions Both;
	TestTrue(TEXT("Reloads"), Both.LoadFromFile(File, Reason));
	TestEqual(TEXT("Both generators' animations in one file"), Both.Num(), 2);
	TestEqual(TEXT("The climb entry kept"), Both.Find(TEXT("CLB_000001_Traverse")) ? Both.Find(TEXT("CLB_000001_Traverse"))->Kind : FString(), FString(TEXT("climb")));
	TestTrue(TEXT("The impact entry replaced, not duplicated"), Both.Find(TEXT("IMP_000001_Run_Kick"))
		&& Both.Find(TEXT("IMP_000001_Run_Kick"))->Descriptions[0].Contains(TEXT("in the head")));
	// A generator re-running drops its own stale entries by where their files live.
	FDatasetDescriptions Stale;
	Stale.Add(Entry(TEXT("Old"), TEXT("impact"), { TEXT("A person falls.") }), Reason);
	FDatasetDescriptionEntry InBatch = Entry(TEXT("InBatch"), TEXT("impact"), { TEXT("A person falls.") });
	InBatch.File = TEXT("IB_Grid/IMP_000001.uasset");
	FDatasetDescriptionEntry OtherBatch = Entry(TEXT("OtherBatch"), TEXT("impact"), { TEXT("A person falls.") });
	OtherBatch.File = TEXT("IB_GridSmall/IMP_000001.uasset");
	Stale.Add(InBatch, Reason);
	Stale.Add(OtherBatch, Reason);
	TestEqual(TEXT("Only the batch's own entries go"), Stale.RemoveFilesUnder(TEXT("IB_Grid/")), 1);
	TestTrue(TEXT("Others stay"), Stale.Num() == 2 && Stale.Find(TEXT("OtherBatch")) && Stale.Find(TEXT("Old")));

	TArray<FString> Left;
	IFileManager::Get().FindFiles(Left, *(Folder / TEXT("*")), true, false);
	TestEqual(TEXT("No temporary file left behind"), Left.Num(), 1);
	IFileManager::Get().DeleteDirectory(*Folder, false, true);
	return true;
}
