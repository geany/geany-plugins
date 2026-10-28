---
name: opus-exec
description: executing a plan written by opus-plan
allowed-tools: java, bash
---

Claude Opus writes phased delivery plans in `.planning/plans/XXXX_PLAN.md`.

Claude Sonnet should execute.  If the agent asked to execute is Opus it should stop
and report this as an error.

Arguments to this skill may be the plan file path and optionally the starting phase.
The plan may also be provided as an attached document to the chat.

**When invoked with no arguments**, query the geanyprogress plugin to auto-discover
the active plan and resume point (see "Auto-discovery" below).

## File naming convention

- Plans:    `.planning/plans/XXXX_PLAN.md`
- Progress: `.planning/state/XXXX_PROGRESS.md`  (written by geanyprogress automatically)

## Auto-discovery (no arguments)

When no plan file is provided, resolve the active plan and starting phase from the
geanyprogress plugin:

```sh
if [ -z "$GEANY_PROGRESS_SOCK" ] || [ ! -S "$GEANY_PROGRESS_SOCK" ]; then
    echo "No plan argument and GEANY_PROGRESS_SOCK is not available." >&2
    echo "Pass the plan file path: /opus-exec .planning/plans/XXXX_PLAN.md [phase]" >&2
    exit 1
fi

state=$(geany-progress query)

# Parse with python3 (always available on Linux)
plan_file=$(python3 -c "import sys,json; d=json.loads(sys.stdin.read()); print(d['plan_file'] or '')" <<< "$state")
next_phase=$(python3 -c "import sys,json; d=json.loads(sys.stdin.read()); print(d['next_phase'] or '')" <<< "$state")

if [ -z "$plan_file" ]; then
    echo "geanyprogress has no active plan. Register one first with 'geany-progress init'." >&2
    exit 1
fi
```

- If `next_phase` is empty or `0`, all phases are already complete — report this and stop.
- Otherwise use `plan_file` as the plan to execute and `next_phase` as the starting phase.

The query response fields:
| Field           | Meaning |
|-----------------|---------|
| `plan`          | Display name of the active plan |
| `plan_file`     | Relative path to the `_PLAN.md` file |
| `progress_file` | Absolute path to the `_PROGRESS.md` file |
| `phases`        | Array of `{index, name, done}` objects |
| `next_phase`    | First pending phase number (1-based); `0` = all done; `null` = no plan |

## Start of execution

The plan was registered with the Geany sidebar by `opus-plan` when it was written.
Do not re-run `geany-progress init` — that would replace the active plan.

If a starting phase is given (or auto-discovered via `next_phase`), skip all earlier
phases and begin execution from that phase number.

## During execution

Execute phases autonomously, one after the other, until the context window is 80% full.

After completing each phase, run the `geany-progress done` command from the plan, which
includes review files and warnings specific to that phase:

```sh
geany-progress done N [-r path[:line]]... [-w "warning"]...
```

The plugin writes `.planning/state/XXXX_PROGRESS.md` automatically after each call.

If the context window reaches 80%, report the last completed phase number and wait for
the human to `/clear` and continue with `/opus-exec` from the next phase.

## Constraints

Never submit code.
Never change `.perf.targets` — humans must review performance regressions.
