---
name: geany-progress
description: "Report AI agent plan progress to the Geany sidebar via the geanyprogress plugin socket API"
argument-hint: "[init | done N | status | check]"
allowed-tools:
  - Bash
---

# geany-progress skill

Report plan progress to the **geanyprogress** Geany plugin. The plugin renders a live
progress panel in the Geany sidebar and persists state to `.planning/state/xxxx_PROGRESS.md`.

## File naming conventions

| Artifact | Path |
|----------|------|
| Plan     | `.planning/plans/MY_PLAN_PLAN.md`     |
| Progress | `.planning/state/MY_PLAN_PROGRESS.md` |

The slug prefix is UPPERCASE_SNAKE_CASE derived from the plan name
(e.g. plan name `"My Task"` → slug `MY_TASK` → files `MY_TASK_PLAN.md` / `MY_TASK_PROGRESS.md`).

## How the API works

The plugin listens on a Unix domain socket. Its path is exported as `GEANY_PROGRESS_SOCK`
in the Geany process environment — every terminal opened inside Geany (geanycli,
geanyagent, VTE) inherits this variable automatically.

Three JSON message types:

```
# Register (or replace) a plan — plan_file is optional but enables "Open plan"
{"plan":"Plan Name","plan_file":".planning/plans/PLAN_NAME_PLAN.md","phases":["Phase 1","Phase 2","Phase 3"]}

# Mark a phase complete (1-based index), with optional review files and warnings
{"phase":1,"status":"complete",
 "files":[{"path":"src/foo.c","line":42},{"path":"src/bar.h"}],
 "warnings":["Possible memory leak on error path","Check timeout handling"]}

# Query current plan state — plugin responds with JSON on the same connection
{"query":"state"}
```

Query response format:
```json
{
  "plan": "Plan Name",
  "plan_file": ".planning/plans/PLAN_NAME_PLAN.md",
  "progress_file": "/abs/path/.planning/state/PLAN_NAME_PROGRESS.md",
  "phases": [
    {"index": 1, "name": "Phase 1", "done": true},
    {"index": 2, "name": "Phase 2", "done": false}
  ],
  "next_phase": 2
}
```

`next_phase` is the 1-based index of the first pending phase, `0` if all phases are
done, or `null` if no plan is active.

`files` and `warnings` are optional. When present:
- **files** — clicking the completed phase row in the sidebar opens each file at the given line
- **warnings** — shown as a tooltip when hovering over the phase row

Messages are sent by writing to the socket with `nc -U` or `socat`.

## Helper script

`geany-progress` (on `$PATH` when the plugin directory is in `PATH`, or at
`geany-plugins/geanyprogress/geany-progress`) wraps the raw socket calls:

```sh
geany-progress init [-f plan_file] "Plan Name" "Phase 1" "Phase 2" "Phase 3"
geany-progress done N [-r file[:line]]... [-w "warning"]...
geany-progress query
geany-progress status
```

Flags for `done`:
- `-r path` or `-r path:line` — mark a file (and optional line) for review; repeat for multiple files
- `-w "message"` — add a warning shown on hover; repeat for multiple warnings

## Sidebar interactions

| Interaction | Action |
|-------------|--------|
| Left-click title row | Open `_PLAN.md` in Geany editor |
| Left-click completed phase | Open all review files at their line numbers |
| Hover over any phase | Show warnings tooltip (⚠ messages + file list) |
| Right-click → Mark finished | Mark the clicked phase complete |
| Right-click → Open review files | Open review files for the right-clicked phase |
| Right-click → Open progress | Open `_PROGRESS.md` in editor |
| Right-click → Open plan | Open `_PLAN.md` in editor |
| Right-click → Load... | File chooser: load any `_PROGRESS.md` into the GUI |

Done phases show a green ✓; pending phases show ○.
Phases with warnings show them as a tooltip on hover.

## Usage patterns

### Check whether the plugin is available

```sh
if [ -n "$GEANY_PROGRESS_SOCK" ] && [ -S "$GEANY_PROGRESS_SOCK" ]; then
    echo "geanyprogress available"
fi
```

### Register a plan at the start of a task

```sh
# With plan file reference (recommended — enables "Open plan" in sidebar)
geany-progress init -f ".planning/plans/MY_TASK_PLAN.md" \
    "My Task" "Research" "Implement" "Test" "Ship"

# Without plan file (sidebar title click does nothing)
geany-progress init "My Task" "Research" "Implement" "Test" "Ship"
```

