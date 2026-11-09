/*
 * geanycontrol.c — Geany plugin: Unix socket + signal IPC for agent UI control
 *
 * Exposes a line-delimited command protocol on a Unix domain socket at
 * ~/.config/geany/geanycontrol.sock and registers matching GLib signals on
 * geany->object for in-process inter-plugin use.
 *
 * Commands (one per line, \n terminated; server replies ok\n or error: …\n):
 *
 *   open-file <path>
 *   close-file <path>
 *   save-file <path>
 *   save-all
 *   scroll-to-line <path>:<line>   (line is 1-based)
 *   get-current-file
 *   list-open-files
 *   fuzzy-open-file <spoken>       (score open docs by spoken tokens, switch to best)
 *   activate-menu-item <label>     (case-insensitive, searches Tools menu)
 *   refresh                        (emits geanycontrol-refresh signal)
 *   ping
 *
 * TreeBrowser IPC commands:
 *   treebrowser-refresh
 *   treebrowser-home
 *   treebrowser-project-root
 *   treebrowser-set-path
 *   treebrowser-activate
 *   treebrowser-focus
 *   treebrowser-popup-menu
 *   treebrowser-navigate <delta>   (signed integer, positive = down)
 *
 * Inter-plugin IPC — emit on geany->object:
 *   "geanycontrol-open-file"          (gchar *path)
 *   "geanycontrol-close-file"         (gchar *path)
 *   "geanycontrol-save-file"          (gchar *path)
 *   "geanycontrol-save-all"           (no args)
 *   "geanycontrol-scroll-to-line"     (gchar *"path:line")
 *   "geanycontrol-activate-menu-item" (gchar *label)
 *   "geanycontrol-refresh"            (no args)
 *
 * Copyright 2025 teknopaul
 * License: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <gtk/gtk.h>

#include <geanyplugin.h>


/* ------------------------------------------------------------------ */

GeanyPlugin  *geany_plugin;
GeanyData    *geany_data;

static GSocketService *socket_service = NULL;
static gchar          *socket_path    = NULL;


/* ------------------------------------------------------------------ */
/* Connection context                                                  */

typedef struct {
    GDataInputStream  *reader;
    GOutputStream     *out;
    GSocketConnection *conn;
} ConnCtx;

static void conn_ctx_free(ConnCtx *ctx)
{
    g_object_unref(ctx->reader);
    g_object_unref(ctx->out);
    g_object_unref(ctx->conn);
    g_free(ctx);
}


/* ------------------------------------------------------------------ */
/* File operation helper                                               */

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


/* ------------------------------------------------------------------ */
/* Phase 2 — document operations                                       */

static gboolean idle_open_file(gpointer data)
{
    FileOpData *d = data;
    document_open_file(d->path, FALSE, NULL, NULL);
    file_op_data_free(d);
    return G_SOURCE_REMOVE;
}

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

static gchar *cmd_open_file(const gchar *path)
{
    FileOpData *d = g_new0(FileOpData, 1);
    d->path = g_strdup(path);
    d->line = -1;
    g_idle_add(idle_open_file, d);
    return g_strdup("ok\n");
}

static gchar *cmd_close_file(const gchar *path)
{
    FileOpData *d = g_new0(FileOpData, 1);
    d->path = g_strdup(path);
    d->line = -1;
    g_idle_add(idle_close_file, d);
    return g_strdup("ok\n");
}

static gchar *cmd_save_file(const gchar *path)
{
    FileOpData *d = g_new0(FileOpData, 1);
    d->path = g_strdup(path);
    d->line = -1;
    g_idle_add(idle_save_file, d);
    return g_strdup("ok\n");
}

static gchar *cmd_save_all(void)
{
    g_idle_add(idle_save_all, NULL);
    return g_strdup("ok\n");
}


/* ------------------------------------------------------------------ */
/* Phase 3 — scroll, query, list                                       */

static gchar *cmd_scroll_to_line(const gchar *arg)
{
    /* Split on last colon to handle paths that may contain colons */
    const gchar *colon = strrchr(arg, ':');
    if (!colon || colon == arg)
        return g_strdup("error: expected path:line\n");

    gchar *path = g_strndup(arg, (gsize)(colon - arg));
    gint   line = (gint)g_ascii_strtoll(colon + 1, NULL, 10) - 1; /* 0-based */

    GeanyDocument *doc = document_open_file(path, FALSE, NULL, NULL);
    g_free(path);
    if (!doc)
        return g_strdup("error: could not open file\n");
    if (line >= 0)
        sci_goto_line(doc->editor->sci, line, TRUE);
    return g_strdup("ok\n");
}

