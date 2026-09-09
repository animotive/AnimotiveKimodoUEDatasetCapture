# Tech Stack

## Platform

| Item | Value |
|------|-------|
| Engine | **Unreal Engine 5.7** (`D:/UE_5.7`) |
| Host project | `D:/Unreal Projects/Kimodo-Workshop/Kimodo.uproject` |
| Language | **C++** (UE coding standard) |
| Platform allow list | `Win64` |
| Shipping footprint | **None** — editor/PIE only, enforced by module types |

## Plugin Modules

| Module | Type | Loading phase | Responsibility |
|--------|------|---------------|----------------|
| `KimodoDatasetCapture` | `UncookedOnly` | `Default` | Per-frame input sampling, capture data structs, sidecar JSON schema (versioned `USTRUCT`s), trim math |
| `KimodoDatasetCaptureEditor` | `Editor` | `PostEngineInit` | Slate capture panel, `UTakeRecorderSource` registration, MovieScene input track + track editor, PIE viewport overlay, labelling & session triage UI, export UI |
| `KimodoDatasetCaptureTests` | `Editor` | `PostDefault` | UE Automation Tests |

`UncookedOnly` for the capture module is deliberate: the code is live during PIE (which runs
in the editor process) but cannot reach a cooked client build. The "no shipping footprint"
non-goal is enforced by the build system, not by convention.

Dependency direction: `Editor` -> `Capture`. **Never the reverse.**

### Include order (added 2026-09-09)

Every module sets `IncludeOrderVersion = EngineIncludeOrderVersion.Latest;` in its
`Build.cs`.

Unset, a module inherits the host target's `IncludeOrderVersion`, which defaults to
`EngineIncludeOrderVersion.Oldest`. At `Oldest`, the engine's public headers still drag in
their legacy transitive includes, so a header of ours that names a type without including or
forward-declaring it compiles locally and fails in a client tree whose target sets `Latest`.
`KimodoExporter` reached a customer that way, which is how this rule was learned.

Set per module rather than on the target, because module-level wins over target-level: no
consumer's target settings can un-pin it, and a module installed under `Engine/Plugins/` is
forced to `Latest` regardless, so the engine-plugin install case becomes a no-op instead of a
break.

Note: the host `KimodoEditor.Target.cs` is at `Unreal5_4` and emits an `[Upgrade]` diagnostic
on every build. That is the host project's setting, not this plugin's, and the per-module pin
above is what protects this plugin's code from it.

## Engine Dependencies

**`.uplugin` plugin dependencies:**
- `Takes` — Take Recorder (provides `TakeRecorder`, `TakesCore`, `TakeTrackRecorders`, `TakeMovieScene`)
- `EnhancedInput` — semantic action layer
- Sequencer editor surface as needed for the custom track editor

**Key module dependencies:**
- Capture module: `Core`, `CoreUObject`, `Engine`, `EnhancedInput`, `MovieScene`, `MovieSceneTracks`, `Json`, `JsonUtilities`, `TakesCore`
- Editor module: the above plus `Slate`, `SlateCore`, `ToolWidgets`, `UnrealEd`, `EditorFramework`, `LevelSequence`, `LevelSequenceEditor`, `Sequencer`, `MovieSceneTools`, `TakeRecorder`, `TakeTrackRecorders`, `WorkspaceMenuStructure`, `InputCore`

## Architecture

### Recording — Take Recorder is the clock

A custom **`UTakeRecorderSource`** is registered with Take Recorder. Take Recorder owns
start/stop, the frame clock, and the output `LevelSequence`; the source writes input samples
as channels on a custom **MovieScene input section** in that same sequence, alongside the
animation track Take Recorder records itself.

Consequences, all of them load-bearing:
- **Frame alignment is structural.** Input and animation share one MovieScene tick resolution
  and one frame clock. Pause, time dilation, and hitches cannot desync them.
- **Preview is free.** Scrubbing the take in Sequencer scrubs the input timeline with it.
- **The sidecar is derived, not parallel.** JSON is generated *from* the track on save, so
  there is no second source of truth to drift.

### Input sources

| Layer | Source | Availability |
|-------|--------|--------------|
| **Raw gamepad (required)** | `APlayerController::PlayerInput` (`UPlayerInput`) polled each capture tick — stick and trigger axes, 14 button bits | Any project, no configuration |
| **Raw keyboard + mouse (required, added 2026-09-09)** | The same `UPlayerInput`, read for 11 keyboard/mouse button bits (WASD, Space, E, LeftShift, LeftCtrl, three mouse buttons) and `MouseX`/`MouseY` delta | Any project, no configuration |
| **Semantic (opt-in)** | `UEnhancedPlayerInput` action instance data — Jump, Interact, directional input | Projects using Enhanced Input |
| **Character state (opt-in)** | `UCharacterMovementComponent` — velocity, movement mode (grounded/falling), crouch | Projects using `ACharacter` |

No client-side modification is required at any layer. The plugin reads public engine surfaces
and degrades to the raw layer when the opt-in layers are not present.

### Triggers

- Editor hotkey via `UICommandList` / `FInputBindingManager`, plus panel button.
- **No maximum duration** (revised 2026-09-09). The operator presses stop, and the PIE
  overlay shows elapsed time rather than a countdown.
