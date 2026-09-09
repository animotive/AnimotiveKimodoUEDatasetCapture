# Plan — End-to-End Capture Slice

**Track ID:** `capture_slice_20260909`

Task lifecycle, TDD split, commit and git-note rules per `conductor/workflow.md`.
Tasks marked **(exempt)** are editor-UX paths verified by hand under the Test Coverage Policy;
they still require the manual verification steps to be written into the task notes.

---

## Phase 1: Plugin Skeleton [checkpoint: 7ea9a45]

- [x] Task: Create `AnimotiveKimodoDatasetCapture.uplugin` — UE 5.7, Win64, `Takes` and `EnhancedInput` plugin dependencies, three module declarations with the types and loading phases from `tech-stack.md` (6d27c0c)
- [x] Task: Add `KimodoDatasetCapture.Build.cs` (`UncookedOnly` deps: `Core`, `CoreUObject`, `Engine`, `EnhancedInput`, `MovieScene`, `MovieSceneTracks`, `Json`, `JsonUtilities`, `TakesCore`) and the module implementation with a dedicated log category (01d45c5)
- [x] Task: Add `KimodoDatasetCaptureEditor.Build.cs` and module implementation (editor deps per `tech-stack.md`); assert the dependency direction editor → capture (28a5ebb)
- [x] Task: Write tests — a trivial automation test in `KimodoDatasetCaptureTests` that fails, to prove the harness runs headless (Red) (e412081)
- [x] Task: Add `KimodoDatasetCaptureTests.Build.cs` and module implementation; make the harness test pass and confirm the headless command from `workflow.md` reports it (Green) (f7cc5a4)
- [x] Task: Add `Resources/Icon128.png` and `Config/FilterPlugin.ini` (a2874e3)
- [x] Task: Verify a clean `Build.bat KimodoEditor Win64 Development` with no new warnings, and that the plugin loads in the editor (91b474d)
- [x] Task: Conductor - User Manual Verification 'Phase 1: Plugin Skeleton' (Protocol in workflow.md) (7ea9a45)

## Phase 2: Capture Data Model and Sidecar Schema v1 [checkpoint: ed7b932]

- [x] Task: Write tests for `FDatasetInputSample` — axis storage, button bitmask set/clear, press/release derivation between consecutive samples, boundary values (Red) (00855dd)
- [x] Task: Implement `FDatasetInputSample` as an allocation-free `USTRUCT` with named button accessors (Green) (808fc46)
- [x] Task: Write tests for the sidecar `USTRUCT` set — all four blocks present, `SchemaVersion` first and readable standalone, frame-keyed input stream (Red) (6cf8e8c)
- [x] Task: Implement the sidecar `USTRUCT`s: identity + timing, raw input stream, skeleton identity, label block (Green) (5d49a53)
- [x] Task: Write tests for sidecar JSON round-trip — write → read → compare equal, including empty tag list, invalid clip with reason, and full-range trim defaults (Red) (4f0bfbf)
- [x] Task: Implement `FDatasetSidecarSerializer` over `FJsonObjectConverter`, writing and reading schema v1 (Green) (a9171ad)
- [x] Task: Write tests for the skeleton fingerprint — stable across runs for identical ordered bone name/parent pairs, different for a changed hierarchy or reordering (Red) (4b0f134)
- [x] Task: Implement the skeleton fingerprint hash over the reference skeleton (Green) (948210a)
- [x] Task: Write tests for trim frame math — defaults spanning the full recorded range, in/out clamped to recorded bounds, points landing exactly on frame edges (Red) (2908d1a)
- [x] Task: Implement trim frame helpers in frame space, no float seconds (Green) (df0ded0)
- [x] Task: Conductor - User Manual Verification 'Phase 2: Capture Data Model and Sidecar Schema v1' (Protocol in workflow.md) (ed7b932)

## Phase 3: MovieScene Input Track (Alignment Core)