static gchar *cmd_get_current_file(void)
{
    GeanyDocument *doc = document_get_current();
    if (!doc || !doc->file_name)
        return g_strdup("none\nok\n");
    return g_strdup_printf("%s\nok\n", doc->file_name);
}

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


/* ------------------------------------------------------------------ */
/* Fuzzy file matching against open documents                         */

static gint fuzzy_file_score(const gchar *path, gchar **words)
{
    gchar *basename = g_path_get_basename(path);
    gchar *lower    = g_utf8_strdown(basename, -1);
    g_free(basename);
    gint score = 0;
    for (gchar **w = words; *w; w++)
        if (**w && strstr(lower, *w))
            score++;
    g_free(lower);
    return score;
}

static gchar *cmd_fuzzy_open_file(const gchar *spoken)
{
    if (!spoken || !*spoken)
        return g_strdup("error: no spoken text\n");

    gchar  *lower = g_utf8_strdown(spoken, -1);
    gchar **words = g_strsplit_set(lower, " -", -1);
    g_free(lower);

    gint   best_score = 0;
    gchar *best_path  = NULL;
    guint  i;
    GeanyDocument *doc;

    foreach_document(i) {
        doc = documents[i];
        if (!doc->file_name) continue;
        gint score = fuzzy_file_score(doc->file_name, words);
        if (score > best_score) {
            best_score = score;
            g_free(best_path);
            best_path = g_strdup(doc->file_name);
        }
    }
    g_strfreev(words);

    if (!best_path)
        return g_strdup("error: no matching open file\n");

    gchar *result = cmd_open_file(best_path);
    g_free(best_path);
    return result;
}


/* ------------------------------------------------------------------ */
/* Phase 4 — menu item activation                                      */

typedef struct {
    const gchar *target;   /* lower-cased label to find */
    gboolean     found;
} MenuWalk;

static void walk_menu(GtkWidget *widget, gpointer data)
{
    MenuWalk *w = data;
    if (w->found) return;

    if (GTK_IS_MENU_ITEM(widget)) {
        GtkWidget *child = gtk_bin_get_child(GTK_BIN(widget));
        if (GTK_IS_LABEL(child)) {
            const gchar *text  = gtk_label_get_text(GTK_LABEL(child));
            gchar       *lower = g_utf8_strdown(text, -1);
            if (strcmp(lower, w->target) == 0) {
                w->found = TRUE;
                gtk_menu_item_activate(GTK_MENU_ITEM(widget));
            }
            g_free(lower);
        }
        GtkWidget *sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
        if (sub)
            gtk_container_foreach(GTK_CONTAINER(sub), walk_menu, data);
    }
}

static gchar *cmd_activate_menu_item(const gchar *label)
{
    gchar    *lower = g_utf8_strdown(label, -1);
    MenuWalk  w     = { lower, FALSE };

    gtk_container_foreach(
        GTK_CONTAINER(geany_data->main_widgets->tools_menu),
        walk_menu, &w);

    g_free(lower);
    return w.found
        ? g_strdup("ok\n")
        : g_strdup("error: menu item not found\n");
}


/* ------------------------------------------------------------------ */
/* Phase 5 — refresh                                                   */

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


/* ------------------------------------------------------------------ */
/* Phase 6 — switch-tab, append-text, send-to-agent                   */

