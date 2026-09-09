# Copyright Animotive. All Rights Reserved.
#
# Fails if the capture module depends on an EDITOR-type module.
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
# Usage:  pwsh -File tools/check-module-deps.ps1

$ErrorActionPreference = 'Stop'

$PluginRoot = Split-Path -Parent $PSScriptRoot
$CaptureModuleDir = Join-Path $PluginRoot 'Source/KimodoDatasetCapture'

if (-not (Test-Path $CaptureModuleDir)) {
    Write-Error "Capture module directory not found: $CaptureModuleDir"
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

# 1. Build rules: no forbidden module in any dependency list.
$BuildFile = Join-Path $CaptureModuleDir 'KimodoDatasetCapture.Build.cs'
$BuildLines = Get-Content $BuildFile
for ($i = 0; $i -lt $BuildLines.Count; $i++) {
    $Line = $BuildLines[$i]
    if ($Line -match '^\s*//') { continue }
    foreach ($Name in $ForbiddenNames) {
        if ($Line -match ('"' + [regex]::Escape($Name) + '"')) {
            $Violations += "KimodoDatasetCapture.Build.cs:$($i + 1): names editor-only module '$Name'"
        }
    }
}

# 2. Sources: no include from an editor-only module.
$SourceFiles = Get-ChildItem -Path $CaptureModuleDir -Recurse -Include *.h, *.cpp -File
foreach ($File in $SourceFiles) {
    $Lines = Get-Content $File.FullName
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        $Line = $Lines[$i]
        if ($Line -notmatch '^\s*#include') { continue }
        # Specific editor headers. Deliberately excludes 'TakeRecorder', which matches the
        # UncookedOnly TakesCore header this module legitimately includes.
        foreach ($Name in @('Editor.h', 'UnrealEd', 'Slate/', 'SlateCore', 'ISequencer', 'MovieSceneTools', 'ToolMenus')) {
            if ($Line -match [regex]::Escape($Name)) {
                $Rel = $File.FullName.Substring($PluginRoot.Length + 1)
                $Violations += "${Rel}:$($i + 1): includes editor-only header matching '$Name'"
            }
        }
    }
}

if ($Violations.Count -gt 0) {
    Write-Host 'FAIL: capture module depends on editor-only code.' -ForegroundColor Red
    $Violations | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}

Write-Host 'PASS: capture module has no editor-only dependencies.' -ForegroundColor Green
exit 0
