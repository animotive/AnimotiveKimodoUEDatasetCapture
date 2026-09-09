# Spec — End-to-End Capture Slice

**Track ID:** `capture_slice_20260909`
**Type:** feature
**Status:** new

## Purpose

Prove the plugin's single load-bearing claim — that recorded animation and recorded input are
frame-aligned by construction — on real data, inside a complete pipeline, before any polish
exists. Everything the product guide describes (trim, timeline overlay, session triage,
semantic input, export bundles) attaches to this slice later. If alignment is wrong, nothing
downstream is salvageable, so it is validated first.

## Success Definition

An operator opens the panel, arms a capture, plays, records ~10 seconds of a character walking
up a staircase and pressing jump, and gets:

1. A Take Recorder `LevelSequence` containing the animation track **and** a MovieScene input
   section whose channels carry the gamepad state for those same frames.
2. A sidecar `.json` next to the take carrying schema version, take identity, timing, the
   per-frame raw input stream keyed by frame number, a skeleton fingerprint, and a label block
   with the caption they typed.
3. Confidence in alignment: scrubbing the take in Sequencer moves through the input channels
   in lockstep with the pose.

## In Scope

### Plugin skeleton

- `AnimotiveKimodoDatasetCapture.uplugin` — UE 5.7, Win64, `CanContainContent: false`,
  dependencies on `Takes` and `EnhancedInput`.
- Three modules building clean with no warnings:
  - `KimodoDatasetCapture` (`UncookedOnly`) — sampling, data structs, sidecar schema
  - `KimodoDatasetCaptureEditor` (`Editor`) — panel, take source registration, track editor
  - `KimodoDatasetCaptureTests` (`Editor`) — UE Automation Tests
- Dedicated log category.
- Dependency direction enforced: editor depends on capture, never the reverse.

### Capture data model

- `FDatasetInputSample` — one frame of raw gamepad state: stick axes (left/right X,Y), trigger
  axes, and a button bitmask with named accessors. Plain `USTRUCT`, no heap members.
- Button transitions derivable from consecutive samples (pressed/released), not stored twice.
- Ring-free, pre-reserved `TArray<FDatasetInputSample>` sized from max duration × frame rate at
  record start. **No allocation during capture.**

### Sidecar JSON v1

Versioned `USTRUCT`s serialised with `FJsonObjectConverter`. Schema version is the first field
and readable without parsing the remainder.

| Block | Fields |
|-------|--------|
| Identity + timing | `SchemaVersion`, `TakeName`, `LevelSequencePath`, `AnimSequencePath`, `FrameRate`, `TickResolution`, `CaptureStartedUtc`, `DurationFrames` |
| Raw input stream | Per-frame samples keyed by `FrameNumber` (never seconds) |
| Skeleton identity | `SkeletonPath`, `BoneCount`, `BoneHierarchyFingerprint` (stable hash of ordered bone name + parent index pairs) |
| Labels | `Caption`, `Tags[]`, `TrimInFrame`, `TrimOutFrame`, `bIsValid`, `InvalidReason` |

`TrimInFrame` / `TrimOutFrame` are written in this slice (defaulting to full range); the UI to
change them is a later track.

### Take Recorder integration

- `UDatasetInputRecorderSource : UTakeRecorderSource`, registered so it appears as a Take
  Recorder source.
- Lifecycle driven entirely by Take Recorder: `PreRecording` reserves buffers and resolves the
  target pawn, `TickRecording` samples input at the take's frame clock, `StopRecording`
  finalises the section and writes the sidecar.
- `UMovieSceneDatasetInputTrack` / `UMovieSceneDatasetInputSection` with one channel per axis
  and one per button, written in frame space (`FFrameNumber`), on the take's own tick
  resolution.
- Target pawn resolution: the local player controller's pawn. If no pawn or no player
  controller exists, the source reports failure — it does not record an empty section.

### Capture panel

- `SDatasetCapturePanel` registered under **Window → Tools → Kimodo Dataset Capture**, docked
  and themed with `FAppStyle` only.
- Controls: arm/disarm, max duration (default 10s), record/stop, live state readout
  (armed / recording / elapsed / stopped / invalid + reason).
- Editor hotkey bound through a `UICommandList` for record toggle during PIE.
- Logic lives in a non-Slate controller type so it is testable without an editor window.

### PIE overlay

- Recording indicator plus remaining-time countdown drawn in the play viewport for the whole
  capture, mirroring panel state. Capture state only — no menus, no settings.

### Labelling (minimal)

- Caption text field and a tag list fed from a project-defined vocabulary (data, not code).
- Save is blocked when the caption is empty; tags are optional.
- A clip that failed capture is marked invalid with a plain-language reason and its sidecar
  records that; invalid clips carry `bIsValid: false`.

### Gamepad trigger

- A bindable controller button/chord toggles record, read from the same raw input path used for
  capture, so an operator never leaves the pad.

### Verification fixture

- A simple third-person test pawn plus small/large staircase geometry placed in
  `Kimodo-Workshop` for dev verification. A fixture in the host project — **never** a
  dependency in plugin code.

## Out of Scope (later tracks)

- Trim handles and the input timeline overlay drawing
- Session triage list, discard/retake, ready-for-export marking
- Enhanced Input semantic action layer
- `UCharacterMovementComponent` state capture
- FBX + JSON manifest export bundle and `IDatasetExporter` implementations
- Retargeting, BVH/SOMA77 output, backend upload
- The legacy-build injector companion tool

## Acceptance Criteria

1. All three modules build clean, no new warnings, from a fresh `Build.bat` invocation.
2. Panel opens from Window → Tools, docks, and reads correctly in light and dark editor themes.
3. Arming and recording during PIE produces a Take Recorder take containing a MovieScene input
   section with populated channels.
4. **Alignment:** for a scripted input pattern (e.g. jump pressed on a known frame), the input
   channel's key lands on the same `FFrameNumber` as the corresponding animation frame, proven
   by an automation test over the recorded section, not by eye.
5. Sidecar JSON is written next to the take, contains all four blocks, and round-trips
   (write → read → compare equal) in an automation test.
6. Skeleton fingerprint is stable across runs for the same skeleton and differs for a different
   skeleton.
7. Capture with no player pawn fails loudly: panel shows the reason, no partial take, no
   sidecar.
8. Auto-stop fires at the configured max duration with the countdown visible in the viewport
   throughout.
9. Gamepad trigger starts and stops a capture with no keyboard input.
10. No heap allocation in the per-frame sampling path (buffers reserved at record start;
    verified by inspection and an allocation-count assertion where practical).
11. All user-facing strings use `LOCTEXT` and follow the voice rules in
    `product-guidelines.md`.

## Risks

| Risk | Mitigation |
|------|-----------|
| MovieScene channel API churn / unfamiliarity is the largest unknown in the track | Build the track/section and a hand-written key first, with a test, before wiring live capture |
| `UTakeRecorderSource` tick cadence may not match the animation recorder's sampling exactly | Take the frame number from the take's own clock rather than counting ticks; assert equality in tests |
| NullRHI headless boot has deadlocked in the sibling plugin | Keep tests pure-logic where possible; fall back to documented user-driven verification per `workflow.md` |
| Sidecar written at `StopRecording` could miss a crash mid-take | Acceptable for this slice — a crashed take is an invalid take by definition (fail loud) |
| Test pawn fixture creeping into plugin assumptions | Plugin resolves the pawn through engine interfaces only; no fixture asset referenced in code |