static gchar *cmd_switch_tab(const gchar *label)
{
    GtkWidget *nb = geany->main_widgets->message_window_notebook;
    if (!nb)
        return g_strdup("error: no message notebook\n");

    gchar *lower_target = g_utf8_strdown(label, -1);
    gint n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(nb));

    for (gint i = 0; i < n; i++) {
        GtkWidget *page  = gtk_notebook_get_nth_page(GTK_NOTEBOOK(nb), i);
        GtkWidget *tab   = gtk_notebook_get_tab_label(GTK_NOTEBOOK(nb), page);
        if (!GTK_IS_LABEL(tab)) {
            /* Some tabs use a composite widget; find child label */
            if (GTK_IS_CONTAINER(tab)) {
                GList *children = gtk_container_get_children(GTK_CONTAINER(tab));
                for (GList *l = children; l; l = l->next) {
                    if (GTK_IS_LABEL(l->data)) {
                        tab = l->data;
                        break;
                    }
                }
                g_list_free(children);
            }
        }
        if (!GTK_IS_LABEL(tab)) continue;

        const gchar *text  = gtk_label_get_text(GTK_LABEL(tab));
        gchar       *lower = g_utf8_strdown(text, -1);
        gboolean     match = strstr(lower, lower_target) != NULL;
        g_free(lower);

        /* Fallback: check plugin-set keyword tag on the page widget itself */
        if (!match) {
            const gchar *kw = g_object_get_data(G_OBJECT(page), "geanyagent-tab-keywords");
            if (kw) {
                gchar *kw_lower = g_utf8_strdown(kw, -1);
                match = strstr(kw_lower, lower_target) != NULL;
                g_free(kw_lower);
            }
        }

        if (match) {
            gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), i);
            gtk_widget_child_focus(page, GTK_DIR_TAB_FORWARD);
            g_free(lower_target);
            return g_strdup("ok\n");
        }
    }

    g_free(lower_target);
    return g_strdup("error: tab not found\n");
}

static gchar *cmd_append_text(const gchar *text)
{
    GeanyDocument *doc = document_get_current();
    if (!doc)
        return g_strdup("error: no active document\n");
    ScintillaObject *sci = doc->editor->sci;
    gint pos = sci_get_current_position(sci);
    sci_insert_text(sci, pos, text);
    sci_set_current_position(sci, pos + (gint)strlen(text), TRUE);
    return g_strdup("ok\n");
}

static gchar *cmd_send_to_agent(const gchar *text)
{
    g_signal_emit_by_name(geany->object, "geanycontrol-send-to-agent", text);
    return g_strdup("ok\n");
}

static gchar *cmd_agent_enter(void)
{
    g_signal_emit_by_name(geany->object, "geanycontrol-send-to-agent", "\n");
    return g_strdup("ok\n");
}

static gchar *cmd_new_skill(const gchar *name)
{
    g_signal_emit_by_name(geany->object, "geanyagent-new-skill", name ? name : "");
    return g_strdup("ok\n");
}

static gchar *cmd_new_prompt(const gchar *name)
{
    g_signal_emit_by_name(geany->object, "geanyagent-new-prompt", name ? name : "");
    return g_strdup("ok\n");
}


/* ------------------------------------------------------------------ */
/* Command dispatch                                                    */

static gchar *dispatch_command(const gchar *line)
{
    if (!line || !*line)
        return g_strdup("error: empty command\n");

    if (g_str_has_prefix(line, "open-file "))
        return cmd_open_file(line + 10);
    if (g_str_has_prefix(line, "close-file "))
        return cmd_close_file(line + 11);
    if (g_str_has_prefix(line, "save-file "))
        return cmd_save_file(line + 10);
    if (strcmp(line, "save-all") == 0)
        return cmd_save_all();
    if (g_str_has_prefix(line, "scroll-to-line "))
        return cmd_scroll_to_line(line + 15);
    if (strcmp(line, "get-current-file") == 0)
        return cmd_get_current_file();
    if (strcmp(line, "list-open-files") == 0)
        return cmd_list_open_files();
    if (g_str_has_prefix(line, "fuzzy-open-file "))
        return cmd_fuzzy_open_file(line + 16);
    if (g_str_has_prefix(line, "activate-menu-item "))
        return cmd_activate_menu_item(line + 19);
    if (strcmp(line, "refresh") == 0)
        return cmd_refresh();
    if (g_str_has_prefix(line, "switch-tab "))
        return cmd_switch_tab(line + 11);
    if (g_str_has_prefix(line, "append-text "))
        return cmd_append_text(line + 12);
    if (g_str_has_prefix(line, "send-to-agent "))
        return cmd_send_to_agent(line + 14);
    if (strcmp(line, "agent-enter") == 0)
        return cmd_agent_enter();
    if (strcmp(line, "new-skill") == 0)
        return cmd_new_skill("");
    if (g_str_has_prefix(line, "new-skill "))
        return cmd_new_skill(line + 10);
    if (strcmp(line, "new-prompt") == 0)
        return cmd_new_prompt("");
    if (g_str_has_prefix(line, "new-prompt "))
        return cmd_new_prompt(line + 11);
    if (strcmp(line, "ping") == 0)
        return g_strdup("ok\n");
    if (strcmp(line, "treebrowser-refresh") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-refresh");
        return g_strdup("ok\n");
    }
    if (strcmp(line, "treebrowser-home") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-home");
        return g_strdup("ok\n");
    }
    if (strcmp(line, "treebrowser-project-root") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-project-root");
        return g_strdup("ok\n");
    }
    if (strcmp(line, "treebrowser-set-path") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-set-path");
        return g_strdup("ok\n");
    }
    if (strcmp(line, "treebrowser-activate") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-activate");
        return g_strdup("ok\n");
    }
    if (strcmp(line, "treebrowser-focus") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-focus");
        return g_strdup("ok\n");
    }
    if (strcmp(line, "treebrowser-popup-menu") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-popup-menu");
        return g_strdup("ok\n");
    }
    if (g_str_has_prefix(line, "treebrowser-navigate ")) {
        gint delta = (gint)g_ascii_strtoll(line + 21, NULL, 10);
        g_signal_emit_by_name(geany->object, "treebrowser-navigate", delta);
        return g_strdup("ok\n");
    }

    return g_strdup("error: unknown command\n");
}


