# Plan: Automated Impact Reaction Dataset Generator

Spec: [./spec.md](./spec.md)

Convention: (Red) = failing tests first, (Green) = minimum code to pass. **(exempt)** = covered
by user-driven editor verification per workflow.md Test Coverage Policy.

---

## Phase 1: Foundations, Docs and Feasibility Spike

- [x] Task: Update `product.md` with a "Synthetic impact generation" feature section and the vision note from the spec (c67c8ed)
- [x] Task: Update `tech-stack.md` — new `KimodoImpactCapture` module (`UncookedOnly`), `PhysicsControl` plugin dependency, direct pose bake path, schema v3, headless runner; dated note (7f29c5f)
- [ ] Task: Add `PhysicsControl` to `.uplugin` dependencies and create `KimodoImpactCapture` module (`Build.cs` with `IncludeOrderVersion = Latest`, log category); verify editor → capture dependency direction holds
- [ ] Task: Spike **(exempt)** — in GameAnimationSample PIE, spawn a mannequin with `UPhysicsControlComponent` playing a GASP jog, apply one impulse to the chest, confirm partial reaction and full ragdoll both work; record findings (API surface, body names, substep settings) in `tech-stack.md`
- [ ] Task: Spike **(exempt)** — measure headless fixed-step throughput (commandlet world tick vs `-game -NullRHI` run) and per-clip bake time/size; record numbers and chosen host in `tech-stack.md`
- [ ] Task: Conductor - User Manual Verification 'Phase 1: Foundations, Docs and Feasibility Spike' (Protocol in workflow.md)

## Phase 2: Batch Definition and Deterministic Parameter Sampling

- [ ] Task: Write tests for the deterministic RNG stream — `(seed, clipIndex)` yields identical streams across runs, different indices diverge, independent of evaluation order (Red)
- [ ] Task: Implement per-clip RNG derivation (Green)
- [ ] Task: Write tests for `FImpactParams` sampling — every field within configured distribution bounds; intensity buckets map to their impulse ranges at boundaries; direction samples are unit vectors covering all octants over N samples (Red)
- [ ] Task: Implement `FImpactParams` and the sampler over distribution structs (Green)
- [ ] Task: Write tests for delivery-mode sampling — instant / burst (2–N hits, spacing in range, ordered frames) / sustained (duration > 0) / projectile (shape, mass, velocity within bounds) (Red)
- [ ] Task: Implement delivery-mode sampling and the hit schedule (Green)
- [ ] Task: Write tests for the body region table — every region resolves to ≥1 physics body for the UE5 mannequin physics asset; unknown body names fail with a stated reason (Red)
- [ ] Task: Implement the per-skeleton region → body mapping as data (Green)
- [ ] Task: Write tests for contact-size distribution — impulse split across bodies within radius sums to the sampled impulse; point contact hits exactly one body (Red)
- [ ] Task: Implement size-based impulse distribution (Green)
- [ ] Task: Write tests for clip ID hashing — stable for identical params, changes when any param changes (Red)
- [ ] Task: Implement deterministic clip IDs (Green)
- [ ] Task: Implement `UImpactBatchDefinition` data asset (source anim set, character setup, distributions, timing, seed, output root, grid/count mode) and write tests for source anim resolution — folder + skeleton filter, empty set reported as a named failure (Red → Green)
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Batch Definition and Deterministic Parameter Sampling' (Protocol in workflow.md)

## Phase 3: Reaction Character, Impact Delivery and Outcome Detection

- [ ] Task: Write tests for the outcome classifier — synthetic pelvis-height / CoM-vs-support / control-blend traces classify as flinch, stagger, knockdown, ragdoll; boundary thresholds deterministic (Red)
- [ ] Task: Implement the outcome classifier as pure logic over per-frame state (Green)
- [ ] Task: Write tests for the clip window state machine — pre-roll → hit(s) → reaction → settle/recover → end; settle requires velocity under threshold for N frames; max-reaction ceiling ends clip and records reason (Red)
- [ ] Task: Implement the clip window state machine (Green)
- [ ] Task: Write tests for control strength profile — sampled profile within bounds; recover blend returns to target strength over configured frames; ragdoll transition zeroes strength (Red)
- [ ] Task: Implement the strength profile and blend logic (Green)
- [ ] Task: Implement `AImpactReactionCharacter` — skeletal mesh, single-node source anim playback with root motion applied to the actor, `UPhysicsControlComponent` setup from the profile **(exempt)**
- [ ] Task: Implement impact delivery — instant impulse, burst, sustained force, spawned projectile actors with collision **(exempt)**
- [ ] Task: Implement root tracking during ragdoll — root follows pelvis ground projection; write tests for the projection math (Red → Green)
- [ ] Task: Implement invalid-clip detection — NaN transforms, velocity explosion, ground penetration beyond threshold; write tests over synthetic traces (Red → Green)
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Reaction Character, Impact Delivery and Outcome Detection' (Protocol in workflow.md)