Or without the helper (plain shell):

```sh
printf '{"plan":"My Task","plan_file":".planning/plans/MY_TASK_PLAN.md","phases":["Research","Implement","Test","Ship"]}' \
    | nc -U "$GEANY_PROGRESS_SOCK"

# Mark phase 2 done with review file and warning
printf '{"phase":2,"status":"complete","files":[{"path":"src/engine.c","line":117}],"warnings":["Check thread safety"]}' \
    | nc -U "$GEANY_PROGRESS_SOCK"
```

### Mark a phase done

```sh
geany-progress done 2                         # simple completion
geany-progress done 2 -r src/foo.c:42        # with a review file at a specific line
geany-progress done 2 -r src/foo.c:42 \
    -r src/bar.h \
    -w "Check error handling on line 42" \
    -w "Possible memory leak in cleanup path"
```

**When to add review files and warnings:**

- Add `-r` for files the human should manually inspect — typically non-trivial C/C++ changes, security-sensitive code, or complex logic. Skip generated files, Makefiles, and boilerplate scripts.
- Add `-w` for anything the human should know before approving: potential regressions, assumptions that could be wrong, deferred TODOs, or anything that needed a trade-off decision.
- Keep it focused — 1-3 files and 1-2 warnings is ideal. The point is to surface the *interesting* changes, not enumerate every modified file.

### Wrap a multi-phase agent workflow

```sh
# At the start — pairs progress with the plan document
geany-progress init -f ".planning/plans/CODE_REVIEW_PLAN.md" \
    "Code Review" "Read files" "Analyse" "Write findings"

# ... read the files ...
geany-progress done 1

# ... analyse ...
geany-progress done 2

# ... write output — flag the interesting output file ...
geany-progress done 3 -r findings.md:1
```

## Execution

When this skill is invoked, execute the sub-command passed as `$ARGUMENTS`:

### `check` or no argument

Verify the socket is reachable and report its path:

```sh
if [ -z "$GEANY_PROGRESS_SOCK" ]; then
    echo "GEANY_PROGRESS_SOCK is not set — geanyprogress plugin may not be loaded"
elif [ ! -S "$GEANY_PROGRESS_SOCK" ]; then
    echo "Socket path set to $GEANY_PROGRESS_SOCK but file does not exist"
else
    echo "geanyprogress OK — socket at $GEANY_PROGRESS_SOCK"
    ls -la "$GEANY_PROGRESS_SOCK"
fi
```

### `init [-f plan_file] <plan> [phases...]`

Register a new plan. Extract arguments from `$ARGUMENTS` and call `geany-progress init`.
Always pass `-f` when the plan file is known.

Example: `/geany-progress init -f ".planning/plans/DEPLOY_PLAN.md" "Deploy" "Build" "Test" "Push"`

### `done N [-r file[:line]]... [-w "warning"]...`

Mark phase N complete, optionally with review files and warnings. Pass all arguments
from `$ARGUMENTS` directly to `geany-progress done`.

Examples:
- `/geany-progress done 2`
- `/geany-progress done 3 -r src/engine.c:117 -w "Review error handling"`

### `query`

Run `geany-progress query` to ask the plugin for its current plan state and print
the JSON response. This is the primary way for agents to auto-discover what plan is
active and which phase to start from — e.g. when `/opus-exec` is invoked with no
arguments.

```sh
state=$(geany-progress query)
echo "$state"
# {"plan":"My Task","plan_file":".planning/plans/MY_TASK_PLAN.md",...,"next_phase":3}
```

### `status`

Run `geany-progress status` to show the socket path and confirm the socket file exists.

## Notes for agents without access to the source

- You do not need the plugin source — just `nc` (or `socat`) and `$GEANY_PROGRESS_SOCK`.
- `GEANY_PROGRESS_SOCK` is set automatically when Geany loads the plugin; set it manually
  only when running outside a Geany terminal (e.g. a CI script pointing at a running Geany).
- Phase indices are **1-based**.
- Sending a second `init` message replaces the active plan entirely.
- Progress is also persisted to `.planning/state/XXX_PROGRESS.md` in the open project
  root after every update — readable from the filesystem without touching the socket.
- The socket accepts one message per connection; `nc -U` handles this correctly by default.
- Relative `plan_file` paths are resolved against the Geany project root automatically.
