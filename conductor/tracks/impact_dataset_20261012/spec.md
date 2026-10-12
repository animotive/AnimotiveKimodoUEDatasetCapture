# Spec: Automated Impact Reaction Dataset Generator

## Overview

A hands-off pipeline that generates very large volumes of physics-driven hit-reaction
animation clips for the Kimodo training dataset. A batch picks source animations from the
GameAnimationSample library, plays each on a character driven by the **Physics Control**
plugin, fires parameterised impacts at it (region, intensity, size, direction, timing, bursts,
sustained force, real projectiles), lets the reaction emerge (flinch → stagger → knockdown →
ragdoll, with recovery where balance allows), bakes the resulting motion straight to a
`UAnimSequence`, and writes an auto-generated caption + sidecar JSON per clip.

**Product note:** this is a deliberate extension of the product vision. `product.md`
describes operator-driven capture of player play; this track adds *synthetic* generation with
no operator in the loop. `product.md` gains a "Synthetic impact generation" feature section and
`tech-stack.md` gains the new module and dependencies **before** implementation (workflow rule 2).

## Functional Requirements

### FR1 — Batch definition (data asset)
- `UImpactBatchDefinition` data asset holds:
  - source animation set (explicit list and/or folder + skeleton filter + tag filter), with
    loop / play-once flag and start-time sampling;
  - character setup: skeletal mesh, physics asset, Physics Control profile set;
  - impact parameter distributions (FR3) and the clip count or "full grid" mode;
  - batch seed, output root folder, clip timing (pre-roll, max reaction, settle).
- Every clip is a pure function of `(batch seed, clip index)` → reproducible, and its
  parameters are fully recorded in its sidecar.

### FR2 — Reaction character
- Dedicated spawned actor (not the player pawn): skeletal mesh + single-node anim playback of
  the source clip + `UPhysicsControlComponent` driving bodies toward the animated pose.
- Root motion from the source clip moves the actor so locomotion clips travel; on ragdoll, the
  baked root bone follows the pelvis ground projection.
- Arena: flat ground fixture level shipped with the plugin's test content (no client content
  edited).

### FR3 — Impact parameter space
Each clip samples:
- **Body region:** head, neck, chest, abdomen, pelvis, upper/lower arm L/R, hand L/R,
  thigh/shin L/R, foot L/R (mapped to physics bodies via a per-skeleton region table).
- **Intensity:** impulse magnitude buckets (tap / light / medium / heavy / knockdown /
  extreme), continuous within bucket.
- **Size:** contact radius (point → fist → shove → wide/vehicle-scale area), distributing
  impulse across affected bodies.
- **Direction:** sampled on a sphere relative to character facing (front/back/left/right/
  above/below + oblique).
- **Timing:** normalised phase in the source animation when the first hit lands.
- **Delivery mode:** instant impulse | multi-hit burst (2–N hits, spacing sampled) |
  sustained force over a duration | physical projectile actor (sphere/box/capsule, mass,
  velocity) that actually collides.

### FR4 — Reaction and outcome
- Physics Control strength profile sampled per clip (stiff → loose).
- Outcome emerges from physics; a detector classifies it from pelvis height, CoM vs support
  polygon, and control-blend state: `flinch` | `stagger` | `knockdown` | `ragdoll`.
- Recoverable hits blend control strength back to the animation. Balance lost → full ragdoll
  until settled (velocity threshold held for N frames).
- Clip window = pre-roll + reaction + settle/recover, capped by a max-reaction ceiling;
  hitting the ceiling is recorded, not silent.

### FR5 — Recording and bake
- Fixed simulation step (configurable, default 60 Hz; physics substeps independent).
- Per step, component-space bone transforms written to a buffer pre-reserved at clip start
  (no per-frame heap allocation).
- On clip end, bake to `UAnimSequence` on the source skeleton via `IAnimationDataController`,
  saved under `<OutputRoot>/<BatchName>/<ClipId>`.
- Clip IDs are deterministic hashes of the clip parameters.

### FR6 — Labels and sidecar
- Sidecar JSON per clip, **schema v3** (additive over v2): `clipSource: "synthetic_impact"`,
  raw input layers absent (not zero-filled), new `impact` block — source anim + start time,
  every hit (frame, region, body, world/local direction, impulse, radius, mode, projectile
  spec), control profile, detected outcome + outcome frame, seed.
- Auto-generated natural-language caption from a template set, e.g. *"a person jogging is
  struck hard in the chest from the left and falls backwards"*, plus tags
  (`impact`, region, outcome, intensity bucket, source-motion type).
- Clips flagged invalid (physics explosion, NaN, interpenetration with ground beyond
  threshold, ceiling hit) are written with a reason or discarded per batch setting.

### FR7 — Execution hosts
- **Panel (PIE):** select a batch asset, Preview N clips in real time with debug draw of
  hit points/forces, Start/Stop a full run, live counters (done / invalid / per outcome).
- **Commandlet / headless game run:** same runner, fixed timestep as fast as possible,
  `-Batch=<asset> -Start=<i> -Count=<n>` for sharding across processes.
- **Resumable:** a batch manifest records completed clip IDs; rerun skips them. A crash mid-
  batch loses at most the in-flight clip.

### FR8 — Dataset distribution report
- Per batch: counts per region / intensity / mode / outcome / source anim, written to the
  manifest, so imbalance is visible before training.

## Non-Functional Requirements
- **Throughput:** headless run substantially faster than real time; target ≥10k clips/night
  per machine (to be measured in Phase 1 spike and recorded).
- **Determinism:** same seed + index → same parameters; physics result reproducible on the
  same machine/build (cross-machine bitwise not required).
- **No shipping footprint:** new code is `UncookedOnly` / `Editor` like existing modules.
- **Memory:** no unbounded growth across a batch (assets unloaded/GC'd periodically).
- **Disk:** per-clip size measured and reported; batch warns before starting if estimate
  exceeds free space.

## Acceptance Criteria
1. A batch asset over ≥5 GASP source anims runs headless to ≥1,000 clips unattended, with no
   editor interaction.
2. Each clip yields a playable `UAnimSequence` on the source skeleton and a valid v3 sidecar
   that round-trips.
3. All four outcome classes appear in a mixed-intensity batch, and the distribution report
   shows them.
4. All four delivery modes produce clips; projectile clips show a real collision.
5. Killing the process mid-batch and rerunning completes the batch with no duplicates.
6. Same seed + index regenerates identical parameters (test) and visually identical motion
   (manual).
7. Panel preview shows hits with debug draw in PIE.

## Out of Scope
- Get-up / stand-up animations after knockdown (ragdoll settles; no authored recovery).
- Character-vs-character collisions or weapons models; impacts are abstract forces or simple
  projectile shapes.
- FBX/BVH export (covered by `export_bundle` / `kimodo_native_export` tracks; this track
  feeds them via the same sidecar).
- Facial, cloth, hair simulation in the baked output (body skeleton only).
- Multiple characters per clip.
- Distributed farm orchestration beyond `-Start/-Count` sharding.
