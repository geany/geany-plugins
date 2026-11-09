# Phased Delivery Plan: `locate` Plugin

Target model: Claude Sonnet  
Source requirement: `ai-prompts/find-files.prompt.md`  
Reference code: `geanyagent/src/geanyagent.c`, `geanycli/src/geanycli.c`, `projectorganizer/src/prjorg-sidebar.c`

---

## Overview

A new Geany plugin `locate` adds a **"Find File"** menu item backed by the
OS `locate` command.  Searches are scoped to the open project root.  Results
follow three distinct UX paths: zero (keep dialog open), one (in-dialog open),
many (clickable links in the Messages tab).  A companion **"Update Database"**
menu item runs `sudo updatedb` in the background.

---

## Phase 1 — Project skeleton and menu items

**Goal:** compilable plugin with two menu items that stub-print to the message
window; no dialog yet.

### Files to create

```
locate/
  Makefile.am        — mirrors geanyagent/Makefile.am
  src/
    Makefile.am      — mirrors geanyagent/src/Makefile.am (no VTE deps)
    locate.c         — main plugin file
```

### Makefile.am (top level)

```makefile
include $(top_srcdir)/build/vars.auxfiles.mk
AUXFILES =
SUBDIRS = src
plugin = locate
```

### src/Makefile.am

```makefile
include $(top_srcdir)/build/vars.build.mk
plugin = locate

geanyplugins_LTLIBRARIES = locate.la

locate_la_SOURCES  = locate.c
locate_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
locate_la_CFLAGS   = $(AM_CFLAGS)
locate_la_LIBADD   = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
```

### Plugin structure

```c
GeanyPlugin *geany_plugin;
GeanyData   *geany_data;

static GtkWidget *menu_item_find   = NULL;
static GtkWidget *menu_item_update = NULL;

static void on_find_file_activate(GtkMenuItem *item, gpointer data);
static void on_updatedb_activate(GtkMenuItem *item, gpointer data);
```

### Menu registration (in `gl_init`)

```c
menu_item_find = gtk_menu_item_new_with_label("Find File");
gtk_widget_show(menu_item_find);
gtk_menu_shell_append(GTK_MENU_SHELL(geany_data->main_widgets->tools_menu), menu_item_find);
g_signal_connect(menu_item_find, "activate", G_CALLBACK(on_find_file_activate), NULL);

menu_item_update = gtk_menu_item_new_with_label("updatedb");
gtk_widget_show(menu_item_update);
gtk_menu_shell_append(GTK_MENU_SHELL(geany_data->main_widgets->tools_menu menu_item_update);
g_signal_connect(menu_item_update, "activate", G_CALLBACK(on_updatedb_activate), NULL);
```

### Stub handlers

`on_find_file_activate` — calls `msgwin_msg_add(COLOR_BLUE, -1, NULL, "locate: TODO")`.  
`on_updatedb_activate` — calls `msgwin_msg_add(COLOR_BLUE, -1, NULL, "updatedb: TODO")`.

### Cleanup (in `gl_cleanup`)

```c
if (menu_item_find)   { gtk_widget_destroy(menu_item_find);   menu_item_find   = NULL; }
if (menu_item_update) { gtk_widget_destroy(menu_item_update); menu_item_update = NULL; }
```

### `geany_load_module`

```c
plugin->info->name        = "Locate";
plugin->info->description = "Find file by name using the OS locate index.";
plugin->info->version     = "0.1";
plugin->info->author      = "teknopaul";
plugin->funcs->init       = gl_init;
plugin->funcs->cleanup    = gl_cleanup;
GEANY_PLUGIN_REGISTER(plugin, 235);
```

### Deliverable

`make` compiles; two menu items appear in Tools menu; clicking either prints to
the message window.

---

## Phase 2 — Find File dialog

**Goal:** functional dialog with search entry, case toggle, status label, and
result pane that correctly implements all three result-count behaviours.

### Dialog layout

```
┌─────────────────────────────────────────────┐
│  Find File                              [×]  │
│─────────────────────────────────────────────│
│  Search: [__________________________]        │
│          [✓] Case sensitive                  │
│─────────────────────────────────────────────│
│  <status label>                             │
│  ┌─────────────────────────────────────────┐│
│  │ result path (only for single-result)    ││
│  └─────────────────────────────────────────┘│
│─────────────────────────────────────────────│
│              [Cancel]  [Find]               │
└─────────────────────────────────────────────┘
```

