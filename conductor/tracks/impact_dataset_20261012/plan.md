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

## Phase 2: Batch Definition and Deterministic Parameter Sampling [checkpoint: 60e7f9d]

- [x] Task: Write tests for the deterministic RNG stream — `(seed, clipIndex)` yields identical streams across runs, different indices diverge, independent of evaluation order (Red) (c485d52)
- [x] Task: Implement per-clip RNG derivation (Green) (53ec1ec)
- [x] Task: Write tests for the impactor archetype catalog — loaded as data; every archetype has noun phrase, description, size, mass/speed/radius ranges and verbs; impulse derived from sampled mass × speed stays within the archetype's implied range; catalog weights respected over N samples; empty or malformed catalog fails with a stated reason (Red) (3113099)
- [x] Task: Implement `UImpactorCatalog` data asset with the starter catalog (small bullet … wrecking ball) and archetype sampling (Green) (3858b95)
- [x] Task: Write tests for `FImpactParams` sampling — every field within configured distribution bounds; reaction impulse = physical momentum × the archetype's reaction gain, both recorded (a real bullet carries ≤380 kg·cm/s, far below the 15k–50k the approved reactions need; people react to being shot, not to its momentum); derived intensity bucket matches impulse at bucket boundaries; direction samples are unit vectors covering all octants over N samples (Red) (875bb21)
- [x] Task: Implement `FImpactParams` and the sampler over distribution structs (Green) (b739d84)
- [x] Task: Write tests for delivery-mode sampling — instant / burst (2–N hits, spacing in range, ordered frames) / sustained (duration > 0) / projectile (shape, mass, velocity within bounds) (Red) (dfe1948)
- [x] Task: Implement delivery-mode sampling and the hit schedule (Green) (66133ba)
- [x] Task: Write tests for the body region table — every region resolves to ≥1 physics body for the UEFN mannequin physics asset (the GASP bake skeleton); unknown body names fail with a stated reason (Red) (747a353)
- [x] Task: Implement the per-skeleton region → body mapping as data (Green) (382708b)
- [x] Task: Write tests for contact-size distribution — impulse split across bodies within radius sums to the sampled impulse; point contact hits exactly one body (Red) (d037e0c)
- [x] Task: Implement size-based impulse distribution (Green) (cd1de36)
- [x] Task: Write tests for clip ID hashing — stable for identical params, changes when any param changes (Red) (50e3288)
- [x] Task: Implement deterministic clip IDs (Green) (c36a4fa)
- [x] Task: Implement `UImpactBatchDefinition` data asset (source anim set, character setup, distributions, timing, seed, output root, grid/count mode) and write tests for source anim resolution — folder + skeleton filter, empty set reported as a named failure (Red → Green) (dfd3054)
- [x] Task: Add editor buttons that fill a catalog with the starter archetypes and a region table with the UEFN entries, appending only what is missing (Red → Green) (e1fd0a9)
- [x] Task: Conductor - User Manual Verification 'Phase 2: Batch Definition and Deterministic Parameter Sampling' (Protocol in workflow.md) (60e7f9d)

## Phase 3: Reaction Character, Impact Delivery and Outcome Detection [checkpoint: 45ea9b9]

