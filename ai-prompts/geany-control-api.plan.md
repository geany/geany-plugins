# Phased Delivery Plan: `geanycontrol` Plugin

Target model: Claude Sonnet  
Source requirement: `ai-prompts/geany-control-api.md`  
Reference code: `geanyagent/src/geanyagent.c`, `geanycli/src/geanycli.c`,
`treebrowser/src/treebrowser.c`

---

## Overview

A new Geany plugin `geanycontrol` gives the agent (and any external process) a
programmatic handle on the Geany UI.  Two equivalent access paths are provided:

1. **Unix domain socket** — a line-delimited text protocol at
   `~/.config/geany/geanycontrol.sock`.  Any process (shell script, `geany-ctrl`
   helper, the agent itself) can `echo "open-file /path" | socat …` to drive the
   editor without GTK knowledge.

2. **GLib signals on `geany->object`** — other plugins emit
   `"geanycontrol-<action>"` signals for zero-copy in-process IPC, exactly as
   geanycli already does for `"geanycli-run-command"`.

All socket I/O is non-blocking (GLib `GSocketService`).  Every GTK-touching
operation is marshalled to the main thread via `g_idle_add` so the socket
listener can safely run from a GLib main-context I/O callback.

---

## Command protocol

Each command is a single UTF-8 line (`\n` terminated).  The server responds
with `ok\n` or `error: <reason>\n`.

| Command | Parameters | Effect |
|---------|-----------|--------|
| `open-file` | `<path>` | Open file in editor |
| `close-file` | `<path>` | Close named document (unsaved changes → discard) |
| `save-file` | `<path>` | Save named document |
| `save-all` | — | Save every unsaved open document |
| `scroll-to-line` | `<path>:<line>` | Open file, go to line (1-based) |
| `get-current-file` | — | Reply: current doc path or `none` |
| `list-open-files` | — | Reply: newline-separated open file paths + `ok` |
| `activate-menu-item` | `<label>` | Activate first Tools-menu item matching label |
| `refresh` | — | Signal treebrowser to reload the file tree |
| `ping` | — | Liveness probe; always replies `ok` |

---

## Phase 1 — Plugin skeleton + socket server

**Goal:** compilable plugin that opens a Unix domain socket, accepts
connections, echoes every received line back as `ok`, and registers the IPC
signals on `geany->object`.

### Files to create

```
geanycontrol/
  Makefile.am
  src/
    Makefile.am
    geanycontrol.c
```

### Makefile.am (top level)

```makefile
include $(top_srcdir)/build/vars.auxfiles.mk
AUXFILES =
SUBDIRS = src
plugin = geanycontrol
```

### src/Makefile.am

```makefile
include $(top_srcdir)/build/vars.build.mk
plugin = geanycontrol

geanyplugins_LTLIBRARIES = geanycontrol.la

geanycontrol_la_SOURCES  = geanycontrol.c
geanycontrol_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
geanycontrol_la_CFLAGS   = $(AM_CFLAGS)
geanycontrol_la_LIBADD   = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
```

### Plugin globals

```c
GeanyPlugin  *geany_plugin;
GeanyData    *geany_data;

static GSocketService *socket_service = NULL;
static gchar          *socket_path    = NULL;
```

### Socket setup in `gc_init`

```c
static void gc_init(GeanyPlugin *plugin, gpointer data)
{
    socket_path = g_build_filename(geany->app->configdir,
                                   "geanycontrol.sock", NULL);
    g_unlink(socket_path);   /* remove stale socket from previous session */

    GSocketAddress *addr = g_unix_socket_address_new(socket_path);
    socket_service = g_socket_service_new();

    GError *err = NULL;
    g_socket_listener_add_address(G_SOCKET_LISTENER(socket_service),
                                  addr, G_SOCKET_TYPE_STREAM,
                                  G_SOCKET_PROTOCOL_DEFAULT,
                                  NULL, NULL, &err);
    g_object_unref(addr);

    if (err) {
        g_warning("geanycontrol: socket bind failed: %s", err->message);
        g_error_free(err);
        g_clear_object(&socket_service);
        return;
    }

    g_signal_connect(socket_service, "incoming",
                     G_CALLBACK(on_incoming_connection), NULL);
    g_socket_service_start(socket_service);

    gc_register_signals();
}
```

