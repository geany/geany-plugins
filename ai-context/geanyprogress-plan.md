# geanyprogress — AI Agent Plan Progress Panel

## Implementation Plan

**Spec source:** `ai-prompts/progress-pannel.prompt.md`  
**Model:** Claude Sonnet 4.6 (one phase per context window)  
**Progress tracking:** `./ai-context/PROGRESS.md`  
**Architecture decision:** Option B — Unix domain socket + env var (see conversation context)

---

## Architecture Overview

A new Geany plugin `geanyprogress` (directory: `geany-plugins/geanyprogress/`) written in C.

It opens a **Unix domain socket** at `/tmp/geany-progress-<PID>.sock` and sets
`GEANY_PROGRESS_SOCK` in the Geany process environment so all child terminals (geanycli,
geanyagent) inherit the path.

An AI agent running inside a VTE terminal writes JSON to the socket to register a plan
and mark phases complete. The plugin renders the plan in a **sidebar panel** (left pane,
beside the treebrowser) using a `GtkTreeView`.

After every update the plugin also writes `.planning/state/<slug>.md` in the project
root for version-control persistence.

### Socket API (two message types)

```
# Register/replace the active plan
{"plan":"Plan Name","phases":["Phase 1","Phase 2","Phase 3"]}

# Mark a phase complete (1-based index)
{"phase":1,"status":"complete"}
```

### Agent helper script

```sh
geany-progress init "Plan Name" "Phase 1" "Phase 2" "Phase 3"
geany-progress done 1
geany-progress status
```

---

## Build system conventions (from existing plugins)

Every plugin follows the same skeleton:

```
geanyprogress/
  Makefile.am          # SUBDIRS = src; plugin = geanyprogress
  src/
    Makefile.am        # geanyplugins_LTLIBRARIES = geanyprogress.la
    geanyprogress.c
```

`geany-plugins/Makefile.am` gains:
```makefile
if ENABLE_GEANYPROGRESS
SUBDIRS += geanyprogress
endif
```

`configure.ac` gains `GP_CHECK_PLUGIN(geanyprogress)` (copy pattern from geanyenv entry).

---

## Phase 1 — Plugin Scaffold & Build System

**Goal:** Bare plugin compiles and loads into Geany. Sidebar tab is visible with static
placeholder text. No socket yet.

### Files to create

**`geanyprogress/Makefile.am`**
```makefile
include $(top_srcdir)/build/vars.auxfiles.mk

AUXFILES =

SUBDIRS = src
plugin = geanyprogress
```

**`geanyprogress/src/Makefile.am`**
```makefile
include $(top_srcdir)/build/vars.build.mk
plugin = geanyprogress

geanyplugins_LTLIBRARIES = geanyprogress.la

geanyprogress_la_SOURCES  = geanyprogress.c
geanyprogress_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
geanyprogress_la_CFLAGS   = $(AM_CFLAGS) $(GIO_CFLAGS)
geanyprogress_la_LIBADD   = $(COMMONLIBS) $(GIO_LIBS)

include $(top_srcdir)/build/cppcheck.mk
```

Note: `$(GIO_CFLAGS)` / `$(GIO_LIBS)` are already available from the top-level
`configure.ac` because treebrowser uses GIO — confirm with `grep GIO configure.ac`.
If not present, add `PKG_CHECK_MODULES([GIO], [gio-2.0])` to configure.ac.

