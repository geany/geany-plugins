# Phased Delivery Plan: `geanyservers` Plugin

Target model: Claude Sonnet  
Source requirement: `ai-prompts/servers.prompt.md`  
Reference code: `geanyagent/src/geanyagent.c`, `geanycli/src/geanycli.c`

---

## Overview

A new Geany plugin `geanyservers` adds a **"Servers"** tab to the message window.
It reads an INI-style `servertools.conf` (same layering convention as `agenttools.conf`),
displays one row per configured server, shows live status/ping indicators, and
provides start/stop/reload/ping/log actions — all without blocking the UI thread.

---

## Phase 1 — Project skeleton and config loading

**Goal:** compilable plugin that loads server definitions from `servertools.conf`.

### Files to create

```
geanyservers/
  Makefile.am          — mirrors geanyagent/Makefile.am (SUBDIRS = src; plugin = geanyservers)
  src/
    Makefile.am        — mirrors geanyagent/src/Makefile.am
    geanyservers.c     — main plugin file
```

### Config format to implement

```ini
[servers]
server_0=🕋 nginx
name_0=nginx
type_0=systemd
changes_0=reload src/www;src/cgi

server_1=🧱 firewall
name_1=ufw
type_1=systemd
```

### Config layering (copy pattern from `geanyagent.c:agent_tools_load`)

1. Load user-level: `~/.config/geany/servertools.conf`
2. Overlay project-level: `<project>/config/geany/servertools.conf`
   (keys in project file overwrite matching keys in user file)
3. Merge additive: `<project>/config/geany/servertools_extra.conf`
   (keys here never overwrite — use indices starting at `_0` offset by
   `MAX_USER_SERVERS` so they never collide; document the convention)
4. Empty `server_N=` means "hide this server" (allows a project to suppress
   a user-level default).

### Data model

```c
typedef enum { SERVER_TYPE_SYSTEM, SERVER_TYPE_SYSTEMD, SERVER_TYPE_LOCAL } ServerType;

typedef struct {
    gchar      *display;   /* server_N= value (label with emoji) */
    gchar      *name;      /* name_N=   (service name for systemd) */
    ServerType  type;
    gchar      *start;
    gchar      *stop;
    gchar      *reload;
    gchar      *status;    /* custom status script; NULL → derive from type */
    gchar      *changes;   /* "method path1;path2" */
    gchar      *icon;
    gchar      *ping;
    gchar      *log;
    gchar      *sudo;
    gchar      *on_open;
    gchar      *on_close;
} ServerDef;
```

Parse all keys from the `[servers]` group into a `GPtrArray *servers` of `ServerDef *`.

### Deliverable

- `servers_load_config()` parses all three files, builds `GPtrArray`.
- `servers_free_config()` frees the array.
- A stub `plugin_init` that calls `servers_load_config()` and prints each server
  name via `geany->msgwin` (msgwin_msg_add) so it can be tested without a UI.
- `plugin_cleanup` calls `servers_free_config()`.

---

## Phase 2 — UI skeleton: "Servers" tab with GtkTreeView

**Goal:** the tab appears in Geany's message window and renders one row per server.

### Tab creation (copy pattern from `geanyagent.c:create_agent_tab`)

```c
static gint tab_index = -1;

static void create_servers_tab(void)
{
    /* GtkTreeView inside a GtkScrolledWindow */
    GtkWidget *label = gtk_label_new("\xf0\x9f\x96\xa5 Servers");
    tab_index = gtk_notebook_append_page(
        GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
        scrolled_window, label);
}
```

### GtkListStore columns

```c
enum {
    COL_STATUS_ICON,   /* GdkPixbuf — green/red dot (systemd state) */
    COL_PING_ICON,     /* GdkPixbuf — green/yellow dot (last ping) */
    COL_DISPLAY,       /* gchararray — server_N display string */
    COL_LAST_BOOT,     /* gchararray — timestamp or "" */
    COL_SERVER_PTR,    /* gpointer  — ServerDef * (non-visible) */
    COL_COUNT
};
```

### GtkTreeView columns (visible)

| # | Title    | Renderer        | Notes |
|---|----------|-----------------|-------|
| 0 | Status   | pixbuf          | green/red dot; tooltip "systemd active/inactive" |
| 1 | Active   | pixbuf          | green/yellow dot; tooltip "last ping OK/failed" |
| 2 | Server   | text            | display string |
| 3 | Actions  | — (see below)   | start · stop · reload · ping buttons |
| 4 | Last boot| text            | blank unless this session started it |
| 5 | Logs     | — (see below)   | button if `log` key is set |