### Connection handler (stub — Phase 2 fills the dispatch)

Each incoming connection spawns a `GDataInputStream` reader.  The reader is
destroyed on EOF.  All received lines are passed to `dispatch_command(line)`
which in Phase 1 just writes `ok\n` back.

```c
static gboolean on_incoming_connection(GSocketService    *svc,
                                       GSocketConnection *conn,
                                       GObject           *source,
                                       gpointer           data)
{
    GInputStream     *in  = g_io_stream_get_input_stream(G_IO_STREAM(conn));
    GOutputStream    *out = g_io_stream_get_output_stream(G_IO_STREAM(conn));
    GDataInputStream *reader = g_data_input_stream_new(in);

    ConnCtx *ctx = g_new0(ConnCtx, 1);
    ctx->reader = reader;
    ctx->out    = g_object_ref(out);
    ctx->conn   = g_object_ref(conn);

    g_data_input_stream_read_line_async(reader, G_PRIORITY_DEFAULT, NULL,
                                        on_read_line, ctx);
    return TRUE;
}
```

```c
typedef struct {
    GDataInputStream *reader;
    GOutputStream    *out;
    GSocketConnection *conn;
} ConnCtx;
```

Line callback:

```c
static void on_read_line(GObject *src, GAsyncResult *res, gpointer data)
{
    ConnCtx *ctx  = data;
    GError  *err  = NULL;
    gchar   *line = g_data_input_stream_read_line_finish(
                        G_DATA_INPUT_STREAM(src), res, NULL, &err);

    if (!line) {          /* EOF or error → close connection */
        conn_ctx_free(ctx);
        if (err) g_error_free(err);
        return;
    }

    gchar *reply = dispatch_command(g_strstrip(line));
    g_free(line);

    g_output_stream_write_all(ctx->out, reply, strlen(reply), NULL, NULL, NULL);
    g_free(reply);

    /* Read next line */
    g_data_input_stream_read_line_async(ctx->reader, G_PRIORITY_DEFAULT, NULL,
                                        on_read_line, ctx);
}
```

Phase 1 stub:

```c
static gchar *dispatch_command(const gchar *line)
{
    (void)line;
    return g_strdup("ok\n");
}
```

### Signal registration

```c
static void gc_register_signals(void)
{
    GType obj_type = G_OBJECT_TYPE(geany->object);

    struct { const gchar *name; GType arg; } sigs[] = {
        { "geanycontrol-open-file",       G_TYPE_STRING },
        { "geanycontrol-close-file",      G_TYPE_STRING },
        { "geanycontrol-save-file",       G_TYPE_STRING },
        { "geanycontrol-save-all",        G_TYPE_NONE   },
        { "geanycontrol-scroll-to-line",  G_TYPE_STRING }, /* "path:line" */
        { "geanycontrol-activate-menu-item", G_TYPE_STRING },
        { "geanycontrol-refresh",         G_TYPE_NONE   },
        { NULL }
    };

    for (gint i = 0; sigs[i].name; i++) {
        if (g_signal_lookup(sigs[i].name, obj_type))
            continue;
        if (sigs[i].arg == G_TYPE_NONE)
            g_signal_new(sigs[i].name, obj_type, G_SIGNAL_RUN_LAST,
                         0, NULL, NULL, NULL, G_TYPE_NONE, 0);
        else
            g_signal_new(sigs[i].name, obj_type, G_SIGNAL_RUN_LAST,
                         0, NULL, NULL, NULL, G_TYPE_NONE,
                         1, sigs[i].arg);
    }
}
```

### Cleanup

```c
static void gc_cleanup(GeanyPlugin *plugin, gpointer data)
{
    if (socket_service) {
        g_socket_service_stop(socket_service);
        g_clear_object(&socket_service);
    }
    if (socket_path) {
        g_unlink(socket_path);
        g_free(socket_path);
        socket_path = NULL;
    }
}
```

### `geany_load_module`