### Key widget refs (module-level statics, reset to NULL on destroy)

```c
static GtkWidget *locate_dialog    = NULL;
static GtkWidget *entry_query      = NULL;
static GtkWidget *check_case       = NULL;
static GtkWidget *label_status     = NULL;
static GtkWidget *label_result     = NULL;  /* single-result filename */
static gchar     *single_result    = NULL;  /* full path, heap-alloc */
```

### Dialog construction helper `locate_dialog_show()`

```c
static void locate_dialog_show(void)
{
    if (locate_dialog) {
        gtk_window_present(GTK_WINDOW(locate_dialog));
        return;
    }

    locate_dialog = gtk_dialog_new_with_buttons(
        "Find File",
        GTK_WINDOW(geany_data->main_widgets->window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Find",   GTK_RESPONSE_OK,
        NULL);

    gtk_dialog_set_default_response(GTK_DIALOG(locate_dialog), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(locate_dialog), 480, -1);

    GtkWidget *ca = gtk_dialog_get_content_area(GTK_DIALOG(locate_dialog));
    gtk_container_set_border_width(GTK_CONTAINER(ca), 8);
    gtk_box_set_spacing(GTK_BOX(ca), 6);

    /* Search row */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *lbl  = gtk_label_new("Search:");
    entry_query     = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry_query), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), lbl,         FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), entry_query,  TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(ca),   hbox,          FALSE, FALSE, 0);

    /* Case toggle */
    check_case = gtk_check_button_new_with_label("Case sensitive");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_case), FALSE);
    gtk_box_pack_start(GTK_BOX(ca), check_case, FALSE, FALSE, 0);

    /* Status label */
    label_status = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label_status), 0.0);
    gtk_box_pack_start(GTK_BOX(ca), label_status, FALSE, FALSE, 4);

    /* Single-result label (hidden until needed) */
    label_result = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label_result), 0.0);
    gtk_label_set_selectable(GTK_LABEL(label_result), TRUE);
    gtk_widget_set_no_show_all(label_result, TRUE);
    gtk_box_pack_start(GTK_BOX(ca), label_result, FALSE, FALSE, 0);

    gtk_widget_show_all(ca);
    gtk_widget_grab_focus(entry_query);

    g_signal_connect(locate_dialog, "response",
                     G_CALLBACK(on_dialog_response), NULL);
    g_signal_connect(locate_dialog, "destroy",
                     G_CALLBACK(on_dialog_destroy), NULL);

    gtk_widget_show(locate_dialog);
}
```

### Response handler `on_dialog_response`

```c
static void on_dialog_response(GtkDialog *dlg, gint response, gpointer data)
{
    if (response == GTK_RESPONSE_OK)
    {
        locate_run_search();   /* Phase 3 */
        /* Do NOT destroy here — Phase 3 will close or keep open */
        return;
    }
    gtk_widget_destroy(GTK_WIDGET(dlg));
}
```

### Destroy handler `on_dialog_destroy`

```c
static void on_dialog_destroy(GtkWidget *w, gpointer data)
{
    locate_dialog = entry_query = check_case = label_status = label_result = NULL;
    g_free(single_result);
    single_result = NULL;
}
```

### Three result modes (stubs for Phase 3 to fill in)

```c
static void result_none(const gchar *query)
{
    gchar *msg = g_strdup_printf("No results for "%s"", query);
    gtk_label_set_text(GTK_LABEL(label_status), msg);
    g_free(msg);
    gtk_widget_hide(label_result);
    /* dialog stays open — user can refine the query */
}

static void result_one(const gchar *path)
{
    gtk_label_set_text(GTK_LABEL(label_status), "Press Enter to open:");
    gtk_label_set_text(GTK_LABEL(label_result), path);
    gtk_widget_show(label_result);
    g_free(single_result);
    single_result = g_strdup(path);
    /* pressing Enter (default response) will now open the file */
}

static void result_many(GPtrArray *paths)
{
    gchar *msg = g_strdup_printf("%u results — see Messages tab", paths->len);
    gtk_label_set_text(GTK_LABEL(label_status), msg);
    g_free(msg);
    gtk_widget_hide(label_result);
    locate_populate_messages(paths);   /* Phase 4 */
    gtk_widget_destroy(locate_dialog); /* close dialog — Messages tab is the UX */
}
```

### "Find" default response: open single file

When `on_dialog_response` sees `GTK_RESPONSE_OK` and `single_result` is set
(the single-result mode), call `document_open_file(single_result, FALSE, NULL, NULL)`
and destroy the dialog.  Otherwise call `locate_run_search()`.

