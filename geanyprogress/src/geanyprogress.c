/*
 * geanyprogress.c — Geany plugin: AI agent plan progress panel
 *
 * Adds a "Progress" tab to the sidebar notebook.  The tab shows the
 * active plan name and a numbered list of phases with completion status.
 *
 * File naming conventions:
 *   Plans:    .planning/plans/xxxx_PLAN.md
 *   Progress: .planning/state/xxxx_PROGRESS.md
 *
 * Inter-process API — an AI agent (or any process) sends JSON to the
 * Unix domain socket advertised in $GEANY_PROGRESS_SOCK:
 *
 *   Register a plan:
 *     {"plan":"Plan Name","plan_file":".planning/plans/xxxx_PLAN.md",
 *      "phases":["Phase 1","Phase 2","Phase 3"]}
 *     (plan_file is optional but enables "Open plan" in the context menu)
 *
 *   Mark a phase complete (1-based index), optionally with review files/warnings:
 *     {"phase":1,"status":"complete",
 *      "files":[{"path":"src/foo.c","line":42},{"path":"Makefile.am"}],
 *      "warnings":["Check the timeout","Possible memory leak"]}
 *     (files and warnings are optional)
 *
 *   Query current plan state (plugin responds with JSON on the same connection):
 *     {"query":"state"}
 *   Response: {"plan":"Name","plan_file":"rel/path","progress_file":"/abs/path",
 *              "phases":[{"index":1,"name":"...","done":true},...],
 *              "next_phase":2}
 *     next_phase is the 1-based index of the first pending phase,
 *     0 if all phases are done, or null if no active plan.
 *
 * Sidebar interactions:
 *   Left-click on title row       — open the plan file in the editor
 *   Left-click on completed phase — open all review files at their line numbers
 *   Right-click anywhere          — context menu
 *   Hover over any row            — tooltip showing warnings for that phase
 *
 * Copyright 2026 teknopaul
 * License: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <geanyplugin.h>

GeanyPlugin  *geany_plugin;
GeanyData    *geany_data;


/* ------------------------------------------------------------------ */
/* Plan model                                                           */

#define MAX_PHASES       32
#define MAX_REVIEW_FILES  8
#define MAX_WARNINGS      8

typedef struct {
    gchar *path;
    gint   line;   /* 0 means no specific line */
} ReviewFile;

typedef struct {
    gchar      *name;
    gboolean    done;
    ReviewFile  files[MAX_REVIEW_FILES];
    gint        n_files;
    gchar      *warnings[MAX_WARNINGS];
    gint        n_warnings;
} Phase;

typedef struct {
    gchar  *plan_name;
    gchar  *plan_file;       /* absolute path to _PLAN.md, may be NULL */
    gchar  *progress_file;   /* absolute path to _PROGRESS.md, may be NULL */
    Phase   phases[MAX_PHASES];
    gint    n_phases;
} ProgressPlan;

static ProgressPlan current_plan = {0};

/* Index of the phase row that was right-clicked (0-based); -1 = header */
static gint ctx_phase_idx = -1;


/* ------------------------------------------------------------------ */
/* Minimal JSON helpers (flat objects only, no nesting required)        */

static gchar *json_escape(const gchar *s)
{
    GString *out = g_string_new(NULL);
    for (; *s; s++) {
        if      (*s == '"')  g_string_append(out, "\\\"");
        else if (*s == '\\') g_string_append(out, "\\\\");
        else if (*s == '\n') g_string_append(out, "\\n");
        else if (*s == '\t') g_string_append(out, "\\t");
        else                 g_string_append_c(out, *s);
    }
    return g_string_free(out, FALSE);
}

static gchar *json_get_string(const gchar *json, const gchar *key)
{
    gchar *pattern = g_strdup_printf("\"%s\"", key);
    const gchar *p = strstr(json, pattern);
    g_free(pattern);
    if (!p)
        return NULL;

    p += strlen(key) + 2;
    while (*p && (*p == ' ' || *p == '\t' || *p == ':' || *p == '\n'))
        p++;
    if (*p != '"')
        return NULL;
    p++;

    GString *val = g_string_new(NULL);
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case 'n':  g_string_append_c(val, '\n'); break;
                case 't':  g_string_append_c(val, '\t'); break;
                default:   g_string_append_c(val, *p);  break;
            }
        } else {
            g_string_append_c(val, *p);
        }
        p++;
    }
    return g_string_free(val, FALSE);
}

static gint json_get_int(const gchar *json, const gchar *key)
{
    gchar *pattern = g_strdup_printf("\"%s\"", key);
    const gchar *p = strstr(json, pattern);
    g_free(pattern);
    if (!p)
        return -1;

    p += strlen(key) + 2;
    while (*p && (*p == ' ' || *p == '\t' || *p == ':' || *p == '\n'))
        p++;
    if (!g_ascii_isdigit(*p) && *p != '-')
        return -1;
    return (gint)strtol(p, NULL, 10);
}

