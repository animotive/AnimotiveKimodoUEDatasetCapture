# Project Tracks

This file tracks all major tracks for the project. Each track has its own detailed plan in its respective folder.

Tracks marked **STUB** are placeholders: purpose, scope sketch and open questions are recorded,
but they are not planned. Expand a stub with `/conductor:new-track` (or by filling in its
`plan.md`) when it comes up for work.

---

## [~] Track: Build the end-to-end capture slice — Slate capture panel, Take Recorder input source writing frame-aligned raw gamepad channels into the take's MovieScene, and a versioned sidecar JSON carrying identity, timing, skeleton fingerprint and labels

_Link: [./conductor/tracks/capture_slice_20260909/](./conductor/tracks/capture_slice_20260909/)_

---

## [ ] Track: **STUB** — Trim handles and input timeline overlay in the capture panel

_Depends on: `capture_slice_20260909`_
_Link: [./conductor/tracks/trim_timeline_20260909/](./conductor/tracks/trim_timeline_20260909/)_

## [ ] Track: **STUB** — Session triage: clip list, discard and retake, label status, mark ready for export

_Depends on: `capture_slice_20260909`_
_Link: [./conductor/tracks/session_triage_20260909/](./conductor/tracks/session_triage_20260909/)_

## [ ] Track: **STUB** — Semantic input layer: Enhanced Input actions and `CharacterMovementComponent` state

_Depends on: `capture_slice_20260909`_
_Link: [./conductor/tracks/semantic_input_20260909/](./conductor/tracks/semantic_input_20260909/)_

## [ ] Track: **STUB** — Export bundle: `IDatasetExporter` interface plus FBX and JSON manifest output

_Depends on: `capture_slice_20260909`, `session_triage_20260909`_
_Link: [./conductor/tracks/export_bundle_20260909/](./conductor/tracks/export_bundle_20260909/)_

## [ ] Track: **STUB** — Kimodo native export: BVH / SOMA77 output reusing `KimodoCore` math

_Depends on: `export_bundle_20260909`_
_Link: [./conductor/tracks/kimodo_native_export_20260909/](./conductor/tracks/kimodo_native_export_20260909/)_

## [ ] Track: **STUB** — Legacy build capture companion application (separate repository; shares only the sidecar schema)

_Shares: sidecar JSON schema + `IDatasetExporter` contract. Not an Unreal plugin._
_Link: [./conductor/tracks/legacy_injector_20260909/](./conductor/tracks/legacy_injector_20260909/)_

---

## [~] Track: Automated impact reaction dataset generator — GASP source animations on a Physics Control driven character, parameterised impacts (region, intensity, size, direction, timing, bursts, sustained force, projectiles), emergent flinch-to-ragdoll reactions baked to AnimSequence plus sidecar v3, run hands-off headless and resumable
*Link: [./conductor/tracks/impact_dataset_20261012/](./conductor/tracks/impact_dataset_20261012/)*