### `on_find_file_activate` update

Replace the `msgwin_msg_add` stub with `locate_dialog_show()`.

### Deliverable

Dialog opens; query and case checkbox work; status label updates; single-result
label shows or hides correctly; Enter on single result opens the file.
(locate is not yet called; use hard-coded test data in stubs.)

---

## Phase 3 — Async `locate` execution

**Goal:** replace hard-coded stubs with a real `locate` invocation that runs
asynchronously and never blocks the GTK main loop.

### Helper: get project root

```c
static gchar *get_project_root(void)
{
    GeanyApp *app = geany->app;
    if (app->project && app->project->base_path && *app->project->base_path)
        return g_strdup(app->project->base_path);
    return g_strdup(g_get_home_dir());
}
```

### Command construction

```c
static gchar **build_locate_argv(const gchar *query,
                                  gboolean case_sensitive,
                                  const gchar *root)
{
    GPtrArray *argv = g_ptr_array_new();
    g_ptr_array_add(argv, g_strdup("locate"));
    if (!case_sensitive)
        g_ptr_array_add(argv, g_strdup("-i"));
    /* Restrict output to files under the project root using a regex anchor */
    gchar *pattern = g_strdup_printf("^%s.*%s", root, query);
    g_ptr_array_add(argv, g_strdup("-r"));
    g_ptr_array_add(argv, pattern);
    g_ptr_array_add(argv, NULL);
    return (gchar **)g_ptr_array_free(argv, FALSE);
}
```

### Async state struct

```c
typedef struct {
    GSubprocess *proc;
    GInputStream *stdout_stream;
    GDataInputStream *reader;
    GPtrArray *results;    /* collected file paths (heap-alloc gchar*) */
    gchar *query;          /* for the "no results" label */
    gchar *root;           /* for filtering / display */
} LocateAsync;

static LocateAsync *current_search = NULL;
static GCancellable *search_cancel = NULL;
```

### `locate_run_search()`

```c
static void locate_run_search(void)
{
    if (!locate_dialog) return;

    const gchar *query = gtk_entry_get_text(GTK_ENTRY(entry_query));
    if (!query || !*query) return;

    gboolean case_sens = gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(check_case));

    /* Cancel any in-flight search */
    if (search_cancel)
    {
        g_cancellable_cancel(search_cancel);
        g_object_unref(search_cancel);
    }
    search_cancel = g_cancellable_new();

    gchar *root = get_project_root();
    gchar **argv = build_locate_argv(query, case_sens, root);

    GError *err = NULL;
    GSubprocess *proc = g_subprocess_newv(
        (const gchar * const *)argv,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
        &err);
    g_strfreev(argv);

    if (!proc)
    {
        gtk_label_set_text(GTK_LABEL(label_status),
                           err ? err->message : "locate failed");
        g_clear_error(&err);
        g_free(root);
        return;
    }

    gtk_label_set_text(GTK_LABEL(label_status), "Searching…");

    LocateAsync *la = g_new0(LocateAsync, 1);
    la->proc          = proc;
    la->stdout_stream = g_subprocess_get_stdout_pipe(proc);
    la->reader        = g_data_input_stream_new(la->stdout_stream);
    la->results       = g_ptr_array_new_with_free_func(g_free);
    la->query         = g_strdup(query);
    la->root          = root;

    current_search = la;

    /* Read all lines asynchronously; on_read_line_done is called per line */
    g_data_input_stream_read_line_async(la->reader, G_PRIORITY_DEFAULT,
                                        search_cancel,
                                        on_read_line_done, la);
}
```

### Line-by-line async reader

```c
static void on_read_line_done(GObject *src, GAsyncResult *res, gpointer data)
{
    LocateAsync *la = data;
    GError *err = NULL;
    gchar *line = g_data_input_stream_read_line_finish(
        G_DATA_INPUT_STREAM(src), res, NULL, &err);

    if (err) { g_error_free(err); locate_async_finish(la); return; }
    if (!line) { locate_async_finish(la); return; }   /* EOF */

    g_ptr_array_add(la->results, line);   /* line is heap-alloc'd by GLib */

    g_data_input_stream_read_line_async(la->reader, G_PRIORITY_DEFAULT,
                                        search_cancel,
                                        on_read_line_done, la);
}
```

### Finish and dispatch

