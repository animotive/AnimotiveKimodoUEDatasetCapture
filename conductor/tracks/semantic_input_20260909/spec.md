# Spec (STUB) — Semantic Input and Character State

**Track ID:** semantic_input_20260909
**Status:** stub — expand with /conductor:new-track before implementing
**Depends on:** capture_slice_20260909

## Purpose

Raw stick and button data says what the pad did. Semantic data says what the game thought the
player meant. Captions like a person presses interact at the top of the stairs are far cheaper
to derive when the action names are already in the clip.

## Scope Sketch

- Enhanced Input layer: read action instance data from UEnhancedPlayerInput and write named
  action channels alongside the raw channels, on the same frames.
- Character state layer: velocity, movement mode (grounded, falling, swimming), crouch, read
  from UCharacterMovementComponent.
- Both layers strictly opt-in and best-effort. Capture must still succeed with raw data alone.
- Graceful detection and reporting of what a project supports, surfaced in the panel before
  arming rather than discovered after a take.
- Zero required modification to the client character controller, pawn, or input component.

## Open Questions

- Discover mapped actions from the active mapping contexts, or require the operator to pick the
  actions of interest?
- Channel per action, or one enum channel of action events?
- Is movement mode enough, or does custom movement in client projects need a generic hook?

## Acceptance Sketch

- A project with no Enhanced Input captures cleanly with raw data only, and the panel says so.
- Action channel keys land on the same frames as the raw input that triggered them.
- No plugin code names a client-specific action or class.
