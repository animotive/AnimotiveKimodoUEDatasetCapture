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
| `KimodoImpactCapture` (added 2026-10-12) | `UncookedOnly` | `Default` | Synthetic impact generation: batch definition, deterministic parameter sampling, reaction character, outcome detection, fixed-step pose recorder, `AnimSequence` bake, batch runner, manifest |
| `KimodoDatasetCaptureTests` | `Editor` | `PostDefault` | UE Automation Tests |

`UncookedOnly` for the capture module is deliberate: the code is live during PIE (which runs
in the editor process) but cannot reach a cooked client build. The "no shipping footprint"
non-goal is enforced by the build system, not by convention.

Dependency direction: `Editor` -> `Capture`. **Never the reverse.**
`KimodoImpactCapture` -> `KimodoDatasetCapture` (it reuses the sidecar schema and serializer);
`Editor` -> `KimodoImpactCapture`. The capture module never depends on the impact module.

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
- `PhysicsControl` (added 2026-10-12) — `UPhysicsControlComponent` drives the reaction
  character toward the animated pose. Engine plugin under `Plugins/Experimental`, marked Beta
  in 5.7; its `PhysicsControl` module is `Runtime`, which is fine because only our
  `UncookedOnly` module links it.
- Sequencer editor surface as needed for the custom track editor

**Key module dependencies:**
- Capture module: `Core`, `CoreUObject`, `Engine`, `EnhancedInput`, `MovieScene`, `MovieSceneTracks`, `Json`, `JsonUtilities`, `TakesCore`
- Impact module (added 2026-10-12): `Core`, `CoreUObject`, `Engine`, `PhysicsCore`, `PhysicsControl`, `Json`, `JsonUtilities`, `KimodoDatasetCapture`
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
- **Schema v3** (2026-10-12, planned in `impact_dataset_20261012`): additive over v2. Adds
  `clipSource` (`player_capture` | `synthetic_impact`) and an optional `impact` block
  (source anim + start time, every hit, control profile, detected outcome + frame, seed).
  Synthetic clips omit the raw input layers rather than writing zeros, because a zeroed stick
  is a real reading. Unlike v1, v2 data may now exist, so v2 files keep reading and default
  to `player_capture`.
- **Wire format** (full notes in `DatasetSidecar.h`): lower camel
  case field names; booleans keep the Unreal `b` prefix on the wire (`bIsValid`); enum fields
  serialise as enumerator **names**, so enumerators may be added but never renamed or
  reordered; floats are written at full double precision and round-trip exactly. The test
  suite writes a live sample to `Saved/KimodoDatasetCapture/SidecarSchemaSample.json`, so the
  documented format cannot drift from the code.

### Synthetic impact generation (added 2026-10-12)

A second producer of dataset clips, with no operator and no Take Recorder. Track:
`impact_dataset_20261012`.

**Why Take Recorder is not the host here.** A Take per clip creates a `LevelSequence`, an
`AnimSequence`, and drives a record lifecycle built for a human pressing buttons. At the target
volume (tens of thousands of clips per batch) that cost dominates. Clips are instead sampled
directly and baked once. Frame alignment is still structural, for a different reason: there is
no input stream to align with, and every pose and every hit is stamped with the same
fixed-step frame index.

**Pipeline per clip:**
1. **Parameters.** `(batch seed, clip index)` derives a per-clip RNG stream; the sampler draws
   source anim + start phase, Physics Control strength profile, and the hit schedule (impactor
   archetype from a data catalog — what hit them, its size, mass and speed — body region,
   direction, timing, delivery mode). Impulse and intensity are derived from the archetype,
   never sampled independently. Parameters are
   pure data and are hashed into a deterministic clip ID.
2. **Character.** A dedicated spawned actor — not the player pawn — with the source skeletal
   mesh, single-node playback of the source anim (root motion applied to the actor), and a
   `UPhysicsControlComponent` whose controls and body modifiers are created from limb bones
   (`CreateControlsAndBodyModifiersFromLimbBones`), then driven by
   `SetControlMultiplier(s)` / `SetBodyModifiersMovementType` /
   `SetBodyModifiersPhysicsBlendWeight` as the strength profile evolves.
3. **Impact.** Instant impulse, burst, sustained force, or a spawned projectile actor that
   genuinely collides. Contact size spreads impulse across bodies inside the radius.
4. **Outcome.** Pure-logic classifier over per-frame pelvis height, centre of mass against
   the support polygon, and control blend: `flinch` / `stagger` / `knockdown` / `ragdoll`.
   Recoverable → blend control strength back up; balance lost → strength to zero, settle.
5. **Record.** Component-space bone transforms written each fixed step into a buffer reserved
   at clip start from `max clip frames × bone count`. During ragdoll the root bone follows the
   pelvis ground projection.