**`geanyprogress/src/geanyprogress.c`** — minimal skeleton:
```c
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <geanyplugin.h>

GeanyPlugin  *geany_plugin;
GeanyData    *geany_data;

static GtkWidget *sidebar_vbox = NULL;
static gint       page_number  = -1;

static gboolean gp_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer data)
{
    geany_plugin = plugin;
    geany_data   = plugin->geany_data;

    sidebar_vbox = gtk_label_new("No active plan");
    gtk_widget_show(sidebar_vbox);

    GtkWidget *label = gtk_label_new("Progress");
    page_number = gtk_notebook_append_page(
        GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
        sidebar_vbox, label);

    return TRUE;
}

static void gp_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin,
                       G_GNUC_UNUSED gpointer data)
{
    if (page_number >= 0) {
        gtk_notebook_remove_page(
            GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
            page_number);
        sidebar_vbox = NULL;
        page_number  = -1;
    }
}

G_MODULE_EXPORT void geany_load_module(GeanyPlugin *plugin)
{
    plugin->info->name        = "Progress";
    plugin->info->description =
        "Shows AI agent plan progress in the sidebar. "
        "Receives JSON updates over a Unix socket ($GEANY_PROGRESS_SOCK).";
    plugin->info->version     = "0.1";
    plugin->info->author      = "teknopaul";

    plugin->funcs->init    = gp_init;
    plugin->funcs->cleanup = gp_cleanup;

    GEANY_PLUGIN_REGISTER(plugin, 235);
}
```

### Files to edit

**`geany-plugins/Makefile.am`** — add after the geanyenv block:
```makefile
if ENABLE_GEANYPROGRESS
SUBDIRS += geanyprogress
endif
```

**`geany-plugins/configure.ac`** — add `GP_CHECK_PLUGIN(geanyprogress)` after
the geanyenv entry. (The macro handles `--enable-geanyprogress` and the
`ENABLE_GEANYPROGRESS` conditional automatically.)

### Build & test

```sh
cd geany-plugins
./autogen.sh
./configure --enable-geanyprogress
make -C geanyprogress
# install .la into geany's plugin dir then restart geany
```

Verify: "Progress" tab appears in the left sidebar.

### Phase 1 done — write to PROGRESS.md

```markdown
| 1 | Plugin scaffold & build system | complete |
```

---

## Phase 2 — Unix Domain Socket Server

**Goal:** Plugin creates a socket on init, sets `GEANY_PROGRESS_SOCK`, accepts
connections, and prints received JSON to stderr. No parsing yet. UI unchanged.

### Key GIO types

