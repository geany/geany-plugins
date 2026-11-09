/*
 * locate.c — Geany plugin: Find File using the OS locate command
 *
 * Searches are scoped to the open project root.  Results follow three
 * UX paths: zero (keep dialog open), one (in-dialog open), many
 * (clickable links in the Messages tab).
 *
 * Copyright 2025 teknopaul
 * License: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <geanyplugin.h>


/* ------------------------------------------------------------------ */

GeanyPlugin *geany_plugin;
GeanyData   *geany_data;

/* ------------------------------------------------------------------ */
/* Menu items                                                          */

static GtkWidget *menu_item_find   = NULL;
static GtkWidget *menu_item_update = NULL;

/* ------------------------------------------------------------------ */
/* Dialog widgets (all reset to NULL in on_dialog_destroy)            */

static GtkWidget *locate_dialog = NULL;
static GtkWidget *entry_query   = NULL;
static GtkWidget *check_case    = NULL;
static GtkWidget *label_status  = NULL;
static GtkWidget *label_result  = NULL;
static gchar     *single_result = NULL;  /* full path of single match */

/* ------------------------------------------------------------------ */
/* Async search state                                                  */

typedef struct {
    GSubprocess      *proc;
    GInputStream     *stdout_stream;
    GDataInputStream *reader;
    GPtrArray        *results;   /* collected file paths (heap-alloc gchar*) */
    gchar            *query;
    gchar            *root;
} LocateAsync;

static LocateAsync  *current_search = NULL;
static GCancellable *search_cancel  = NULL;

/* ------------------------------------------------------------------ */
/* Double-shift keybinding state (Phase 5)                            */

#define DOUBLE_SHIFT_MS 300
static guint32 last_shift_time = 0;

/* ------------------------------------------------------------------ */
/* Standard Geany keybinding                                          */

static GeanyKeyGroup *key_group = NULL;
enum { KB_FIND_FILE, KB_COUNT };

/* ------------------------------------------------------------------ */
/* Helpers                                                             */

static gchar *get_project_root(void)
{
    GeanyApp *app = geany->app;
    if (app->project && app->project->base_path && *app->project->base_path)
        return g_strdup(app->project->base_path);
    return g_strdup(g_get_home_dir());
}

/* ------------------------------------------------------------------ */
/* Messages tab (Phase 4)                                              */

