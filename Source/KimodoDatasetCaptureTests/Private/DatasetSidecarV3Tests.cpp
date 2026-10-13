// Copyright Animotive. All Rights Reserved.

#include "DatasetSidecarSerializer.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace DatasetSidecarV3Tests
{
	/** A synthetic impact clip's sidecar with every impact field set. */
	FDatasetSidecar MakeImpactSidecar()
	{
		FDatasetSidecar Sidecar;
		Sidecar.ClipSource = DatasetClipSource::SyntheticImpact;
		Sidecar.Identity.TakeName = TEXT("imp_3f2a9c0d11e4b7a5");
		Sidecar.Identity.AnimSequencePath = TEXT("/Game/ImpactDataset/Batch01/imp_3f2a9c0d11e4b7a5.imp_3f2a9c0d11e4b7a5");
		Sidecar.Identity.FrameRateNumerator = 60;
		Sidecar.Identity.FrameRateDenominator = 1;
		Sidecar.Identity.DurationFrames = 109;
		Sidecar.Skeleton.SkeletonPath = TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin.SK_UEFN_Mannequin");
		Sidecar.Skeleton.BoneCount = 92;
		Sidecar.Labels.Caption = TEXT("A person is shot in the foot with a small bullet while running and trips, falling forwards.");
		Sidecar.Labels.Tags = { TEXT("impact"), TEXT("SmallBullet"), TEXT("FootLeft"), TEXT("Knockdown"), TEXT("Medium"), TEXT("run") };
		Sidecar.Labels.TrimOutFrame = 108;

		FDatasetSidecarImpact& Impact = Sidecar.Impact;
		Impact.ClipId = TEXT("imp_3f2a9c0d11e4b7a5");
		Impact.BatchSeed = 3;
		Impact.ClipIndex = 3;
		Impact.SourceAnimation = TEXT("/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F");
		Impact.SourceStartSeconds = 0.25f;
		Impact.SourceMotion = TEXT("while running");
		Impact.Intensity = TEXT("Medium");
		Impact.TransferredImpulseKgCmPerSecond = 302.3f;
		Impact.ReactionGain = 60.0f;
		Impact.Outcome = TEXT("Knockdown");
		Impact.OutcomeFrame = 11;
		Impact.Steps = 0;
		Impact.bTrip = true;
		Impact.End = TEXT("Settled");
		Impact.Control.WorldStrength = 8.0f;
		Impact.Control.ParentStrength = 16.0f;
		Impact.Control.ReleasedToneStart = 0.14f;
		Impact.Control.ReleasedToneEnd = 0.042f;

		FDatasetSidecarImpactHit Hit;
		Hit.Frame = 31;
		Hit.ImpactorId = TEXT("SmallBullet");
		Hit.ImpactorNoun = TEXT("a small bullet");
		Hit.ImpactorDescription = TEXT("A small-calibre pistol round.");
		Hit.ImpactorSizeCm = 0.9f;
		Hit.ImpactorMassKg = 0.008f;
		Hit.ImpactorSpeedMetresPerSecond = 370.0f;
		Hit.Region = TEXT("FootLeft");
		Hit.Body = TEXT("foot_l");
		Hit.DirectionWorld = FVector(0.0, -1.0, 0.0);
		Hit.DirectionLocal = FVector(-1.0, 0.0, 0.0);
		Hit.ImpulseKgCmPerSecond = 18142.0f;
		Hit.ContactRadiusCm = 0.5f;
		Hit.DeliveryMode = TEXT("Instant");
		Impact.Hits.Add(Hit);

		FDatasetSidecarImpactHit Ball = Hit;
		Ball.Frame = 33;
		Ball.ImpactorId = TEXT("Baseball");
		Ball.bProjectile = true;
		Ball.ProjectileShape = TEXT("Sphere");
		Ball.ProjectileLaunchDistanceCm = 150.0f;
		Ball.DeliveryMode = TEXT("Projectile");
		Impact.Hits.Add(Ball);
		return Sidecar;
	}

	TSharedPtr<FJsonObject> Parse(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Json), Root);
		return Root;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDatasetSidecarV3Test,
	"KimodoDatasetCapture.SidecarSerializer.SchemaV3",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDatasetSidecarV3Test::RunTest(const FString& Parameters)
{
	using namespace DatasetSidecarV3Tests;
	TestEqual(TEXT("The schema is version 3"), KDatasetSidecarSchemaVersion, 3);
	TestTrue(TEXT("Version 3 is read"), FDatasetSidecarSerializer::IsSupportedSchemaVersion(3));
	TestTrue(TEXT("Version 2 files are still read"), FDatasetSidecarSerializer::IsSupportedSchemaVersion(2));
	TestFalse(TEXT("Version 1 is still rejected"), FDatasetSidecarSerializer::IsSupportedSchemaVersion(1));

	// A synthetic impact clip: source recorded, impact block written, raw input absent.
	const FDatasetSidecar Original = MakeImpactSidecar();
	TestTrue(TEXT("Recognised as a synthetic impact clip"), Original.IsSyntheticImpact());
	FString Json;
	TestTrue(TEXT("Serialises"), FDatasetSidecarSerializer::ToJson(Original, Json));
	const TSharedPtr<FJsonObject> Root = Parse(Json);
	if (!TestTrue(TEXT("Writes a JSON object"), Root.IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("clipSource is synthetic_impact"), Root->GetStringField(TEXT("clipSource")), FString(TEXT("synthetic_impact")));
	TestTrue(TEXT("The impact block is written"), Root->HasField(TEXT("impact")));
	TestFalse(TEXT("No raw input stream is written, not even an empty one"), Root->HasField(TEXT("inputStream")));
	TestFalse(TEXT("No input layers are written"), Root->HasField(TEXT("layers")));

	// Round trip.
	FDatasetSidecar Loaded;
	FText Reason;
	TestTrue(*FString::Printf(TEXT("Reads back: %s"), *Reason.ToString()), FDatasetSidecarSerializer::FromJson(Json, Loaded, Reason));
	TestTrue(TEXT("A v3 impact sidecar round-trips"), Loaded == Original);
	TestEqual(TEXT("Both hits survive"), Loaded.Impact.Hits.Num(), 2);
	if (Loaded.Impact.Hits.Num() == 2)
	{
		TestEqual(TEXT("Impactor noun survives"), Loaded.Impact.Hits[0].ImpactorNoun, FString(TEXT("a small bullet")));
		TestEqual(TEXT("Impactor size survives"), Loaded.Impact.Hits[0].ImpactorSizeCm, 0.9f);
		TestTrue(TEXT("Projectile spec survives"), Loaded.Impact.Hits[1].bProjectile && Loaded.Impact.Hits[1].ProjectileShape == TEXT("Sphere"));
	}

	// Equality sees every impact field.
	{
		FDatasetSidecar Changed = Original;
		Changed.Impact.Hits[0].ImpactorSizeCm = 1.1f;
		TestFalse(TEXT("A changed hit field breaks equality"), Changed == Original);
		Changed = Original;
		Changed.Impact.Outcome = TEXT("Ragdoll");
		TestFalse(TEXT("A changed outcome breaks equality"), Changed == Original);
		Changed = Original;
		Changed.Impact.Control.ReleasedToneStart = 0.2f;
		TestFalse(TEXT("A changed control profile breaks equality"), Changed == Original);
		Changed = Original;
		Changed.ClipSource = DatasetClipSource::TakeRecorder;
		TestFalse(TEXT("A changed clip source breaks equality"), Changed == Original);
	}

	// A recorded take: no impact block on the wire; it still round-trips.
	{
		FDatasetSidecar Take;
		Take.Identity.TakeName = TEXT("Take_0001");
		Take.Labels.Caption = TEXT("a person walks forward");
		Take.AddLayer(EDatasetCaptureLayer::RawGamepadInput);
		FDatasetInputSample Sample;
		Sample.FrameNumber = 10;
		Take.AddSample(Sample);
		TestFalse(TEXT("A take is not a synthetic impact clip"), Take.IsSyntheticImpact());
		FString TakeJson;
		FDatasetSidecarSerializer::ToJson(Take, TakeJson);
		const TSharedPtr<FJsonObject> TakeRoot = Parse(TakeJson);
		TestTrue(TEXT("A take writes clipSource take_recorder"), TakeRoot.IsValid() && TakeRoot->GetStringField(TEXT("clipSource")) == TEXT("take_recorder"));
		TestTrue(TEXT("A take writes no impact block"), TakeRoot.IsValid() && !TakeRoot->HasField(TEXT("impact")));
		TestTrue(TEXT("A take keeps its input stream"), TakeRoot.IsValid() && TakeRoot->HasField(TEXT("inputStream")));
		FDatasetSidecar TakeLoaded;
		TestTrue(TEXT("A take round-trips"), FDatasetSidecarSerializer::FromJson(TakeJson, TakeLoaded, Reason) && TakeLoaded == Take);
	}

	// A v2 file (no clipSource, no impact) still reads, as a recorded take.
	{
		FDatasetSidecar Take;
		Take.Identity.TakeName = TEXT("Take_v2");
		Take.Labels.Caption = TEXT("a person jumps");
		Take.AddLayer(EDatasetCaptureLayer::RawGamepadInput);
		FString V2Json;
		FDatasetSidecarSerializer::ToJson(Take, V2Json);
		const TSharedPtr<FJsonObject> V2 = Parse(V2Json);
		if (TestTrue(TEXT("v2 fixture parses"), V2.IsValid()))
		{
			V2->SetNumberField(TEXT("schemaVersion"), 2);
			V2->RemoveField(TEXT("clipSource"));
			V2->RemoveField(TEXT("impact"));
			FString Written;
			FJsonSerializer::Serialize(V2.ToSharedRef(), TJsonWriterFactory<>::Create(&Written));
			FDatasetSidecar Read;
			TestTrue(*FString::Printf(TEXT("A v2 sidecar reads: %s"), *Reason.ToString()), FDatasetSidecarSerializer::FromJson(Written, Read, Reason));
			TestEqual(TEXT("A v2 sidecar is a recorded take"), Read.ClipSource, FString(DatasetClipSource::TakeRecorder));
			TestEqual(TEXT("Its caption survives"), Read.Labels.Caption, Take.Labels.Caption);
			TestEqual(TEXT("Its version is kept as read"), Read.SchemaVersion, 2);
		}
	}
	return true;
}