/* ------------------------------------------------------------------ */
/* Async line reader                                                   */

static void on_read_line(GObject *src, GAsyncResult *res, gpointer data)
{
    ConnCtx *ctx  = data;
    GError  *err  = NULL;
    gchar   *line = g_data_input_stream_read_line_finish(
                        G_DATA_INPUT_STREAM(src), res, NULL, &err);

    if (!line) {
        conn_ctx_free(ctx);
        if (err) g_error_free(err);
        return;
    }

    gchar *reply = dispatch_command(g_strstrip(line));
    g_free(line);

    g_output_stream_write_all(ctx->out, reply, strlen(reply), NULL, NULL, NULL);
    g_free(reply);

    g_data_input_stream_read_line_async(ctx->reader, G_PRIORITY_DEFAULT, NULL,
                                        on_read_line, ctx);
}


/* ------------------------------------------------------------------ */
/* Incoming connection                                                 */

static gboolean on_incoming_connection(G_GNUC_UNUSED GSocketService    *svc,
                                       GSocketConnection *conn,
                                       G_GNUC_UNUSED GObject           *source,
                                       G_GNUC_UNUSED gpointer           udata)
{
    GInputStream     *in     = g_io_stream_get_input_stream(G_IO_STREAM(conn));
    GOutputStream    *out    = g_io_stream_get_output_stream(G_IO_STREAM(conn));
    GDataInputStream *reader = g_data_input_stream_new(in);

    ConnCtx *ctx = g_new0(ConnCtx, 1);
    ctx->reader  = reader;
    ctx->out     = g_object_ref(out);
    ctx->conn    = g_object_ref(conn);

    g_data_input_stream_read_line_async(reader, G_PRIORITY_DEFAULT, NULL,
                                        on_read_line, ctx);
    return TRUE;
}


/* ------------------------------------------------------------------ */
/* Signal registration                                                 */

static void gc_register_signals(void)
{
    GType obj_type = G_OBJECT_TYPE(geany->object);

    static const struct { const gchar *name; GType arg; } sigs[] = {
        { "geanycontrol-open-file",          G_TYPE_STRING },
        { "geanycontrol-close-file",         G_TYPE_STRING },
        { "geanycontrol-save-file",          G_TYPE_STRING },
        { "geanycontrol-save-all",           G_TYPE_NONE   },
        { "geanycontrol-scroll-to-line",     G_TYPE_STRING },
        { "geanycontrol-activate-menu-item", G_TYPE_STRING },
        { "geanycontrol-refresh",            G_TYPE_NONE   },
        { "geanycontrol-switch-tab",         G_TYPE_STRING },
        { "geanycontrol-append-text",        G_TYPE_STRING },
        { "geanycontrol-send-to-agent",      G_TYPE_STRING },
        { "treebrowser-refresh",             G_TYPE_NONE    },
        { "treebrowser-home",                G_TYPE_NONE    },
        { "treebrowser-project-root",        G_TYPE_NONE    },
        { "treebrowser-set-path",            G_TYPE_NONE    },
        { "treebrowser-navigate",            G_TYPE_INT     },
        { "treebrowser-activate",            G_TYPE_NONE    },
        { "treebrowser-focus",               G_TYPE_NONE    },
        { "treebrowser-popup-menu",          G_TYPE_NONE    },
        { "treebrowser-menu-ready",          G_TYPE_POINTER },
        { "geanyagent-new-skill",            G_TYPE_STRING  },
        { "geanyagent-new-prompt",           G_TYPE_STRING  },
        { NULL, 0 }
    };

    for (gint i = 0; sigs[i].name; i++) {
        if (g_signal_lookup(sigs[i].name, obj_type))
            continue;
        if (sigs[i].arg == G_TYPE_NONE)
            g_signal_new(sigs[i].name, obj_type, G_SIGNAL_RUN_LAST,
                         0, NULL, NULL, NULL, G_TYPE_NONE, 0);
        else
            g_signal_new(sigs[i].name, obj_type, G_SIGNAL_RUN_LAST,
                         0, NULL, NULL, NULL, G_TYPE_NONE, 1, sigs[i].arg);
    }
}


