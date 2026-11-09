---
name: control
description: "Control the Geany editor UI from the agent terminal — open files, save, scroll to changes, refresh the file tree"
argument-hint: "<command> [args] | save-all | refresh | open <path> | scroll <path>:<line> | list"
allowed-tools:
  - Bash
  - Read
---

<objective>
Drive the live Geany editor from within this agent session using the geanycontrol plugin.

This skill lets the agent:
- Save open files before starting work so Geany does not prompt about conflicts
- Open files the agent just created or modified
- Scroll Geany to the exact line of a change
- Refresh the treebrowser after adding or deleting files on disk
- Query which files are currently open
- Trigger Tools-menu items by name

All operations go through the geany-ctrl script which talks to the
geanycontrol Unix socket at ~/.config/geany/geanycontrol.sock.
The plugin must be loaded in the running Geany instance.
</objective>

<context>
## Socket path

    ~/.config/geany/geanycontrol.sock
    (expands to: $XDG_CONFIG_HOME/geany/geanycontrol.sock if XDG_CONFIG_HOME is set)

## geany-ctrl script location (in this repo)

    /home/teknopaul/github_workspace/geany-plugins/geanycontrol/geany-ctrl

Add it to PATH or call it with the full path.  Requires `socat` at runtime.

## Availability check

Before sending commands, verify the plugin is running:

    printf 'ping\n' | socat - UNIX-CONNECT:$HOME/.config/geany/geanycontrol.sock

If the socket does not exist, Geany is not running or the GeanyControl
plugin is not loaded — skip UI operations, they are not required for the
work to succeed.

## All commands

| Command | What it does |
|---------|-------------|
| `ping` | Liveness check; always replies `ok` |
| `save-all` | Save every unsaved open document |
| `save-file <path>` | Save one specific document |
| `open-file <path>` | Open a file in the editor |
| `close-file <path>` | Close a document (unsaved ch1nges are discarded) |
| `scroll-to-line <path>:<line>` | Open file and jump to line (1-based) |
| `get-current-file` | Return the path of the currently active document |
| `list-open-files` | Return newline-separated list of all open file paths |
| `activate-menu-item <label>` | Activate a Tools-menu item by label (case-insensitive) |
| `refresh` | Signal treebrowser to reload the file tree |

## geany-ctrl usage

    geany-ctrl ping
    geany-ctrl save-all
    geany-ctrl open-file /path/to/file.c
    geany-ctrl close-file /path/to/file.c
    geany-ctrl save-file /path/to/file.c
    geany-ctrl scroll-to-line /path/to/file.c:42
    geany-ctrl get-current-file
    geany-ctrl list-open-files
    geany-ctrl activate-menu-item "Agent Tools"
    geany-ctrl refresh

## Raw socat usage (no script dependency)

    SOCK="$HOME/.config/geany/geanycontrol.sock"
    echo "save-all"          | socat - "UNIX-CONNECT:$SOCK"
    echo "refresh"           | socat - "UNIX-CONNECT:$SOCK"
    echo "open-file /tmp/x"  | socat - "UNIX-CONNECT:$SOCK"

## Inter-plugin signal API (C code only)

Other plugins can drive geanycontrol without going through the socket by
emitting signals on `geany->object`:

    g_signal_emit_by_name(geany->object, "geanycontrol-open-file", path);
    g_signal_emit_by_name(geany->object, "geanycontrol-close-file", path);
    g_signal_emit_by_name(geany->object, "geanycontrol-save-file", path);
    g_signal_emit_by_name(geany->object, "geanycontrol-save-all");
    g_signal_emit_by_name(geany->object, "geanycontrol-scroll-to-line", "path:42");
    g_signal_emit_by_name(geany->object, "geanycontrol-activate-menu-item", label);
    g_signal_emit_by_name(geany->object, "geanycontrol-refresh");

Check the signal exists before emitting (geanycontrol may not be loaded):

    if (g_signal_lookup("geanycontrol-open-file", G_OBJECT_TYPE(geany->object)))
        g_signal_emit_by_name(geany->object, "geanycontrol-open-file", path);

## treebrowser refresh wiring

treebrowser.c already connects to "geanycontrol-refresh" in plugin_init —
no extra code needed.  If geanycontrol is loaded before treebrowser, the
connection happens automatically.  If treebrowser loads first, the signal
lookup returns 0 and the connection is skipped (harmless; refresh will
still emit the signal but nothing will be connected).
</context>

<process>
Determine what the user (or prior task) needs done with the Geany UI, then
execute the minimal set of geany-ctrl commands to accomplish it.

## Step 1 — Check availability

    SOCK="$HOME/.config/geany/geanycontrol.sock"
    if [ ! -S "$SOCK" ]; then
        # Geany not running or plugin not loaded — skip UI steps, note in output
        exit 0
    fi

## Step 2 — Common workflow patterns

### Before starting work on files Geany has open

    geany-ctrl save-all

This prevents "file changed on disk" prompts while the agent edits files.

### After editing or creating files

    geany-ctrl refresh

This reloads the treebrowser so newly created or deleted files appear
immediately.

### Open a specific file

    geany-ctrl open-file /path/to/changed-file.c

### Jump to a specific line (e.g., where a bug was fixed)

    geany-ctrl scroll-to-line /path/to/changed-file.c:42

### Full post-edit sequence

    geany-ctrl refresh
    geany-ctrl open-file /path/to/changed-file.c
    geany-ctrl scroll-to-line /path/to/changed-file.c:42

### Query what is open before deciding what to close

    geany-ctrl list-open-files

## Step 3 — Report result

If any command returns a line starting with `error:`, report it to the user.
`ok` responses can be silently swallowed unless the user asked for verbose output.

## Error handling

- Socket missing → note "GeanyControl not available" and continue without UI steps.
- `error: could not open file` → the path does not exist; verify the path before retrying.
- `error: menu item not found` → the label did not match; check the exact label text
  in Geany's Tools menu.
- `socat` not installed → install it (`sudo apt install socat`) or use the raw
  printf/socat one-liner pattern from the context section above.
</process>
