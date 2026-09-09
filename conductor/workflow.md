# Project Workflow

## Guiding Principles

1. **The Plan is the Source of Truth:** All work must be tracked in `plan.md`
2. **The Tech Stack is Deliberate:** Changes to the tech stack must be documented in `tech-stack.md` _before_ implementation
3. **Test-Driven Development:** Write automation tests before implementing functionality
4. **Test Coverage Policy:** See **Test Coverage Policy** below — UE C++ has no built-in coverage tool, so the gate is stated in terms of what must be tested, not an unmeasurable percentage
5. **Operator Experience First:** The capture operator is a client animator who has never seen this tool. Every decision prioritises their session going smoothly
6. **Non-Interactive & CI-Aware:** Prefer non-interactive commands. Editor runs use `-Unattended -NullRHI -nopause` so they terminate without a human

## Test Coverage Policy

This project replaces a percentage gate with an obligation, because a percentage nobody can
measure is a gate nobody enforces.

**Required — a task is not complete without automation tests covering:**
- All pure logic: input sampling, frame mapping, trim math, JSON serialisation, tag/label
  state, export manifest construction
- Boundary values: first frame, last frame, zero-length clip, trim points landing exactly on
  frame edges, missing optional layers (no Enhanced Input, no `ACharacter`)
- Every bug fix — a failing test that reproduces the bug comes before the fix

**Exempt — verified by user-driven editor testing instead, and stated as such in the task:**
- Slate widget rendering and layout
- PIE viewport overlay drawing
- Take Recorder end-to-end record lifecycle in a live editor session

**Structural rule that makes this work:** if logic can only be tested with an editor window
open, extract it from the widget until it can't. A widget that is hard to test is a widget
holding logic that does not belong to it.

**Optional measurement:** where a numeric figure is wanted, run OpenCppCoverage over the
headless automation pass. It is a diagnostic, not a merge gate.

## Task Workflow

All tasks follow a strict lifecycle:

### Standard Task Workflow

1. **Select Task:** Choose the next available task from `plan.md` in sequential order

2. **Mark In Progress:** Before beginning work, edit `plan.md` and change the task from `[ ]` to `[~]`

3. **Write Failing Tests (Red Phase):**
   - Create or extend a test file in `KimodoDatasetCaptureTests`.
   - Write automation tests that define the expected behaviour and acceptance criteria for the task.
   - **CRITICAL:** Build and run the tests, and confirm that they fail as expected. This is the "Red" phase of TDD. Do not proceed until you have failing tests.
   - If the task is in the exempt list above, state that explicitly in the task notes and define the manual verification steps instead.

4. **Implement to Pass Tests (Green Phase):**
   - Write the minimum amount of code necessary to make the failing tests pass.
   - Rebuild and rerun the test suite; confirm all tests now pass. This is the "Green" phase.

5. **Refactor (Optional but Recommended):**
   - With passing tests as a safety net, improve clarity, remove duplication, and tighten the hot path without changing behaviour.
   - Rerun tests to confirm they still pass.

