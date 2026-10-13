"""Writes the KimodoBatch manifest that generates the impact pipeline's source motion.

The impact batches draw their source animations from Kimodo rather than from Epic's
GameAnimationSample clips. Run the manifest with the Kimodo plugin's batch commandlet against a
local Kimodo server (see the -ini override in the memory/notes; never the hosted backend):

    UnrealEditor-Cmd <Project>.uproject -run=KimodoBatch -manifest=<out.json> -unattended -nullrhi

Recipes follow the Kimodo locomotion R&D (sprint ceiling and jump reliability reports, 2026-10-08/09):

  * Gaits are loops on the UEFN mannequin, so an impact clip can start at any phase and run
    past the end of the source. The seed carries a dense root2d path, one key per Kimodo frame
    (30 fps), as `smooth_root_2d` only, so the pace is steady and the facing follows the prompt.
  * Speed is capped at 7.0 m/s: above it the planted foot skates. Every seed path stays under
    20 m: past that a single take floats or teleports.
  * The run uses a short seed for a one-cycle loop, the cleanest length in the report. The
    sprint is several cycles long at 6.5 m/s, where multi-cycle loops still pass about half the
    time; several seeds are generated so the cleanest can be kept.
  * Idle holds the same dense path at the origin, so it does not drift.
  * The jump on the spot is the in-place recipe that landed 18 of 20: one prompt, 3.6 s, root
    held at both ends only, no loop.

Usage: python kimodo-impact-sources.py <out.json> [id filter]
"""

import json
import sys

KIMODO_FPS = 30
MAX_SPEED = 7.0           # metres per second
MAX_PATH = 20.0           # metres, the whole generated path

MESH = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"
SKELETON = "/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin"
SKELETON_MAP = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin_KimodoMap"
DESTINATION = "/Game/Kimodo/ImpactSources"


def dense_root2d(frames, speed):
    """Forward path in the soma basis (X = left, Z = forward), metres, a key on every frame."""
    return [{
        "type": "root2d",
        "frame_indices": list(range(frames)),
        "smooth_root_2d": [[0.0, round(speed * i / KIMODO_FPS, 6)] for i in range(frames)],
    }]


def held_ends_root2d(frames):
    """Root held at the origin on the first and last frame only."""
    return [{
        "type": "root2d",
        "frame_indices": [0, frames - 1],
        "smooth_root_2d": [[0.0, 0.0], [0.0, 0.0]],
    }]


def entry(clip_id, name, prompt, seconds, seed, constraints, loop, anchor=20):
    return {
        "spec": {
            "id": clip_id,
            "output_name": name,
            "prompt": prompt,
            "duration_seconds": seconds,
            "seed": seed,
            "sample_count": 1,
            "constraints_json": json.dumps(constraints),
            "target_skeleton": SKELETON,
            "target_mesh": MESH,
            "forward_retargeter": "",
            "reverse_retargeter": "",
            "skeleton_map": SKELETON_MAP,
            "use_skeleton_map_baker": True,
            "destination_package": DESTINATION,
            "generate_loop": loop,
            "loop_anchor_frame": 20 if loop else 0,
            "loop_fixed_velocity": True,
            "lead_foot_suffix": False,
        },
        "status": "Pending",
        "output_object_path": "",
        "error": "",
        "attempts": 0,
    }


def gait(clip_id, name, prompt, speed, seconds, seed, anchor=20):
    if not 0.0 <= speed <= MAX_SPEED:
        sys.exit(f"{clip_id}: speed {speed} m/s is over the {MAX_SPEED} m/s ceiling")
    if speed * seconds >= MAX_PATH:
        sys.exit(f"{clip_id}: path {speed * seconds:.1f} m is not under {MAX_PATH} m")
    frames = int(round(seconds * KIMODO_FPS))
    return entry(clip_id, name, prompt, seconds, seed, dense_root2d(frames, speed), True, anchor)


# The plugin snaps a loop to whole gait cycles of its seed only when it can measure a gait period
# in the seed. A 26-frame run seed measured one (12-frame steps) and snapped to a 24-frame,
# one-cycle loop.
RUN_SEED_SECONDS = 26 / KIMODO_FPS
RUN_SEED = 4102
# The sprint is several cycles long (user, 2026-10-13: a one-cycle loop read as "very short"),
# like Epic's 2.27 s sprint loop. Multi-cycle loops skate more at 7 m/s (1-4 of 10 clean in the
# report against 5 of 10 at 6.5), so the sprint runs at 6.5 m/s and several seeds are generated
# to keep the cleanest. A 2 s seed covers 13 m and snaps to a 66-frame (2.2 s) loop.
# Of seeds 4301-4351 (L1-L6), L4 was kept: 0.1% seam velocity, steadiest pace, best by eye.
SPRINT_SPEED = 6.5
SPRINT_SEED_SECONDS = 2.0
SPRINT_KEPT = {4: 4331}


def main():
    out = sys.argv[1]
    entries = [
        gait("impact_idle", "M_Kimodo_Stand_Idle_Loop",
             "A person is standing idle, breathing and shifting their weight slightly.", 0.0, 4.0, 4101),
        # The Kimodo locomotion set's crouch idle row: its prompt and length.
        gait("impact_crouch_idle", "M_Kimodo_Crouch_Idle_Loop",
             "A person is in a crouch position, slightly swaying up and down while maintaining balance.", 0.0, 8.8, 4107),
        gait("impact_walk_f", "M_Kimodo_Walk_Loop_F", "A person walks forward.", 1.65, 4.07, 4103),
        gait("impact_jog_f", "M_Kimodo_Jog_Loop_F", "A person jogs forward in a straight line.", 3.75, 2.77, 4104),
        gait("impact_crouch_f", "M_Kimodo_Crouch_Walk_Loop_F",
             "A person is crouching while walking forward.", 2.23, 3.82, 4105),
    ]
    entries.append(gait("impact_run_f_s1", "M_Kimodo_Run_Loop_F_S1",
                        "A person is running forward at a steady pace.", 6.0, RUN_SEED_SECONDS, RUN_SEED))
    for index, seed in SPRINT_KEPT.items():
        entries.append(gait(f"impact_sprint_f_l{index}", f"M_Kimodo_Sprint_Loop_F_L{index}",
                            "A person sprints forward.", SPRINT_SPEED, SPRINT_SEED_SECONDS, seed))
    jump_frames = int(round(3.6 * KIMODO_FPS))
    entries.append(entry("impact_jump_in_place", "M_Kimodo_Jump_In_Place",
                         "A person energetically performs a jump and then stands idle.",
                         3.6, 4106, held_ends_root2d(jump_frames), False))

    # Optional filter: only the clip ids that contain this text (to add clips without redoing all).
    if len(sys.argv) > 2:
        entries = [e for e in entries if sys.argv[2] in e["spec"]["id"]]
    manifest = {"version": 1, "batch_id": "impact_sources", "entries": entries}
    with open(out, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent="\t")
    print(f"wrote {out}: {len(entries)} clips")


if __name__ == "__main__":
    main()
