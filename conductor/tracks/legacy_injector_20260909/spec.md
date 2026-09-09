# Spec (STUB) — Legacy Build Capture (Companion Application)

**Track ID:** legacy_injector_20260909
**Status:** stub — planning placeholder only. SEPARATE REPOSITORY, not this plugin.
**Shares with this plugin:** the versioned sidecar JSON schema and the IDatasetExporter output contract

## Purpose

Capture skeletal animation from Animotive titles that shipped as compiled Unreal builds whose
original projects no longer exist. Same dataset schema, same downstream pipeline, different
problem domain entirely.

## Why it is not part of this plugin

Reverse engineering and process injection against arbitrary engine versions has nothing in
common with editor tooling: different language of problem, different build system, different
maintenance cadence. See the Out of Scope section of conductor/tech-stack.md.

## Approach Sketch

- Base on RE-UE4SS rather than a from-scratch reflection layer or UEVR, whose scope is far
  wider. Verify licences before any reuse.
- Inject, then locate the retained reflection globals (GUObjectArray, FName pool, UClass and
  UFunction tables) by pattern scan.
- Hook once per frame at the swapchain Present for a stable cadence.
- Resolve the local player skeletal mesh component and read the pose reflectively, using
  BlueprintCallable transform queries so unreflected struct offsets are not required. Bone names
  double as socket names. Direct memory reads of bone transform arrays are faster but break on
  every engine version.
- Reference skeleton (bone names plus parent indices) read reflectively for the fingerprint.
- Inputs from the XInput or DirectInput boundary, or from the game UPlayerInput via reflection.
- Emit the same sidecar schema this plugin defines, then reuse the offline bake.

## Risks

- Engine version spread across the back catalogue is the real cost, and it is ongoing.
- Anti-cheat, DRM, or stripped and obfuscated binaries in shipped builds.
- No ground truth to diff against, since the source projects are gone.
- Retargeting captured skeletons that do not match the training rig.

## Open Questions

- Which titles, and which engine versions, are actually in scope?
- Is per-title bespoke work acceptable, or must one tool cover the catalogue?
- Who owns and maintains it, given it is a separate codebase and skill set?