6. **Verify Coverage:** Confirm the Test Coverage Policy above is satisfied for the code this task touched — every piece of pure logic added has a test, boundaries included. Optionally:

   ```powershell
   # Diagnostic only, not a gate
   OpenCppCoverage.exe --sources AnimotiveKimodoDatasetCapture -- `
     "D:/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" `
     "D:/Unreal Projects/Kimodo-Workshop/Kimodo.uproject" `
     -ExecCmds="Automation RunTests KimodoDatasetCapture;Quit" -Unattended -NullRHI -nopause
   ```

7. **Document Deviations:** If implementation differs from the tech stack:
   - **STOP** implementation
   - Update `tech-stack.md` with the new design
   - Add a dated note explaining the change
   - Resume implementation

8. **Commit Code Changes:**
   - Stage all code changes related to the task.
   - Propose a clear, concise commit message e.g. `feat(capture): Add UTakeRecorderSource registration`.
   - Perform the commit. **One commit per task.**

9. **Attach Task Summary with Git Notes:**
   - **Step 9.1: Get Commit Hash:** Obtain the hash of the _just-completed commit_ (`git log -1 --format="%H"`).
   - **Step 9.2: Draft Note Content:** Create a detailed summary for the completed task: task name, summary of changes, list of created/modified files, and the core "why".
   - **Step 9.3: Attach Note:**
     ```bash
     git notes add -m "<note content>" <commit_hash>
     ```

10. **Get and Record Task Commit SHA:**
    - **Step 10.1: Update Plan:** Read `plan.md`, find the line for the completed task, update its status from `[~]` to `[x]`, and append the first 7 characters of the just-completed commit hash.
    - **Step 10.2: Write Plan:** Write the updated content back to `plan.md`.

11. **Commit Plan Update:**
    - Stage the modified `plan.md`.
    - Commit with a descriptive message (e.g. `conductor(plan): Mark task 'Register take source' as complete`).

### Phase Completion Verification and Checkpointing Protocol

**Trigger:** Executed immediately after a task is completed that also concludes a phase in `plan.md`.

1.  **Announce Protocol Start:** Inform the user that the phase is complete and the verification and checkpointing protocol has begun.

2.  **Ensure Test Coverage for Phase Changes:**
    - **Step 2.1: Determine Phase Scope:** Read `plan.md` to find the Git commit SHA of the _previous_ phase's checkpoint. If no previous checkpoint exists, the scope is all changes since the first commit.
    - **Step 2.2: List Changed Files:** Execute `git diff --name-only <previous_checkpoint_sha> HEAD`.
    - **Step 2.3: Verify and Create Tests:** For each file in the list:
      - **CRITICAL:** Exclude non-code files (`.md`, `.json`, `.uplugin`, `.ini`) and files in the exempt list of the Test Coverage Policy.
      - For each remaining code file, verify corresponding automation tests exist covering the logic added in this phase.
      - If tests are missing, create them, matching the naming and style of existing tests in `KimodoDatasetCaptureTests`.

3.  **Execute Automated Tests with Proactive Debugging:**
    - Announce the exact command before running it.
    - **Example Announcement:** "I will now run the automated test suite to verify the phase. **Command:** `UnrealEditor-Cmd.exe Kimodo.uproject -ExecCmds="Automation RunTests KimodoDatasetCapture;Quit" -Unattended -NullRHI -nopause`"
    - Execute it.
    - If tests fail, inform the user and debug. Propose a fix a **maximum of two times**. If tests still fail, **stop**, report the persistent failure, and ask for guidance.
    - If the NullRHI editor boot deadlocks (a known hazard in this project's sibling plugin), report it and fall back to user-driven editor verification rather than retrying blindly.

4.  **Propose a Detailed, Actionable Manual Verification Plan:**
    - **CRITICAL:** Analyse `product.md`, `product-guidelines.md`, and `plan.md` to determine the operator-facing goals of the completed phase.
    - Generate step-by-step verification instructions with exact commands and specific expected outcomes.
    - Format:

      ```
      The automated tests have passed. For manual verification, please follow these steps:

      **Manual Verification Steps:**
      1.  **Open** `Kimodo.uproject` in Unreal Editor 5.7.
      2.  **Open the panel:** Window -> Tools -> Kimodo Dataset Capture.
      3.  **Press Play (PIE)** and arm a capture from the panel.
      4.  **Confirm that you see:** a recording indicator with a counting-down timer in the play viewport, and the panel showing "Recording".
      ```

5.  **Await Explicit User Feedback:**
    - Ask: "**Does this meet your expectations? Please confirm with yes or provide feedback on what needs to be changed.**"
    - **PAUSE** and await an explicit yes before proceeding.

6.  **Create Checkpoint Commit:**
    - Stage all changes. If no changes occurred, make an empty commit.
    - Commit with a clear message (e.g. `conductor(checkpoint): Checkpoint end of Phase X`).

7.  **Attach Auditable Verification Report using Git Notes:**
    - Draft a verification report: the automated test command and result, the manual verification steps, and the user's confirmation.
    - Attach it to the checkpoint commit with `git notes add`.

8.  **Get and Record Phase Checkpoint SHA:**
    - Obtain the checkpoint commit hash (`git log -1 --format="%H"`).
    - Append the first 7 characters to the phase heading in `plan.md` as `[checkpoint: <sha>]`.
    - Write `plan.md` back.

9.  **Commit Plan Update:**
    - Stage `plan.md` and commit as `conductor(plan): Mark phase '<PHASE NAME>' as complete`.

10. **Announce Completion:** Inform the user the phase is complete and the checkpoint created, with the verification report attached as a git note.

### Quality Gates

Before marking any task complete, verify:

- [ ] All automation tests pass
- [ ] Test Coverage Policy satisfied for the code this task touched
- [ ] Code follows `code_styleguides/general.md` and `code_styleguides/unreal-cpp.md`
- [ ] Module boundary respected — capture module includes nothing editor-only
- [ ] All public types and functions documented with intent, not restatement
- [ ] Per-frame capture path allocates nothing on the heap
- [ ] All user-facing text uses `LOCTEXT` / `FText`, and follows the voice rules in `product-guidelines.md`
- [ ] No new compiler warnings
- [ ] Sidecar JSON schema version bumped if the schema changed
- [ ] Documentation updated if needed

## Development Commands

### Setup

```bash
# Generate project files after adding or renaming a module
"D:/UE_5.7/Engine/Build/BatchFiles/Build.bat" -projectfiles \
  -project="D:/Unreal Projects/Kimodo-Workshop/Kimodo.uproject" -game -engine
