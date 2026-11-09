---
name: opus-plan
description: use Claude Opus to plan development for Claude Sonnet
allowed-tools: java, bash
---

Rather than implementing any code, output should be a markdown file in
`.planning/plans/XXXX_PLAN.md` that contains a phased implementation plan where each
phase fits in the Claude Sonnet 4.6 context window.

The filename slug `XXXX` should be a short kebab-case description of the plan
(e.g. `geany-progress_PLAN.md`, `auth-refactor_PLAN.md`).

No changes should be made other than writing the one new plan document and registering
the plan with the Geany progress sidebar.

## Register the plan with the sidebar

After writing the plan file, run `geany-progress init` to register it:

```sh
geany-progress init -f ".planning/plans/XXXX_PLAN.md" "Plan Name" \
    "Phase 1 title" "Phase 2 title" ...
```

This pairs the sidebar entry with the plan file so clicking the title opens it in Geany.
If `$GEANY_PROGRESS_SOCK` is not set or the socket does not exist, skip silently.

## Phase completion markers

At the end of each phase section, include the exact `geany-progress done` command that
Claude Sonnet should run after completing that phase:

```sh
geany-progress done N [-r path[:line]]... [-w "warning"]...
```

where N is the 1-based phase number. Include `-r` flags for the key files the human
should review and `-w` flags for any important caveats or trade-offs.

Progress state is tracked in `.planning/state/XXXX_PROGRESS.md` (written automatically
by the geanyprogress plugin after each `geany-progress done` call).
