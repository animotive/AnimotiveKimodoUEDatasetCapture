// Copyright Animotive. All Rights Reserved.

using UnrealBuildTool;

public class KimodoDatasetCaptureEditor : ModuleRules
{
	public KimodoDatasetCaptureEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Current engine include order regardless of the host target; see the comment in
		// KimodoDatasetCapture.Build.cs. Module-level wins over target-level, so a client
		// target cannot un-pin it.
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				// The one direction this dependency is allowed to run: editor -> capture.
				// tools/check-module-deps.ps1 fails the build if it is ever reversed.
				"KimodoDatasetCapture",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EditorFramework",
				"InputCore",
				"LevelSequence",
				"LevelSequenceEditor",
				"MovieScene",
				"MovieSceneTools",
				"MovieSceneTracks",
				"Projects",
				"Sequencer",
				"Slate",
				"SlateCore",
				"TakeRecorder",
				"TakeTrackRecorders",
				"TakesCore",
				"ToolMenus",
				"ToolWidgets",
				"UnrealEd",
				"WorkspaceMenuStructure",
			});
	}
}