```

### Daily Development

```bash
# Build the editor target
"D:/UE_5.7/Engine/Build/BatchFiles/Build.bat" KimodoEditor Win64 Development \
  "D:/Unreal Projects/Kimodo-Workshop/Kimodo.uproject"

# Run this plugin's automation tests headless
"D:/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "D:/Unreal Projects/Kimodo-Workshop/Kimodo.uproject" \
  -ExecCmds="Automation RunTests KimodoDatasetCapture;Quit" \
  -Unattended -NullRHI -nopause
```

### Before Committing

```bash
# Clean build of the editor target, then the full headless test pass.
# Both must succeed with no new warnings.
```

## Testing Requirements

### Unit Testing

- Every module with logic has corresponding automation tests.
- Mock or stub engine dependencies where possible; prefer pure functions that need no world.
- Test both success and failure cases — especially degraded capture (missing Enhanced Input,
  missing `ACharacter`, pawn destroyed mid-take).

### Integration Testing

- Full record lifecycle: arm -> record -> auto-stop -> preview -> trim -> label -> mark ready.
- Sidecar JSON written next to the take, readable, schema version present.
- Export bundle produced from a labelled clip and re-openable.
- Invalid clips excluded from export.

### Editor UX Verification

- Verified by hand in the editor, since these paths are exempt from automation:
  - Panel docks, resizes, and reads correctly in both light and dark editor themes.
  - PIE overlay shows recording state and a counting-down timer.
  - Gamepad trigger toggles recording without touching the keyboard.
  - Error states are legible to an operator who does not read the Output Log.

## Code Review Process

### Self-Review Checklist

Before requesting review:

1. **Functionality**
   - Feature works as specified
   - Edge cases handled (mid-take pause, PIE exit during record, pawn respawn)
   - Error messages are operator-friendly and state the next action

2. **Code Quality**
   - Follows the style guides
   - No duplication of engine functionality that Take Recorder already provides
   - Clear names drawn from the domain
   - Comments explain the load-bearing decisions

3. **Testing**
   - Tests cover the logic added, including boundaries
   - Exempt paths named, with manual verification steps written down

4. **Data Integrity**
   - No path can write a partial or misaligned clip
   - Failures surface in the panel, not only in the log
   - Schema version handled

5. **Performance**
   - No heap allocation per frame; buffers reserved at record start
   - No measurable PIE frame-time cost

6. **Operator Experience**
   - Capture state always visible where the operator is looking
   - No modal dialog unless input is genuinely required
   - Engine-native styling, no hardcoded colours

## Commit Guidelines

### Message Format

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

### Types

- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation only
- `style`: Formatting
- `refactor`: Neither fixes a bug nor adds a feature
- `test`: Adding missing tests
- `chore`: Maintenance tasks
- `conductor`: Conductor plan/state bookkeeping

### Examples

```bash
git commit -m "feat(capture): Sample raw gamepad state into MovieScene channels"
git commit -m "fix(trim): Clamp out-point to the last recorded frame"
git commit -m "test(sidecar): Cover JSON round-trip at schema v1"
git commit -m "feat(panel): Add session clip list with label status"
```

## Definition of Done

A task is complete when:

1. All code implemented to specification
2. Automation tests written and passing
3. Test Coverage Policy satisfied
4. Documentation complete (if applicable)
5. Builds clean with no new warnings
6. Editor UX verified by hand where the path is exempt from automation
7. Implementation notes added to `plan.md`
8. Changes committed with a proper message
9. Git note with the task summary attached to the commit

## Emergency Procedures

### Corrupt or Misaligned Dataset Discovered

1. Stop capture sessions using the affected build
2. Write a failing test reproducing the misalignment
3. Fix, and bump the sidecar schema version if the format is implicated
4. Identify which captured clips are affected and mark them invalid
5. Document in `plan.md` and in a git note

### Engine Upgrade Breaks the Plugin

1. Capture the exact build errors before changing anything
2. Fix against the new API, documenting each API change in `tech-stack.md` with a date
3. Rerun the full headless test pass
4. Re-verify the exempt editor UX paths by hand

## Continuous Improvement

- Review workflow when it gets in the way
- Update based on real pain points, not hypotheticals
- Document lessons learned in git notes so they stay attached to the code
- Keep the operator's session fast and the dataset trustworthy