/* ------------------------------------------------------------------ */
/* Signal handlers (in-process IPC)                                   */

static void on_signal_open_file(G_GNUC_UNUSED GObject *obj, const gchar *path,
                                G_GNUC_UNUSED gpointer data)
{
    gchar *r = cmd_open_file(path);
    g_free(r);
}

static void on_signal_close_file(G_GNUC_UNUSED GObject *obj, const gchar *path,
                                 G_GNUC_UNUSED gpointer data)
{
    gchar *r = cmd_close_file(path);
    g_free(r);
}

static void on_signal_save_file(G_GNUC_UNUSED GObject *obj, const gchar *path,
                                G_GNUC_UNUSED gpointer data)
{
    gchar *r = cmd_save_file(path);
    g_free(r);
}

static void on_signal_save_all(G_GNUC_UNUSED GObject *obj,
                               G_GNUC_UNUSED gpointer data)
{
    gchar *r = cmd_save_all();
    g_free(r);
}

static void on_signal_scroll_to_line(G_GNUC_UNUSED GObject *obj,
                                     const gchar *arg,
                                     G_GNUC_UNUSED gpointer data)
{
    gchar *r = cmd_scroll_to_line(arg);
    g_free(r);
}

static void on_signal_append_text(G_GNUC_UNUSED GObject *obj, const gchar *text,
                                  G_GNUC_UNUSED gpointer data)
{
    gchar *r = cmd_append_text(text);
    g_free(r);
}

static void on_signal_switch_tab(G_GNUC_UNUSED GObject *obj, const gchar *label,
                                 G_GNUC_UNUSED gpointer data)
{
    gchar *r = cmd_switch_tab(label);
    g_free(r);
}


/* ------------------------------------------------------------------ */
/* Test dialog                                                         */

static GtkWidget *test_window    = NULL;
static GtkWidget *test_output    = NULL;
static GtkWidget *tab_entry      = NULL;
static GtkWidget *nav_spin       = NULL;
static GtkWidget *agent_entry    = NULL;
static GtkWidget *append_entry   = NULL;
static GtkWidget *skill_entry    = NULL;
static GtkWidget *prompt_entry   = NULL;
static GtkWidget *tools_sep      = NULL;
static GtkWidget *tools_item     = NULL;
static GtkWidget *file_item      = NULL;

static void test_run(const gchar *cmd)
{
    gchar *reply = dispatch_command(cmd);
    if (test_output) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(test_output));
        GtkTextIter end;
        gchar *line = g_strdup_printf("$ %s\n\342\206\222 %s", cmd, reply ? reply : "(null)\n");
        gtk_text_buffer_get_end_iter(buf, &end);
        gtk_text_buffer_insert(buf, &end, line, -1);
        g_free(line);
        gtk_text_buffer_get_end_iter(buf, &end);
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(test_output), &end, 0.0, FALSE, 0, 0);
    }
    g_free(reply);
}

static void on_tbtn(G_GNUC_UNUSED GtkButton *b, gpointer cmd)
{
    test_run((const gchar *)cmd);
}

static void on_switch_custom(G_GNUC_UNUSED GtkButton *b, G_GNUC_UNUSED gpointer d)
{
    const gchar *label = gtk_entry_get_text(GTK_ENTRY(tab_entry));
    gchar *cmd = g_strdup_printf("switch-tab %s", label);
    test_run(cmd);
    g_free(cmd);
}

static void on_nav_up(G_GNUC_UNUSED GtkButton *b, G_GNUC_UNUSED gpointer d)
{
    gint n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(nav_spin));
    gchar *cmd = g_strdup_printf("treebrowser-navigate -%d", n);
    test_run(cmd);
    g_free(cmd);
}