- [x] Task: Write tests for the outcome classifier — synthetic pelvis-height / CoM-vs-support / control-blend traces classify as flinch, stagger, knockdown, ragdoll; boundary thresholds deterministic (Red) (d35b834)
- [x] Task: Implement the outcome classifier as pure logic over per-frame state (Green) (8330c80)
- [x] Task: Write tests for the clip window state machine — pre-roll → hit(s) → reaction → settle/recover → end; settle requires velocity under threshold for N frames; max-reaction ceiling ends clip and records reason (Red) (db747fe)
- [x] Task: Implement the clip window state machine (Green) (1dfd2ed)
- [x] Task: Write tests for control strength profile — sampled profile within bounds; per-body release by body-tree distance from contact (strongest at contact, joint release only within 2 links); impact phase (upper body near-zero for the impact frames, then blend); legs follow the hips' impact curve on moving sources; recover blend returns to full strength; ragdoll transition zeroes strength; no strength changes in a single frame (Red) (326d3df)
- [x] Task: Implement the strength profile and blend logic, matching the approved spike values in `tech-stack.md` (Green) (8bc8383)
- [x] Task: Write tests for the foot swing arc — endpoints exact, peak lift at mid-swing, monotonic horizontal progress, smooth start and end (Red) — moved ahead of the planner, which uses it (4b7eac8)
- [x] Task: Implement the swing arc (Green) (34e2521)
- [x] Task: Write tests for the balance planner as pure logic — capture point from CoM, excess velocity and height; low-pass on excess velocity; step trigger threshold; landing at capture point with stance width; reach clamp; feet alternate; minimum stance frames; step budget; fall on repeated short steps or low CoM; settle detection; target frame ends centred over new footing (Red) (c0af2d2)
- [x] Task: Implement `FImpactBalancePlanner` (Green) (36304d4)
- [x] Task: Write tests for fall decisions as pure logic — high-impact threshold (impulse vs body and struck-body mass) releases to full ragdoll with no recovery; trip: a leg hit on a moving source disrupts that leg's stride and, when momentum carries the CoM past a support the feet cannot catch, falls forward; moving sources run the capture-point balance check (stumble, recover or fall); outcome records the trip (Red) (b761fe9)
- [x] Task: Implement fall and trip decisions in the balance planner (Green) (930cce6)
- [x] Task: Write tests for the source clock — moving/standing classified once from unwarped root speed; time warp drop proportional to momentum removed along travel, clamped, recovering over the configured frames; root motion carried across loops (Red) (69ddd35)
- [x] Task: Implement the source pose sampler and source clock (Green) (25d86c7)
- [x] Task: Write tests for surface-point delivery — contact point on the struck body's surface along the incoming direction; impulse spread across bodies within the contact radius sums to the sampled impulse; off-centre contact yields torque (Red) (ca2867e)
- [x] Task: Implement root tracking during ragdoll — root follows pelvis ground projection; write tests for the projection math (Red → Green) (c5ea5a0)
- [x] Task: Implement invalid-clip detection — NaN transforms, velocity explosion, ground penetration beyond threshold; write tests over synthetic traces (Red → Green) (eab0c0f)
- [x] Task: Implement `AImpactReactionCharacter` — skeletal mesh with no anim instance (posed only by physics), named per-body controls and body modifiers (`W_`/`P_`/`M_`), targets from the source pose sampler in a fixed reference frame carrying root motion **(exempt)** — moved after the pure-logic tasks it is assembled from (7aa425b)
- [x] Task: Implement impact delivery — `AddImpulseAtLocation` at the surface point, contact-radius spread, burst, sustained force, spawned projectile actors with collision **(exempt for the physics scene; spread maths covered above)** (068ede8)
- [x] Task: Wire the reaction character: kinematic pre-roll, kinematic planted/stepping feet, explicit per-body targets in a fixed reference frame, joint projection, one world per process **(exempt)** (7aa425b, 068ede8 - delivered inside the character and simulator)
- [x] Task: Write tests for ragdoll tone and joint limits (verification feedback: limbs bent unnaturally in the foot-shot trip) — a released body keeps a joint-only tone fading from a sampled start to a sampled end with raised damping and no world hold, changing by at most 0.1 per frame; joint angles decomposed into twist / swing1 / swing2 in the constraint frame; excess beyond an asset limit measured per axis (free axes never exceed); validity rejects a joint beyond its limit by more than the margin (Red) (33bd8f4)
- [x] Task: Implement ragdoll tone and hard joint limits — on release only the world controls switch off; joint controls hold the pose frozen at release at the faded tone; the physics asset's soft limits become hard limits at the same angles; joint-limit excess measured each frame (Green) (18270d7)
- [x] Task: Write tests for whole-body impact transfer (verification feedback: the car and foot-shot ragdolls still bent intensely) — a wide contact shares impulse by mass so no neighbour is flung faster than the struck body; each impactor has a contact time and an instant hit is pushed over it (car at least 0.1 s, bullets and punches one frame); a falling body braces its trunk and neck harder than its limbs (Red) (1d9b192)
- [x] Task: Implement whole-body impact transfer — mass-weighted spread, contact-time delivery and a braced core in the ragdoll (Green) (f20aa53)
- [x] Task: Write tests for natural ragdoll hip ranges (verification feedback: legs went into the splits in the car and trip clips) — a range relative to the standing pose fits inside the asset's symmetric limit as a centre and half-width; turning the parent frame to the centre measures swing1 from there (Red) (031cc8c)
- [x] Task: Implement natural ragdoll hip ranges — on release the hips are held to 70 degrees of flexion, 15 of extension and 20 sideways and twisting (Green) (860e5cd)
- [x] Task: Tune the ragdoll to the approved "Loose" look (verification feedback: slightly too rigid) — lighter tone (0.1-0.18, fading to 20-40%), core bracing 1.25x, joint damping 4.5x, hips 90 forward / 25 back / 30 sideways and twist; tests adjusted to the new ranges (c2e427f)
- [x] Task: Conductor - User Manual Verification 'Phase 3: Reaction Character, Impact Delivery and Outcome Detection' (Protocol in workflow.md) (45ea9b9)