## Phase 4: Pose Recording, Bake and Sidecar v3

- [ ] Task: Write tests for the pose buffer — pre-reserved from max clip frames × bone count at clip start, no reallocation across a full simulated clip, overflow invalidates with reason (Red)
- [ ] Task: Implement the fixed-step pose recorder (component-space transforms) (Green)
- [ ] Task: Write tests for the bake — recorded buffer → `UAnimSequence` with matching frame count, frame rate, bone tracks, and sampled transforms equal at first/middle/last frame (Red)
- [ ] Task: Implement the bake via `IAnimationDataController` and save under `<OutputRoot>/<Batch>/<ClipId>` (Green)
- [ ] Task: Write tests for sidecar schema v3 — `clipSource`, absent input layers, `impact` block with all hits and outcome; v2 files still read; v3 round-trips (Red)
- [ ] Task: Implement schema v3 structs and serializer changes; regenerate `SidecarSchemaSample.json` (Green)
- [ ] Task: Write tests for caption + tag generation — templates fill motion type, intensity word, region, direction, outcome; every outcome × mode combination produces a non-empty caption; tag set matches params (Red)
- [ ] Task: Implement template-based caption and tag generation (Green)
- [ ] Task: Conductor - User Manual Verification 'Phase 4: Pose Recording, Bake and Sidecar v3' (Protocol in workflow.md)

## Phase 5: Batch Runner, Resume and Headless Host

- [ ] Task: Write tests for the batch manifest — completed IDs persisted after each clip; reload skips completed; partially written clip (no manifest entry) is regenerated; atomic write survives truncation (Red)
- [ ] Task: Implement the resumable batch manifest (Green)
- [ ] Task: Write tests for sharding — `-Start/-Count` ranges partition indices with no overlap or gap; shards merge into one manifest (Red)
- [ ] Task: Implement sharding and manifest merge (Green)
- [ ] Task: Write tests for the distribution report — counts per region / intensity / mode / outcome / source anim match the manifest (Red)
- [ ] Task: Implement the distribution report (Green)
- [ ] Task: Write tests for the disk estimate — warns when estimate exceeds free space (Red → Green)
- [ ] Task: Implement `FImpactBatchRunner` — clip loop driving character, recorder, bake, sidecar, manifest; periodic GC; fixed timestep (Green, covered by the runner tests above plus an integration test on a tiny batch)
- [ ] Task: Implement the headless host chosen in Phase 1 (`-Batch= -Start= -Count=`), exit codes for success / partial / failure **(exempt)**
- [ ] Task: Conductor - User Manual Verification 'Phase 5: Batch Runner, Resume and Headless Host' (Protocol in workflow.md)

## Phase 6: Panel, Preview and End-to-End Run

- [ ] Task: Write tests for the impact panel controller — select batch, preview N, start/stop, live counters, illegal transitions rejected (Red)
- [ ] Task: Implement the controller with no Slate dependency (Green)
- [ ] Task: Implement `SImpactBatchPanel` under Window → Tools, `FAppStyle`, `LOCTEXT` **(exempt)**
- [ ] Task: Implement PIE debug draw — hit points, force arrows, projectile paths, outcome label **(exempt)**
- [ ] Task: Author a sample `UImpactBatchDefinition` over ≥5 GASP source anims and the arena fixture level **(exempt)**
- [ ] Task: End-to-end — headless run ≥1,000 clips unattended; kill mid-batch and resume with no duplicates; confirm all four outcomes and all four delivery modes in the report; spot-check baked clips and sidecars; record measured throughput in `tech-stack.md`
- [ ] Task: Conductor - User Manual Verification 'Phase 6: Panel, Preview and End-to-End Run' (Protocol in workflow.md)