static void on_nav_down(G_GNUC_UNUSED GtkButton *b, G_GNUC_UNUSED gpointer d)
{
    gint n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(nav_spin));
    gchar *cmd = g_strdup_printf("treebrowser-navigate %d", n);
    test_run(cmd);
    g_free(cmd);
}

static void on_send_agent(G_GNUC_UNUSED GtkButton *b, G_GNUC_UNUSED gpointer d)
{
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(agent_entry));
    gchar *cmd = g_strdup_printf("send-to-agent %s", text);
    test_run(cmd);
    g_free(cmd);
}

static void on_append_text(G_GNUC_UNUSED GtkButton *b, G_GNUC_UNUSED gpointer d)
{
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(append_entry));
    gchar *cmd = g_strdup_printf("append-text %s", text);
    test_run(cmd);
    g_free(cmd);
}

static void on_new_skill_btn(G_GNUC_UNUSED GtkButton *b, G_GNUC_UNUSED gpointer d)
{
    const gchar *name = gtk_entry_get_text(GTK_ENTRY(skill_entry));
    gchar *cmd = (name && *name) ? g_strdup_printf("new-skill %s", name) : g_strdup("new-skill");
    test_run(cmd);
    g_free(cmd);
}

static void on_new_prompt_btn(G_GNUC_UNUSED GtkButton *b, G_GNUC_UNUSED gpointer d)
{
    const gchar *name = gtk_entry_get_text(GTK_ENTRY(prompt_entry));
    gchar *cmd = (name && *name) ? g_strdup_printf("new-prompt %s", name) : g_strdup("new-prompt");
    test_run(cmd);
    g_free(cmd);
}

static void on_test_win_destroy(G_GNUC_UNUSED GtkWidget *w, G_GNUC_UNUSED gpointer d)
{
    test_window  = NULL;
    test_output  = NULL;
    tab_entry    = NULL;
    nav_spin     = NULL;
    agent_entry  = NULL;
    append_entry = NULL;
    skill_entry  = NULL;
    prompt_entry = NULL;
}

static GtkWidget *make_btn(const gchar *label, GCallback cb, gpointer data)
{
    GtkWidget *btn = gtk_button_new_with_label(label);
    g_signal_connect(btn, "clicked", cb, data);
    return btn;
}

static GtkWidget *section_label(const gchar *title)
{
    GtkWidget *lbl = gtk_label_new(NULL);
    gchar *markup = g_strdup_printf("<b>%s</b>", title);
    gtk_label_set_markup(GTK_LABEL(lbl), markup);
    g_free(markup);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_widget_set_margin_top(lbl, 8);
    return lbl;
}

static void on_new_instance_activate(G_GNUC_UNUSED GtkMenuItem *item,
                                     G_GNUC_UNUSED gpointer data)
{
    GError *error = NULL;
    const gchar *argv[] = { "geany", "--new-instance", NULL };
    gchar **envp = g_environ_unsetenv(g_get_environ(), "GEANY_PROGRESS_SOCK");
    if (!g_spawn_async(NULL, (gchar **)argv, envp, G_SPAWN_SEARCH_PATH,
                       NULL, NULL, NULL, &error)) {
        g_warning("geanycontrol: failed to open new instance: %s", error->message);
        g_error_free(error);
    }
    g_strfreev(envp);
}

