# Plan: Procedural Climbing Dataset Generator

Spec: [./spec.md](./spec.md)

Convention: (Red) = failing tests first, (Green) = minimum code to pass. **(exempt)** = covered
by user-driven editor verification per workflow.md Test Coverage Policy.

Depends on: `impact_dataset_20261012` (reuses RNG, pose buffer, pose rebuild, IK, joint limits,
ragdoll tone, bake, manifest, shard, report, disk estimate, sim world, commandlet host).

If the Phase 1 spike triggers the fallback, Phase 4's pin/character tasks become a kinematic
pose plus physics for breaks only; Phases 2, 3, 5 and 6 are unchanged.

---

## Phase 1: Foundations, Docs and Feasibility Spike

- [ ] Task: Update `product.md` with a "Synthetic climbing generation" feature section
- [ ] Task: Update `tech-stack.md` — new `KimodoClimbCapture` module (`UncookedOnly`), dependency direction (Climb → Impact → Capture, never reversed), schema v4, climbing architecture; dated note
- [ ] Task: Create `KimodoClimbCapture` module (`Build.cs` with `IncludeOrderVersion = Latest`, log category); verify dependency direction holds
- [ ] Task: Spike **(exempt)** — static four-pin hang on a vertical box wall in GameAnimationSample: kinematic hand/foot bodies at fixed points, Physics Control torso targets, joint projection; measure chain stretch and jitter (per-joint separation, elbow/knee angular velocity at rest); tune solver iterations, mass conditioning, parent-space strength
- [ ] Task: Spike **(exempt)** — body–surface collision: knees/chest/pelvis against the wall next to pinned limbs, no pops, no penetration beyond threshold
- [ ] Task: Spike **(exempt)** — one weight shift + kinematic hand reach along an arc to a new hold; one hand-hold break (barn-door swing) and a both-hands drop; bake through the impact recorder for review
- [ ] Task: Record spike findings and the **kill decision** in `tech-stack.md` — physics-pinned body (spec default) or fallback (kinematic planner + IK pose, physics for breaks/swings/falls only); tuning baseline
- [ ] Task: Conductor - User Manual Verification 'Phase 1: Foundations, Docs and Feasibility Spike' (Protocol in workflow.md)

## Phase 2: Surfaces and Hold Generation

- [ ] Task: Write tests for the per-run RNG — `(seed, runIndex)` streams stable, independent of impact streams (Red)
- [ ] Task: Implement climb run RNG derivation reusing `ImpactRandom` (Green)
- [ ] Task: Write tests for primitive surface params — each of the 7 types (vertical, slab, overhang, stepped, cylinder, sphere, wedge) builds transform + dimensions within configured ranges; slab/overhang angles signed correctly (Red)
- [ ] Task: Implement surface param sampling and arena builder (engine basic-shape meshes + ground; actor spawn part **exempt**) (Green)
- [ ] Task: Write tests for triangle extraction from a `UStaticMesh` — world-space triangles + normals for cube, cylinder, sphere; count and area match the mesh (Red)
- [ ] Task: Implement mesh triangle extraction (Green)
- [ ] Task: Write tests for Poisson-disk hold sampling — no pair closer than spacing; only climbable-band triangles used; coverage of a known wall within tolerance; deterministic per seed (Red)
- [ ] Task: Implement Poisson-disk hold sampling (Green)
- [ ] Task: Write tests for hold typing — jug/crimp/sloper/foothold from normal vs gravity and local curvature at bucket boundaries; weights respected (Red)
- [ ] Task: Implement hold typing (Green)
- [ ] Task: Write tests for edge (ledge) detection — box top lip found as a segment; flat interior edges rejected below dihedral threshold; sphere/cylinder yield no ledges; stepped-wall lips all found (Red)
- [ ] Task: Implement edge detection and edge-hold segments (Green)
- [ ] Task: Write tests for the local tangent frame — derived from a contact set's normals on plane, cylinder, sphere; wall-plane projection consistent across a lateral move on a cylinder (Red)
- [ ] Task: Implement the local tangent frame (Green)
- [ ] Task: Implement `UClimbBatchDefinition` (surfaces, holds, character setup, scenario weights, directions, moves per run, count/grid, seed, output root, timing) and write tests for validation — empty surface set or zero-weight scenarios fail with a stated reason (Red → Green)
- [ ] Task: Hold debug draw in PIE preview **(exempt)**
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Surfaces and Hold Generation' (Protocol in workflow.md)

## Phase 3: Move Planner (Pure Logic)

- [ ] Task: Write tests for reach limits — per-limb reach from shoulder/hip scaled to bone lengths; out-of-reach holds rejected; hand-above-foot ordering; crossed-limb threshold (Red)
- [ ] Task: Implement reach and ordering constraints (Green)
- [ ] Task: Write tests for the support region — CoM projected in the tangent frame inside the convex hull of remaining contacts with margin; edge holds contribute segments; 3- and 2-contact cases (Red)
- [ ] Task: Implement the support region (Green)
- [ ] Task: Write tests for target selection — best progress along each of 8 directions; never a move leaving CoM unsupported; `stuck` when no valid move; deterministic tie-break (Red)
- [ ] Task: Implement target selection (Green)
- [ ] Task: Write tests for the move phase machine — weight shift → release → reach → grab → settle; release never before CoM reaches the support target; settle duration in range; no single-frame target jumps (Red)
- [ ] Task: Implement the move phase machine (Green)
- [ ] Task: Write tests for torso targets — hips below hand line; weight-shift path; pelvis turn about surface normal on lateral reaches proportional to reach; drop-knee/outside-edge switch above threshold; idle breathing/sway noise bounded and deterministic (Red)
- [ ] Task: Implement the torso target generator (Green)
- [ ] Task: Write tests for the reach arc — reuses impact swing arc, offset from surface along the normal; endpoints exact; wrist/ankle orientation blends to hold normal at grab (Red)
- [ ] Task: Implement limb reach arc and end orientation (Green)
- [ ] Task: Write tests for head look-at — targets the next hold from weight shift through grab, returns after; angular limits and rate clamped (Red)
- [ ] Task: Implement head look-at targets (Green)
- [ ] Task: Write tests for scenarios —
  - three-point: free limb never pinned; leg flags opposite the reaching hand;
  - hold break: fires at the sampled move;
  - re-grab: only if a hold is within reach during the swing;
  - ledge hang: braced when footholds are in reach, else free;
  - shimmy: hand-over-hand along the edge segment, hands never crossing;
  - ledge break: one or both hands (Red)