**Actions column** — use a `GtkCellRendererPixbuf` or embed a `GtkHBox` of icon
buttons via a `gtk_tree_view_column_set_cell_data_func`.  Simplest correct approach
for GTK3: one column per action button using `gtk_button_new_from_icon_name` placed
in a custom `GtkCellRenderer` subclass — **but** because custom cell renderers are
complex, use instead a fixed-height overlay approach: place a `GtkHBox` of four
`GtkButton` widgets above each row using `gtk_overlay`, keyed by row.  Easiest
maintainable approach: put the buttons in a `GtkGrid` below the `GtkTreeView` that
updates when a row is selected.

Use the **selection-based button bar** approach (simpler, correct):
- Below the `GtkTreeView`, add a `GtkToolbar` or `GtkButtonBox` with:
  `[▶ Start] [■ Stop] [↻ Reload] [⚡ Ping]` and `[📋 Logs]`
- Buttons are sensitive only when a row is selected.
- Connect `GtkTreeSelection::changed` to update sensitivity and store selected `ServerDef *`.

### Status dots

Create two 12×12 `GdkPixbuf` programmatically using Cairo surface:

```c
static GdkPixbuf *make_dot(double r, double g, double b);
```

Reuse across all rows; keep `pixbuf_green`, `pixbuf_red`, `pixbuf_yellow` as module
globals.

### Deliverable

Tab appears, lists servers from config with placeholder grey dots and greyed-out buttons.

---

## Phase 3 — Async status and ping

**Goal:** when the Servers tab is focused, all servers are pinged/checked without
blocking the UI thread.

### Design constraint

Must not block GTK main loop.  Use `g_spawn_async_with_pipes` +
`g_io_add_watch` to read output, or `g_subprocess` (GLib ≥ 2.40, available on
Ubuntu 22.04+).  Prefer `g_subprocess` for cleaner resource management.

### Status check per type

```
systemd:  systemctl is-active <name>   → stdout "active\n" means up
system:   same as systemd (systemctl)
local:    run the custom `status` script; exit code 0 = up
```

### Ping

Run the configured `ping` command.  If not set, skip (leave dot grey).  Exit code 0
= success (green dot), non-zero = failure (yellow dot).

### Async flow

```c
typedef struct {
    GtkListStore *store;
    GtkTreeRowReference *row_ref;  /* stable even if list is reordered */
    gint          col;             /* COL_STATUS_ICON or COL_PING_ICON */
    GSubprocess  *proc;
} AsyncCheck;

static void check_done_cb(GObject *src, GAsyncResult *res, gpointer data);
```

On tab switch (`switch-page` signal on the message notebook):

```c
static void on_tab_switched(GtkNotebook *nb, GtkWidget *page,
                            guint page_num, gpointer data)
{
    if (page_num != (guint)tab_index) return;
    servers_refresh_all();
}
```

`servers_refresh_all()` iterates all rows in `GtkListStore`, spawns one
`AsyncCheck` per server for status and one for ping.  Use a `GCancellable` stored
as a module global so `plugin_cleanup` can cancel in-flight checks.

### Result callback

On completion, call `gtk_list_store_set` on the row to update the `GdkPixbuf`
column.  Guard with `gtk_tree_row_reference_valid`.

### Deliverable

Focusing the Servers tab updates all dots within a second or two.

---

## Phase 4 — Action buttons: start, stop, reload, ping

**Goal:** clicking Start/Stop/Reload/Ping executes the relevant command.

### Command resolution per server

| Button  | Command used                                        |
|---------|-----------------------------------------------------|
| Start   | `start` key → else `systemctl start <name>`         |
| Stop    | `stop` key  → else `systemctl stop <name>`          |
| Reload  | `reload` key → else `systemctl reload <name>`       |
| Ping    | `ping` key → run it; if absent, show msgwin warning |

### Sudo handling

If `sudo` key is set (non-empty), prefix the command with that value.
If blank/absent, run as current user (systemd handles privilege via polkit popups).

### Execution

Use `geanycli_run_command` IPC (signal `"geanycli-run-command"`) so output appears
in the CLI tab:

```c
g_signal_emit_by_name(geany->object, "geanycli-run-command", cmd, TRUE);
```

Pass `TRUE` for new_tab so each action gets a fresh terminal tab.

If geanycli is not loaded (signal has no handlers), fall back to
`g_spawn_command_line_async`.

### Last boot column

When **Start** is clicked, record `g_date_time_format(now, "%H:%M:%S")` and set
`COL_LAST_BOOT` for that row.

### Logs button

If `log` key is set:

```c
gchar *cmd = g_strdup_printf("tail -f %s", server->log);
g_signal_emit_by_name(geany->object, "geanycli-run-command", cmd, TRUE);
g_free(cmd);
```