```c
plugin->info->name        = "GeanyControl";
plugin->info->description = "Unix socket + signal IPC for agent-driven UI control.";
plugin->info->version     = "0.1";
plugin->info->author      = "teknopaul";
plugin->funcs->init       = gc_init;
plugin->funcs->cleanup    = gc_cleanup;
GEANY_PLUGIN_REGISTER(plugin, 235);
```

### Deliverable

`make` compiles; `socat - UNIX-CONNECT:~/.config/geany/geanycontrol.sock`
accepts a line and replies `ok`.

---

## Phase 2 — Document operations (open, close, save)

**Goal:** `dispatch_command` implements `open-file`, `close-file`, `save-file`,
and `save-all`.  All GTK calls are marshalled to the main thread.

### Thread-safe dispatch pattern

Socket I/O callbacks run on the GLib default main context — the same thread as
GTK — so `g_idle_add` is not strictly required, but wrapping each operation in
a one-shot idle keeps the architecture clean and future-proof.

```c
typedef struct {
    gchar *path;
    gint   line;   /* -1 if not applicable */
} FileOpData;

static void file_op_data_free(gpointer p)
{
    FileOpData *d = p;
    g_free(d->path);
    g_free(d);
}
```

### `open-file <path>`

```c
static gboolean idle_open_file(gpointer data)
{
    FileOpData *d = data;
    document_open_file(d->path, FALSE, NULL, NULL);
    file_op_data_free(d);
    return G_SOURCE_REMOVE;
}
```

Dispatch:

```c
FileOpData *d = g_new0(FileOpData, 1);
d->path = g_strdup(arg);   /* arg = everything after "open-file " */
g_idle_add(idle_open_file, d);
return g_strdup("ok\n");
```

### `close-file <path>`

```c
static gboolean idle_close_file(gpointer data)
{
    FileOpData *d = data;
    guint i;
    GeanyDocument *doc;

    foreach_document(i) {
        doc = documents[i];
        if (doc->file_name && strcmp(doc->file_name, d->path) == 0) {
            document_close(doc);
            break;
        }
    }
    file_op_data_free(d);
    return G_SOURCE_REMOVE;
}
```

### `save-file <path>`

```c
static gboolean idle_save_file(gpointer data)
{
    FileOpData *d = data;
    guint i;
    GeanyDocument *doc;

    foreach_document(i) {
        doc = documents[i];
        if (doc->file_name && strcmp(doc->file_name, d->path) == 0) {
            document_save_file(doc, FALSE);
            break;
        }
    }
    file_op_data_free(d);
    return G_SOURCE_REMOVE;
}
```

### `save-all`

```c
static gboolean idle_save_all(G_GNUC_UNUSED gpointer data)
{
    guint i;
    GeanyDocument *doc;

    foreach_document(i) {
        doc = documents[i];
        if (doc->file_name && doc->changed)
            document_save_file(doc, FALSE);
    }
    return G_SOURCE_REMOVE;
}
```

Dispatch for `save-all` does not allocate a `FileOpData`; passes `NULL`.

### Signal handlers (in-process IPC)

Connect in `gc_init` after `gc_register_signals()`:

```c
g_signal_connect(geany->object, "geanycontrol-open-file",
                 G_CALLBACK(on_signal_open_file), NULL);
g_signal_connect(geany->object, "geanycontrol-close-file",
                 G_CALLBACK(on_signal_close_file), NULL);
g_signal_connect(geany->object, "geanycontrol-save-file",
                 G_CALLBACK(on_signal_save_file), NULL);
g_signal_connect(geany->object, "geanycontrol-save-all",
                 G_CALLBACK(on_signal_save_all), NULL);
```

Each signal handler duplicates the path argument and calls the same idle
function as the socket path to avoid code duplication.

### Updated `dispatch_command`

```c
static gchar *dispatch_command(const gchar *line)
{
    if (g_str_has_prefix(line, "open-file "))
        return cmd_open_file(line + 10);
    if (g_str_has_prefix(line, "close-file "))
        return cmd_close_file(line + 11);
    if (g_str_has_prefix(line, "save-file "))
        return cmd_save_file(line + 10);
    if (strcmp(line, "save-all") == 0)
        return cmd_save_all();
    if (strcmp(line, "ping") == 0)
        return g_strdup("ok\n");
    return g_strdup("error: unknown command\n");
}
```

### Deliverable

