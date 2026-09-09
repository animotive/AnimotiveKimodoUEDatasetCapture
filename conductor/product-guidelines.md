# Dataset Capture Plugin — Product Guidelines

## Visual Identity

**Engine-native, always.** The panel is built from standard Slate widgets and `FAppStyle`
brushes, uses engine icons, and docks like any other editor tool. It should be
indistinguishable from Take Recorder in look and behaviour.

- No custom palettes, no logo chrome, no bespoke fonts in the panel.
- Inherit editor theming — the panel must read correctly in both light and dark editor themes
  without per-theme code.
- Styling that fights an engine upgrade is a bug, not a feature.

## Capture Feedback

**The operator's eyes are on the game, not the panel.** State the operator needs mid-take is
drawn where they are looking.

- A recording indicator with a **remaining-time countdown** is drawn in the PIE viewport for
  the duration of every capture.
- The panel mirrors the same state (armed / recording / elapsed / stopped).
- Auto-stop is never a surprise: the countdown is the contract that makes a fixed max
  duration safe.
- The overlay is capture-state only. It never carries menus, labels, or settings.

## Labelling

**Controlled tags plus a free-text caption.** Two layers, each doing a job the other can't.

- **Tags** come from a project-defined vocabulary (e.g. `locomotion`, `stairs-up`, `jump`,
  `interact`). They keep a dataset queryable and consistent across operators and studios.
- **Caption** is natural language, written by the operator, and carries the nuance the model
  trains on: "walks up a small staircase carrying a box".
- The vocabulary is **data, not code** — editable per project, extendable by the client
  without a plugin rebuild.
- Never block a save on tag choice alone; never accept a clip with no caption.

## Data Integrity

**Fail loud. Never write a half-clip.**

- If the input stream cannot be aligned frame-for-frame with the recorded animation, or the
  target pawn disappears, or Take Recorder reports an error, the clip is marked **invalid** in
  the session list with the reason in plain language, and the operator is prompted to retake.
- A retake costs the operator ten seconds. Degraded data costs the ML team a fine-tune run
  and is rarely traced back to its source.
- No silent partial writes, no quality flags used as a substitute for correctness.
- Invalid clips are never included in an export bundle.

## Voice

**Plain and operator-first.** The reader is a client animator who does not know the plugin's
internals.

- Short sentences. Name the thing, name the action.
- Errors state what happened and what to do next: "No player pawn found. Start play, then arm
  capture."
- Avoid engine-internal vocabulary in operator-facing strings (`FGuid`, spawnable, binding
  proxy) unless the operator must act on it.
- No padding, no encouragement, no exclamation marks — this text is read on take forty.
- Engineer-facing diagnostics in the Output Log may be as technical as needed; that's a
  different audience.

## Coding Conventions

Follow Unreal Engine coding standards throughout:

- Class prefixes: `F` for structs/plain classes, `U` for `UObject`, `A` for `AActor`,
  `S` for Slate widgets, `I` for interfaces.
- `TArray`, `TMap`, `TOptional` over STL equivalents.
- `UCLASS`, `UPROPERTY`, `UFUNCTION` macros only where UE reflection is actually needed.
- `TEXT()` macro for all string literals; `LOCTEXT` / `NSLOCTEXT` for anything user-facing.
- `UE_LOG` for diagnostics under a dedicated log category, with appropriate verbosity.
- `check()` / `ensure()` for internal invariants; never silent failures.
- Headers in `Public/`, implementation in `Private/`.
- No raw `new` / `delete` — UE smart pointers (`TSharedPtr`, `TUniquePtr`) or `UObject` GC.

## API Design

- Runtime capture code and editor UI live in separate modules; the editor module may depend
  on the runtime module, **never the reverse**.
- No required changes to a client's character controller. The plugin reads the game through
  public engine surfaces (Enhanced Input, `CharacterMovementComponent`) and degrades to the
  required capture layer when the opt-in layer isn't available.
- Functions that can fail return `bool` or `TOptional` — never throw.
- The per-frame capture path must not allocate on the heap. A 10-second capture at 60fps is
  600 samples; it should cost nothing observable in frame time.
- The sidecar JSON schema is versioned from the first commit. A reader must be able to tell
  which version produced a file.

## Testing

- New functionality requires tests in the plugin's own test module, runnable headless.
- Frame alignment between the input stream and the animation track is the plugin's core
  correctness claim and must be covered by tests, not by eyeballing.
- Sidecar JSON serialisation round-trips in tests: write, read, compare.
- Trim math (in/out points mapped to frames and to exported ranges) must be unit tested at
  boundary values.
- Slate panel behaviour is verified by user-driven editor testing; the logic behind it is
  factored out so it can be tested without an editor window.