```c
static void locate_async_finish(LocateAsync *la)
{
    guint n = la->results->len;

    if (n == 0)
        result_none(la->query);
    else if (n == 1)
        result_one((const gchar *)la->results->pdata[0]);
    else
        result_many(la->results);   /* ownership of array transferred */

    g_object_unref(la->reader);
    g_object_unref(la->proc);
    g_ptr_array_unref(la->results);   /* freed only if not transferred */
    g_free(la->query);
    g_free(la->root);
    g_free(la);

    if (current_search == la) current_search = NULL;
}
```

In `result_many`, transfer ownership of `la->results` to `locate_populate_messages`
and set `la->results = NULL` before calling `locate_async_finish` so the
`g_ptr_array_unref` in the finish function is a no-op.

### Cancellation in `gl_cleanup`

```c
if (search_cancel) {
    g_cancellable_cancel(search_cancel);
    g_clear_object(&search_cancel);
}
if (locate_dialog) {
    gtk_widget_destroy(locate_dialog);
    locate_dialog = NULL;
}
```

### Deliverable

`locate` runs on each "Find" click; results dispatch to the correct mode;
zero results keeps dialog open; one result shows path + Enter opens file;
many results stub (still hard-coded) closes dialog.

---

## Phase 4 — Multiple results: Messages tab clickable links

**Goal:** when `locate` returns more than one match, populate the Messages tab
with clickable file links and switch to it.

### `locate_populate_messages(GPtrArray *paths)`

```c
static void locate_populate_messages(GPtrArray *paths)
{
    gchar *root = get_project_root();
    gchar *root_locale = utils_get_locale_from_utf8(root);

    msgwin_clear_tab(MSG_MESSAGE);
    msgwin_set_messages_dir(root_locale);

    for (guint i = 0; i < paths->len; i++)
    {
        const gchar *path = (const gchar *)paths->pdata[i];
        /* Make path relative to project root for a cleaner display */
        const gchar *rel = path;
        if (g_str_has_prefix(path, root))
            rel = path + strlen(root);
        while (*rel == G_DIR_SEPARATOR)
            rel++;
        msgwin_msg_add(COLOR_BLACK, -1, NULL, "%s", rel);
    }

    msgwin_switch_tab(MSG_MESSAGE, TRUE);

    g_free(root_locale);
    g_free(root);
}
```

**Why relative paths work:** `msgwin_set_messages_dir` sets the base for
resolving relative paths when the user clicks a line in the Messages tab, so
Geany knows the full absolute path to open.

### Deliverable

Multiple results appear as clickable lines in the Messages tab; clicking any
opens the file.  Dialog is closed.

---

## Phase 5 — Update Database menu item + double-shift keybinding

**Goal:** `sudo updatedb` runs in the background; double-tap shift opens the
Find File dialog.

### `on_updatedb_activate`

Prefer geanycli IPC if available; fall back to `g_spawn_command_line_async`:

```c
static void on_updatedb_activate(GtkMenuItem *item, gpointer data)
{
    const gchar *cmd = "sudo updatedb";
    /* Try geanycli IPC first */
    guint n = 0;
    g_signal_query(g_signal_lookup("geanycli-run-command", G_TYPE_OBJECT), NULL);
    /* simpler: just emit and check */
    g_signal_emit_by_name(geany->object, "geanycli-run-command", cmd, TRUE);
    /* If geanycli is not loaded the signal has no effect — fall back */
    if (n == 0)
        g_spawn_command_line_async(cmd, NULL);
}
```

The correct pattern (mirrors geanyservers.c):

```c
static void on_updatedb_activate(GtkMenuItem *item, gpointer data)
{
    const gchar *cmd = "sudo updatedb";
    gulong id = g_signal_lookup("geanycli-run-command",
                                G_OBJECT_TYPE(geany->object));
    if (id != 0 && g_signal_has_handler_pending(geany->object, id, 0, FALSE))
        g_signal_emit_by_name(geany->object, "geanycli-run-command", cmd, TRUE);
    else
        g_spawn_command_line_async(cmd, NULL);
}
```

### Double-shift keybinding

GTK keybindings do not natively support double-tap modifiers.  The approach is:
connect a `key-press-event` handler to the Geany main window that tracks the
time of the last Shift key-press; if a second Shift arrives within a threshold
(300 ms), open the dialog.

