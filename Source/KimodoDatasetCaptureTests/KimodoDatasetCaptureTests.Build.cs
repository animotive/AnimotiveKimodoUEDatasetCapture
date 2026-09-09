// Copyright Animotive. All Rights Reserved.

using UnrealBuildTool;

public class KimodoDatasetCaptureTests : ModuleRules
{
	public KimodoDatasetCaptureTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Current engine include order regardless of the host target; see the comment in
		// KimodoDatasetCapture.Build.cs. Module-level wins over target-level, so a client
		// target cannot un-pin it.
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

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
