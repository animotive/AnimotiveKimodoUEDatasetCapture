// Copyright Animotive. All Rights Reserved.

using UnrealBuildTool;

public class KimodoDatasetCapture : ModuleRules
{
	public KimodoDatasetCapture(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Compile against the CURRENT engine include order, whatever the host target asks for.
		//
		// Unset, a module inherits the target's IncludeOrderVersion, which defaults to
		// EngineIncludeOrderVersion.Oldest. At Oldest the engine's public headers still drag in
		// their legacy transitive includes, so a header of ours that names a type without
		// including or forward-declaring it compiles here and fails in a client tree whose
		// target sets Latest. KimodoExporter reached a customer that way.
		//
		// Set per module rather than on the target, because module-level wins over target-level:
		// no consumer's target settings can un-pin it.
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MovieScene",
				"MovieSceneTracks",
				// Public: DatasetInputSampler.h names FKey in its interface, so anything
				// including it needs InputCore too.
				"InputCore",
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
