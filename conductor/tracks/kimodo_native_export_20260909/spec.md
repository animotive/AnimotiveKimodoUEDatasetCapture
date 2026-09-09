# Spec (STUB) — Kimodo Native Export

**Track ID:** kimodo_native_export_20260909
**Status:** stub — expand with /conductor:new-track before implementing
**Depends on:** export_bundle_20260909 (needs the IDatasetExporter interface)

## Purpose

Emit datasets in the format the Kimodo model actually trains on, rather than a generic
interchange the pipeline has to convert first.

## Scope Sketch

- Second IDatasetExporter implementation producing BVH and/or SOMA77 JSON.
- Reuse the retarget and rotation math already solved in KimodoCore rather than writing a
  second implementation that will drift from the first.
- Decide the coupling: link KimodoCore directly, vendor the specific math, or invoke the
  existing KimodoExporter tooling out of process.
- Bone mapping from an arbitrary client skeleton to the training rig, including the case where
  no clean mapping exists and the clip must be rejected rather than guessed.

## Open Questions

- Is SOMA77 the training input, or is BVH sufficient?
- Does taking a KimodoCore dependency drag in the whole KimodoExporter dependency chain, and is
  that acceptable in a client-facing plugin?
- Where does retargeting happen: in the client project at export, or centrally at Animotive?

## Acceptance Sketch

- Exported motion matches a reference conversion produced by the existing Kimodo pipeline.
- An unmappable skeleton fails loudly with the offending bones named.
- No duplicated rotation math between this plugin and KimodoCore.
