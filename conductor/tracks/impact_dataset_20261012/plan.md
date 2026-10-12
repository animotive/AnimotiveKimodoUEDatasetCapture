# Plan: Automated Impact Reaction Dataset Generator

Spec: [./spec.md](./spec.md)

Convention: (Red) = failing tests first, (Green) = minimum code to pass. **(exempt)** = covered
by user-driven editor verification per workflow.md Test Coverage Policy.

---

## Phase 1: Foundations, Docs and Feasibility Spike [checkpoint: dfcc722]

- [x] Task: Update `product.md` with a "Synthetic impact generation" feature section and the vision note from the spec (c67c8ed)
- [x] Task: Update `tech-stack.md` — new `KimodoImpactCapture` module (`UncookedOnly`), `PhysicsControl` plugin dependency, direct pose bake path, schema v3, headless runner; dated note (7f29c5f)
- [x] Task: Add `PhysicsControl` to `.uplugin` dependencies and create `KimodoImpactCapture` module (`Build.cs` with `IncludeOrderVersion = Latest`, log category); verify editor → capture dependency direction holds (14e90b4)
- [x] Task: Spike **(exempt)** — in GameAnimationSample PIE, spawn a mannequin with `UPhysicsControlComponent` playing a GASP jog, apply one impulse to the chest, confirm partial reaction and full ragdoll both work; record findings (API surface, body names, substep settings) in `tech-stack.md` (c8ec4b6)
- [x] Task: Spike **(exempt)** — measure headless fixed-step throughput (commandlet world tick vs `-game -NullRHI` run) and per-clip bake time/size; record numbers and chosen host in `tech-stack.md` (b1342ce)
- [x] Task: Conductor - User Manual Verification 'Phase 1: Foundations, Docs and Feasibility Spike' (Protocol in workflow.md) (dfcc722)

## Phase 2: Batch Definition and Deterministic Parameter Sampling

- [x] Task: Write tests for the deterministic RNG stream — `(seed, clipIndex)` yields identical streams across runs, different indices diverge, independent of evaluation order (Red) (c485d52)
- [x] Task: Implement per-clip RNG derivation (Green) (53ec1ec)
- [x] Task: Write tests for the impactor archetype catalog — loaded as data; every archetype has noun phrase, description, size, mass/speed/radius ranges and verbs; impulse derived from sampled mass × speed stays within the archetype's implied range; catalog weights respected over N samples; empty or malformed catalog fails with a stated reason (Red) (3113099)
- [x] Task: Implement `UImpactorCatalog` data asset with the starter catalog (small bullet … wrecking ball) and archetype sampling (Green) (3858b95)
- [x] Task: Write tests for `FImpactParams` sampling — every field within configured distribution bounds; reaction impulse = physical momentum × the archetype's reaction gain, both recorded (a real bullet carries ≤380 kg·cm/s, far below the 15k–50k the approved reactions need; people react to being shot, not to its momentum); derived intensity bucket matches impulse at bucket boundaries; direction samples are unit vectors covering all octants over N samples (Red) (875bb21)
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
- [ ] Task: Write tests for control strength profile — sampled profile within bounds; per-body release by body-tree distance from contact (strongest at contact, joint release only within 2 links); impact phase (upper body near-zero for the impact frames, then blend); legs follow the hips' impact curve on moving sources; recover blend returns to full strength; ragdoll transition zeroes strength; no strength changes in a single frame (Red)
- [ ] Task: Implement the strength profile and blend logic, matching the approved spike values in `tech-stack.md` (Green)
- [ ] Task: Write tests for the balance planner as pure logic — capture point from CoM, excess velocity and height; low-pass on excess velocity; step trigger threshold; landing at capture point with stance width; reach clamp; feet alternate; minimum stance frames; step budget; fall on repeated short steps or low CoM; settle detection; target frame ends centred over new footing (Red)
- [ ] Task: Implement `FImpactBalancePlanner` (Green)
- [ ] Task: Write tests for fall decisions as pure logic — high-impact threshold (impulse vs body and struck-body mass) releases to full ragdoll with no recovery; trip: a leg hit on a moving source disrupts that leg's stride and, when momentum carries the CoM past a support the feet cannot catch, falls forward; moving sources run the capture-point balance check (stumble, recover or fall); outcome records the trip (Red)
- [ ] Task: Implement fall and trip decisions in the balance planner (Green)
- [ ] Task: Write tests for the foot swing arc — endpoints exact, peak lift at mid-swing, monotonic horizontal progress, smooth start and end (Red)
- [ ] Task: Implement the swing arc (Green)
- [ ] Task: Write tests for the source clock — moving/standing classified once from unwarped root speed; time warp drop proportional to momentum removed along travel, clamped, recovering over the configured frames; root motion carried across loops (Red)
- [ ] Task: Implement the source pose sampler and source clock (Green)
- [ ] Task: Implement `AImpactReactionCharacter` — skeletal mesh, single-node source anim playback with root motion applied to the actor, `UPhysicsControlComponent` setup from the profile **(exempt)**
- [ ] Task: Write tests for surface-point delivery — contact point on the struck body's surface along the incoming direction; impulse spread across bodies within the contact radius sums to the sampled impulse; off-centre contact yields torque (Red)
- [ ] Task: Implement impact delivery — `AddImpulseAtLocation` at the surface point, contact-radius spread, burst, sustained force, spawned projectile actors with collision **(exempt for the physics scene; spread maths covered above)**
- [ ] Task: Wire the reaction character: kinematic pre-roll, kinematic planted/stepping feet, explicit per-body targets in a fixed reference frame, joint projection, one world per process **(exempt)**
- [ ] Task: Implement root tracking during ragdoll — root follows pelvis ground projection; write tests for the projection math (Red → Green)
- [ ] Task: Implement invalid-clip detection — NaN transforms, velocity explosion, ground penetration beyond threshold; write tests over synthetic traces (Red → Green)
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Reaction Character, Impact Delivery and Outcome Detection' (Protocol in workflow.md)

