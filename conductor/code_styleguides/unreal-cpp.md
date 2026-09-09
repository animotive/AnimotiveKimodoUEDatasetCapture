# Unreal Engine C++ Style Guide

Conventions for this plugin's C++. Follows Epic's coding standard, with the project-specific
rules that matter for a Take Recorder / MovieScene / Slate plugin called out explicitly.

## Naming

- **Type prefixes** are mandatory and meaningful:
  - `F` — plain structs and non-`UObject` classes (`FInputSample`, `FCaptureSession`)
  - `U` — `UObject` subclasses (`UDatasetCaptureSource`, `UMovieSceneInputTrack`)
  - `A` — `AActor` subclasses
  - `S` — Slate widgets (`SDatasetCapturePanel`)
  - `I` — interfaces (`IDatasetExporter`)
  - `E` — enums (`EClipLabelState`), `T` — templates
  - `b` — boolean variables (`bIsRecording`, `bHasSemanticLayer`)
- `PascalCase` for types, functions, and variables. No Hungarian notation beyond the above.
- Enum values are prefixed by their enum in `enum class` form and never abbreviated to the
  point of ambiguity.
- Name things after what they mean in the domain — a clip, a take, a sample, a session — not
  after their implementation.

## Files and Modules

- Public headers in `Public/`, implementation and private headers in `Private/`.
- One primary type per header; file name matches the type without its prefix
  (`FInputSample` -> `InputSample.h`).
- Every header is self-sufficient: include what you use, forward declare where you can.
  Prefer forward declarations in headers and full includes in `.cpp`.
- `#pragma once` at the top of every header. No include guards.
- **Module boundaries are enforced, not advisory.** The editor module may include capture
  module headers; the capture module must never include an editor header, an editor-only
  module, or `UnrealEd`. If capture code needs something from the editor, the editor pushes it
  in (delegate, interface, registration call).

## Reflection Macros

- `UCLASS`, `USTRUCT`, `UPROPERTY`, `UFUNCTION` only where reflection is actually used:
  serialisation, Blueprint exposure, details panels, GC tracking of `UObject` pointers.
- Any raw `UObject*` member held by a `UObject` needs `UPROPERTY()` or the GC will collect it.
- `GENERATED_BODY()` first line in the type body.
- Do not add reflection to hot per-frame structs unless they are serialised through it —
  reflection is not free.

## Memory and Ownership

- No raw `new` / `delete`. Use `NewObject<T>()` for `UObject`s, `MakeShared<T>()` /
  `MakeUnique<T>()` otherwise.
- `TSharedRef<T>` when the reference can never be null; `TSharedPtr<T>` only when nullness is
  a real state. Slate widgets are constructed into `TSharedRef` and stored as `TSharedPtr`
  only if optional.
- `TWeakObjectPtr<T>` for any `UObject` reference held across frames by non-`UObject` code
  (Slate widgets, capture sources holding the recorded pawn) — PIE ends and actors die.
- `TWeakPtr<T>` to break Slate parent/child cycles.

## Containers and Strings

- `TArray`, `TMap`, `TSet`, `TOptional`, `TVariant` over STL equivalents.
- `Reserve()` before a loop with a known count. The per-frame capture path must not allocate.
- `TEXT()` around every string literal.
- `FString` for mutable text, `FName` for identifiers and lookup keys (bone names, action
  names, tags), `FText` for anything a user reads — never `FString` in the UI.
- `LOCTEXT` / `NSLOCTEXT` for all user-facing text, with `#define LOCTEXT_NAMESPACE` scoped per
  file.

## Error Handling

- No exceptions. Fallible functions return `bool`, `TOptional<T>`, or a small result struct
  carrying a reason.
- `check()` for invariants that indicate a programming error; `ensure()` where recovery is
  possible and worth logging; never a silent early return.
- A failed capture is a **reported** failure, not a truncated file — see the data-integrity
  rule in `product-guidelines.md`.
- `UE_LOG` under this plugin's own log category, with verbosity chosen deliberately:
  `Error` for operator-visible failure, `Warning` for degraded capture, `Verbose` for
  per-frame detail. Never `Log` spam in a per-frame path.

## Slate

- Widget construction uses the `SLATE_BEGIN_ARGS` / `Construct` pattern; no logic in
  constructors.
- **Keep logic out of widgets.** A widget queries and displays; decisions live in a
  non-Slate type that can be unit tested without an editor window. This is what makes the
  panel testable at all.
- Bind to data with attributes and delegates (`TAttribute`, `FOnClicked`) rather than caching
  copies of state in the widget.
- Style comes from `FAppStyle` — no hardcoded colours, no custom brushes, no bespoke fonts.
- Anything that can fail must render its failure state; never leave a widget blank on error.

## MovieScene and Take Recorder

- Input data is written as channels on a MovieScene section — never as a side buffer keyed by
  wall-clock time. The sequence's `FFrameRate` / tick resolution is the single time authority.
- Convert to `FFrameNumber` / `FFrameTime` once, at the boundary, and keep frame-space
  arithmetic in frame space. No `float` seconds in the middle of the pipeline.
- Take Recorder owns record lifecycle; the capture source reacts to its callbacks and never
  starts or stops recording on its own.

## Documentation

- Every public type and function gets a comment saying what it is for and what the caller must
  guarantee — not a restatement of the signature.
- Comment the non-obvious and the load-bearing: why frame alignment is structural, why a
  module boundary points one way, why a buffer is pre-reserved.
- `// TODO(name):` with a name attached, or don't write it.

## Testing

- Automation tests live in the tests module and exercise plain logic: sampling, frame mapping,
  JSON round-trip, trim math.
- Test at boundaries — first frame, last frame, zero-length clip, trim points on frame edges.
- If something can only be tested with an editor window, that is a signal the logic needs
  extracting from the widget.

**BE CONSISTENT.** When editing existing code, match the surrounding style over this document.

_References: [Epic C++ Coding Standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)_