- [ ] Task: Implement scenario sampling and scenario rules in the planner (Green)
- [ ] Task: Write tests for run outcome and event log — `completed` / `stuck` / `recovered` / `hanging` / `fell` from synthetic traces; event list (`release`, `reach`, `grab`, `slip`, `break`, `regrab`) consistent with the per-frame contact mask (Red)
- [ ] Task: Implement the run outcome classifier and event log (Green)
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Move Planner' (Protocol in workflow.md)

## Phase 4: Climbing Character and Scenarios in Physics

- [ ] Task: Implement `AClimbCharacter` — impact Physics Control setup (`W_`/`P_`/`M_`), explicit targets in a fixed reference frame, joint projection, spike tuning baseline, body–surface collision setup **(exempt)**
- [ ] Task: Wire pins — contact limbs kinematic at holds, reaching limb kinematic along the arc, released limbs simulated under joint tone **(exempt)**
- [ ] Task: Wire the planner → character loop for traverse, with debug draw (holds, pins, CoM, support region, look target) **(exempt)**
- [ ] Task: Wire hold break, re-grab, ledge break and fall under impact ragdoll tone and joint limits **(exempt)**
- [ ] Task: Wire ledge hang (braced/free) and shimmy **(exempt)**
- [ ] Task: Write tests for climb validity — pinned limb within 1 cm of its hold while marked gripping; limb penetration into the surface beyond threshold invalidates with a reason; NaN/explosion/fold checks reused (Red → Green)
- [ ] Task: Review harness — preview runs per scenario × surface, baked for eye review with timestamps **(exempt)**
- [ ] Task: Conductor - User Manual Verification 'Phase 4: Climbing Character and Scenarios in Physics' (Protocol in workflow.md)

## Phase 5: Recording, Bake, Sidecar v4 and Captions

- [ ] Task: Write tests for four-limb recorder IK — generalised two-bone IK places pinned hands/feet exactly on holds with simulated elbow/knee as pole; bones below wrist/ankle keep locals; unreachable targets do not stretch (Red)
- [ ] Task: Generalise impact leg IK to arms and implement the climb recorder pass (Green)
- [ ] Task: Write tests for run IDs — stable for identical params, change when any param changes (Red)
- [ ] Task: Implement run IDs (Green)
- [ ] Task: Write tests for the bake of a climb run — frame count, rate, bone tracks, transform equality at first/middle/last frame (Red → Green, reusing `ImpactClipBaker`)
- [ ] Task: Write tests for sidecar schema v4 — `clipSource: synthetic_climb`; `climb` block (seed, surface, direction, scenario, free limb, holds, events, contact mask, outcome); v2/v3 still read; v4 round-trips (Red)
- [ ] Task: Implement schema v4 structs and serializer; regenerate `SidecarSchemaSample.json` (Green)
- [ ] Task: Write tests for climb captions and tags — every scenario × direction × surface × outcome yields a sentence with no unfilled slot; multiple phrasings deterministic per seed; tags match params (Red)
- [ ] Task: Implement climb caption and tag generation (Green)
- [ ] Task: Write tests for the climb sidecar builder from run setup + recording (Red)
- [ ] Task: Implement the climb sidecar builder; review harness writes sidecars beside clips (Green)
- [ ] Task: Finger curl best-effort — fixed grip/open poses on finger bones if one small task suffices; otherwise record as out of scope **(exempt)**
- [ ] Task: Conductor - User Manual Verification 'Phase 5: Recording, Bake, Sidecar v4 and Captions' (Protocol in workflow.md)

## Phase 6: Batch Runner, Headless Host and Report

- [ ] Task: Write tests for the climb batch runner over manifest resume + sharding (reused types) — completed run IDs skipped, partial runs regenerated (Red)
- [ ] Task: Implement `FClimbBatchRunner` (Green)
- [ ] Task: Implement the `ClimbBatch` commandlet (`-Batch= -Start= -Count=`) on the impact sim world **(exempt for the host; runner covered above)**
- [ ] Task: Write tests for the distribution report — counts per scenario / direction / surface / outcome / event type match the manifest (Red)
- [ ] Task: Implement the climb distribution report and disk estimate guard (Green)
- [ ] Task: Panel entry for climb batch preview in PIE **(exempt)**
- [ ] Task: Full-grid sample batch over all surfaces × scenarios × directions; measure throughput (target ≥5,000 runs/hour/process) and record in `tech-stack.md`
- [ ] Task: Conductor - User Manual Verification 'Phase 6: Batch Runner, Headless Host and Report' (Protocol in workflow.md)