static void open_test_dialog(G_GNUC_UNUSED GtkMenuItem *item, G_GNUC_UNUSED gpointer d)
{
    if (test_window) { gtk_window_present(GTK_WINDOW(test_window)); return; }

    test_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(test_window), "GeanyControl Tester");
    gtk_window_set_default_size(GTK_WINDOW(test_window), 580, 640);
    gtk_window_set_transient_for(GTK_WINDOW(test_window),
                                 GTK_WINDOW(geany_data->main_widgets->window));
    g_signal_connect(test_window, "destroy", G_CALLBACK(on_test_win_destroy), NULL);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 8);
    gtk_container_add(GTK_CONTAINER(test_window), outer);

    /* Scrollable top half for buttons */
    GtkWidget *btn_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(btn_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(btn_scroll, -1, 360);
    gtk_box_pack_start(GTK_BOX(outer), btn_scroll, FALSE, FALSE, 0);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(btn_scroll), vbox);

    /* ---- Basic ---- */
    gtk_box_pack_start(GTK_BOX(vbox), section_label("Basic"), FALSE, FALSE, 0);
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Ping",            G_CALLBACK(on_tbtn), (gpointer)"ping"),            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Save All",        G_CALLBACK(on_tbtn), (gpointer)"save-all"),        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Refresh",         G_CALLBACK(on_tbtn), (gpointer)"refresh"),         FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Get Current File",G_CALLBACK(on_tbtn), (gpointer)"get-current-file"),FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("List Open Files", G_CALLBACK(on_tbtn), (gpointer)"list-open-files"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Agent Enter",     G_CALLBACK(on_tbtn), (gpointer)"agent-enter"),     FALSE, FALSE, 0);

    /* ---- Switch Tab ---- */
    gtk_box_pack_start(GTK_BOX(vbox), section_label("Switch Tab"), FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("\342\206\222 Agent", G_CALLBACK(on_tbtn), (gpointer)"switch-tab Agent"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("\342\206\222 CLI",   G_CALLBACK(on_tbtn), (gpointer)"switch-tab CLI"),   FALSE, FALSE, 0);
    tab_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(tab_entry), "tab name\342\200\246");
    gtk_entry_set_width_chars(GTK_ENTRY(tab_entry), 14);
    gtk_box_pack_start(GTK_BOX(row), tab_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("\342\206\222 Custom", G_CALLBACK(on_switch_custom), NULL), FALSE, FALSE, 0);

    /* ---- Treebrowser ---- */
    gtk_box_pack_start(GTK_BOX(vbox), section_label("Treebrowser"), FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Focus",        G_CALLBACK(on_tbtn), (gpointer)"treebrowser-focus"),        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Refresh",      G_CALLBACK(on_tbtn), (gpointer)"treebrowser-refresh"),      FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Home",         G_CALLBACK(on_tbtn), (gpointer)"treebrowser-home"),         FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Project Root", G_CALLBACK(on_tbtn), (gpointer)"treebrowser-project-root"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Set Path",     G_CALLBACK(on_tbtn), (gpointer)"treebrowser-set-path"),     FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Activate",     G_CALLBACK(on_tbtn), (gpointer)"treebrowser-activate"),     FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Popup Menu",   G_CALLBACK(on_tbtn), (gpointer)"treebrowser-popup-menu"),   FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), gtk_label_new("Steps:"), FALSE, FALSE, 0);
    nav_spin = gtk_spin_button_new_with_range(1, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(nav_spin), 1);
    gtk_widget_set_size_request(nav_spin, 60, -1);
    gtk_box_pack_start(GTK_BOX(row), nav_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Navigate \342\226\262", G_CALLBACK(on_nav_up),   NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Navigate \342\226\274", G_CALLBACK(on_nav_down), NULL), FALSE, FALSE, 0);

    /* ---- Send to Agent ---- */
    gtk_box_pack_start(GTK_BOX(vbox), section_label("Send to Agent"), FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    agent_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(agent_entry), "text\342\200\246");
    gtk_widget_set_size_request(agent_entry, 300, -1);
    gtk_box_pack_start(GTK_BOX(row), agent_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Send", G_CALLBACK(on_send_agent), NULL), FALSE, FALSE, 0);

    /* ---- Agent Actions ---- */
    gtk_box_pack_start(GTK_BOX(vbox), section_label("Agent Actions"), FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    skill_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(skill_entry), "skill-name\342\200\246");
    gtk_widget_set_size_request(skill_entry, 160, -1);
    gtk_box_pack_start(GTK_BOX(row), skill_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("New Skill", G_CALLBACK(on_new_skill_btn), NULL), FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    prompt_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(prompt_entry), "prompt-name\342\200\246");
    gtk_widget_set_size_request(prompt_entry, 160, -1);
    gtk_box_pack_start(GTK_BOX(row), prompt_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("New Prompt", G_CALLBACK(on_new_prompt_btn), NULL), FALSE, FALSE, 0);

    /* ---- Append Text ---- */
    gtk_box_pack_start(GTK_BOX(vbox), section_label("Append Text"), FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    append_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(append_entry), "text\342\200\246");
    gtk_widget_set_size_request(append_entry, 300, -1);
    gtk_box_pack_start(GTK_BOX(row), append_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), make_btn("Append", G_CALLBACK(on_append_text), NULL), FALSE, FALSE, 0);

    /* ---- Output ---- */
    gtk_box_pack_start(GTK_BOX(outer), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(outer), section_label("Output"), FALSE, FALSE, 0);

    GtkWidget *out_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(out_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(out_scroll, TRUE);
    gtk_box_pack_start(GTK_BOX(outer), out_scroll, TRUE, TRUE, 0);

    test_output = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(test_output), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(test_output), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(test_output), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(out_scroll), test_output);

    gtk_widget_show_all(test_window);
}