- The capture buffer is pre-reserved to an internal safety ceiling (5 minutes) purely so the
  per-frame path never allocates. Exceeding it invalidates the take with a stated reason; it is
  not a user-facing setting and must never be presented as one.
- Bindable gamepad button/chord, read from the same raw input path as capture.

### Storage

- **Canonical:** Take Recorder's own `LevelSequence` / `AnimSequence` assets in the project,
  plus a per-take sidecar `.json` (input stream, character state, caption, tags, trim points,
  frame rate, skeleton identity, schema version).
- Sidecar serialisation via `FJsonObjectConverter` over versioned `USTRUCT`s. Schema version
  is written from the first commit and is readable without parsing the rest of the file.
- **Schema v2** (2026-09-09): keyboard and mouse are a **separate raw device**, not normalised
  onto the gamepad axes. Samples carry `keyboardButtons` (its own bitmask, named by key rather
  than by assumed meaning, because a client may bind `W` to anything) plus `mouseDeltaX` /
  `mouseDeltaY` in engine units, unscaled — a mouse has no rest position or maximum, so a
  normalised value would be an invention indistinguishable from a real stick reading. The
  `RawInput` layer split into `RawGamepadInput` + `RawKeyboardMouseInput`, both always
  recorded. v1 files are rejected rather than upgraded: nothing had been captured at v1, so a
  migration path would be untested code guarding an empty set.
- **Wire format** (full notes in `DatasetSidecar.h`): lower camel
  case field names; booleans keep the Unreal `b` prefix on the wire (`bIsValid`); enum fields
  serialise as enumerator **names**, so enumerators may be added but never renamed or
  reordered; floats are written at full double precision and round-trip exactly. The test
  suite writes a live sample to `Saved/KimodoDatasetCapture/SidecarSchemaSample.json`, so the
  documented format cannot drift from the code.

### Export

Behind an `IDatasetExporter` interface so formats can be added without touching capture:
- **v1:** `AnimSequence` -> **FBX** via the editor's FBX exporter, plus a **JSON manifest**
  per bundle.
- Later, if the Kimodo pipeline wants its native format: a BVH/SOMA77 exporter, reusing
  `KimodoCore` math rather than reimplementing it. Not a dependency today.

## Performance Constraints

- Per-frame sampling path allocates nothing on the heap. A 10-second capture at 60fps is ~600
  samples; buffers are reserved at record start.
- Capture must not measurably affect PIE frame time — the data describes the game's motion, so
  perturbing the game corrupts the data.

## Testing

Framework: **UE Automation Tests** in `KimodoDatasetCaptureTests`.

```bash
# Build the editor target (run from the engine directory)
"D:/UE_5.7/Engine/Build/BatchFiles/Build.bat" KimodoEditor Win64 Development \
  "D:/Unreal Projects/Kimodo-Workshop/Kimodo.uproject"

# Run this plugin's tests headless
"D:/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "D:/Unreal Projects/Kimodo-Workshop/Kimodo.uproject" \
  -ExecCmds="Automation RunTests KimodoDatasetCapture;Quit" \
  -Unattended -NullRHI -nopause
```

Required coverage: frame alignment of input samples to animation frames, sidecar JSON
round-trip (write -> read -> compare), trim math at boundary values, graceful degradation when
Enhanced Input or `ACharacter` is absent.

Panel behaviour is verified by user-driven editor testing; logic behind the panel is factored
out of the widgets so it is testable without an editor window.

> Note: NullRHI editor boots have deadlocked locally for some scene fixtures in
> `KimodoExporter`. If that recurs, user-driven editor verification is the documented fallback.

## Out of Scope / Companion Tooling

**Legacy-build capture via process injection — separate application, not this plugin.**

Animotive holds shipped Unreal titles whose original projects are no longer available.
Skeletal data can still be captured from those builds by injecting into the running process
and driving UE's retained reflection system (`GUObjectArray`, `FName` pool, `UClass` /
`UFunction` tables), in the manner of UEVR / RE-UE4SS: locate the reflection globals by
pattern scan, hook once per frame at the swapchain `Present`, find the local player's
`USkeletalMeshComponent`, and read the pose reflectively (bone names double as socket names,
so `BlueprintCallable` transform queries via `ProcessEvent` avoid depending on unreflected
struct offsets). Direct memory reads of the bone transform arrays are faster but break on
every engine version, since those members carry no reflection data.

This is deliberately **not** part of this plugin: different problem domain (reverse
engineering and injection vs editor tooling), different build system, separate repository.
RE-UE4SS is the appropriate base rather than a from-scratch reflection layer or UEVR, whose
scope is far wider; licences to be verified before any reuse.

**The one contract shared with this plugin is the versioned sidecar JSON schema and the
`IDatasetExporter` output format.** That shared contract is the reason the schema is versioned
from the first commit — a legacy-capture tool built later must be able to emit datasets the
same training pipeline ingests, with no downstream special-casing.

## Repository Layout

```
AnimotiveKimodoDatasetCapture/
├── AnimotiveKimodoDatasetCapture.uplugin
├── Source/
│   ├── KimodoDatasetCapture/{Public,Private}/
│   ├── KimodoDatasetCaptureEditor/{Public,Private}/
│   └── KimodoDatasetCaptureTests/Private/
├── Resources/            # plugin icon
├── Config/               # FilterPlugin.ini
└── conductor/            # Conductor guides, tracks, plans
```