static gint json_get_string_array(const gchar *json, const gchar *key,
                                   gchar **out, gint max)
{
    gchar *pattern = g_strdup_printf("\"%s\"", key);
    const gchar *p = strstr(json, pattern);
    g_free(pattern);
    if (!p)
        return 0;

    p += strlen(key) + 2;
    while (*p && (*p == ' ' || *p == '\t' || *p == ':' || *p == '\n'))
        p++;
    if (*p != '[')
        return 0;
    p++;

    gint count = 0;
    while (*p && *p != ']' && count < max) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == ','))
            p++;
        if (*p != '"')
            break;
        p++;

        GString *val = g_string_new(NULL);
        while (*p && *p != '"') {
            if (*p == '\\' && *(p + 1)) {
                p++;
                switch (*p) {
                    case 'n':  g_string_append_c(val, '\n'); break;
                    case 't':  g_string_append_c(val, '\t'); break;
                    default:   g_string_append_c(val, *p);  break;
                }
            } else {
                g_string_append_c(val, *p);
            }
            p++;
        }
        if (*p == '"')
            p++;

        out[count++] = g_string_free(val, FALSE);
    }
    return count;
}

/* Parse the "files" array of objects: [{"path":"...","line":N}, ...] */
static gint json_get_file_array(const gchar *json, ReviewFile *out, gint max)
{
    const gchar *p = strstr(json, "\"files\"");
    if (!p)
        return 0;

    p += 7; /* strlen("\"files\"") */
    while (*p && (*p == ' ' || *p == '\t' || *p == ':' || *p == '\n'))
        p++;
    if (*p != '[')
        return 0;
    p++;

    gint count = 0;
    while (*p && *p != ']' && count < max) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == ','))
            p++;
        if (*p != '{')
            break;

        /* Find the matching closing brace */
        const gchar *start = p;
        gint depth = 0;
        const gchar *q = p;
        while (*q) {
            if (*q == '{')      depth++;
            else if (*q == '}') { depth--; if (!depth) { q++; break; } }
            q++;
        }

        gchar *obj = g_strndup(start, q - start);
        out[count].path = json_get_string(obj, "path");
        gint line = json_get_int(obj, "line");
        out[count].line = (line > 0) ? line : 0;
        g_free(obj);

        if (out[count].path)
            count++;

        p = q;
    }
    return count;
}


/* ------------------------------------------------------------------ */
/* Tree view columns                                                    */

enum {
    COL_NUM       = 0,
    COL_NAME      = 1,
    COL_STATUS    = 2,
    COL_STATUS_FG = 3,   /* foreground color string for status cell */
    COL_TOOLTIP   = 4,   /* tooltip text shown on hover             */
    N_COLS
};


/* ------------------------------------------------------------------ */
/* Module globals                                                       */

static GtkWidget      *sidebar_vbox = NULL;
static gint            page_number  = -1;
static GtkListStore   *list_store   = NULL;
static GtkWidget      *tree_view    = NULL;

static GSocketService *sock_service = NULL;
static gchar          *sock_path    = NULL;


/* ------------------------------------------------------------------ */
/* Build tooltip text for a phase (warnings + file list)               */

static gchar *build_phase_tooltip(const Phase *phase)
{
    if (!phase->done || (phase->n_warnings == 0 && phase->n_files == 0))
        return NULL;

    GString *tip = g_string_new(NULL);

    for (gint i = 0; i < phase->n_warnings; i++) {
        if (i > 0) g_string_append_c(tip, '\n');
        g_string_append_printf(tip, "⚠ %s", phase->warnings[i]);
    }

    if (phase->n_files > 0) {
        if (phase->n_warnings > 0)
            g_string_append(tip, "\n\nFiles for review:");
        else
            g_string_append(tip, "Files for review:");

        for (gint i = 0; i < phase->n_files; i++) {
            if (phase->files[i].line > 0)
                g_string_append_printf(tip, "\n  %s:%d",
                                        phase->files[i].path, phase->files[i].line);
            else
                g_string_append_printf(tip, "\n  %s", phase->files[i].path);
        }
    }

    return g_string_free(tip, FALSE);
}


/* ------------------------------------------------------------------ */
/* UI refresh                                                           */