static void locate_populate_messages(GPtrArray *paths)
{
    gchar *root        = get_project_root();
    gchar *root_locale = utils_get_locale_from_utf8(root);

    msgwin_clear_tab(MSG_MESSAGE);
    msgwin_set_messages_dir(root_locale);

    for (guint i = 0; i < paths->len; i++)
    {
        const gchar *path = (const gchar *)paths->pdata[i];
        const gchar *rel  = path;
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

/* ------------------------------------------------------------------ */
/* Result modes (Phase 2 stubs, Phase 3 wires them to real data)      */

static void result_none(const gchar *query)
{
    if (!locate_dialog) return;
    gchar *msg = g_strdup_printf("No results for \"%s\"", query);
    gtk_label_set_text(GTK_LABEL(label_status), msg);
    g_free(msg);
    gtk_widget_hide(label_result);
    /* dialog stays open for query refinement */
}

static void result_one(const gchar *path)
{
    if (!locate_dialog) return;
    gtk_label_set_text(GTK_LABEL(label_status), _("Press Enter to open:"));
    gtk_label_set_text(GTK_LABEL(label_result), path);
    gtk_widget_show(label_result);
    g_free(single_result);
    single_result = g_strdup(path);
    /* selectable label steals focus; restore it so Enter fires the default response */
    gtk_widget_grab_focus(entry_query);
}

static void result_many(GPtrArray *paths)
{
    locate_populate_messages(paths);
    if (locate_dialog)
        gtk_widget_destroy(locate_dialog);
}

/* ------------------------------------------------------------------ */
/* Async locate reader (Phase 3)                                       */

static void locate_async_finish(LocateAsync *la);

static void on_read_line_done(GObject *src, GAsyncResult *res, gpointer data)
{
    LocateAsync *la  = data;
    GError      *err = NULL;
    gchar       *line = g_data_input_stream_read_line_finish(
        G_DATA_INPUT_STREAM(src), res, NULL, &err);

    if (err)  { g_error_free(err); locate_async_finish(la); return; }
    if (!line) { locate_async_finish(la); return; }   /* EOF */

    g_ptr_array_add(la->results, line);   /* line is heap-alloc'd by GLib */

    g_data_input_stream_read_line_async(la->reader, G_PRIORITY_DEFAULT,
                                        search_cancel,
                                        on_read_line_done, la);
}

static void locate_async_finish(LocateAsync *la)
{
    guint n = la->results ? la->results->len : 0;

    if (n == 0)
        result_none(la->query);
    else if (n == 1)
        result_one((const gchar *)la->results->pdata[0]);
    else
    {
        GPtrArray *paths = la->results;
        la->results = NULL;   /* transfer ownership to result_many */
        result_many(paths);
        g_ptr_array_unref(paths);
    }

    g_object_unref(la->reader);
    g_object_unref(la->proc);
    if (la->results)
        g_ptr_array_unref(la->results);
    g_free(la->query);
    g_free(la->root);
    if (current_search == la) current_search = NULL;
    g_free(la);
}

static gchar **build_locate_argv(const gchar *query, gboolean case_sensitive,
                                  const gchar *root)
{
    GPtrArray *argv = g_ptr_array_new();
    g_ptr_array_add(argv, g_strdup("locate"));
    if (!case_sensitive)
        g_ptr_array_add(argv, g_strdup("-i"));
    gchar *escaped = g_regex_escape_string(query, -1);
    gchar *pattern = g_strdup_printf("^%s.*%s", root, escaped);
    g_free(escaped);
    g_ptr_array_add(argv, g_strdup("-r"));
    g_ptr_array_add(argv, pattern);
    g_ptr_array_add(argv, NULL);
    return (gchar **)g_ptr_array_free(argv, FALSE);
}

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

    gchar  *root = get_project_root();
    gchar **argv = build_locate_argv(query, case_sens, root);

    GError      *err  = NULL;
    GSubprocess *proc = g_subprocess_newv(
        (const gchar * const *)argv,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
        &err);
    g_strfreev(argv);

    if (!proc)
    {
        gtk_label_set_text(GTK_LABEL(label_status),
                           (err && err->message)
                           ? err->message
                           : "locate not found \xe2\x80\x94 install mlocate or plocate");
        g_clear_error(&err);
        g_free(root);
        return;
    }

    gtk_label_set_text(GTK_LABEL(label_status), "Searching\xe2\x80\xa6");

    LocateAsync *la   = g_new0(LocateAsync, 1);
    la->proc          = proc;
    la->stdout_stream = g_subprocess_get_stdout_pipe(proc);
    la->reader        = g_data_input_stream_new(la->stdout_stream);
    la->results       = g_ptr_array_new_with_free_func(g_free);
    la->query         = g_strdup(query);
    la->root          = root;

    current_search = la;

    g_data_input_stream_read_line_async(la->reader, G_PRIORITY_DEFAULT,
                                        search_cancel,
                                        on_read_line_done, la);
}

/* ------------------------------------------------------------------ */
/* Dialog (Phase 2)                                                    */

static void on_dialog_destroy(G_GNUC_UNUSED GtkWidget *w,
                               G_GNUC_UNUSED gpointer data)
{
    locate_dialog = entry_query = check_case = label_status = label_result = NULL;
    g_free(single_result);
    single_result = NULL;
}

static void on_dialog_response(GtkDialog *dlg, gint response,
                                G_GNUC_UNUSED gpointer data)
{
    if (response == GTK_RESPONSE_OK)
    {
        if (single_result)
        {
            /* Single-result mode: Enter opens the file */
            gchar *path = g_strdup(single_result);
            gtk_widget_destroy(GTK_WIDGET(dlg));
            document_open_file(path, FALSE, NULL, NULL);
            g_free(path);
        }
        else
        {
            locate_run_search();
        }
        return;
    }
    gtk_widget_destroy(GTK_WIDGET(dlg));
}

static void locate_dialog_show(void)
{
    if (locate_dialog)
    {
        gtk_window_present(GTK_WINDOW(locate_dialog));
        return;
    }

    locate_dialog = gtk_dialog_new_with_buttons(
        _("Find File"),
        GTK_WINDOW(geany_data->main_widgets->window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Find"),   GTK_RESPONSE_OK,
        NULL);

    gtk_dialog_set_default_response(GTK_DIALOG(locate_dialog), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(locate_dialog), 480, -1);

    GtkWidget *ca = gtk_dialog_get_content_area(GTK_DIALOG(locate_dialog));
    gtk_container_set_border_width(GTK_CONTAINER(ca), 8);
    gtk_box_set_spacing(GTK_BOX(ca), 6);

    /* Search row */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *lbl  = gtk_label_new(_("Search:"));
    entry_query     = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry_query), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), lbl,        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), entry_query, TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(ca),   hbox,        FALSE, FALSE, 0);

    /* Case toggle */
    check_case = gtk_check_button_new_with_label(_("Case sensitive"));
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

    g_signal_connect(locate_dialog, "response", G_CALLBACK(on_dialog_response), NULL);
    g_signal_connect(locate_dialog, "destroy",  G_CALLBACK(on_dialog_destroy),  NULL);

    gtk_widget_show(locate_dialog);
}

