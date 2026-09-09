# Spec (STUB) — Export Bundle

**Track ID:** export_bundle_20260909
**Status:** stub — expand with /conductor:new-track before implementing
**Depends on:** capture_slice_20260909, and in practice session_triage_20260909 for ready state

## Purpose

Get labelled clips out of the client project and into an Animotive training pipeline with no
Unreal install and no manual repair at the far end.

## Scope Sketch

- IDatasetExporter interface, so formats are additive and never touch the capture path.
- v1 implementation: AnimSequence to FBX via the editor FBX exporter, plus a JSON manifest per
  bundle listing every clip, its labels, its trim range and its skeleton fingerprint.
- Export respects trim ranges and excludes invalid clips.
- Export UI: pick clips or take the ready set, choose an output directory, report per-clip
  success and failure in plain language.
- Manifest carries the schema version so the pipeline can reject unknown versions loudly.

## Open Questions

- One bundle per session or per clip?
- Bake trim into the exported motion, or export full range and let the manifest carry the range?
- Does the pipeline want the sidecars verbatim in the bundle as well as the manifest?

## Acceptance Sketch

- A bundle opens on a machine with no Unreal project present.
- Invalid clips never appear in a bundle.
- Round trip: manifest plus FBX re-imports to the same frame count and skeleton.