static void ui_refresh(void)
{
    if (!list_store)
        return;

    gtk_list_store_clear(list_store);
    GtkTreeIter iter;

    /* Header row — plan name */
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter,
                       COL_NUM,       "",
                       COL_NAME,      current_plan.plan_name
                                          ? current_plan.plan_name
                                          : "No active plan",
                       COL_STATUS,    "",
                       COL_STATUS_FG, (gchar *)NULL,
                       COL_TOOLTIP,   (gchar *)NULL,
                       -1);

    for (gint i = 0; i < current_plan.n_phases; i++) {
        Phase *ph  = &current_plan.phases[i];
        gchar *num = g_strdup_printf("%d", i + 1);
        gchar *tip = build_phase_tooltip(ph);

        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter,
                           COL_NUM,       num,
                           COL_NAME,      ph->name,
                           COL_STATUS,    ph->done ? "✓" : "○",
                           COL_STATUS_FG, ph->done ? "#22aa22" : (gchar *)NULL,
                           COL_TOOLTIP,   tip,
                           -1);
        g_free(num);
        g_free(tip);
    }
}


/* ------------------------------------------------------------------ */
/* Plan model management                                                */

static void phase_free(Phase *ph)
{
    g_free(ph->name);
    for (gint i = 0; i < ph->n_files; i++)
        g_free(ph->files[i].path);
    for (gint i = 0; i < ph->n_warnings; i++)
        g_free(ph->warnings[i]);
}

static void plan_free(void)
{
    g_free(current_plan.plan_name);
    g_free(current_plan.plan_file);
    g_free(current_plan.progress_file);
    for (gint i = 0; i < current_plan.n_phases; i++)
        phase_free(&current_plan.phases[i]);
    memset(&current_plan, 0, sizeof(current_plan));
}


/* ------------------------------------------------------------------ */
/* Persistence                                                          */

static gchar *make_slug(const gchar *name)
{
    GString *s = g_string_new(NULL);
    for (const gchar *p = name; *p; p++) {
        if (g_ascii_isalnum(*p))
            g_string_append_c(s, g_ascii_toupper(*p));
        else if (*p == ' ' || *p == '-' || *p == '_')
            g_string_append_c(s, '_');
    }
    return g_string_free(s, FALSE);
}

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

    gchar *slug     = make_slug(current_plan.plan_name);
    gchar *basename = g_strconcat(slug, "_PROGRESS.md", NULL);
    g_free(slug);
    gchar *md_path  = g_build_filename(state_dir, basename, NULL);
    g_free(basename);
    g_free(state_dir);

    g_free(current_plan.progress_file);
    current_plan.progress_file = g_strdup(md_path);

    GString *content = g_string_new(NULL);
    g_string_append_printf(content, "# %s \xe2\x80\x94 Progress\n\n",
                           current_plan.plan_name);
    g_string_append(content, "| Phase | Description | Status |\n");
    g_string_append(content, "|-------|-------------|--------|\n");

    for (gint i = 0; i < current_plan.n_phases; i++) {
        Phase *ph = &current_plan.phases[i];
        g_string_append_printf(content, "| %d | %s | %s |\n",
                               i + 1, ph->name,
                               ph->done ? "complete" : "pending");
    }

    /* Append review info sections for completed phases */
    gboolean has_review = FALSE;
    for (gint i = 0; i < current_plan.n_phases; i++) {
        Phase *ph = &current_plan.phases[i];
        if (!ph->done || (ph->n_files == 0 && ph->n_warnings == 0))
            continue;
        if (!has_review) {
            g_string_append(content, "\n## Review notes\n");
            has_review = TRUE;
        }
        g_string_append_printf(content, "\n### Phase %d \xe2\x80\x94 %s\n",
                               i + 1, ph->name);
        for (gint j = 0; j < ph->n_warnings; j++)
            g_string_append_printf(content, "\n⚠ %s\n", ph->warnings[j]);
        if (ph->n_files > 0) {
            g_string_append(content, "\nFiles for review:\n");
            for (gint j = 0; j < ph->n_files; j++) {
                if (ph->files[j].line > 0)
                    g_string_append_printf(content, "- `%s:%d`\n",
                                           ph->files[j].path, ph->files[j].line);
                else
                    g_string_append_printf(content, "- `%s`\n", ph->files[j].path);
            }
        }
    }

    GError *err = NULL;
    if (!g_file_set_contents(md_path, content->str, -1, &err)) {
        g_warning("geanyprogress: write failed: %s", err->message);
        g_error_free(err);
    }

    g_string_free(content, TRUE);
    g_free(md_path);
}


/* ------------------------------------------------------------------ */
/* Load progress from a _PROGRESS.md file                              */

