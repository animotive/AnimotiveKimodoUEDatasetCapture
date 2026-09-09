# Initial Concept

I want to create a new unreal plugin for capturing game motion datasets we can use to fine tune the kimodo model, it will be running on Unreal 5.7. I want it to be a new slate panel that acts as an extension of the take recorder, it should capture the in scene animation data being triggered by a player at runtime along with the button inputs pressed, we should probably capture the raw inputs from the controller, but also maybe the interactions of the character controller if it doesnt take too much customisation of the character controller. The plugin should be designed so we can quickly take short clips of player interaction, around 10 seconds or so, and when we hit stop recording, we should get a preview of what was captured and the ability to label it. We need to track things like the directional input, if the press jump, if they press interact etc. This would let us be able to build out more custom animation clips driven by motion matching datasets in the engine so we could do things like "A person walks up a small staircase, a person walks up a large staircase" but with the game animations as the driver for it

# Product Guide

## Vision

An Unreal Engine 5.7 editor plugin that turns ordinary gameplay into training data. A client
plays their own game, records short clips of player interaction, labels them in natural
language, and hands Animotive a dataset whose motion is *their* game's animation — not
generic mocap. Those labelled clips fine-tune the Kimodo model and feed motion-matching
datasets in-engine, so a prompt like "a person walks up a small staircase" resolves to
animation that already looks like the client's game.

## Users

| Who | Role | What they need |
|-----|------|----------------|
| **Client teams** (game/animation devs, capture operators) | Produce the data | Record a 10-second clip without leaving the pad, see what was captured, label it, move on. No C++, no pipeline knowledge, no per-project engineering. |
| **Animotive ML + animation teams** | Consume the data | Bespoke, well-labelled datasets: motion aligned frame-for-frame with the inputs that caused it, in a format a training pipeline can ingest without hand-repair. |

The plugin's operators and its beneficiaries are different people in different companies.
Every design decision follows from that: the capture surface is optimised for a client who
has never seen it before, and the output format is optimised for a training pipeline that
will never see the client's project.

## Goals

1. **Capture is fast enough to do fifty times in a session.** Arm, record, label, repeat —
   a clip should cost seconds of the operator's attention, not minutes.
2. **Motion and intent stay aligned.** Every animation frame carries the controller state
   that produced it. A dataset where inputs drift out of sync with pose is worthless.
3. **Zero integration tax.** The plugin reads the game; it does not ask the game to change.
   No required edits to the client's character controller, no mandatory base classes.
4. **Labels are the product.** Natural-language captions are what make a clip trainable;
   labelling is a first-class step in the UI, not an afterthought spreadsheet.
5. **Take Recorder is the host, not a rival.** The panel extends the Take Recorder workflow
   clients already know, and lets Unreal own the animation recording it is good at.

## Core Features

### 1. Capture panel (Slate)

A dedicated editor panel that acts as an extension of Take Recorder: arm a capture, pick the
target player pawn, set the max clip duration (default ~10 seconds), watch record state live.

### 2. Recording triggers

- **Editor hotkey + panel button** — toggle recording during PIE; auto-stop when the
  configured max duration is reached.
- **Gamepad trigger** — bindable controller button/chord, so an operator mid-session never
  puts the pad down to reach the keyboard.

### 3. What gets captured

**Required:**
- Per-frame skeletal animation of the player character, recorded through Take Recorder.
- Raw controller state — stick axes, button down/up transitions — timestamped to the same
  frames as the animation.

**Opt-in, best-effort** (included only where it costs no character-controller surgery):
- Mapped Enhanced Input actions (Jump, Interact, Sprint, directional input) as semantic events.
- `CharacterMovementComponent` state: velocity, grounded/falling, crouch.

If a client's project can't supply the opt-in layer without modification, capture still
succeeds with the required layer alone.

### 4. Review, trim and label

On stop, the panel presents the take for review:
- **Scrub playback** of the clip just captured.
- **Input timeline overlay** — stick and button events drawn under the scrub bar, aligned to
  frames, so the operator can see what the clip actually contains.
- **Trim handles** — drag in/out points to cut the slop off both ends of a 10-second grab.
- **Caption + tags** — free-text natural-language label ("a person walks up a small
  staircase") plus reusable tags.

### 5. Session triage

The panel keeps the session's clips in a list with label status, so an operator can:
- discard a bad take and immediately retake it,
- see at a glance which clips are still unlabelled,
- mark clips ready for export.

### 6. Storage and export

- **In-project storage (canonical).** Take Recorder writes its native `LevelSequence` /
  `AnimSequence` as usual. The plugin drops a sidecar JSON per take holding the input stream,
  character state, caption, tags, trim points, frame rate and skeleton identity. Nothing is
  duplicated, nothing drifts, and every clip stays replayable in-engine.
- **Export bundle (explicit UI action).** A separate export action bakes selected clips to a
  portable interchange bundle — motion (BVH/FBX) plus a JSON manifest — consumable by a
  training pipeline with no Unreal project attached.

## Non-Goals

- **Scene and environment context capture.** World-space geometry the character touched
  (stair heights, ledge depths) is not captured; labels and motion carry that meaning for now.
- **Direct upload to the Kimodo backend.** Export produces files; shipping them is out of
  band until backend ingest endpoints exist.
- **Shipping runtime footprint.** This is a capture tool for development builds, not a
  feature that goes out in a client's shipped game.
- **Replacing Take Recorder.** Animation recording stays Unreal's job.

## Success Criteria

- An operator new to the plugin records, trims, labels and marks a clip ready inside a minute.
- A captured clip's input stream and animation frames line up exactly, verifiable by replay.
- A client project with an unmodified character controller yields usable required-layer data.
- An exported bundle ingests into an Animotive training pipeline with no manual repair.