6. **Bake.** Buffer → `UAnimSequence` on the source skeleton through
   `IAnimationDataController`, saved under `<OutputRoot>/<Batch>/<ClipId>`, plus a schema v3
   sidecar and a generated human-language description naming the source motion, the
   impactor, the region and the outcome ("A person is shot in the foot with a small bullet
   while running and stumbles but keeps going").
7. **Manifest.** Completed clip ID appended and flushed atomically; rerun skips completed IDs.

**Hosts.** One runner, two hosts: the editor panel runs it in PIE for preview and tuning with
debug draw; a headless host runs it as fast as the fixed step allows, sharded with
`-Batch= -Start= -Count=`. Which headless host (commandlet ticking its own world vs a
`-game -NullRHI` run) is decided by the Phase 1 throughput spike and recorded here.

**Physics Control spike findings (2026-10-12).** Proven headless by
`KimodoDatasetCapture.Impact.Spike.PhysicsControlProbe` on the GameAnimationSample UEFN
mannequin (`SKM_UEFN_Mannequin` + `PA_UEFN_Mannequin`, 22 bodies; GASP animations are authored
on the UEFN skeleton, so that is the bake skeleton). Each item is load-bearing for the runner:

- **One step per engine frame.** The tick task manager runs a tick function at most once per
  `GFrameCounter`. Stepping a world N times inside one frame ran actors, components and the
  Chaos solver exactly once and froze (solver frame stayed at 1). Each step increments
  `GFrameCounter` (or is a real engine frame).
- **Bare worlds need `WorldSettings->NotifyBeginPlay()`.** No game mode means `BeginPlay` never
  reaches actors, and components never register their ticks.
- **One world per process; actors respawned per clip.** Creating a world per run crashed the
  Water world subsystem on the seventh world. Teardown order is `EndPlay` → `BeginTearingDown`
  → destroy context → `DestroyWorld` → GC.
- **Explicit targets, not `bUseSkeletalAnimation`.** Physics Control's own animation-target path
  did not hold the character up headless (stronger controls collapsed it further). The runner
  samples the source sequence itself (`UAnimSequence::GetBoneTransform` per mesh bone, composed
  to component space) and sets every control target by hand. No anim graph, deterministic.
- **Controls created per physics body, named.** `W_<bone>` world-space, `P_<bone>` parent-space
  (parent = nearest ancestor with a body), `M_<bone>` body modifier. 43 controls for 22 bodies.
- **Targets live in a fixed reference frame** (spawn transform × sampled root motion). The mesh
  component follows its simulated root body, so targets built from the live component
  transform chase the body and the character drifts away (it floated up at ~4 m/s).
- **Root motion carried across loops** by re-basing each loop on where the last ended; a 2.5 s
  run loop travels 12.5 m with no discontinuity.
- **Pre-roll is kinematic on the exact source pose**; bodies switch to simulated at the reaction.
  The impulse lands one frame after the switch — on the switch frame the body is still
  kinematic and the impulse is dropped.
- **A strength profile is required, not optional.** At full world-space strength (8) the controls
  cancelled a 30000 kg·cm/s chest hit within one frame. Dropping world-space strength to zero at
  the hit and smooth-stepping back over 60 frames, with parent-space strength held, produced:
  idle — chest displaced 86 cm, pelvis dipped to 80 cm, recovered to 6 cm; run — chest displaced
  89 cm, stumbled and kept running, recovered to 13 cm. Releasing all controls collapsed the
  character (pelvis 15 cm).
- **Deterministic.** Repeated runs produced bit-identical numbers in one process.
- **Headless runs terminate.** Every probe run exited by itself in 40–60 s; the test runner
  still enforces a post-completion kill and a hard timeout.

Tuning baseline: world strength 8, parent strength 16, critically damped, 60 Hz fixed step.
UEFN mannequin faces +Y.

**Logic stays out of the actor.** Sampling, classification, clip-window state, strength
blending, root projection, invalid detection, manifest and report are plain types tested
headless; the actor and the physics scene are the exempt layer.

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

## Test projects

The plugin is developed in `Kimodo-Workshop` and reached from other projects by a Windows
**directory junction**, matching the convention already used for `KimodoExporter`:

```powershell
New-Item -ItemType Junction `
  -Path   "D:\Unreal Projects\<Project>\Plugins\AnimotiveKimodoDatasetCapture" `
  -Target "D:\Unreal Projects\Kimodo-Workshop\Plugins\AnimotiveKimodoDatasetCapture"
```

One source tree, one set of binaries, no copies to drift. Nothing creates these links
automatically; a new test project needs one made by hand.

**`GameAnimationSample`** (added 2026-09-09) is linked this way and is the better manual
verification fixture: engine 5.7, a full locomotion character, and geometry to walk up. It is
also **Blueprint-only, with no C++ modules at all**, which makes it a genuine test of the
product's zero-integration-tax requirement rather than a convenience.

Verified 2026-09-09: all 57 automation tests run and pass inside `GameAnimationSample` with
no rebuild and no project changes. A client project therefore needs no C++, no target file and
no code of its own for this plugin to load and work.

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

Impact module coverage (2026-10-12): deterministic per-clip RNG, parameter bounds at bucket
edges, region-to-body mapping, impulse distribution summing to the sampled impulse, outcome
classification on synthetic traces, clip-window state machine, pose buffer reservation, bake
frame/bone/transform equality, v3 round-trip, caption coverage of every outcome × mode,
manifest resume and sharding.

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
│   ├── KimodoImpactCapture/{Public,Private}/
│   ├── KimodoDatasetCaptureEditor/{Public,Private}/
│   └── KimodoDatasetCaptureTests/Private/
├── Resources/            # plugin icon
├── Config/               # FilterPlugin.ini
└── conductor/            # Conductor guides, tracks, plans
```