static void load_progress_from_file(const gchar *filepath)
{
    gchar  *content = NULL;
    GError *err     = NULL;
    if (!g_file_get_contents(filepath, &content, NULL, &err)) {
        g_warning("geanyprogress: load failed: %s", err->message);
        g_error_free(err);
        return;
    }

    plan_free();

    gchar **lines = g_strsplit(content, "\n", -1);
    g_free(content);

    for (gint i = 0; lines[i]; i++) {
        const gchar *line = lines[i];

        /* Plan name: first "# ..." line */
        if (g_str_has_prefix(line, "# ") && !current_plan.plan_name) {
            gchar *name = g_strdup(line + 2);
            gchar *suffix = strstr(name, " \xe2\x80\x94 Progress");
            if (!suffix) suffix = strstr(name, " - Progress");
            if (suffix)  *suffix = '\0';
            current_plan.plan_name = name;
            continue;
        }

        /* Phase rows: | N | description | status | */
        if (g_str_has_prefix(line, "| ") && current_plan.n_phases < MAX_PHASES) {
            gchar **cols = g_strsplit(line, "|", -1);
            if (cols[1] && cols[2] && cols[3]) {
                gchar *num = g_strstrip(cols[1]);
                if (g_ascii_isdigit(num[0])) {
                    gchar *name   = g_strstrip(g_strdup(cols[2]));
                    gchar *status = g_strstrip(cols[3]);
                    current_plan.phases[current_plan.n_phases].name = name;
                    current_plan.phases[current_plan.n_phases].done =
                        (strcmp(status, "complete") == 0);
                    current_plan.n_phases++;
                }
            }
            g_strfreev(cols);
        }
    }
    g_strfreev(lines);

    current_plan.progress_file = g_strdup(filepath);

    /* Try to derive plan file: .planning/state/XXX_PROGRESS.md
     *                       → .planning/plans/XXX_PLAN.md           */
    gchar *base = g_path_get_basename(filepath);
    if (g_str_has_suffix(base, "_PROGRESS.md")) {
        gchar *state_dir = g_path_get_dirname(filepath);
        gchar *planning  = g_path_get_dirname(state_dir);
        gchar *plans_dir = g_build_filename(planning, "plans", NULL);
        gsize  slug_len  = strlen(base) - strlen("_PROGRESS.md");
        gchar *slug      = g_strndup(base, slug_len);
        gchar *plan_base = g_strconcat(slug, "_PLAN.md", NULL);
        gchar *plan_path = g_build_filename(plans_dir, plan_base, NULL);

        if (g_file_test(plan_path, G_FILE_TEST_EXISTS))
            current_plan.plan_file = plan_path;
        else
            g_free(plan_path);

        g_free(slug);
        g_free(plan_base);
        g_free(plans_dir);
        g_free(planning);
        g_free(state_dir);
    }
    g_free(base);

    ui_refresh();
}


/* ------------------------------------------------------------------ */
/* Open a file in the Geany editor, optionally scrolling to a line    */

static void open_file_at_line(const gchar *path, gint line)
{
    if (!path || !*path)
        return;

    gchar *abs_path;
    if (g_path_is_absolute(path)) {
        abs_path = g_strdup(path);
    } else {
        GeanyApp *app = geany->app;
        if (app->project && app->project->base_path)
            abs_path = g_build_filename(app->project->base_path, path, NULL);
        else
            abs_path = g_strdup(path);
    }

    if (!g_file_test(abs_path, G_FILE_TEST_EXISTS)) {
        g_warning("geanyprogress: file not found: %s", abs_path);
        g_free(abs_path);
        return;
    }

    GeanyDocument *doc = document_open_file(abs_path, FALSE, NULL, NULL);
    g_free(abs_path);

    if (doc && line > 0)
        sci_goto_line(doc->editor->sci, line - 1, TRUE);

    if (doc)
        keybindings_send_command(GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_EDITOR);
}

static void open_in_editor(const gchar *path)
{
    open_file_at_line(path, 0);
}


/* ------------------------------------------------------------------ */
/* Context menu callbacks                                              */

static void on_mark_done_clicked(G_GNUC_UNUSED GtkMenuItem *item,
                                  G_GNUC_UNUSED gpointer data)
{
    if (ctx_phase_idx >= 0 && ctx_phase_idx < current_plan.n_phases) {
        current_plan.phases[ctx_phase_idx].done = TRUE;
        ui_refresh();
        persist_plan();
    }
}

static void on_open_progress_clicked(G_GNUC_UNUSED GtkMenuItem *item,
                                      G_GNUC_UNUSED gpointer data)
{
    open_in_editor(current_plan.progress_file);
}

static void on_open_plan_clicked(G_GNUC_UNUSED GtkMenuItem *item,
                                  G_GNUC_UNUSED gpointer data)
{
    open_in_editor(current_plan.plan_file);
}

static void on_open_review_files_clicked(G_GNUC_UNUSED GtkMenuItem *item,
                                          G_GNUC_UNUSED gpointer data)
{
    if (ctx_phase_idx < 0 || ctx_phase_idx >= current_plan.n_phases)
        return;
    Phase *ph = &current_plan.phases[ctx_phase_idx];
    for (gint i = 0; i < ph->n_files; i++)
        open_file_at_line(ph->files[i].path, ph->files[i].line);
}

/* ------------------------------------------------------------------ */
/* Relative path helper                                                 */

