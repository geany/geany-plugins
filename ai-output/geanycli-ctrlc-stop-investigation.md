# geanycli: Ctrl+C and Stop Button Investigation

## Status: Fixes implemented and compiled successfully — needs Geany reload to test

## Problem Summary

Two related but distinct issues:

1. **Ctrl+C does not interrupt running processes** in the VTE terminal
2. **Stop button (⏹) does not interrupt running processes**

---

## Root Cause Analysis

### Issue 1: Ctrl+C key press not reaching VTE

**File:** `geanycli/src/geanycli.c` → `on_vte_key_press()`

The handler is connected to the **VTE widget's** `key-press-event`:
```c
g_signal_connect(vte_widget, "key-press-event",
                 G_CALLBACK(on_vte_key_press), tab);
```

**Why this fails:** In GTK3, `GtkWindow` processes key events before propagating
them to the focused widget. The default `GtkWindow::key_press_event` handler:
1. First tries all `GtkAccelGroup` accelerators registered on the window
2. Only if *not consumed*, propagates the event to the focused widget

Geany has `Edit → Copy` bound to `Ctrl+C` as an accelerator on the main window.
That accelerator returns `TRUE` (event handled), so GTK never propagates the event
to the VTE widget. `on_vte_key_press` is never called.

**Fix:** Connect to the **main Geany window's** `key-press-event` instead, and
fire only when a VTE terminal widget has focus. Window-level `g_signal_connect`
handlers run *before* the window's own default handler processes accelerators.

### Issue 2: Stop button uses wrong process group

**File:** `geanycli/src/geanycli.c` → `on_stop_btn_clicked()`

Current code:
```c
if (tab->child_pid > 0)
    kill(-getpgid((pid_t)tab->child_pid), SIGINT);
```

`tab->child_pid` is the PID of the **shell** (set in `on_spawn_ready`).
When the user runs a command like `sleep 100` or `npm test`, the shell forks
a child process. That child may be in a **different process group** (shells
typically create a new pgroup for each pipeline).

`getpgid(shell_pid)` returns the shell's own process group, not the foreground
process group currently running in the terminal. So the SIGINT either hits the
shell (which ignores it while waiting for a child) or hits the wrong pgroup.

**Fix:** Use the PTY's foreground process group via `tcgetpgrp()`:
```c
VtePty *pty = vte_terminal_get_pty(tab->vte);
int fd = vte_pty_get_fd(pty);
pid_t fg = tcgetpgrp(fd);
if (fg > 0)
    kill(-fg, SIGINT);
```

`tcgetpgrp(fd)` asks the kernel: "what is the foreground process group of the
terminal attached to this PTY fd?" — exactly what we need.

---

## Changes Made

In `geanycli/src/geanycli.c`:

### 1. Added `#include <termios.h>` (for tcgetpgrp)

### 2. Added module-level window key handler state
```c
static gulong window_key_handler_id = 0;
```

### 3. New function: `on_window_key_press`
Replaces the per-VTE `on_vte_key_press` for Ctrl+C / Ctrl+X interception.
Checks `gtk_window_get_focus()` to verify a VTE has focus. Feeds ETX (^C)
or CAN (^X) to that VTE.

### 4. `on_vte_key_press` still connected per-tab for future per-tab shortcuts
But the Ctrl+C / Ctrl+X interception moved to window-level handler.

### 5. `on_stop_btn_clicked` uses `tcgetpgrp(vte_pty_get_fd(pty))`
Sends SIGINT to the actual foreground process group of the PTY, not the shell.

### 6. Connect/disconnect window handler in plugin init/cleanup
`g_signal_connect` on `geany_data->main_widgets->window` during `plugin_init`,
`g_signal_handler_disconnect` during `plugin_cleanup`.

---

## Testing Required (after rebuild and reload)

1. Start a long-running command (e.g. `sleep 60` or `ping localhost`)
2. Press Ctrl+C — should interrupt immediately
3. Run same command, click the ⏹ stop button — should interrupt
4. Verify Ctrl+Shift+C still copies selected text (standard VTE copy shortcut)
5. Verify Ctrl+C in text editor tabs still works (copies text)
6. Verify shell itself is not killed — just the foreground command

---

## Files Modified

- `geanycli/src/geanycli.c`

---

## Build Command

```sh
cd /java/tools/geany-plugins/geanycli
make
# Then reload plugin in Geany: Tools → Plugin Manager
```