```sh
echo "save-all" | socat - UNIX-CONNECT:~/.config/geany/geanycontrol.sock
echo "open-file /path/to/foo.c" | socat - UNIX-CONNECT:~/.config/geany/geanycontrol.sock
```

Both commands act on the live Geany editor.

---

## Phase 3 — Scroll, query, and list operations

**Goal:** implement `scroll-to-line`, `get-current-file`, and
`list-open-files`; these require synchronous replies and use the main-thread
assumption of the GLib socket callbacks (no extra idle needed for reads).

### `scroll-to-line <path>:<line>`

Parse `<path>:<line>` by splitting on the last `:`.

```c
static gchar *cmd_scroll_to_line(const gchar *arg)
{
    /* Find last colon to support paths with colons in them */
    const gchar *colon = strrchr(arg, ':');
    if (!colon || colon == arg)
        return g_strdup("error: expected path:line\n");

    gchar *path = g_strndup(arg, colon - arg);
    gint   line = (gint)g_ascii_strtoll(colon + 1, NULL, 10) - 1; /* 0-based */

    GeanyDocument *doc = document_open_file(path, FALSE, NULL, NULL);
    if (!doc) {
        g_free(path);
        return g_strdup("error: could not open file\n");
    }
    if (line >= 0)
        sci_goto_line(doc->editor->sci, line, TRUE);
    g_free(path);
    return g_strdup("ok\n");
}
```

Note: `document_open_file` is safe to call from the same thread as GTK (the
GLib default main context), so no `g_idle_add` wrapper is required here.

### `get-current-file`

```c
static gchar *cmd_get_current_file(void)
{
    GeanyDocument *doc = document_get_current();
    if (!doc || !doc->file_name)
        return g_strdup("none\nok\n");
    return g_strdup_printf("%s\nok\n", doc->file_name);
}
```

### `list-open-files`

```c
static gchar *cmd_list_open_files(void)
{
    GString *buf = g_string_new(NULL);
    guint i;
    GeanyDocument *doc;

    foreach_document(i) {
        doc = documents[i];
        if (doc->file_name)
            g_string_append_printf(buf, "%s\n", doc->file_name);
    }
    g_string_append(buf, "ok\n");
    return g_string_free(buf, FALSE);
}
```

### Add to `dispatch_command`

```c
if (g_str_has_prefix(line, "scroll-to-line "))
    return cmd_scroll_to_line(line + 15);
if (strcmp(line, "get-current-file") == 0)
    return cmd_get_current_file();
if (strcmp(line, "list-open-files") == 0)
    return cmd_list_open_files();
```

### `geanycontrol-scroll-to-line` signal

```c
g_signal_connect(geany->object, "geanycontrol-scroll-to-line",
                 G_CALLBACK(on_signal_scroll_to_line), NULL);
```

Handler parses the `"path:line"` string argument and calls `cmd_scroll_to_line`.

### Deliverable

```sh
echo "scroll-to-line /path/foo.c:42" | socat - UNIX-CONNECT:…/geanycontrol.sock
echo "get-current-file"              | socat - UNIX-CONNECT:…/geanycontrol.sock
echo "list-open-files"               | socat - UNIX-CONNECT:…/geanycontrol.sock
```

All three reply with meaningful output.

---

## Phase 4 — Menu item activation + `geany-ctrl` CLI helper

**Goal:** implement `activate-menu-item <label>` to trigger any Tools-menu
item by label; create the `geany-ctrl` shell script for ergonomic CLI use.

### `activate-menu-item <label>`

Walk the Tools menu looking for a `GtkMenuItem` whose label text matches
`<label>` (case-insensitive).  Activate the first match.