static gchar *plan_file_rel_path(void)
{
    if (!current_plan.plan_file)
        return NULL;
    GeanyApp *app = geany->app;
    if (app->project && app->project->base_path) {
        const gchar *base    = app->project->base_path;
        gsize        baselen = strlen(base);
        if (g_str_has_prefix(current_plan.plan_file, base)) {
            const gchar *rel = current_plan.plan_file + baselen;
            while (*rel == G_DIR_SEPARATOR)
                rel++;
            if (*rel)
                return g_strdup(rel);
        }
    }
    return g_strdup(current_plan.plan_file);
}

/* ------------------------------------------------------------------ */
/* Build query response JSON from current plan state                    */

static gchar *build_state_json(void)
{
    GString *s = g_string_new("{");

    if (current_plan.plan_name) {
        gchar *esc = json_escape(current_plan.plan_name);
        g_string_append_printf(s, "\"plan\":\"%s\"", esc);
        g_free(esc);
    } else {
        g_string_append(s, "\"plan\":null");
    }

    if (current_plan.plan_file) {
        gchar *rel = plan_file_rel_path();
        gchar *esc = json_escape(rel ? rel : current_plan.plan_file);
        g_string_append_printf(s, ",\"plan_file\":\"%s\"", esc);
        g_free(esc);
        g_free(rel);
    } else {
        g_string_append(s, ",\"plan_file\":null");
    }

    if (current_plan.progress_file) {
        gchar *esc = json_escape(current_plan.progress_file);
        g_string_append_printf(s, ",\"progress_file\":\"%s\"", esc);
        g_free(esc);
    } else {
        g_string_append(s, ",\"progress_file\":null");
    }

    g_string_append(s, ",\"phases\":[");
    for (gint i = 0; i < current_plan.n_phases; i++) {
        Phase *ph  = &current_plan.phases[i];
        gchar *esc = json_escape(ph->name);
        if (i > 0) g_string_append_c(s, ',');
        g_string_append_printf(s,
            "{\"index\":%d,\"name\":\"%s\",\"done\":%s}",
            i + 1, esc, ph->done ? "true" : "false");
        g_free(esc);
    }
    g_string_append(s, "]");

    /* next_phase: first pending phase (1-based); 0 = all done; null = no plan */
    if (current_plan.plan_name && current_plan.n_phases > 0) {
        gint next = 0;
        for (gint i = 0; i < current_plan.n_phases; i++) {
            if (!current_plan.phases[i].done) { next = i + 1; break; }
        }
        g_string_append_printf(s, ",\"next_phase\":%d", next);
    } else {
        g_string_append(s, ",\"next_phase\":null");
    }

    g_string_append(s, "}\n");
    return g_string_free(s, FALSE);
}