/* ------------------------------------------------------------------ */
/* Menu item handlers                                                  */

static void on_find_file_activate(G_GNUC_UNUSED GtkMenuItem *item,
                                   G_GNUC_UNUSED gpointer data)
{
    locate_dialog_show();
}

static void on_updatedb_activate(G_GNUC_UNUSED GtkMenuItem *item,
                                  G_GNUC_UNUSED gpointer data)
{
    const gchar *cmd = "sudo /usr/bin/updatedb";
    gulong id = g_signal_lookup("geanycli-run-command", G_OBJECT_TYPE(geany->object));
    if (id != 0 && g_signal_has_handler_pending(geany->object, id, 0, FALSE))
        g_signal_emit_by_name(geany->object, "geanycli-run-command", cmd, TRUE);
    else
        g_spawn_command_line_async(cmd, NULL);
}

/* ------------------------------------------------------------------ */
/* Double-shift handler (Phase 5)                                      */

static gboolean on_key_press(G_GNUC_UNUSED GtkWidget *widget,
                              GdkEventKey *event,
                              G_GNUC_UNUSED gpointer data)
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

static void kb_find_file(G_GNUC_UNUSED guint key_id)
{
    locate_dialog_show();
}

/* ------------------------------------------------------------------ */
/* IPC: "locate-find-file" signal                                      */

static void on_locate_find_file_signal(G_GNUC_UNUSED GObject *obj,
                                        const gchar *query,
                                        G_GNUC_UNUSED gpointer data)
{
    locate_dialog_show();
    if (entry_query && query && *query) {
        gtk_entry_set_text(GTK_ENTRY(entry_query), query);
        locate_run_search();
    }
}

/* ------------------------------------------------------------------ */
/* Plugin lifecycle                                                    */

static gboolean gl_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer data)
{
    geany_plugin = plugin;
    geany_data   = plugin->geany_data;

    menu_item_find = gtk_menu_item_new_with_label(_("Find File"));
    gtk_widget_show(menu_item_find);
    gtk_menu_shell_append(GTK_MENU_SHELL(geany_data->main_widgets->tools_menu),
                          menu_item_find);
    g_signal_connect(menu_item_find, "activate",
                     G_CALLBACK(on_find_file_activate), NULL);

    menu_item_update = gtk_menu_item_new_with_label("updatedb");
    gtk_widget_show(menu_item_update);
    gtk_menu_shell_append(GTK_MENU_SHELL(geany_data->main_widgets->tools_menu),
                          menu_item_update);
    g_signal_connect(menu_item_update, "activate",
                     G_CALLBACK(on_updatedb_activate), NULL);

    g_signal_connect(geany_data->main_widgets->window, "key-press-event",
                     G_CALLBACK(on_key_press), NULL);

    key_group = plugin_set_key_group(geany_plugin, "locate", KB_COUNT, NULL);
    keybindings_set_item(key_group, KB_FIND_FILE, kb_find_file,
                         0, 0, "find_file", _("Find File"), menu_item_find);

    GType obj_type = G_OBJECT_TYPE(geany->object);
    if (!g_signal_lookup("locate-find-file", obj_type))
        g_signal_new("locate-find-file", obj_type, G_SIGNAL_RUN_LAST,
                     0, NULL, NULL,
                     g_cclosure_marshal_VOID__STRING,
                     G_TYPE_NONE, 1, G_TYPE_STRING);
    plugin_signal_connect(plugin, geany->object, "locate-find-file", FALSE,
                          G_CALLBACK(on_locate_find_file_signal), NULL);

    return TRUE;
}

static void gl_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin,
                        G_GNUC_UNUSED gpointer data)
{
    if (search_cancel)
    {
        g_cancellable_cancel(search_cancel);
        g_clear_object(&search_cancel);
    }
    if (locate_dialog)
    {
        gtk_widget_destroy(locate_dialog);
        locate_dialog = NULL;
    }

    g_signal_handlers_disconnect_by_func(geany_data->main_widgets->window,
                                         G_CALLBACK(on_key_press), NULL);

    if (menu_item_find)   { gtk_widget_destroy(menu_item_find);   menu_item_find   = NULL; }
    if (menu_item_update) { gtk_widget_destroy(menu_item_update); menu_item_update = NULL; }
}

/* ------------------------------------------------------------------ */

G_MODULE_EXPORT void geany_load_module(GeanyPlugin *plugin)
{
    plugin->info->name        = "Locate";
    plugin->info->description = "Find file by name using the OS locate index.";
    plugin->info->version     = "0.1";
    plugin->info->author      = "teknopaul";
    plugin->funcs->init       = gl_init;
    plugin->funcs->cleanup    = gl_cleanup;
    GEANY_PLUGIN_REGISTER(plugin, 235);
}