```c
typedef struct {
    const gchar *target;   /* label to match, lower-cased */
    gboolean     found;
} MenuWalk;

static void walk_menu(GtkWidget *widget, gpointer data)
{
    MenuWalk *w = data;
    if (w->found) return;

    if (GTK_IS_MENU_ITEM(widget)) {
        GtkWidget *child = gtk_bin_get_child(GTK_BIN(widget));
        if (GTK_IS_LABEL(child)) {
            const gchar *text = gtk_label_get_text(GTK_LABEL(child));
            gchar *lower = g_utf8_strdown(text, -1);
            if (strcmp(lower, w->target) == 0) {
                w->found = TRUE;
                gtk_menu_item_activate(GTK_MENU_ITEM(widget));
            }
            g_free(lower);
        }
        /* Recurse into sub-menus */
        GtkWidget *sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
        if (sub)
            gtk_container_foreach(GTK_CONTAINER(sub), walk_menu, data);
    }
}

static gchar *cmd_activate_menu_item(const gchar *label)
{
    gchar    *lower  = g_utf8_strdown(label, -1);
    MenuWalk  w      = { lower, FALSE };

    gtk_container_foreach(
        GTK_CONTAINER(geany_data->main_widgets->tools_menu),
        walk_menu, &w);

    g_free(lower);
    return w.found
        ? g_strdup("ok\n")
        : g_strdup("error: menu item not found\n");
}
```

### `geany-ctrl` shell script

Create `geanycontrol/geany-ctrl`:

```sh
#!/bin/sh
# geany-ctrl — send a command to the running Geany instance
# Usage: geany-ctrl <command> [args...]
#
# Examples:
#   geany-ctrl open-file /path/to/file.c
#   geany-ctrl save-all
#   geany-ctrl scroll-to-line /path/to/file.c:42
#   geany-ctrl list-open-files
#   geany-ctrl activate-menu-item "Agent Tools"
#   geany-ctrl refresh

SOCK="${XDG_CONFIG_HOME:-$HOME/.config}/geany/geanycontrol.sock"

if [ ! -S "$SOCK" ]; then
    echo "error: geanycontrol socket not found at $SOCK" >&2
    exit 1
fi

if [ $# -eq 0 ]; then
    echo "Usage: geany-ctrl <command> [args]" >&2
    exit 1
fi

# Combine all arguments into a single command line
CMD="$*"
printf '%s\n' "$CMD" | socat - "UNIX-CONNECT:$SOCK"
```

Make it executable (`chmod +x`).  It does not need to be installed system-wide;
the agent can call it with a full path, or the user adds it to PATH.

### Deliverable

```sh
geany-ctrl open-file /path/to/main.c
geany-ctrl activate-menu-item "Agent Tools"
geany-ctrl ping   # should reply: ok
```

---

## Phase 5 — Treebrowser refresh + full integration

**Goal:** implement `refresh` to reload the treebrowser's file tree; wire up the
`geanycontrol-refresh` signal that the treebrowser (or any other plugin) can
receive; demonstrate the complete round-trip from the agent terminal.

### `refresh` command

The treebrowser already listens for `"geanycli-run-file"` and
`"geanycli-run-command"` signals.  It does not (yet) expose a dedicated
refresh signal, but its refresh action is wired to an internal menu item
labelled "Refresh".  Two strategies:

**Strategy A — trigger via menu item** (no treebrowser change needed):

```c
static gchar *cmd_refresh(void)
{
    return cmd_activate_menu_item("refresh");
}
```

This works because treebrowser registers its right-click "Refresh" item on a
`GtkMenuItem` in the sidebar's context menu.  Walking from the Tools menu will
not find it; instead walk the sidebar widget tree.

**Strategy B — emit `geanycontrol-refresh` and let treebrowser connect to it**
(requires a one-line addition to `treebrowser.c`):

In `treebrowser.c` `plugin_init`:
```c
if (g_signal_lookup("geanycontrol-refresh", G_OBJECT_TYPE(geany->object)))
    g_signal_connect(geany->object, "geanycontrol-refresh",
                     G_CALLBACK(on_geanycontrol_refresh), NULL);
```

Where `on_geanycontrol_refresh` calls the existing `refresh_subtree(root)`.

Implement Strategy B as it is cleaner and demonstrates the cross-plugin signal
pattern.  Fall back to a direct call to `refresh_subtree` via symbol lookup if
the treebrowser signal handler is not connected.

### `cmd_refresh` implementation

```c
static gboolean idle_refresh(G_GNUC_UNUSED gpointer data)
{
    g_signal_emit_by_name(geany->object, "geanycontrol-refresh");
    return G_SOURCE_REMOVE;
}

static gchar *cmd_refresh(void)
{
    g_idle_add(idle_refresh, NULL);
    return g_strdup("ok\n");
}
```

