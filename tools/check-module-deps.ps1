# Copyright Animotive. All Rights Reserved.
#
# Fails if an UncookedOnly module of this plugin depends on an EDITOR-type module, or if the
# capture module depends on the impact module.
#
# The capture module is UncookedOnly so that capture code is live during Play In Editor but
# cannot reach a cooked client build. A dependency on an Editor-type module, or an editor-only
# include in its source, silently breaks that guarantee. This check makes the breakage loud.
#
# What is NOT a violation, and why this list is specific rather than keyword-based:
# TakesCore, TakeRecorder and TakeTrackRecorders are all UncookedOnly modules, exactly like
# this one, so depending on them is legitimate - UTakeRecorderSource itself lives in TakesCore.
# An earlier version of this check forbade anything matching "TakeRecorder" and produced a
# false positive on TakeRecorderSource.h. The rule is about module TYPE, not about names that
# look editorial.
#
# Modules checked (2026-10-12): KimodoDatasetCapture and KimodoImpactCapture, both UncookedOnly.
# The impact module depends on the capture module for the sidecar schema; the reverse would
# make player capture carry Physics Control and the whole synthetic pipeline with it.
#
# Usage:  pwsh -File tools/check-module-deps.ps1

$ErrorActionPreference = 'Stop'

$PluginRoot = Split-Path -Parent $PSScriptRoot

# Module name -> names forbidden for that module in addition to the shared editor-only list.
$CheckedModules = [ordered]@{
    'KimodoDatasetCapture' = @('KimodoImpactCapture')
    'KimodoImpactCapture'  = @()
}

# Editor-only modules and headers the capture module must never name.
# Editor-type modules only. Verified against each plugin's .uplugin module type rather
# than guessed from the name.
$ForbiddenNames = @(
    'UnrealEd',
    'EditorFramework',
    'EditorStyle',
    'EditorSubsystem',
    'LevelSequenceEditor',
    'MovieSceneTools',
    'Sequencer',
    'TakeSequencer',
    'ToolMenus',
    'ToolWidgets',
    'Slate',
    'SlateCore',
    'WorkspaceMenuStructure',
    'KimodoDatasetCaptureEditor'
)

$Violations = @()

foreach ($ModuleName in $CheckedModules.Keys) {
    $ModuleDir = Join-Path $PluginRoot "Source/$ModuleName"
    if (-not (Test-Path $ModuleDir)) {
        Write-Error "Module directory not found: $ModuleDir"
    }
    $ModuleForbidden = $ForbiddenNames + $CheckedModules[$ModuleName]

    # 1. Build rules: no forbidden module in any dependency list.
    $BuildFile = Join-Path $ModuleDir "$ModuleName.Build.cs"
    $BuildLines = Get-Content $BuildFile
    for ($i = 0; $i -lt $BuildLines.Count; $i++) {
        $Line = $BuildLines[$i]
        if ($Line -match '^\s*//') { continue }
        foreach ($Name in $ModuleForbidden) {
            if ($Line -match ('"' + [regex]::Escape($Name) + '"')) {
                $Violations += "${ModuleName}.Build.cs:$($i + 1): names forbidden module '$Name'"
            }
        }
    }

    # 2. Sources: no include from an editor-only module, nor from a forbidden sibling module.
    $HeaderPatterns = @('Editor.h', 'UnrealEd', 'Slate/', 'SlateCore', 'ISequencer', 'MovieSceneTools', 'ToolMenus')
    if ($CheckedModules[$ModuleName] -contains 'KimodoImpactCapture') { $HeaderPatterns += 'KimodoImpactCapture' }
    $SourceFiles = Get-ChildItem -Path $ModuleDir -Recurse -Include *.h, *.cpp -File
    foreach ($File in $SourceFiles) {
        $Lines = Get-Content $File.FullName
        for ($i = 0; $i -lt $Lines.Count; $i++) {
            $Line = $Lines[$i]
            if ($Line -notmatch '^\s*#include') { continue }
            # Specific editor headers. Deliberately excludes 'TakeRecorder', which matches the
            # UncookedOnly TakesCore header this module legitimately includes.
            foreach ($Name in $HeaderPatterns) {
                if ($Line -match [regex]::Escape($Name)) {
                    $Rel = $File.FullName.Substring($PluginRoot.Length + 1)
                    $Violations += "${Rel}:$($i + 1): includes editor-only header matching '$Name'"
                }
            }
        }
    }

}

if ($Violations.Count -gt 0) {
    Write-Host 'FAIL: an UncookedOnly module has a forbidden dependency.' -ForegroundColor Red
    $Violations | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}

Write-Host 'PASS: capture and impact modules have no forbidden dependencies.' -ForegroundColor Green
exit 0