- [x] Task: Write tests that construct a `UMovieSceneDatasetInputSection`, write known keys at known `FFrameNumber`s, and read them back at the same frames on a given tick resolution (Red) (804632e)
- [x] Task: Implement `UMovieSceneDatasetInputTrack` and `UMovieSceneDatasetInputSection` with one channel per axis and one per button, in frame space (Green) (14064f7)
- [x] Task: Write tests for sample-to-channel conversion — a sequence of `FDatasetInputSample`s at frames N..N+k produces keys on exactly those frames, with no off-by-one at either end (Red) (c4dc86f)
- [x] Task: Implement the sample-to-channel writer, taking the frame number from the sequence clock rather than counting ticks (Green) (bbf0c9b)
- [x] Task: Write tests for channel-to-sidecar extraction — reading the section back yields the same frame-keyed stream that was written (Red) (67045c8)
- [x] Task: Implement sidecar generation derived from the section, so JSON has no independent source of truth (Green) (2d3822b)
- [ ] Task: Register a minimal track editor so the input track is visible in Sequencer **(exempt)**
- [ ] Task: Conductor - User Manual Verification 'Phase 3: MovieScene Input Track (Alignment Core)' (Protocol in workflow.md)

## Phase 4: Take Recorder Source

- [ ] Task: Write tests for target pawn resolution — resolves the local player controller's pawn, and reports a named failure when there is no controller or no pawn (Red)
- [ ] Task: Implement target pawn resolution through engine interfaces only, with no project-specific assumptions (Green)
- [ ] Task: Write tests for buffer pre-reservation — capacity derived from max duration × frame rate at record start, and no reallocation across a full simulated capture (Red)
- [ ] Task: Implement `UDatasetInputRecorderSource : UTakeRecorderSource` with `PreRecording` / `TickRecording` / `StopRecording`, sampling `UPlayerInput` into pre-reserved storage (Green)
- [ ] Task: Register the source with Take Recorder so it appears in the sources list **(exempt)**
- [ ] Task: Write tests for the alignment claim — a scripted input pattern (button pressed at a known frame) lands on the same `FFrameNumber` in the section as the corresponding animation frame (Red)
- [ ] Task: Make the alignment test pass, taking frame numbers from the take's clock (Green)
- [ ] Task: Write tests for fail-loud behaviour — pawn destroyed mid-take and no-pawn-at-arm produce an invalid result with a reason, and no partial section or sidecar is written (Red)
- [ ] Task: Implement fail-loud capture invalidation per `product-guidelines.md` (Green)
- [ ] Task: Write tests for auto-stop — capture ends exactly at the configured max duration in frames (Red)
- [ ] Task: Implement auto-stop at max duration (Green)
- [ ] Task: Conductor - User Manual Verification 'Phase 4: Take Recorder Source' (Protocol in workflow.md)

## Phase 5: Capture Panel, Overlay and Labels

- [ ] Task: Write tests for the non-Slate panel controller — arm/disarm/record/stop state machine, elapsed and remaining time in frames, invalid state with reason, illegal transitions rejected (Red)
- [ ] Task: Implement the panel controller as a plain type with no Slate dependency (Green)
- [ ] Task: Implement `SDatasetCapturePanel` bound to the controller, registered under Window → Tools, `FAppStyle` only, all strings `LOCTEXT` **(exempt)**
- [ ] Task: Implement the PIE viewport overlay — recording indicator plus remaining-time countdown, capture state only **(exempt)**
- [ ] Task: Write tests for the record-toggle command — editor hotkey and gamepad chord both drive the same controller transition (Red)
- [ ] Task: Implement the `UICommandList` hotkey binding and the gamepad trigger read from the raw input path (Green)
- [ ] Task: Write tests for label validation — save blocked with an empty caption, tags optional, tags accepted only from the project vocabulary, vocabulary loaded as data (Red)
- [ ] Task: Implement caption + tag entry, project-defined tag vocabulary loading, and save gating (Green)
- [ ] Task: Write tests for the full slice path — simulated capture produces a section and a sidecar containing all four blocks with the entered caption (Red)
- [ ] Task: Wire the panel to write the sidecar next to the take on stop, derived from the section (Green)
- [ ] Task: Add the third-person test pawn and small/large staircase geometry to `Kimodo-Workshop` as a verification fixture, referenced by no plugin code **(exempt)**
- [ ] Task: End-to-end verification against the fixture — record ~10s walking up both staircases with jump and interact presses, confirm alignment by scrubbing in Sequencer, confirm the sidecar contents
- [ ] Task: Conductor - User Manual Verification 'Phase 5: Capture Panel, Overlay and Labels' (Protocol in workflow.md)