/* ------------------------------------------------------------------ */
/* Plugin lifecycle                                                    */

static gboolean gc_init(GeanyPlugin *plugin,
                        gpointer data G_GNUC_UNUSED)
{
    geany_plugin = plugin;
    geany_data   = plugin->geany_data;

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
        g_free(socket_path);
        socket_path = NULL;
        return TRUE;
    }

    g_signal_connect(socket_service, "incoming",
                     G_CALLBACK(on_incoming_connection), NULL);
    g_socket_service_start(socket_service);

    tools_sep  = gtk_separator_menu_item_new();
    tools_item = gtk_menu_item_new_with_label("GeanyControl Tester\342\200\246");
    gtk_widget_show(tools_sep);
    gtk_widget_show(tools_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(geany_data->main_widgets->tools_menu), tools_sep);
    gtk_menu_shell_append(GTK_MENU_SHELL(geany_data->main_widgets->tools_menu), tools_item);
    g_signal_connect(tools_item, "activate", G_CALLBACK(open_test_dialog), NULL);

    GtkWidget *quit_item = ui_lookup_widget(geany->main_widgets->window, "menu_quit1");
    if (quit_item) {
        GtkWidget *file_menu = gtk_widget_get_parent(quit_item);
        GList *children = gtk_container_get_children(GTK_CONTAINER(file_menu));
        gint pos = g_list_index(children, quit_item);
        g_list_free(children);
        file_item = gtk_menu_item_new_with_label(_("New Instance"));
        gtk_widget_show(file_item);
        gtk_menu_shell_insert(GTK_MENU_SHELL(file_menu), file_item, pos);
        g_signal_connect(file_item, "activate", G_CALLBACK(on_new_instance_activate), NULL);
    }

    gc_register_signals();

    plugin_signal_connect(plugin, geany->object, "geanycontrol-open-file", FALSE,
                          G_CALLBACK(on_signal_open_file), NULL);
    plugin_signal_connect(plugin, geany->object, "geanycontrol-close-file", FALSE,
                          G_CALLBACK(on_signal_close_file), NULL);
    plugin_signal_connect(plugin, geany->object, "geanycontrol-save-file", FALSE,
                          G_CALLBACK(on_signal_save_file), NULL);
    plugin_signal_connect(plugin, geany->object, "geanycontrol-save-all", FALSE,
                          G_CALLBACK(on_signal_save_all), NULL);
    plugin_signal_connect(plugin, geany->object, "geanycontrol-scroll-to-line", FALSE,
                          G_CALLBACK(on_signal_scroll_to_line), NULL);
    plugin_signal_connect(plugin, geany->object, "geanycontrol-append-text", FALSE,
                          G_CALLBACK(on_signal_append_text), NULL);
    plugin_signal_connect(plugin, geany->object, "geanycontrol-switch-tab", FALSE,
                          G_CALLBACK(on_signal_switch_tab), NULL);

    return TRUE;
}

static void gc_cleanup(GeanyPlugin *plugin G_GNUC_UNUSED,
                       gpointer data G_GNUC_UNUSED)
{
    if (test_window) { gtk_widget_destroy(test_window); test_window = NULL; }
    if (tools_item)  { gtk_widget_destroy(tools_item);  tools_item  = NULL; }
    if (tools_sep)   { gtk_widget_destroy(tools_sep);   tools_sep   = NULL; }
    if (file_item)   { gtk_widget_destroy(file_item);   file_item   = NULL; }
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

G_MODULE_EXPORT void geany_load_module(GeanyPlugin *plugin)
{
    plugin->info->name        = "GeanyControl";
    plugin->info->description = "Unix socket + signal IPC for agent-driven UI control.";
    plugin->info->version     = "0.1";
    plugin->info->author      = "teknopaul";
    plugin->funcs->init       = gc_init;
    plugin->funcs->cleanup    = gc_cleanup;
    GEANY_PLUGIN_REGISTER(plugin, 235);
}