## Phase 4: Pose Recording, Bake and Sidecar v3 [checkpoint: 479f236]

- [x] Task: Write tests for the pose buffer — pre-reserved from max clip frames × bone count at clip start, no reallocation across a full simulated clip, overflow invalidates with reason (Red) (c877c30)
- [x] Task: Implement the fixed-step pose recorder (component-space transforms) (Green) (241de31)
- [x] Task: Write tests for the pose rebuild — body rotations kept, positions rebuilt from source bone lengths (no joint separation for any input), non-body bones from the source, root carries displacement (Red) (3db151f)
- [x] Task: Implement the pose rebuild (Green) (9842442)
- [x] Task: Write tests for leg IK in the recorder — planted foot lands exactly on target; source-contact detection for moving sources; knee follows the pole; bones below the foot keep their locals; unreachable targets do not stretch (Red) (b3ca9a9)
- [x] Task: Implement recorder leg IK (Green) (af832ed)
- [x] Task: Write tests for the bake — recorded buffer → `UAnimSequence` with matching frame count, frame rate, bone tracks, and sampled transforms equal at first/middle/last frame (Red) (a379b28)
- [x] Task: Implement the bake via `IAnimationDataController` and save under `<OutputRoot>/<Batch>/<ClipId>` (Green) (01e04c9)
- [x] Task: Write tests for sidecar schema v3 — `clipSource`, absent input layers, `impact` block with all hits and outcome; v2 files still read; v3 round-trips (Red) (7525826)
- [x] Task: Implement schema v3 structs and serializer changes; regenerate `SidecarSchemaSample.json` (Green) (c7c303c)
- [x] Task: Write tests for the human-language description — names subject, source motion, impactor noun with its verb, region, direction and outcome; every archetype × region × outcome combination yields a non-empty sentence with no unfilled slot; multiple phrasings selected deterministically from the clip seed; source-motion phrase from the per-animation table with name-derived fallback; tag set matches params (Red) (4f79bac)
- [x] Task: Implement template-based description and tag generation, plus the per-animation source-motion description table as data (Green) (4f4ecf4)
- [x] Task: Write tests for the clip sidecar builder — added so Phase 4 verification shows real clips' sidecars: from a clip's setup and recording, a v3 synthetic-impact sidecar with identity (clip id, baked asset, frame rate, duration), skeleton (path, bone count, fingerprint), labels (caption and tags from the caption generator, full trim, validity and reason), and the impact block (seed, source animation and start, source motion, intensity, transferred impulse and gain, control profile, outcome, every hit with frame, duration including contact time, impactor noun/description/size/mass/speed, region, struck body, local and world direction, impulse, radius, mode, projectile spec) (Red) (87cc3c8)
- [x] Task: Implement the clip sidecar builder; record the struck body and character orientation in the clip recording; the Phase 3 review writes a sidecar beside each review clip (Green) (3085add)
- [x] Task: Conductor - User Manual Verification 'Phase 4: Pose Recording, Bake and Sidecar v3' (Protocol in workflow.md) (479f236)

## Phase 5: Batch Runner, Resume and Headless Host