static void emit_insert_to_agent(const gchar *text)
{
    GType obj_type = G_OBJECT_TYPE(geany->object);
    if (!g_signal_lookup("geanyagent-insert-to-agent", obj_type))
        g_signal_new("geanyagent-insert-to-agent", obj_type, G_SIGNAL_RUN_LAST,
                     0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
    g_signal_emit_by_name(geany->object, "geanyagent-insert-to-agent", text);
}

static void emit_run_in_agent(const gchar *text)
{
    GType obj_type = G_OBJECT_TYPE(geany->object);
    if (!g_signal_lookup("geanyagent-run-in-agent", obj_type))
        g_signal_new("geanyagent-run-in-agent", obj_type, G_SIGNAL_RUN_LAST,
                     0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
    g_signal_emit_by_name(geany->object, "geanyagent-run-in-agent", text);
}

static void on_run_opus_exec_clicked(G_GNUC_UNUSED GtkMenuItem *item,
                                      G_GNUC_UNUSED gpointer data)
{
    gchar *rel = plan_file_rel_path();
    if (!rel)
        return;
    gchar *cmd = g_strdup_printf("/opus-exec %s", rel);
    g_free(rel);
    emit_run_in_agent(cmd);
    g_free(cmd);
}

static void on_insert_opus_exec_clicked(G_GNUC_UNUSED GtkMenuItem *item,
                                         G_GNUC_UNUSED gpointer data)
{
    gchar *rel = plan_file_rel_path();
    if (!rel)
        return;
    gchar *cmd = g_strdup_printf("/opus-exec %s", rel);
    g_free(rel);
    emit_insert_to_agent(cmd);
    g_free(cmd);
}

static void on_copy_plan_path_clicked(G_GNUC_UNUSED GtkMenuItem *item,
                                       G_GNUC_UNUSED gpointer data)
{
    gchar *rel = plan_file_rel_path();
    if (!rel)
        return;
    GtkClipboard *clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clip, rel, -1);
    g_free(rel);
}

static void on_continue_phase_clicked(G_GNUC_UNUSED GtkMenuItem *item,
                                       gpointer data)
{
    gint phase_idx = GPOINTER_TO_INT(data);
    gchar *rel = plan_file_rel_path();
    if (!rel)
        return;
    gchar *cmd = g_strdup_printf("/opus-exec %s %d", rel, phase_idx + 1);
    g_free(rel);
    emit_run_in_agent(cmd);
    g_free(cmd);
}

static void on_load_clicked(G_GNUC_UNUSED GtkMenuItem *item,
                             G_GNUC_UNUSED gpointer data)
{
    GtkWidget *dlg = gtk_file_chooser_dialog_new(
        "Open Progress File",
        GTK_WINDOW(geany->main_widgets->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open",   GTK_RESPONSE_ACCEPT,
        NULL);

    GeanyApp *app = geany->app;
    if (app->project && app->project->base_path) {
        gchar *state_dir = g_build_filename(app->project->base_path,
                                             ".planning", "state", NULL);
        if (g_file_test(state_dir, G_FILE_TEST_IS_DIR))
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), state_dir);
        g_free(state_dir);
    }

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*_PROGRESS.md");
    gtk_file_filter_set_name(filter, "Progress files (*_PROGRESS.md)");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), filter);

    GtkFileFilter *all = gtk_file_filter_new();
    gtk_file_filter_add_pattern(all, "*.md");
    gtk_file_filter_set_name(all, "Markdown files (*.md)");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), all);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        gtk_widget_destroy(dlg);

        load_progress_from_file(filename);
        g_free(filename);

        /* If the plan file wasn't found automatically, ask the user */
        if (!current_plan.plan_file) {
            GtkWidget *dlg2 = gtk_file_chooser_dialog_new(
                "Select Plan File (press Skip to continue without)",
                GTK_WINDOW(geany->main_widgets->window),
                GTK_FILE_CHOOSER_ACTION_OPEN,
                "_Skip", GTK_RESPONSE_CANCEL,
                "_Open", GTK_RESPONSE_ACCEPT,
                NULL);

            if (app->project && app->project->base_path) {
                gchar *plans_dir = g_build_filename(app->project->base_path,
                                                     ".planning", "plans", NULL);
                if (g_file_test(plans_dir, G_FILE_TEST_IS_DIR))
                    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg2), plans_dir);
                g_free(plans_dir);
            }

            GtkFileFilter *f2 = gtk_file_filter_new();
            gtk_file_filter_add_pattern(f2, "*_PLAN.md");
            gtk_file_filter_set_name(f2, "Plan files (*_PLAN.md)");
            gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg2), f2);

            if (gtk_dialog_run(GTK_DIALOG(dlg2)) == GTK_RESPONSE_ACCEPT)
                current_plan.plan_file =
                    gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg2));

            gtk_widget_destroy(dlg2);
        }
    } else {
        gtk_widget_destroy(dlg);
    }
}


/* ------------------------------------------------------------------ */
/* Context menu                                                         */

