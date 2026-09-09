# Spec (STUB) — Trim and Input Timeline

**Track ID:** trim_timeline_20260909
**Status:** stub — expand with /conductor:new-track before implementing
**Depends on:** capture_slice_20260909 (needs the MovieScene input section and frame-space trim helpers)

## Purpose

Let the operator see what a take actually contains and cut the slop off both ends. A 10-second
grab always has dead time at the start and end; without trim, every clip ships that noise into
training data.

## Scope Sketch

- Input timeline widget under the scrub bar: stick axes as line/area channels, buttons as
  event ticks, drawn in frame space against the take frame rate.
- Draggable in and out handles writing TrimInFrame / TrimOutFrame into the label block.
- Snap handles to frames; never to interpolated sub-frame positions.
- Scrub position synchronised with Sequencer playback of the same take.
- Trim is non-destructive metadata. The recorded section and animation are never cut.

## Open Questions

- Reuse a Sequencer widget for the timeline, or draw a custom Slate panel from the section channels?
- Does the operator need per-channel visibility toggles at this stage, or is one combined lane enough?
- Should trim defaults auto-tighten to the first and last frame of non-neutral input?

## Acceptance Sketch

- Handles move only to whole frames and clamp to the recorded range.
- Trim values round-trip through the sidecar unchanged.
- Timeline draws correctly for a capture containing no input at all.
