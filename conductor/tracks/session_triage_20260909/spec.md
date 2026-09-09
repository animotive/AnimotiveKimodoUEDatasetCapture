# Spec (STUB) — Session Triage

**Track ID:** session_triage_20260909
**Status:** stub — expand with /conductor:new-track before implementing
**Depends on:** capture_slice_20260909

## Purpose

A capture session is fifty takes, not one. The operator needs to see the session at a glance,
throw away the bad grabs, and know which clips are still unlabelled.

## Scope Sketch

- Session clip list in the panel: take name, duration, label status, validity.
- Discard a take and immediately retake it with the same settings.
- Mark clips ready for export; ready state stored in the sidecar label block.
- Filter or sort by unlabelled / invalid / ready.
- Session scope definition: per editor session, or per level, or explicit start-session action.

## Open Questions

- Does discard delete the Take Recorder assets, or only mark the sidecar invalid?
- Is a session a persisted object, or reconstructed by scanning sidecars next to takes?
- Bulk labelling for a run of similar takes — worth it, or does it encourage lazy captions?

## Acceptance Sketch

- Invalid clips are visibly distinct and excluded from ready state.
- List survives closing and reopening the panel within the same session.
- Discard never leaves an orphaned sidecar or an orphaned take asset.