Add to `dispatch_command`:

```c
if (strcmp(line, "refresh") == 0)
    return cmd_refresh();
```

### `treebrowser.c` addition

In `treebrowser.c`, add near the end of `plugin_init` (after the existing
signal connections for `geanycli-*`):

```c
GType obj_type = G_OBJECT_TYPE(geany->object);
if (g_signal_lookup("geanycontrol-refresh", obj_type))
    g_signal_connect(geany->object, "geanycontrol-refresh",
                     G_CALLBACK(on_treebrowser_refresh_signal), NULL);
```

And add the handler (calls the same `treebrowser_browse_dir` or
`refresh_subtree` that the menu item already uses):

```c
static void on_treebrowser_refresh_signal(GObject *obj, gpointer data)
{
    treebrowser_browse_dir(addressbar_last_address);
}
```

Disconnect in `plugin_cleanup`:

```c
g_signal_handlers_disconnect_by_func(geany->object,
    G_CALLBACK(on_treebrowser_refresh_signal), NULL);
```

### Complete `geany-ctrl` usage from the agent

After Phase 5 the agent can run:

```sh
geany-ctrl save-all
# <agent does work, edits files on disk>
geany-ctrl refresh
geany-ctrl open-file /path/to/changed-file.c
geany-ctrl scroll-to-line /path/to/changed-file.c:17
```

---

## Build integration

After all phases, wire `geanycontrol` into the top-level build:

1. **`configure.ac`** — add `geanycontrol/Makefile` and
   `geanycontrol/src/Makefile` to `AC_CONFIG_FILES`, following the
   `geanyagent` entries as a template.
2. **Top-level `Makefile.am`** — add `geanycontrol` to `SUBDIRS`
   (alphabetical order, between `geanycli` and `geanyctags`).
3. Run `./autogen.sh` to regenerate.

---

## Implementation notes for Sonnet

- **Follow `geanycli.c`** for: `GSocketService` setup, signal registration on
  `geany->object`, and the `foreach_document` macro pattern.
- **Follow `geanyagent.c`** for: config dir path construction, plugin
  lifecycle (`gl_init` / `gl_cleanup` / `geany_load_module`), and
  `document_open_file` / `document_save_file` call sites.
- **Do not block the main thread**: socket I/O is async; for GTK mutations
  that cannot run in-line (e.g., menu walking during an I/O callback) use
  `g_idle_add`.
- **`foreach_document(i)`** iterates `documents` array with index `i`;
  `documents[i]` is a `GeanyDocument *`.  The macro is from `geanyplugin.h`.
- **`sci_goto_line(sci, line, TRUE)`** scrolls to `line` (0-based) and centres
  the view.  `doc->editor->sci` is the `ScintillaObject *`.
- **Socket path**: `g_build_filename(geany->app->configdir,
  "geanycontrol.sock", NULL)` expands to
  `~/.config/geany/geanycontrol.sock` on a standard install.
- **`g_unlink(socket_path)` at startup** avoids `EADDRINUSE` if the previous
  session crashed without cleanup.
- **`GSocketService` is already a `GSocketListener`**: call
  `g_socket_listener_add_address` directly on the cast listener — no separate
  `g_socket_listener_new` call needed.
- **`dispatch_command` receives a stripped line** (leading/trailing whitespace
  removed by `g_strstrip` at the call site) — no extra trimming inside each
  `cmd_*` function.
- **Error replies must end with `\n`** so `socat` / `read` in shell scripts
  gets a clean line.
- **`geany-ctrl` requires `socat`** at runtime.  The plugin itself has no
  dependency on `socat`; it is a convenience wrapper only.
- **treebrowser change is optional**: the `geanycontrol` plugin functions
  correctly without it; `geany-ctrl refresh` will reply `ok` (the signal
  fires) even if no handler is connected.

---

## Delivery sequence

```
Phase 1  →  Phase 2  →  Phase 3  →  Phase 4  →  Phase 5
socket      document     scroll /     menu        treebrowser
skeleton    ops          query        activate    refresh +
                                    + geany-ctrl  integration
```

Each phase leaves the plugin in a compilable, testable state.