Use `GSocketService` (GLib's high-level async socket API, GTK-thread-safe):

```c
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
```

### What to add to `geanyprogress.c`

**New module globals:**
```c
static GSocketService *sock_service  = NULL;
static gchar          *sock_path     = NULL;
```

**Socket init function** (called from `gp_init`):
```c
static gboolean socket_init(void)
{
    sock_path = g_strdup_printf("/tmp/geany-progress-%d.sock", (int)getpid());

    GSocketAddress *addr = g_unix_socket_address_new(sock_path);
    sock_service = g_socket_service_new();

    GError *err = NULL;
    if (!g_socket_listener_add_address(G_SOCKET_LISTENER(sock_service),
                                       addr, G_SOCKET_TYPE_STREAM,
                                       G_SOCKET_PROTOCOL_DEFAULT,
                                       NULL, NULL, &err)) {
        g_warning("geanyprogress: socket bind failed: %s", err->message);
        g_error_free(err);
        g_object_unref(addr);
        g_object_unref(sock_service);
        sock_service = NULL;
        g_free(sock_path);
        sock_path = NULL;
        return FALSE;
    }
    g_object_unref(addr);

    g_signal_connect(sock_service, "incoming",
                     G_CALLBACK(on_incoming_connection), NULL);
    g_socket_service_start(sock_service);

    /* Advertise to child processes */
    g_setenv("GEANY_PROGRESS_SOCK", sock_path, TRUE);
    return TRUE;
}
```

**Socket cleanup** (called from `gp_cleanup`):
```c
static void socket_cleanup(void)
{
    if (sock_service) {
        g_socket_service_stop(sock_service);
        g_object_unref(sock_service);
        sock_service = NULL;
    }
    if (sock_path) {
        g_unlink(sock_path);
        g_unsetenv("GEANY_PROGRESS_SOCK");
        g_free(sock_path);
        sock_path = NULL;
    }
}
```

**Incoming connection handler:**
```c
static gboolean on_incoming_connection(G_GNUC_UNUSED GSocketService *service,
                                       GSocketConnection *connection,
                                       G_GNUC_UNUSED GObject *source,
                                       G_GNUC_UNUSED gpointer data)
{
    GInputStream *in = g_io_stream_get_input_stream(G_IO_STREAM(connection));
    gchar buf[4096];
    gssize n = g_input_stream_read(in, buf, sizeof(buf) - 1, NULL, NULL);
    if (n > 0) {
        buf[n] = '\0';
        g_printerr("geanyprogress: received: %s\n", buf);
        /* Phase 3 will parse buf here */
    }
    return TRUE;
}
```

Note: `GSocketService` callbacks run on the GLib main loop — they are GTK-safe without
any extra locking. This is the key reason to use `GSocketService` over raw POSIX sockets
in a separate thread.

### Build & test

```sh
make -C geanyprogress && # install + restart geany
echo 'hello' | nc -U "$GEANY_PROGRESS_SOCK"
# Check Geany's terminal / stderr for: geanyprogress: received: hello
```

Also verify `echo $GEANY_PROGRESS_SOCK` inside a geanycli or geanyagent terminal shows
the socket path (child process env inheritance).

### Phase 2 done — update PROGRESS.md

```markdown
| 2 | Unix domain socket server | complete |
```

---

## Phase 3 — JSON Parsing + In-Memory Model

**Goal:** Parse both message types into a `ProgressPlan` struct. Log parsed content to
stderr. No UI update yet.

### Model structs

```c
#define MAX_PHASES 32

typedef struct {
    gchar    *name;
    gboolean  done;
} Phase;

typedef struct {
    gchar   *plan_name;
    Phase    phases[MAX_PHASES];
    gint     n_phases;
} ProgressPlan;

static ProgressPlan current_plan = {0};
```

### Hand-rolled JSON parser strategy

The two message formats are simple enough for a minimal hand-parser.
No external library needed. Key helpers needed:

```c
/* Extract string value for a key in a flat JSON object.
 * Returns newly allocated string or NULL. Non-recursive — only top-level keys. */
static gchar *json_get_string(const gchar *json, const gchar *key);

/* Extract integer value for a key. Returns -1 if not found. */
static gint json_get_int(const gchar *json, const gchar *key);

/* Extract JSON array of strings for a key.
 * Fills out[] (caller-allocated) up to max entries. Returns count. */
static gint json_get_string_array(const gchar *json, const gchar *key,
                                  gchar **out, gint max);
```

Implementation approach for `json_get_string`: find `"key"`, skip `:`, skip whitespace,
read the quoted value respecting `\"` escapes. Use `g_strstr_len` and pointer arithmetic.

### Dispatch function

```c
static void plan_free(void)
{
    g_free(current_plan.plan_name);
    for (gint i = 0; i < current_plan.n_phases; i++)
        g_free(current_plan.phases[i].name);
    memset(&current_plan, 0, sizeof(current_plan));
}

static void handle_message(const gchar *json)
{
    /* Detect message type by key presence */
    if (strstr(json, "\"phases\"")) {
        /* Init plan */
        plan_free();
        current_plan.plan_name = json_get_string(json, "plan");
        gchar *phase_names[MAX_PHASES] = {0};
        current_plan.n_phases = json_get_string_array(json, "phases",
                                                       phase_names, MAX_PHASES);
        for (gint i = 0; i < current_plan.n_phases; i++) {
            current_plan.phases[i].name = phase_names[i];
            current_plan.phases[i].done = FALSE;
        }
    } else if (strstr(json, "\"phase\"")) {
        /* Update phase */
        gint idx = json_get_int(json, "phase") - 1; /* 1-based → 0-based */
        if (idx >= 0 && idx < current_plan.n_phases)
            current_plan.phases[idx].done = TRUE;
    }

    /* Debug: log parsed state */
    g_printerr("geanyprogress: plan='%s' phases=%d\n",
               current_plan.plan_name ? current_plan.plan_name : "(none)",
               current_plan.n_phases);
}
```

Call `handle_message(buf)` from `on_incoming_connection` instead of the printerr stub.

### Build & test

```sh
echo '{"plan":"My Plan","phases":["Phase 1","Phase 2","Phase 3"]}' \
  | nc -U "$GEANY_PROGRESS_SOCK"
# expect: geanyprogress: plan='My Plan' phases=3

echo '{"phase":1,"status":"complete"}' | nc -U "$GEANY_PROGRESS_SOCK"
# expect: geanyprogress: plan='My Plan' phases=3  (phase[0].done == TRUE)
```

### Phase 3 done — update PROGRESS.md

```markdown
| 3 | JSON parsing + in-memory model | complete |
```

---

## Phase 4 — GTK Panel UI

**Goal:** Replace the placeholder label with a `GtkTreeView` that shows the active plan
name and numbered phases with checkmarks. Refreshes live when socket messages arrive.

### Tree model columns

```c
enum {
    COL_NUM    = 0,  /* gchar * — "1", "2", ... or "" for header row */
    COL_NAME   = 1,  /* gchar * — phase name or plan name */
    COL_STATUS = 2,  /* gchar * — "✓" or "○" or "" */
    N_COLS
};

static GtkListStore *list_store = NULL;
static GtkWidget    *tree_view  = NULL;
```

### UI construction (replaces the `gtk_label_new` stub in `gp_init`)

```c
static GtkWidget *build_panel(void)
{
    list_store = gtk_list_store_new(N_COLS,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING);
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);

    GtkCellRenderer *r;
    GtkTreeViewColumn *col;

    r   = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("#", r, "text", COL_NUM, NULL);
    gtk_tree_view_column_set_min_width(col, 24);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);

    r   = gtk_cell_renderer_text_new();
    g_object_set(r, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    col = gtk_tree_view_column_new_with_attributes("Phase", r, "text", COL_NAME, NULL);
    gtk_tree_view_column_set_expand(col, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);

    r   = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("", r, "text", COL_STATUS, NULL);
    gtk_tree_view_column_set_min_width(col, 24);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), tree_view);
    gtk_widget_show_all(scroll);
    return scroll;
}
```

### UI refresh function (called after every `handle_message`)

```c
static void ui_refresh(void)
{
    if (!list_store)
        return;

    gtk_list_store_clear(list_store);
    GtkTreeIter iter;

    /* Header row — plan name */
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter,
                       COL_NUM, "",
                       COL_NAME, current_plan.plan_name ? current_plan.plan_name
                                                        : "No active plan",
                       COL_STATUS, "",
                       -1);

    for (gint i = 0; i < current_plan.n_phases; i++) {
        gchar *num = g_strdup_printf("%d", i + 1);
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter,
                           COL_NUM,    num,
                           COL_NAME,   current_plan.phases[i].name,
                           COL_STATUS, current_plan.phases[i].done ? "✓" : "○",
                           -1);
        g_free(num);
    }
}
```

**Thread safety note:** `on_incoming_connection` runs on the GLib main loop (not a
separate thread) because `GSocketService` uses the default main context. Therefore
calling GTK functions directly from the handler is safe — no `g_idle_add()` needed.

Call `ui_refresh()` at the end of `handle_message()`.

### Build & test

Load the plugin. Send an init message. Verify the sidebar panel shows the plan name and
phase list with "○" icons. Send a done message. Verify the corresponding row shows "✓".

```sh
echo '{"plan":"geanyprogress","phases":["Scaffold","Socket","JSON","UI","Persist","Script"]}' \
  | nc -U "$GEANY_PROGRESS_SOCK"
echo '{"phase":1,"status":"complete"}' | nc -U "$GEANY_PROGRESS_SOCK"
echo '{"phase":2,"status":"complete"}' | nc -U "$GEANY_PROGRESS_SOCK"
```

### Phase 4 done — update PROGRESS.md

```markdown
| 4 | GTK panel UI | complete |
```

---

## Phase 5 — Persistence (.planning/state/xxx.md)

**Goal:** After every model update, write a markdown progress file to the project root
under `.planning/state/<slug>.md`. Matches the format of the existing `PROGRESS.md`.

### Slug derivation

```c
/* Convert plan name to filesystem-safe slug: lowercase, spaces→dashes, strip rest */
static gchar *make_slug(const gchar *name)
{
    GString *s = g_string_new(NULL);
    for (const gchar *p = name; *p; p++) {
        if (g_ascii_isalnum(*p))
            g_string_append_c(s, g_ascii_tolower(*p));
        else if (*p == ' ' || *p == '-' || *p == '_')
            g_string_append_c(s, '-');
    }
    return g_string_free(s, FALSE);
}
```

### Write function

```c
static void persist_plan(void)
{
    GeanyApp *app = geany->app;
    if (!app->project || !app->project->base_path)
        return;
    if (!current_plan.plan_name)
        return;

    gchar *state_dir = g_build_filename(app->project->base_path,
                                        ".planning", "state", NULL);
    g_mkdir_with_parents(state_dir, 0755);

    gchar *slug = make_slug(current_plan.plan_name);
    gchar *path = g_build_filename(state_dir, slug, NULL);
    g_free(slug);

    /* Append .md if not already present */
    gchar *md_path = g_strconcat(path, ".md", NULL);
    g_free(path);

    GString *content = g_string_new(NULL);
    g_string_append_printf(content, "# %s — Progress\n\n", current_plan.plan_name);
    g_string_append(content, "| Phase | Description | Status |\n");
    g_string_append(content, "|-------|-------------|--------|\n");

    for (gint i = 0; i < current_plan.n_phases; i++) {
        g_string_append_printf(content, "| %d | %s | %s |\n",
                               i + 1,
                               current_plan.phases[i].name,
                               current_plan.phases[i].done ? "complete" : "pending");
    }

    GError *err = NULL;
    if (!g_file_set_contents(md_path, content->str, -1, &err)) {
        g_warning("geanyprogress: write failed: %s", err->message);
        g_error_free(err);
    }

    g_string_free(content, TRUE);
    g_free(md_path);
    g_free(state_dir);
}
```

Call `persist_plan()` at the end of `handle_message()`, after `ui_refresh()`.

### Build & test

```sh
echo '{"plan":"My Test Plan","phases":["Step A","Step B"]}' \
  | nc -U "$GEANY_PROGRESS_SOCK"
cat .planning/state/my-test-plan.md
# Should show markdown table with both phases pending

echo '{"phase":1,"status":"complete"}' | nc -U "$GEANY_PROGRESS_SOCK"
cat .planning/state/my-test-plan.md
# Step A should now show "complete"
```

### Phase 5 done — update PROGRESS.md

```markdown
| 5 | Persistence (.planning/state/) | complete |
```

---

## Phase 6 — geanyenv Integration + Helper Script

**Goal:** Emit `geanyagent-restart` so the agent VTE picks up `GEANY_PROGRESS_SOCK`
without a manual restart. Ship a `geany-progress` shell helper for clean agent calls.

### geanyagent-restart signal

In `socket_init()`, after `g_setenv(...)`, add:

```c
/* Restart agent terminal so it inherits the new env var.
 * Pattern taken from geanyenv.c:env_load(). */
GType obj_type = G_OBJECT_TYPE(geany->object);
if (!g_signal_lookup("geanyagent-restart", obj_type))
    g_signal_new("geanyagent-restart", obj_type, G_SIGNAL_RUN_LAST,
                 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
if (g_signal_lookup("geanyagent-restart", obj_type))
    g_signal_emit_by_name(geany->object, "geanyagent-restart");
```

This mirrors the pattern in `geanyenv.c:146-147` exactly.

### Helper script: `geany-progress`

Create `geanyprogress/geany-progress` (installed to `$(bindir)` or kept as a project
script that users copy to a location on `$PATH`):

```sh
#!/bin/sh
# geany-progress — Send plan progress updates to the geanyprogress Geany plugin
#
# Usage:
#   geany-progress init "Plan Name" "Phase 1" "Phase 2" ...
#   geany-progress done N
#   geany-progress status
#
# Requires: nc (netcat) or socat; GEANY_PROGRESS_SOCK must be set.

set -e

SOCK="${GEANY_PROGRESS_SOCK:-}"

if [ -z "$SOCK" ]; then
    echo "geany-progress: GEANY_PROGRESS_SOCK is not set" >&2
    exit 1
fi

_send() {
    if command -v nc >/dev/null 2>&1; then
        printf '%s' "$1" | nc -U "$SOCK"
    elif command -v socat >/dev/null 2>&1; then
        printf '%s' "$1" | socat - "UNIX-CONNECT:$SOCK"
    else
        echo "geany-progress: need nc or socat" >&2
        exit 1
    fi
}

_json_str() {
    # Minimal JSON string escape: backslash and double-quote only
    printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

cmd="$1"
shift || true

case "$cmd" in
  init)
    plan_name="$1"; shift
    json="{\"plan\":\"$(_json_str "$plan_name")\",\"phases\":["
    sep=""
    for phase in "$@"; do
        json="${json}${sep}\"$(_json_str "$phase")\""
        sep=","
    done
    json="${json}]}"
    _send "$json"
    ;;
  done)
    n="$1"
    _send "{\"phase\":${n},\"status\":\"complete\"}"
    ;;
  status)
    echo "GEANY_PROGRESS_SOCK=$SOCK"
    ls -la "$SOCK" 2>/dev/null || echo "(socket not found)"
    ;;
  *)
    echo "Usage: geany-progress init <plan> [phases...]" >&2
    echo "       geany-progress done N" >&2
    echo "       geany-progress status" >&2
    exit 1
    ;;
esac
```

Make it executable: `chmod +x geanyprogress/geany-progress`.

To make it available in VTE terminals without a full install, the agent can set
`PATH=$PATH:/path/to/geany-plugins/geanyprogress` in `geany.env`.

### Build & test

Reload the plugin. Open a geanycli terminal. Verify `GEANY_PROGRESS_SOCK` is set.
Run the script end-to-end:

```sh
geany-progress init "geanyprogress" \
    "Scaffold" "Socket" "JSON" "UI" "Persist" "Script"
geany-progress done 1
geany-progress done 2
geany-progress done 3
geany-progress done 4
geany-progress done 5
geany-progress done 6
```

Verify: sidebar updates in real time; `.planning/state/geanyprogress.md` is written.

### Phase 6 done — update PROGRESS.md

```markdown
| 6 | geanyenv integration + helper script | complete |
```

---

## Summary of Phases

| Phase | Description                              | Key output                          |
|-------|------------------------------------------|-------------------------------------|
| 1     | Plugin scaffold & build system           | Sidebar "Progress" tab loads        |
| 2     | Unix domain socket server                | `GEANY_PROGRESS_SOCK` set; nc works |
| 3     | JSON parsing + in-memory model           | Both message types parsed correctly |
| 4     | GTK panel UI                             | Live-updating GtkTreeView           |
| 5     | Persistence (.planning/state/)           | Markdown file written on update     |
| 6     | geanyenv integration + helper script     | `geany-progress` script works       |

## Resuming after /clear

Start a new session with:
> "Continue geanyprogress Phase N per ai-context/geanyprogress-plan.md.
>  Current progress is in ai-context/PROGRESS.md."
