// Copyright Animotive. All Rights Reserved.

using UnrealBuildTool;

public class KimodoImpactCapture : ModuleRules
{
	public KimodoImpactCapture(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Pinned for the same reason as KimodoDatasetCapture.Build.cs: a header that compiles
		// only because the Oldest include order dragged in a transitive include will fail in a
		// client tree whose target sets Latest.
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				// The one direction this dependency is allowed to run: impact -> capture. The
				// impact module reuses the sidecar schema; the capture module never knows it
				// exists. tools/check-module-deps.ps1 fails if either rule is broken.
				"KimodoDatasetCapture",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Json",
				"JsonUtilities",
				"PhysicsCore",
				// PhysicsControl is a Runtime module, but only this UncookedOnly module links it,
				// so it still cannot reach a cooked client build through this plugin.
				"PhysicsControl",
			});

		// Like the capture module, this module must never depend on an editor-only module.
	}
}