- [x] Task: Write tests for the batch manifest — completed IDs persisted after each clip; reload skips completed; partially written clip (no manifest entry) is regenerated; atomic write survives truncation (Red) (f8334d8)
- [x] Task: Implement the resumable batch manifest (Green) (04a4e6c)
- [x] Task: Write tests for sharding — `-Start/-Count` ranges partition indices with no overlap or gap; shards merge into one manifest (Red) (d2d6daf)
- [x] Task: Implement sharding and manifest merge (Green) (b4f9ff3)
- [x] Task: Write tests for the distribution report — counts per region / intensity / mode / outcome / source anim match the manifest (Red) (536dc21)
- [x] Task: Implement the distribution report (Green) (270cd85)
- [x] Task: Write tests for the disk estimate — warns when estimate exceeds free space (Red → Green) (5a9194f)
- [x] Task: Write tests for folds judged against the source pose — added when source motion moved to Kimodo-generated clips (user, 2026-10-13: "replace Epic"); Kimodo's authored arms sit up to 34 degrees outside `PA_UEFN_Mannequin`'s cones, so every run clip invalidated on frame 1. A joint's limit is widened to include the source pose's own angle (per axis; an elliptical cone scaled to reach it), and only the physics' excess beyond that counts (Red) (575d3b3)
- [x] Task: Implement source-relative fold detection in the reaction character's samples (Green) (f457ddf)
- [x] Task: Write tests for folds judged against recent targets — the one-cycle Kimodo run invalidated light-hit clips on a 20-26 degree elbow "fold": held joints trail their targets, so they sit at a target from a few frames ago (and after release at the release pose, not the playing source). Excess is the least over the targets of the last 0.1 s (Red) (3a80389)
- [x] Task: Implement target-window fold detection in the simulator and reaction character (Green) (3e0cefd)
- [x] Task: Write tests for joint limits opened to hold the source — Kimodo's forearms twist up to 41 degrees and bend sideways up to 27 on axes `PA_UEFN_Mannequin` locks; the hard lock snapped run elbows to the opposite side when the body went physical (user chose "open axes to source", 2026-10-13). Per clip, an axis the source exceeds opens to the source's range plus a margin; axes the source keeps within are untouched; a cone is scaled to hold every source angle (Red) (cbc764c)
- [x] Task: Implement per-clip limit opening on the reaction character from a scan of the source (Green) (7d5b436)
- [x] Task: Implement `FImpactBatchRunner` — clip loop driving character, recorder, bake, sidecar, manifest; periodic GC; fixed timestep (Green, covered by the runner tests above plus an integration test on a tiny batch) (8a93384)
- [x] Task: Implement the headless host chosen in Phase 1 (`-Batch= -Start= -Count=`), exit codes for success / partial / failure **(exempt)** (75a78ac)
- [x] Task: Write tests for human-readable clip names — added at Phase 5 verification (user: "I need the file names human readable"): IMP_<6-digit index>_<Source>_<Impactor>_<Region>_<Outcome>, source label cleaned from the asset name (M_Kimodo_Crouch_Idle_Loop -> CrouchIdle, seed suffixes and Loop/F dropped), path-safe; the hash id stays the identity in sidecar and manifest (Red) (5dda73b)
- [x] Task: Implement readable names in the runner: asset and sidecar named alike, manifest records the name, a resumed clip's leftovers found by its index prefix (Green) (a068ea2)
- [ ] Task: Conductor - User Manual Verification 'Phase 5: Batch Runner, Resume and Headless Host' (Protocol in workflow.md)

## Phase 6: Panel, Preview and End-to-End Run

- [ ] Task: Write tests for the impact panel controller — select batch, preview N, start/stop, live counters, illegal transitions rejected (Red)
- [ ] Task: Implement the controller with no Slate dependency (Green)
- [ ] Task: Implement `SImpactBatchPanel` under Window → Tools, `FAppStyle`, `LOCTEXT` **(exempt)**
- [ ] Task: Implement PIE debug draw — hit points, force arrows, projectile paths, outcome label **(exempt)**
- [ ] Task: Author a sample `UImpactBatchDefinition` over ≥5 Kimodo-generated source anims (idle and run exist; generate the rest with `tools/kimodo-impact-sources.py`) and the arena fixture level **(exempt)**
- [ ] Task: End-to-end — headless run ≥1,000 clips unattended; kill mid-batch and resume with no duplicates; confirm all four outcomes and all four delivery modes in the report; spot-check baked clips and sidecars; record measured throughput in `tech-stack.md`
- [ ] Task: Conductor - User Manual Verification 'Phase 6: Panel, Preview and End-to-End Run' (Protocol in workflow.md)