### Deliverable

All four action buttons and the Logs button work; output appears in the CLI tab.

---

## Phase 5 — Project lifecycle hooks and file-change watching

**Goal:** implement `on_open`, `on_close`, and `changes` attributes.

### Project open/close signals

Connect in `plugin_init`:

```c
plugin_signal_connect(geany_plugin, NULL, "project-open",  TRUE, G_CALLBACK(on_project_open),  NULL);
plugin_signal_connect(geany_plugin, NULL, "project-close", TRUE, G_CALLBACK(on_project_close), NULL);
```

In each handler, reload config, rebuild the list store, then for every server
whose `on_open`/`on_close` matches `"start"`, `"stop"`, or `"reload"`, fire the
corresponding action.

### File-change watching (`changes` attribute)

Format: `"method path1;path2"` — first token is the restart method, rest are
project-relative paths to watch.

Use `GFileMonitor` (via `g_file_monitor_directory`) for each watched path.
On `G_FILE_MONITOR_EVENT_CHANGED` or `CREATED`, call the configured method after
a 500 ms debounce (use `g_timeout_add` — cancel previous timeout if still pending).

Store active monitors in a `GPtrArray *monitors`; cancel and free all in
`plugin_cleanup` and on config reload.

### Deliverable

Servers with `on_open=start` auto-start when a Geany project is opened.
Servers with `changes=reload src/www` hot-reload when watched files change.

---

## Build integration

After all phases, wire `geanyservers` into the top-level build:

1. Add `geanyservers` to `configure.ac` `AC_CONFIG_FILES` list (follow the
   `geanyagent` entry as a template).
2. Add `geanyservers` to the top-level `Makefile.am` `SUBDIRS`.
3. Run `./autogen.sh` to regenerate.

---

## Implementation notes for Sonnet

- **Follow `geanyagent.c` exactly** for: config file path construction
  (`g_build_filename(geany->app->configdir, …)`), plugin lifecycle
  (`plugin_init` / `plugin_cleanup` / `plugin_set_info`), and tab creation.
- **Follow `geanycli.c`** for IPC signal emission and the `get_work_dir()` helper.
- Keep all GTK calls on the main thread.  All `g_subprocess` / `g_spawn_async`
  callbacks already run on the main loop via GLib's default context.
- Do not link against geanycli — use only the GLib signal IPC.
- Prefer `GSubprocess` over `g_spawn_async_with_pipes`; it handles fd leaks
  correctly and the API is cleaner.
- The `GtkListStore` row reference pattern (`gtk_tree_row_reference_new`) is
  required because async callbacks complete after the user may have reloaded config.
- UTF-8 dot characters for status: U+1F7E2 🟢 (green), U+1F534 🔴 (red),
  U+1F7E1 🟡 (yellow) — or use Cairo-drawn pixbufs for precise sizing.

---

## Delivery sequence

```
Phase 1  →  Phase 2  →  Phase 3  →  Phase 4  →  Phase 5
skeleton    UI tab       async        actions      hooks
```

Each phase leaves the plugin in a compilable, testable state.


# Result

All 5 phases delivered in a single compilable plugin:

  - Phase 1 — Config loading: servers_load_config() reads ~/.config/geany/servertools.conf, overlays <project>/config/geany/servertools.conf, and merges servertools_extra.conf (indices offset by
   MAX_USER_SERVERS=100). Empty server_N= hides a server. ServerDef struct holds all 14 keys.
  - Phase 2 — UI tab: GtkListStore with 5 columns (status dot, ping dot, display, last boot, server pointer), GtkTreeView with 4 visible columns, selection-based button bar (▶ Start ■ Stop ↻ 
  Reload ⚡ Ping 📋 Logs). Cairo-drawn 12×12 colored dots (pixbuf_green/red/yellow/grey).
  - Phase 3 — Async status/ping: servers_refresh_all() spawns one GSubprocess per server per check type; on_check_done callback updates the list store via GtkTreeRowReference. Triggered on
  switch-page. Cancelable via GCancellable.
  - Phase 4 — Actions: buttons emit geanycli-run-command IPC signal (with new_tab=TRUE); fall back to g_spawn_command_line_async if geanycli isn't loaded. Start records a timestamp in "Last
  boot". sudo key prefixes commands. 
  - Phase 5 — Hooks + file watching: project-open/project-close signals reload config and fire on_open/on_close actions. GFileMonitor watches changes= paths with 500ms debounce.
  - Build integration: geanyservers.m4, configure.ac entry, Makefile.am entry — confirmed working with autogen.sh and make.

✻ Cooked for 4m 52s

