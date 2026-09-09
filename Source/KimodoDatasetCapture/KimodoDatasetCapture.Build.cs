// Copyright Animotive. All Rights Reserved.

using UnrealBuildTool;

public class KimodoDatasetCapture : ModuleRules
{
	public KimodoDatasetCapture(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MovieScene",
				"MovieSceneTracks",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EnhancedInput",
				"Json",
				"JsonUtilities",
				"TakesCore",
			});

		// This module must never depend on an editor-only module. The editor module
		// depends on this one; the reverse would break the UncookedOnly guarantee that
		// keeps capture code out of a cooked client build.
	}
}
