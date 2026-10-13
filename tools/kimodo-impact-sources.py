"""Writes the KimodoBatch manifest that generates the impact pipeline's source motion.

The impact batches draw their source animations from Kimodo rather than from Epic's
GameAnimationSample clips. Run the manifest with the Kimodo plugin's batch commandlet:

    UnrealEditor-Cmd <Project>.uproject -run=KimodoBatch -manifest=<out.json> -unattended -nullrhi

Both clips are generated as loops on the UEFN mannequin, so an impact clip can start at any
phase and run past the end of the source.

  * Run: a dense root2d path, one key per Kimodo frame (30 fps), as `smooth_root_2d` only, so
    the pace is steady and the facing is left to the prompt. Speed is capped at 7 m/s and the
    whole path stays under 20 m.
  * Idle: the same dense root2d path held at the origin, so the idle does not drift.

Usage: python kimodo-impact-sources.py <out.json> [run speed m/s]
"""

import json
import sys

KIMODO_FPS = 30
MAX_RUN_SPEED = 7.0       # metres per second
MAX_RUN_DISTANCE = 20.0   # metres, the whole generated path

MESH = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"
SKELETON = "/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin"
SKELETON_MAP = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin_KimodoMap"
DESTINATION = "/Game/Kimodo/ImpactSources"


def root2d(frames, speed):
    """Ground-plane path in the soma basis (X = left, Z = forward), metres, one key per frame."""
    return [{
        "type": "root2d",
        "frame_indices": list(range(frames)),
        "smooth_root_2d": [[0.0, round(speed * i / KIMODO_FPS, 6)] for i in range(frames)],
    }]


def entry(clip_id, name, prompt, seconds, seed, speed):
    frames = int(round(seconds * KIMODO_FPS))
    return {
        "spec": {
            "id": clip_id,
            "output_name": name,
            "prompt": prompt,
            "duration_seconds": seconds,
            "seed": seed,
            "sample_count": 1,
            "constraints_json": json.dumps(root2d(frames, speed)),
            "target_skeleton": SKELETON,
            "target_mesh": MESH,
            "forward_retargeter": "",
            "reverse_retargeter": "",
            "skeleton_map": SKELETON_MAP,
            "use_skeleton_map_baker": True,
            "destination_package": DESTINATION,
            "generate_loop": True,
            "loop_anchor_frame": 20,
            "loop_fixed_velocity": True,
            "lead_foot_suffix": False,
        },
        "status": "Pending",
        "output_object_path": "",
        "error": "",
        "attempts": 0,
    }


def main():
    out = sys.argv[1]
    run_speed = float(sys.argv[2]) if len(sys.argv) > 2 else 6.0
    if not 0.0 < run_speed <= MAX_RUN_SPEED:
        sys.exit(f"run speed must be above 0 and at most {MAX_RUN_SPEED} m/s")
    # As long as the distance cap allows, to give the loop several strides.
    run_seconds = min(3.0, round(MAX_RUN_DISTANCE * 0.98 / run_speed, 2))

    manifest = {
        "version": 1,
        "batch_id": "impact_sources",
        "entries": [
            entry("impact_idle", "M_Kimodo_Stand_Idle_Loop",
                  "A person is standing idle, breathing and shifting their weight slightly.",
                  4.0, 4101, 0.0),
            entry("impact_run_f", "M_Kimodo_Run_Loop_F",
                  "A person is running forward at a steady pace.",
                  run_seconds, 4102, run_speed),
        ],
    }
    with open(out, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent="\t")
    print(f"wrote {out}: run {run_speed} m/s for {run_seconds} s = {run_speed * run_seconds:.2f} m")


if __name__ == "__main__":
    main()
