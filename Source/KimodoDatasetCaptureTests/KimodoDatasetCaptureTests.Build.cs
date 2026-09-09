// Copyright Animotive. All Rights Reserved.

using UnrealBuildTool;

public class KimodoDatasetCaptureTests : ModuleRules
{
	public KimodoDatasetCaptureTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"KimodoDatasetCapture",
				"MovieScene",
				"MovieSceneTracks",
			});
	}
}