## Phase 4: Pose Recording, Bake and Sidecar v3

- [ ] Task: Write tests for the pose buffer — pre-reserved from max clip frames × bone count at clip start, no reallocation across a full simulated clip, overflow invalidates with reason (Red)
- [ ] Task: Implement the fixed-step pose recorder (component-space transforms) (Green)
- [ ] Task: Write tests for the pose rebuild — body rotations kept, positions rebuilt from source bone lengths (no joint separation for any input), non-body bones from the source, root carries displacement (Red)
- [ ] Task: Implement the pose rebuild (Green)
- [ ] Task: Write tests for leg IK in the recorder — planted foot lands exactly on target; source-contact detection for moving sources; knee follows the pole; bones below the foot keep their locals; unreachable targets do not stretch (Red)
- [ ] Task: Implement recorder leg IK (Green)
- [ ] Task: Write tests for the bake — recorded buffer → `UAnimSequence` with matching frame count, frame rate, bone tracks, and sampled transforms equal at first/middle/last frame (Red)
- [ ] Task: Implement the bake via `IAnimationDataController` and save under `<OutputRoot>/<Batch>/<ClipId>` (Green)
- [ ] Task: Write tests for sidecar schema v3 — `clipSource`, absent input layers, `impact` block with all hits and outcome; v2 files still read; v3 round-trips (Red)
- [ ] Task: Implement schema v3 structs and serializer changes; regenerate `SidecarSchemaSample.json` (Green)
- [ ] Task: Write tests for the human-language description — names subject, source motion, impactor noun with its verb, region, direction and outcome; every archetype × region × outcome combination yields a non-empty sentence with no unfilled slot; multiple phrasings selected deterministically from the clip seed; source-motion phrase from the per-animation table with name-derived fallback; tag set matches params (Red)
- [ ] Task: Implement template-based description and tag generation, plus the per-animation source-motion description table as data (Green)
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
