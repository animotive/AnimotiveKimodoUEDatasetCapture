# Spec: Procedural Climbing Dataset Generator

## Overview

A hands-off pipeline that generates labelled climbing animation clips (Uncharted / Breath of
the Wild style) for the Kimodo training dataset. The UEFN mannequin is spawned on a climbable
surface covered in sampled holds; its hands and feet are pinned to holds by physics while a
planner moves it across the surface in set directions. Weight distribution, swings and drops
emerge from gravity acting on a Physics Control body hanging off the pinned limbs. Each run
bakes to a `UAnimSequence` with a sidecar that frame-stamps every limb move, grip, release and
break, plus a generated caption.

Reuses `impact_dataset_20261012` infrastructure: Physics Control character setup, deterministic
RNG, fixed-step pose buffer, pose rebuild, two-bone IK, joint limits + ragdoll tone, bake,
manifest, sharding, report, disk estimate, headless commandlet host.

**Product note:** a third producer in the product vision. `product.md` gains a "Synthetic
climbing generation" section and `tech-stack.md` gains the new module **before**
implementation (workflow rule 2).

## Functional Requirements

### FR1 — Module and batch definition
- New `KimodoClimbCapture` module (`UncookedOnly`, `IncludeOrderVersion = Latest`), depending on
  `KimodoImpactCapture` and `KimodoDatasetCapture`. Never the reverse.
- `UClimbBatchDefinition` data asset: surface set (FR2 shape types + size/angle ranges +
  weights), hold generation settings (spacing, hold-type weights, edge threshold), character
  setup (reuses impact character mesh/physics asset/control profile), scenario weights (FR5),
  direction set, moves per run, run count or full grid, seed, output root, timing (settle
  frames, max run length).
- Every run is a pure function of `(batch seed, run index)` → reproducible; all params recorded
  in the sidecar.

### FR2 — Surfaces (primitives in v1)
- **Walls:** vertical, slab (leaning back 5–30°), overhang (leaning out 5–45°), stepped ledges
  (stacked boxes with horizontal lips).
- **Curved:** cylinder pillar, sphere boulder, wedge.
- Surfaces built from engine basic-shape static meshes in a plugin-owned arena, with ground
  below so falls land. No client content edited.
- **Mesh-agnostic by design:** hold sampling reads only triangles + normals from a
  `UStaticMesh`, so imported meshes need no sampler changes later (see Out of Scope).

### FR3 — Hold generation
- Poisson-disk sampling over mesh triangles whose normal lies within a climbable band (not
  floors/ceilings); spacing set in batch.
- Hold record: position, surface normal, type, grip quality.
  - **Point holds:** `jug` / `crimp` / `sloper` / `foothold`, from normal vs gravity and local
    curvature.
  - **Edge holds (ledges):** convex mesh edges with dihedral angle above threshold and a
    near-horizontal edge line; stored as a segment so a hand can grip anywhere along it.
- **Local tangent frame** per contact set (plane, cylinder, sphere), so support-region and
  direction maths work on curved surfaces.
- Hold sets are deterministic per seed and drawn as debug in PIE preview.

### FR4 — Climbing character
- Same Physics Control body as the impact character: named `W_`/`P_`/`M_` controls, explicit
  targets in a fixed reference frame, joint projection, approved ragdoll tone and limits.