```c
#define DOUBLE_SHIFT_MS 300

static guint32 last_shift_time = 0;

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R)
    {
        guint32 now = event->time;
        if (last_shift_time != 0 && (now - last_shift_time) < DOUBLE_SHIFT_MS)
        {
            last_shift_time = 0;   /* reset so triple-tap does not re-fire */
            locate_dialog_show();
            return TRUE;           /* consume event */
        }
        last_shift_time = now;
    }
    else
    {
        last_shift_time = 0;   /* any other key resets the double-tap window */
    }
    return FALSE;
}
```

Connect in `gl_init`:

```c
g_signal_connect(geany_data->main_widgets->window, "key-press-event",
                 G_CALLBACK(on_key_press), NULL);
```

Disconnect in `gl_cleanup`:

```c
g_signal_handlers_disconnect_by_func(geany_data->main_widgets->window,
                                     G_CALLBACK(on_key_press), NULL);
```

### Standard keybinding (fallback / configurable)

Register a keybinding so users can also map it in Geany preferences:

```c
static GeanyKeyGroup *key_group = NULL;
enum { KB_FIND_FILE, KB_COUNT };

static void kb_find_file(G_GNUC_UNUSED guint key_id)
{
    locate_dialog_show();
}
```

In `gl_init`:

```c
key_group = plugin_set_key_group(geany_plugin, "locate", KB_COUNT, NULL);
keybindings_set_item(key_group, KB_FIND_FILE, kb_find_file,
                     0, 0, "find_file", "Find File", menu_item_find);
```

No default key is assigned (pass `0, 0`) — the double-shift handler is the
primary activation path.

### Deliverable

- Double-tap Shift opens the Find File dialog from anywhere in Geany.
- Any other key press during the gap resets the timer so normal shift-modified
  shortcuts are not accidentally intercepted.
- "Update Database" runs `sudo updatedb` (in the CLI tab if geanycli is loaded,
  otherwise in the background).

---

## Build integration

After all phases, wire `locate` into the top-level build:

1. **`configure.ac`** — add `locate/Makefile` and `locate/src/Makefile`
   to `AC_CONFIG_FILES` following the `geanyagent` entries as a template.
2. **Top-level `Makefile.am`** — add `locate` to `SUBDIRS` (same position
   as other single-file plugins alphabetically).
3. Run `./autogen.sh` to regenerate.

---

## Implementation notes for Sonnet

- **Follow `geanyagent.c`** for: config dir construction (`g_build_filename`),
  plugin lifecycle (`gl_init` / `gl_cleanup` / `geany_load_module`), and menu
  item append to `tools_menu`.
- **Follow `projectorganizer/src/prjorg-sidebar.c`** for the
  `msgwin_clear_tab` / `msgwin_set_messages_dir` / `msgwin_msg_add` /
  `msgwin_switch_tab(MSG_MESSAGE, TRUE)` pattern that produces clickable links.
- **Do not use `g_spawn_sync`** or any blocking subprocess call; always use
  `GSubprocess` with async read (`g_data_input_stream_read_line_async`).
- All GTK widget access must be on the main thread.  Async callbacks from
  `GSubprocess` already run on the GLib default main context (same thread).
- Guard every widget pointer with a NULL check before use — the dialog may have
  been destroyed while a search was in flight.
- `utils_get_locale_from_utf8` is available from `geanyplugin.h`; use it to
  convert UTF-8 paths to locale before passing to `msgwin_set_messages_dir`.
- The `locate` command must be available on the system.  If `g_subprocess_new`
  returns an error (locate not found), set the status label to
  `"locate not found — install mlocate or plocate"`.
- For `sudo updatedb`: do not attempt password prompting in the plugin.
  If geanycli is loaded its terminal handles the sudo interaction.  If not,
  `g_spawn_command_line_async("sudo updatedb", NULL)` will either succeed
  (NOPASSWD) or silently fail — that is acceptable.

---

## Keyboard UX target

The complete keyboard flow after Phase 5:

```
Shift  Shift    → dialog opens, focus on search entry
<type query>    → entry fills
Enter           → locate runs; if one result: result label shown
Enter           → file opens in editor, dialog closes, file has focus
```

For multiple-result workflow:
```
Shift  Shift    → dialog opens
<type query>    → entry fills
Enter           → Messages tab populated and shown, dialog closes
click link      → file opens
```

---

## Delivery sequence

```
Phase 1  →  Phase 2  →  Phase 3  →  Phase 4  →  Phase 5
skeleton    dialog UI    async        messages     keybinding
                         locate       tab          + updatedb
```

Each phase leaves the plugin in a compilable, testable state.