static void show_context_menu(GdkEventButton *event)
{
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *item;

    gboolean phase_done = (ctx_phase_idx >= 0 &&
                           ctx_phase_idx < current_plan.n_phases &&
                           current_plan.phases[ctx_phase_idx].done);
    gboolean phase_pending = (ctx_phase_idx >= 0 &&
                              ctx_phase_idx < current_plan.n_phases &&
                              !current_plan.phases[ctx_phase_idx].done);

    item = gtk_menu_item_new_with_label("Mark finished");
    gtk_widget_set_sensitive(item, phase_pending);
    g_signal_connect(item, "activate", G_CALLBACK(on_mark_done_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    /* Open review files (only for completed phases that have them) */
    item = gtk_menu_item_new_with_label("Open review files");
    gtk_widget_set_sensitive(item,
        phase_done && ctx_phase_idx >= 0 &&
        current_plan.phases[ctx_phase_idx].n_files > 0);
    g_signal_connect(item, "activate", G_CALLBACK(on_open_review_files_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    item = gtk_menu_item_new_with_label("Open progress");
    gtk_widget_set_sensitive(item, current_plan.progress_file != NULL);
    g_signal_connect(item, "activate", G_CALLBACK(on_open_progress_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Open plan");
    gtk_widget_set_sensitive(item, current_plan.plan_file != NULL);
    g_signal_connect(item, "activate", G_CALLBACK(on_open_plan_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    item = gtk_menu_item_new_with_label("Load...");
    g_signal_connect(item, "activate", G_CALLBACK(on_load_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    item = gtk_menu_item_new_with_label("Run with opus-exec");
    gtk_widget_set_sensitive(item, current_plan.plan_file != NULL);
    g_signal_connect(item, "activate", G_CALLBACK(on_run_opus_exec_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    if (phase_pending && current_plan.plan_file) {
        gchar *label = g_strdup_printf("Run opus-exec from phase %d", ctx_phase_idx + 1);
        item = gtk_menu_item_new_with_label(label);
        g_free(label);
        g_signal_connect_data(item, "activate",
                              G_CALLBACK(on_continue_phase_clicked),
                              GINT_TO_POINTER(ctx_phase_idx), NULL, 0);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }

    item = gtk_menu_item_new_with_label("Prepare in agent");
    gtk_widget_set_sensitive(item, current_plan.plan_file != NULL);
    g_signal_connect(item, "activate", G_CALLBACK(on_insert_opus_exec_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Copy plan path");
    gtk_widget_set_sensitive(item, current_plan.plan_file != NULL);
    g_signal_connect(item, "activate", G_CALLBACK(on_copy_plan_path_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
}


/* ------------------------------------------------------------------ */
/* Tree view click handler                                              */

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event,
                                 G_GNUC_UNUSED gpointer data)
{
    GtkTreePath *path = NULL;
    gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                   (gint)event->x, (gint)event->y,
                                   &path, NULL, NULL, NULL);

    gint row = -1;
    if (path) {
        gint *indices = gtk_tree_path_get_indices(path);
        row = indices ? indices[0] : -1;
        gtk_tree_path_free(path);
    }

    if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
        if (row == 0) {
            /* Title row: open plan file */
            if (current_plan.plan_file) {
                open_in_editor(current_plan.plan_file);
                return TRUE;
            }
            return FALSE;
        }

        if (row > 0) {
            /* Phase row: open review files if phase is done and has files */
            gint phase_idx = row - 1;
            if (phase_idx < current_plan.n_phases) {
                Phase *ph = &current_plan.phases[phase_idx];
                if (ph->done && ph->n_files > 0) {
                    for (gint i = 0; i < ph->n_files; i++)
                        open_file_at_line(ph->files[i].path, ph->files[i].line);
                    return TRUE;
                }
            }
        }

        return FALSE;
    }

    if (event->button == 3) {
        ctx_phase_idx = (row > 0) ? row - 1 : -1;
        show_context_menu(event);
        return TRUE;
    }

    return FALSE;
}


/* ------------------------------------------------------------------ */
/* Message handling                                                     */

static void handle_message(const gchar *json)
{
    if (strstr(json, "\"phases\"")) {
        plan_free();
        current_plan.plan_name = json_get_string(json, "plan");
        current_plan.plan_file = json_get_string(json, "plan_file");

        /* Resolve relative plan_file against the project root */
        if (current_plan.plan_file && !g_path_is_absolute(current_plan.plan_file)) {
            GeanyApp *app = geany->app;
            if (app->project && app->project->base_path) {
                gchar *abs = g_build_filename(app->project->base_path,
                                              current_plan.plan_file, NULL);
                g_free(current_plan.plan_file);
                current_plan.plan_file = abs;
            }
        }

        gchar *phase_names[MAX_PHASES] = {0};
        current_plan.n_phases = json_get_string_array(json, "phases",
                                                        phase_names, MAX_PHASES);
        for (gint i = 0; i < current_plan.n_phases; i++) {
            current_plan.phases[i].name = phase_names[i];
            current_plan.phases[i].done = FALSE;
        }
    } else if (strstr(json, "\"phase\"")) {
        gint idx = json_get_int(json, "phase") - 1;  /* 1-based → 0-based */
        if (idx >= 0 && idx < current_plan.n_phases) {
            Phase *ph = &current_plan.phases[idx];
            ph->done = TRUE;

            /* Parse optional review files */
            ph->n_files = json_get_file_array(json, ph->files, MAX_REVIEW_FILES);

            /* Resolve relative file paths */
            GeanyApp *app = geany->app;
            if (app->project && app->project->base_path) {
                for (gint i = 0; i < ph->n_files; i++) {
                    if (ph->files[i].path && !g_path_is_absolute(ph->files[i].path)) {
                        gchar *abs = g_build_filename(app->project->base_path,
                                                       ph->files[i].path, NULL);
                        g_free(ph->files[i].path);
                        ph->files[i].path = abs;
                    }
                }
            }

            /* Parse optional warnings */
            ph->n_warnings = json_get_string_array(json, "warnings",
                                                    ph->warnings, MAX_WARNINGS);
        }
    }

    ui_refresh();
    persist_plan();
}


/* ------------------------------------------------------------------ */
/* Socket callbacks                                                     */

static gboolean on_incoming_connection(G_GNUC_UNUSED GSocketService *service,
                                       GSocketConnection *connection,
                                       G_GNUC_UNUSED GObject *source,
                                       G_GNUC_UNUSED gpointer data)
{
    GInputStream *in = g_io_stream_get_input_stream(G_IO_STREAM(connection));
    gchar buf[8192];
    gssize n = g_input_stream_read(in, buf, sizeof(buf) - 1, NULL, NULL);
    if (n > 0) {
        buf[n] = '\0';
        if (strstr(buf, "\"query\"")) {
            gchar         *json = build_state_json();
            GOutputStream *out  = g_io_stream_get_output_stream(G_IO_STREAM(connection));
            g_output_stream_write_all(out, json, strlen(json), NULL, NULL, NULL);
            g_output_stream_flush(out, NULL, NULL);
            g_free(json);
            g_io_stream_close(G_IO_STREAM(connection), NULL, NULL);
        } else {
            handle_message(buf);
        }
    }
    return TRUE;
}


/* ------------------------------------------------------------------ */
/* Socket lifecycle                                                     */

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

    g_setenv("GEANY_PROGRESS_SOCK", sock_path, TRUE);

    GType obj_type = G_OBJECT_TYPE(geany->object);
    if (!g_signal_lookup("geanyagent-restart", obj_type))
        g_signal_new("geanyagent-restart", obj_type, G_SIGNAL_RUN_LAST,
                     0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    if (g_signal_lookup("geanyagent-restart", obj_type))
        g_signal_emit_by_name(geany->object, "geanyagent-restart");

    return TRUE;
}

static void socket_cleanup(void)
{
    if (sock_service) {
        g_socket_service_stop(sock_service);
        g_object_unref(sock_service);
        sock_service = NULL;
    }
    if (sock_path) {
        unlink(sock_path);
        g_unsetenv("GEANY_PROGRESS_SOCK");
        g_free(sock_path);
        sock_path = NULL;
    }
}


/* ------------------------------------------------------------------ */
/* Sidebar panel construction                                           */

static GtkWidget *build_panel(void)
{
    list_store = gtk_list_store_new(N_COLS,
                                    G_TYPE_STRING,   /* COL_NUM       */
                                    G_TYPE_STRING,   /* COL_NAME      */
                                    G_TYPE_STRING,   /* COL_STATUS    */
                                    G_TYPE_STRING,   /* COL_STATUS_FG */
                                    G_TYPE_STRING);  /* COL_TOOLTIP   */

    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);
    gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(tree_view), COL_TOOLTIP);

    GtkCellRenderer   *r;
    GtkTreeViewColumn *col;

    /* # column */
    r   = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("#", r, "text", COL_NUM, NULL);
    gtk_tree_view_column_set_min_width(col, 24);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);

    /* Name column */
    r   = gtk_cell_renderer_text_new();
    g_object_set(r, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    col = gtk_tree_view_column_new_with_attributes("Phase", r, "text", COL_NAME, NULL);
    gtk_tree_view_column_set_expand(col, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);

    /* Status column — foreground color bound to COL_STATUS_FG */
    r = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_min_width(col, 28);
    gtk_tree_view_column_pack_start(col, r, FALSE);
    gtk_tree_view_column_add_attribute(col, r, "text",       COL_STATUS);
    gtk_tree_view_column_add_attribute(col, r, "foreground", COL_STATUS_FG);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col);

    g_signal_connect(tree_view, "button-press-event",
                     G_CALLBACK(on_button_press), NULL);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), tree_view);
    gtk_widget_show_all(scroll);

    ui_refresh();

    return scroll;
}


/* ------------------------------------------------------------------ */
/* Plugin lifecycle                                                     */

static gboolean gp_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer data)
{
    geany_plugin = plugin;
    geany_data   = plugin->geany_data;

    sidebar_vbox = build_panel();

    GtkWidget *label = gtk_label_new("Progress");
    page_number = gtk_notebook_append_page(
        GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
        sidebar_vbox, label);

    socket_init();

    return TRUE;
}

static void gp_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin,
                       G_GNUC_UNUSED gpointer data)
{
    socket_cleanup();
    plan_free();

    if (list_store) {
        g_object_unref(list_store);
        list_store = NULL;
        tree_view  = NULL;
    }

    if (page_number >= 0) {
        gtk_notebook_remove_page(
            GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
            page_number);
        sidebar_vbox = NULL;
        page_number  = -1;
    }
}


/* ------------------------------------------------------------------ */

G_MODULE_EXPORT void geany_load_module(GeanyPlugin *plugin)
{
    plugin->info->name        = "Progress";
    plugin->info->description =
        "Shows AI agent plan progress in the sidebar. "
        "Receives JSON via $GEANY_PROGRESS_SOCK. "
        "Init: {\"plan\":\"Name\",\"plan_file\":\"path\",\"phases\":[...]}. "
        "Done: {\"phase\":N,\"status\":\"complete\","
              "\"files\":[{\"path\":\"f\",\"line\":N}],"
              "\"warnings\":[\"msg\"]}. "
        "Query: {\"query\":\"state\"} — responds with plan+progress JSON. "
        "Naming: .planning/plans/xxxx_PLAN.md / .planning/state/xxxx_PROGRESS.md";
    plugin->info->version     = "0.4";
    plugin->info->author      = "teknopaul";

    plugin->funcs->init    = gp_init;
    plugin->funcs->cleanup = gp_cleanup;

    GEANY_PLUGIN_REGISTER(plugin, 235);
}