- **Pins:** a limb on a hold makes its hand/foot body kinematic at the hold (impact's "feet
  kinematic while planted" finding, applied to all four limbs). Wrist/ankle orientation aligns
  to the hold normal.
- **Reaching limb is kinematic along its arc**, so it lands exactly on time; physics resolves
  only elbows, knees, shoulders, hips and torso. Released (broken/free) limbs are simulated
  under joint tone.
- **Torso and pelvis targets** come from the planner (FR6). The simulated chain between pelvis
  and pins resolves elbow, knee, hip and shoulder.
- **Body–surface collision:** knees, chest and pelvis collide with the surface without popping
  against adjacent pins; the setup is proven in the Phase 1 spike.
- No base animation. Rest pose + planner targets are the whole source; an additive Kimodo layer
  is a possible later addition (Out of Scope).

### FR5 — Scenarios
Each run samples one:
- **Traverse:** four-point climbing, one limb moving at a time, ≥3 contacts kept. Directions:
  up, down, left, right, four diagonals.
- **Three-point hang:** one designated limb stays off the wall for the run. An arm hangs; a leg
  flags opposite the reaching hand for counterbalance.
- **Hold break:** at a sampled move, a hand or foot pin releases unplanned. The body swings on
  the remaining contacts (barn-door when one side lets go). The free limb re-grabs if a hold is
  in reach. If all hand contacts are lost, the body falls under impact ragdoll tone.
- **Ledge hang:** both hands on an edge hold.
  - *Braced:* feet pinned on the wall below.
  - *Free:* no footholds in reach (or overhang); legs swing as a pendulum and damp out.
- **Shimmy:** hand-over-hand left/right along an edge, braced or free.
- **Ledge break:** the edge gives way under one or both hands. One hand: swing on the other.
  Both hands: drop and fall.

### FR6 — Move planner (pure logic)
- **Target selection:** the limb + hold with the best progress along the run direction,
  subject to:
  - per-limb reach (from shoulder/hip, scaled to bone lengths);
  - hand-above-foot ordering;
  - no crossed limbs beyond a threshold;
  - CoM, projected to the local tangent frame, inside the remaining contacts' support region
    with a margin.
- **Move phases:**
  1. **Weight shift:** pelvis/CoM target moves over the remaining supports before release.
     Hips hang below hands; arms mostly straight.
  2. **Release.**
  3. **Reach:** swing-arc path reused from impact, offset off the surface.
  4. **Grab:** pin.
  5. **Settle.**
- **Body rotation / drop knee:** on lateral hand reaches the pelvis turns about the surface
  normal, bringing the reaching side's hip into the wall. The opposite foot switches to outside
  edge / drop knee above a reach threshold.
- **Head look-at:** head and neck target the next hold from weight shift through grab, then
  return.
- **Idle life:** small bounded breathing/sway noise on torso targets, so pauses are not dead
  still.
- **Run end:** move count reached, no valid move (`stuck`), fall, or max length (recorded).
- **Run outcome:** `completed` | `stuck` | `recovered` (re-grab after break) | `hanging` (ended
  on fewer contacts) | `fell`.

### FR7 — Recording and bake
- Fixed-step recording into the impact pose buffer (pre-reserved, no per-frame allocation).
- Pose rebuild from source bone lengths, then two-bone IK on **all four limbs**: pinned
  hands/feet land exactly on holds, simulated elbow/knee as pole. Impact leg IK generalised to
  arms.
- Bake to `UAnimSequence` on the UEFN skeleton under `<OutputRoot>/<Batch>/<RunId>`; run IDs
  are deterministic hashes of run params.
- Validity reused: NaN, velocity explosion, folds beyond limits. New check: limb penetration
  into the surface beyond a threshold.

### FR8 — Sidecar v4, captions, tags
- **Schema v4**, additive over v3: `clipSource: "synthetic_climb"`, no input layers, and a
  `climb` block containing:
  - seed, surface type + params, run direction, scenario, free limb;
  - hold list (id, position, normal, type);
  - event list — every limb `release` / `reach` / `grab` / `slip` / `break` / `regrab` with
    frame, limb, hold id;
  - per-frame contact state (4-bit mask);
  - outcome + outcome frame.
- v2/v3 files still read.
- **Captions** summarise the run, e.g.:
  - *"A person climbs up and to the left across a steep overhanging wall."*
  - *"A person shimmies right along a ledge, hanging with their feet swinging free."*
  - *"A person climbing a rock pillar loses their right handhold, swings out and grabs a new
    hold."*
  - *"A person hanging from a ledge loses their grip and falls."*
- Multiple phrasings, chosen deterministically from the seed.
- Tags: `climb`, scenario, direction, surface type, outcome, free limb, event types present.

### FR9 — Hosts, resume, report
- PIE preview from the panel with debug draw (holds, pins, CoM, support region, look target).
- Headless commandlet `-run=ClimbBatch -Batch= -Start= -Count=`, following the impact
  sim-world rules: one world per process, one step per `GFrameCounter`, actors respawned per
  run.
- Manifest resume, sharding, disk estimate, and a distribution report by scenario / direction /
  surface / outcome / event type.

### FR10 — Fingers (best-effort)
- If cheap: fixed curl pose on finger bones while gripping, open while reaching. Kimodo's
  skeleton has no finger bones, so not required. If it needs more than one small task, drop it
  and record as out of scope.

## Non-Functional Requirements
- **Look first:** a Phase 1 spike proves a static hang, weight shift, one reach, one hold break
  and a drop on a vertical wall in PIE. The user approves by eye before the planner is built on
  it. Each verification phase re-checks the look with re-baked clips and their timestamps.
- **Spike kill criteria:** if a static four-pin hang still visibly jitters or stretches after
  tuning (solver iterations, mass conditioning, parent-space strength), fall back to a
  kinematic pose from planner + IK, with physics only for breaks, swings and falls. The
  decision is recorded in `tech-stack.md`. Planner, sampler, sidecar and runner are unaffected
  by either choice.
- **Deterministic:** same seed + index → bit-identical output within one process.
- **No per-frame heap allocation** in the record path.
- **Logic stays out of the actor:** hold sampling, edge detection, tangent frame, planner,
  support region, event log, outcome, captions, run ID, sidecar and report are plain types
  tested headless. Actor + physics scene are the exempt layer.
- **Throughput:** ≥5,000 runs/hour/process for 6 s runs; disk guarded by estimate.

## Risks
- Heavy-body-on-light-pin chain stretch/jitter in Chaos (spike measures it first; kill criteria
  above).
- The look comes from planner target tuning, not physics alone; expect several rounds of
  by-eye feedback, as with impact.
- **Kimodo elbow roll:** the known ~20° arm-roll retarget issue (being fixed in
  `KimodoExporter`) will be far more visible in climbing, where the arms carry the body.

## Acceptance Criteria
1. A batch over all seven primitive surfaces × all scenarios × eight directions runs headless
   to completion, survives kill + resume, and its report covers every scenario × surface cell.
2. Pinned hands/feet sit on their holds with ≤1 cm error on every frame they are marked
   gripping in baked clips.
3. Every traverse move shows the CoM shifting over the remaining supports before the limb
   releases, measurable in the recorded CoM trace.
4. Hold-break and ledge-break clips show a physics-emergent swing or drop, with no authored
   keyframes.
5. Every clip has a non-empty caption and a v4 sidecar whose event list matches its contact
   mask frame for frame.
6. The user approves the look of sample clips for each scenario by eye.

## Out of Scope (candidate follow-up tracks)
- Imported meshes (rock, cliff, facade) and hand-placed hold markers; the sampler is already
  mesh-agnostic.
- Dynos / jump-catches; top-out / mantle over a ledge.
- Base or additive Kimodo climbing animation layer.
- Climbing from locomotion (walk up to the wall, mount).
- Finger animation beyond the FR10 best-effort pose.
