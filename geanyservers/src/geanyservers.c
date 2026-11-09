/*
 * geanyservers.c — Geany plugin: server management tab in the message window
 *
 * Reads servertools.conf (INI format, same layering as agenttools.conf),
 * displays one row per server with live status/ping dots, and provides
 * start/stop/reload/ping/log actions via a selection-based button bar.
 * All subprocess operations are non-blocking (GSubprocess / GLib async).
 *
 * Copyright 2025 teknopaul
 * License: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <cairo/cairo.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <geanyplugin.h>


/* ------------------------------------------------------------------ */

GeanyPlugin  *geany_plugin;
GeanyData    *geany_data;

#define SERVER_TOOLS_CONFIG       "servertools.conf"
#define SERVER_TOOLS_EXTRA_CONFIG "servertools_extra.conf"
#define SERVER_TOOLS_GROUP        "servers"
#define ARSE_TOOLS_GROUP          "arse"

/* Indices for servertools_extra.conf start at this offset so they never
 * collide with user-level indices 0..(MAX_USER_SERVERS-1). */
#define MAX_USER_SERVERS 100


/* ------------------------------------------------------------------ */
/* Data model                                                          */

typedef enum {
    SERVER_TYPE_SYSTEM,
    SERVER_TYPE_SYSTEMD,
    SERVER_TYPE_LOCAL
} ServerType;

typedef struct {
    gchar      *display;   /* server_N= (label with emoji) */
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


/* ------------------------------------------------------------------ */
/* Data model — ARSE (automatic recovery of server execution)         */

typedef struct {
    gchar *display;   /* arse_N=       label with emoji */
    gchar *check;     /* arse_check_N= grep to detect stale processes */
    gchar *kill_cmd;  /* arse_kill_N=  grep + kill pipeline */
} ArseDef;


/* ------------------------------------------------------------------ */
/* Config                                                              */

static GPtrArray *servers = NULL;
static GPtrArray *arses   = NULL;

static void arse_def_free(gpointer data)
{
    ArseDef *a = data;
    g_free(a->display);
    g_free(a->check);
    g_free(a->kill_cmd);
    g_free(a);
}

static void server_def_free(gpointer data)
{
    ServerDef *s = data;
    g_free(s->display);
    g_free(s->name);
    g_free(s->start);
    g_free(s->stop);
    g_free(s->reload);
    g_free(s->status);
    g_free(s->changes);
    g_free(s->icon);
    g_free(s->ping);
    g_free(s->log);
    g_free(s->sudo);
    g_free(s->on_open);
    g_free(s->on_close);
    g_free(s);
}

static ServerType parse_type(const gchar *s)
{
    if (!s)
        return SERVER_TYPE_SYSTEMD;
    if (g_strcmp0(s, "local") == 0)
        return SERVER_TYPE_LOCAL;
    if (g_strcmp0(s, "system") == 0)
        return SERVER_TYPE_SYSTEM;
    return SERVER_TYPE_SYSTEMD;
}

/* Overlay all keys from src onto dst (same group names). */
static void keyfile_overlay(GKeyFile *dst, GKeyFile *src)
{
    gsize ng;
    gchar **groups = g_key_file_get_groups(src, &ng);
    for (gsize gi = 0; gi < ng; gi++) {
        gsize nk;
        gchar **keys = g_key_file_get_keys(src, groups[gi], &nk, NULL);
        for (gsize ki = 0; ki < nk; ki++) {
            gchar *val = g_key_file_get_string(src, groups[gi], keys[ki], NULL);
            g_key_file_set_string(dst, groups[gi], keys[ki], val);
            g_free(val);
        }
        g_strfreev(keys);
    }
    g_strfreev(groups);
}

/* Additive merge: keys from src go into dst, renumbered so indices start
 * at offset (prevents collisions with user-level keys). */
static void keyfile_merge_additive(GKeyFile *dst, GKeyFile *src, gint offset)
{
    if (!g_key_file_has_group(src, SERVER_TOOLS_GROUP))
        return;

    /* Find highest existing index in src for server_N */
    gint src_max = -1;
    for (gint i = 0; i < 1000; i++) {
        gchar *k = g_strdup_printf("server_%d", i);
        gboolean has = g_key_file_has_key(src, SERVER_TOOLS_GROUP, k, NULL);
        g_free(k);
        if (has)
            src_max = i;
        else if (i > src_max + 10)
            break;
    }

    for (gint i = 0; i <= src_max; i++) {
        const gchar *src_keys[] = {
            "server", "name", "type", "start", "stop", "reload",
            "status", "changes", "icon", "ping", "log", "sudo",
            "on_open", "on_close", NULL
        };
        for (gint ki = 0; src_keys[ki]; ki++) {
            gchar *sk = g_strdup_printf("%s_%d", src_keys[ki], i);
            gchar *val = g_key_file_get_string(src, SERVER_TOOLS_GROUP, sk, NULL);
            g_free(sk);
            if (val) {
                gchar *dk = g_strdup_printf("%s_%d", src_keys[ki], i + offset);
                g_key_file_set_string(dst, SERVER_TOOLS_GROUP, dk, val);
                g_free(dk);
                g_free(val);
            }
        }
    }
}

static void servers_load_config(void)
{
    if (servers)
        g_ptr_array_free(servers, TRUE);
    servers = g_ptr_array_new_with_free_func(server_def_free);

    GKeyFile *kf = g_key_file_new();

    /* 1. User-level */
    gchar *user_path = g_build_filename(geany->app->configdir,
                                        SERVER_TOOLS_CONFIG, NULL);
    g_key_file_load_from_file(kf, user_path, G_KEY_FILE_NONE, NULL);
    g_free(user_path);

    GeanyProject *proj = geany->app->project;

    /* 2. Project-level overlay */
    if (proj && proj->base_path) {
        gchar *proj_path = g_build_filename(proj->base_path, "config", "geany",
                                            SERVER_TOOLS_CONFIG, NULL);
        GKeyFile *pkf = g_key_file_new();
        if (g_key_file_load_from_file(pkf, proj_path, G_KEY_FILE_NONE, NULL))
            keyfile_overlay(kf, pkf);
        g_key_file_free(pkf);
        g_free(proj_path);
    }

    /* 3. Additive extra (indices start at MAX_USER_SERVERS, never overwrite) */
    if (proj && proj->base_path) {
        gchar *extra_path = g_build_filename(proj->base_path, "config", "geany",
                                             SERVER_TOOLS_EXTRA_CONFIG, NULL);
        GKeyFile *ekf = g_key_file_new();
        if (g_key_file_load_from_file(ekf, extra_path, G_KEY_FILE_NONE, NULL))
            keyfile_merge_additive(kf, ekf, MAX_USER_SERVERS);
        g_key_file_free(ekf);
        g_free(extra_path);
    }

    if (!g_key_file_has_group(kf, SERVER_TOOLS_GROUP)) {
        g_key_file_free(kf);
        return;
    }

    for (gint i = 0; i < 1000; i++) {
        gchar *disp_key = g_strdup_printf("server_%d", i);
        gchar *display  = g_key_file_get_string(kf, SERVER_TOOLS_GROUP, disp_key, NULL);
        g_free(disp_key);
        if (!display)
            break;

        /* Empty server_N= means "hide this server" */
        if (!*display) {
            g_free(display);
            continue;
        }

        ServerDef *s = g_new0(ServerDef, 1);
        s->display = display;

#define GET(field) \
        { gchar *_k = g_strdup_printf(#field "_%d", i); \
          s->field = g_key_file_get_string(kf, SERVER_TOOLS_GROUP, _k, NULL); \
          g_free(_k); }

        GET(name)
        GET(start)
        GET(stop)
        GET(reload)
        GET(status)
        GET(changes)
        GET(icon)
        GET(ping)
        GET(log)
        GET(sudo)
        GET(on_open)
        GET(on_close)
#undef GET

        gchar *type_key = g_strdup_printf("type_%d", i);
        gchar *type_str = g_key_file_get_string(kf, SERVER_TOOLS_GROUP, type_key, NULL);
        g_free(type_key);
        s->type = parse_type(type_str);
        g_free(type_str);

        g_ptr_array_add(servers, s);
    }

    /* Load [arse] group from the same merged keyfile */
    if (arses)
        g_ptr_array_free(arses, TRUE);
    arses = g_ptr_array_new_with_free_func(arse_def_free);

    if (g_key_file_has_group(kf, ARSE_TOOLS_GROUP)) {
        for (gint i = 0; i < 1000; i++) {
            gchar *disp_key = g_strdup_printf("arse_%d", i);
            gchar *display  = g_key_file_get_string(kf, ARSE_TOOLS_GROUP, disp_key, NULL);
            g_free(disp_key);
            if (!display)
                break;
            if (!*display) {
                g_free(display);
                continue;
            }

            ArseDef *a  = g_new0(ArseDef, 1);
            a->display  = display;

            gchar *ck = g_strdup_printf("arse_check_%d", i);
            a->check  = g_key_file_get_string(kf, ARSE_TOOLS_GROUP, ck, NULL);
            g_free(ck);

            gchar *kk    = g_strdup_printf("arse_kill_%d", i);
            a->kill_cmd  = g_key_file_get_string(kf, ARSE_TOOLS_GROUP, kk, NULL);
            g_free(kk);

            g_ptr_array_add(arses, a);
        }
    }

    g_key_file_free(kf);
}

static void servers_free_config(void)
{
    if (servers) {
        g_ptr_array_free(servers, TRUE);
        servers = NULL;
    }
    if (arses) {
        g_ptr_array_free(arses, TRUE);
        arses = NULL;
    }
}


/* ------------------------------------------------------------------ */
/* Status dot pixbufs                                                  */

static GdkPixbuf *pixbuf_green  = NULL;
static GdkPixbuf *pixbuf_red    = NULL;
static GdkPixbuf *pixbuf_yellow = NULL;
static GdkPixbuf *pixbuf_grey   = NULL;

static GdkPixbuf *make_dot(double r, double g, double b)
{
    const gint sz = 12;
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sz, sz);
    cairo_t         *cr   = cairo_create(surf);
    cairo_arc(cr, sz / 2.0, sz / 2.0, sz / 2.0 - 1, 0, 2 * G_PI);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_fill(cr);
    cairo_destroy(cr);
    GdkPixbuf *pb = gdk_pixbuf_get_from_surface(surf, 0, 0, sz, sz);
    cairo_surface_destroy(surf);
    return pb;
}

static void dots_init(void)
{
    pixbuf_green  = make_dot(0.0, 0.8, 0.0);
    pixbuf_red    = make_dot(0.9, 0.1, 0.1);
    pixbuf_yellow = make_dot(0.9, 0.8, 0.0);
    pixbuf_grey   = make_dot(0.5, 0.5, 0.5);
}

static void dots_free(void)
{
    g_clear_object(&pixbuf_green);
    g_clear_object(&pixbuf_red);
    g_clear_object(&pixbuf_yellow);
    g_clear_object(&pixbuf_grey);
}


/* ------------------------------------------------------------------ */
/* GtkListStore / GtkTreeView                                         */

enum {
    COL_STATUS_ICON,   /* GdkPixbuf — green/red dot (systemd state)  */
    COL_PING_ICON,     /* GdkPixbuf — green/yellow dot (last ping)   */
    COL_DISPLAY,       /* gchararray — server_N / arse_N label        */
    COL_LAST_BOOT,     /* gchararray — timestamp or ""                */
    COL_SERVER_PTR,    /* gpointer  — ServerDef * or NULL             */
    COL_ARSE_PTR,      /* gpointer  — ArseDef *  or NULL             */
    COL_COUNT
};

static GtkListStore *list_store  = NULL;
static GtkWidget    *tree_view   = NULL;
static GtkWidget    *tab_widget  = NULL;  /* vbox with tree + button bar */
static gint          tab_index   = -1;
static ServerDef    *selected_server = NULL;

static ArseDef   *selected_arse  = NULL;

static GtkWidget *btn_start  = NULL;
static GtkWidget *btn_stop   = NULL;
static GtkWidget *btn_reload = NULL;
static GtkWidget *btn_ping   = NULL;
static GtkWidget *btn_logs   = NULL;
static GtkWidget *btn_kill   = NULL;

static void update_button_sensitivity(void)
{
    gboolean sel  = selected_server != NULL;
    gboolean arse = selected_arse   != NULL;
    gtk_widget_set_sensitive(btn_start,  sel);
    gtk_widget_set_sensitive(btn_stop,   sel);
    gtk_widget_set_sensitive(btn_reload, sel);
    gtk_widget_set_sensitive(btn_ping,   sel);
    gtk_widget_set_sensitive(btn_logs,   sel && selected_server->log != NULL
                                              && *selected_server->log);
    gtk_widget_set_sensitive(btn_kill,   arse && selected_arse->kill_cmd != NULL
                                              && *selected_arse->kill_cmd);
}

static void on_tree_selection_changed(GtkTreeSelection *sel,
                                      G_GNUC_UNUSED gpointer data)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        gtk_tree_model_get(model, &iter,
                           COL_SERVER_PTR, &selected_server,
                           COL_ARSE_PTR,   &selected_arse,
                           -1);
    } else {
        selected_server = NULL;
        selected_arse   = NULL;
    }
    update_button_sensitivity();
}

static void populate_list_store(void)
{
    gtk_list_store_clear(list_store);

    if (servers) {
        for (guint i = 0; i < servers->len; i++) {
            ServerDef *s = g_ptr_array_index(servers, i);
            GtkTreeIter iter;
            gtk_list_store_append(list_store, &iter);
            gtk_list_store_set(list_store, &iter,
                               COL_STATUS_ICON, pixbuf_grey,
                               COL_PING_ICON,   pixbuf_grey,
                               COL_DISPLAY,     s->display,
                               COL_LAST_BOOT,   "",
                               COL_SERVER_PTR,  s,
                               COL_ARSE_PTR,    NULL,
                               -1);
        }
    }

    if (arses) {
        for (guint i = 0; i < arses->len; i++) {
            ArseDef *a = g_ptr_array_index(arses, i);
            GtkTreeIter iter;
            gtk_list_store_append(list_store, &iter);
            gtk_list_store_set(list_store, &iter,
                               COL_STATUS_ICON, pixbuf_grey,
                               COL_PING_ICON,   pixbuf_grey,
                               COL_DISPLAY,     a->display,
                               COL_LAST_BOOT,   "",
                               COL_SERVER_PTR,  NULL,
                               COL_ARSE_PTR,    a,
                               -1);
        }
    }
}

static GtkWidget *create_servers_tab(void)
{
    list_store = gtk_list_store_new(COL_COUNT,
                                    GDK_TYPE_PIXBUF,  /* COL_STATUS_ICON */
                                    GDK_TYPE_PIXBUF,  /* COL_PING_ICON   */
                                    G_TYPE_STRING,    /* COL_DISPLAY     */
                                    G_TYPE_STRING,    /* COL_LAST_BOOT   */
                                    G_TYPE_POINTER,   /* COL_SERVER_PTR  */
                                    G_TYPE_POINTER);  /* COL_ARSE_PTR    */

    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    g_object_unref(list_store);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), TRUE);

    /* Status column */
    GtkCellRenderer *pr = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn *col_status = gtk_tree_view_column_new_with_attributes(
        _("Status"), pr, "pixbuf", COL_STATUS_ICON, NULL);
    gtk_tree_view_column_set_sizing(col_status, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(col_status, 50);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col_status);

    /* Active (ping) column */
    GtkCellRenderer *pr2 = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn *col_ping = gtk_tree_view_column_new_with_attributes(
        _("Active"), pr2, "pixbuf", COL_PING_ICON, NULL);
    gtk_tree_view_column_set_sizing(col_ping, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(col_ping, 50);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col_ping);

    /* Server name column */
    GtkCellRenderer *tr = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col_name = gtk_tree_view_column_new_with_attributes(
        _("Server"), tr, "text", COL_DISPLAY, NULL);
    gtk_tree_view_column_set_expand(col_name, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col_name);

    /* Last boot column */
    GtkCellRenderer *tr2 = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col_boot = gtk_tree_view_column_new_with_attributes(
        _("Last boot"), tr2, "text", COL_LAST_BOOT, NULL);
    gtk_tree_view_column_set_sizing(col_boot, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(col_boot, 80);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col_boot);

    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);
    g_signal_connect(sel, "changed", G_CALLBACK(on_tree_selection_changed), NULL);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), tree_view);

    /* Button bar */
    GtkWidget *bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_START);
    gtk_box_set_spacing(GTK_BOX(bbox), 4);

    btn_start  = gtk_button_new_with_label(_("\xe2\x96\xb6 Start"));
    btn_stop   = gtk_button_new_with_label(_("\xe2\x96\xa0 Stop"));
    btn_reload = gtk_button_new_with_label(_("\xe2\x86\xbb Reload"));
    btn_ping   = gtk_button_new_with_label(_("\xe2\x9a\xa1 Ping"));
    btn_logs   = gtk_button_new_with_label(_("\xf0\x9f\x93\x8b Logs"));
    /* \xf0\x9f\xa7\xbb = 🧻 (matches arse section label style) */
    btn_kill   = gtk_button_new_with_label(_("\xf0\x9f\xa7\xbb Kill"));

    gtk_container_add(GTK_CONTAINER(bbox), btn_start);
    gtk_container_add(GTK_CONTAINER(bbox), btn_stop);
    gtk_container_add(GTK_CONTAINER(bbox), btn_reload);
    gtk_container_add(GTK_CONTAINER(bbox), btn_ping);
    gtk_container_add(GTK_CONTAINER(bbox), btn_logs);
    gtk_container_add(GTK_CONTAINER(bbox), btn_kill);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(vbox), bbox,     FALSE, FALSE, 4);

    gtk_widget_show_all(vbox);

    /* All buttons insensitive until a row is selected */
    update_button_sensitivity();

    populate_list_store();

    return vbox;
}


/* ------------------------------------------------------------------ */
/* Async status/ping checks (Phase 3)                                 */

static GCancellable *check_cancellable = NULL;

typedef struct {
    GtkListStore        *store;
    GtkTreeRowReference *row_ref;
    gint                 col;
    gboolean             is_arse; /* TRUE: exit-0→yellow (found), exit-1→grey */
} AsyncCheck;

static void async_check_free(AsyncCheck *ac)
{
    gtk_tree_row_reference_free(ac->row_ref);
    g_free(ac);
}

static void on_check_done(GObject *src, GAsyncResult *res, gpointer data)
{
    AsyncCheck  *ac  = data;
    GSubprocess *proc = G_SUBPROCESS(src);
    GError      *err  = NULL;

    gboolean ok = g_subprocess_wait_check_finish(proc, res, &err);
    if (err)
        g_error_free(err);

    if (gtk_tree_row_reference_valid(ac->row_ref)) {
        GtkTreePath *path = gtk_tree_row_reference_get_path(ac->row_ref);
        GtkTreeIter  iter;
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(ac->store), &iter, path)) {
            GdkPixbuf *pb;
            if (ac->is_arse) {
                /* exit-0 means grep found stale processes → yellow warning */
                pb = ok ? pixbuf_yellow : pixbuf_grey;
            } else if (ac->col == COL_PING_ICON) {
                pb = ok ? pixbuf_green : pixbuf_yellow;
            } else {
                pb = ok ? pixbuf_green : pixbuf_red;
            }
            gtk_list_store_set(ac->store, &iter, ac->col, pb, -1);
        }
        gtk_tree_path_free(path);
    }
    async_check_free(ac);
}

static void spawn_check(GtkTreePath *path, const gchar *cmd, gint col,
                         gboolean is_arse)
{
    gchar    *argv[4] = { (gchar *)"/bin/sh", (gchar *)"-c", (gchar *)cmd, NULL };
    GError   *err     = NULL;

    GSubprocess *proc = g_subprocess_newv((const gchar * const *)argv,
                                          G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                          &err);
    if (!proc) {
        g_error_free(err);
        return;
    }

    AsyncCheck *ac  = g_new0(AsyncCheck, 1);
    ac->store       = list_store;
    ac->row_ref     = gtk_tree_row_reference_new(GTK_TREE_MODEL(list_store), path);
    ac->col         = col;
    ac->is_arse     = is_arse;

    g_subprocess_wait_check_async(proc, check_cancellable,
                                  on_check_done, ac);
    g_object_unref(proc);
}

static void servers_refresh_all(void)
{
    if (!list_store || !servers)
        return;

    if (check_cancellable) {
        g_cancellable_cancel(check_cancellable);
        g_object_unref(check_cancellable);
    }
    check_cancellable = g_cancellable_new();

    GtkTreeModel *model = GTK_TREE_MODEL(list_store);
    GtkTreeIter   iter;
    if (!gtk_tree_model_get_iter_first(model, &iter))
        return;

    do {
        ServerDef *s    = NULL;
        ArseDef   *arse = NULL;
        gtk_tree_model_get(model, &iter,
                           COL_SERVER_PTR, &s,
                           COL_ARSE_PTR,   &arse,
                           -1);

        GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

        if (s) {
            /* Status check */
            if (s->status) {
                spawn_check(path, s->status, COL_STATUS_ICON, FALSE);
            } else if (s->type == SERVER_TYPE_SYSTEMD || s->type == SERVER_TYPE_SYSTEM) {
                if (s->name && *s->name) {
                    gchar *cmd = g_strdup_printf("systemctl is-active %s", s->name);
                    spawn_check(path, cmd, COL_STATUS_ICON, FALSE);
                    g_free(cmd);
                }
            }

            /* Ping check */
            if (s->ping && *s->ping)
                spawn_check(path, s->ping, COL_PING_ICON, FALSE);

        } else if (arse) {
            /* ARSE check: exit-0 means stale processes exist → yellow */
            if (arse->check && *arse->check)
                spawn_check(path, arse->check, COL_STATUS_ICON, TRUE);
        }

        gtk_tree_path_free(path);
    } while (gtk_tree_model_iter_next(model, &iter));
}

static void on_tab_switched(GtkNotebook *nb, G_GNUC_UNUSED GtkWidget *page,
                             guint page_num, G_GNUC_UNUSED gpointer data)
{
    if ((gint)page_num == tab_index)
        servers_refresh_all();
}


/* ------------------------------------------------------------------ */
/* Command execution (Phase 4)                                        */

static void run_server_command(const gchar *cmd)
{
    if (!cmd || !*cmd)
        return;

    /* Try IPC signal to geanycli first */
    GType obj_type = G_OBJECT_TYPE(geany->object);
    if (g_signal_lookup("geanycli-run-command", obj_type)) {
        /* Save the current message-window page (Servers tab) so we can restore
         * it after: geanycli always switches to the terminal tab, and we want
         * server commands to run there silently without stealing focus. */
        GtkNotebook *msgnb = GTK_NOTEBOOK(
            geany_data->main_widgets->message_window_notebook);
        gint saved = gtk_notebook_get_current_page(msgnb);
        g_signal_emit_by_name(geany->object, "geanycli-run-command", cmd, FALSE);
        if (saved >= 0)
            gtk_notebook_set_current_page(msgnb, saved);
        return;
    }

    /* Fallback: fire-and-forget */
    g_spawn_command_line_async(cmd, NULL);
}

static gchar *build_action_cmd(ServerDef *s, const gchar *action_key,
                                const gchar *systemctl_verb)
{
    gchar *custom = NULL;
    if (g_strcmp0(action_key, "start") == 0)  custom = s->start;
    if (g_strcmp0(action_key, "stop") == 0)   custom = s->stop;
    if (g_strcmp0(action_key, "reload") == 0) custom = s->reload;

    gchar *cmd;
    if (custom && *custom) {
        cmd = g_strdup(custom);
    } else if (s->name && *s->name) {
        cmd = g_strdup_printf("systemctl %s %s", systemctl_verb, s->name);
    } else {
        return NULL;
    }

    if (s->sudo && *s->sudo) {
        gchar *prefixed = g_strdup_printf("%s %s", s->sudo, cmd);
        g_free(cmd);
        cmd = prefixed;
    }
    return cmd;
}

static void update_last_boot(void)
{
    if (!list_store || !selected_server)
        return;
    GtkTreeModel *model = GTK_TREE_MODEL(list_store);
    GtkTreeIter   iter;
    if (!gtk_tree_model_get_iter_first(model, &iter))
        return;
    do {
        ServerDef *s = NULL;
        gtk_tree_model_get(model, &iter, COL_SERVER_PTR, &s, -1);
        if (s == selected_server) {
            GDateTime *now = g_date_time_new_now_local();
            gchar     *ts  = g_date_time_format(now, "%H:%M:%S");
            gtk_list_store_set(list_store, &iter, COL_LAST_BOOT, ts, -1);
            g_free(ts);
            g_date_time_unref(now);
            break;
        }
    } while (gtk_tree_model_iter_next(model, &iter));
}

static void on_btn_start_clicked(G_GNUC_UNUSED GtkButton *btn,
                                 G_GNUC_UNUSED gpointer data)
{
    if (!selected_server)
        return;
    gchar *cmd = build_action_cmd(selected_server, "start", "start");
    if (cmd) {
        run_server_command(cmd);
        g_free(cmd);
        update_last_boot();
    }
}

static void on_btn_stop_clicked(G_GNUC_UNUSED GtkButton *btn,
                                G_GNUC_UNUSED gpointer data)
{
    if (!selected_server)
        return;
    gchar *cmd = build_action_cmd(selected_server, "stop", "stop");
    if (cmd) {
        run_server_command(cmd);
        g_free(cmd);
    }
}

static void on_btn_reload_clicked(G_GNUC_UNUSED GtkButton *btn,
                                  G_GNUC_UNUSED gpointer data)
{
    if (!selected_server)
        return;
    gchar *cmd = build_action_cmd(selected_server, "reload", "reload");
    if (cmd) {
        run_server_command(cmd);
        g_free(cmd);
    }
}

static void on_btn_ping_clicked(G_GNUC_UNUSED GtkButton *btn,
                                G_GNUC_UNUSED gpointer data)
{
    if (!selected_server)
        return;
    if (!selected_server->ping || !*selected_server->ping) {
        msgwin_msg_add(COLOR_RED, -1, NULL, "geanyservers: no ping command configured");
        return;
    }
    run_server_command(selected_server->ping);
}

static void on_btn_logs_clicked(G_GNUC_UNUSED GtkButton *btn,
                                G_GNUC_UNUSED gpointer data)
{
    if (!selected_server || !selected_server->log || !*selected_server->log)
        return;
    gchar *cmd = g_strdup_printf("tail -f %s", selected_server->log);
    run_server_command(cmd);
    g_free(cmd);
}

static void on_btn_kill_clicked(G_GNUC_UNUSED GtkButton *btn,
                                G_GNUC_UNUSED gpointer data)
{
    if (!selected_arse || !selected_arse->kill_cmd || !*selected_arse->kill_cmd)
        return;
    run_server_command(selected_arse->kill_cmd);
}

static void connect_button_signals(void)
{
    g_signal_connect(btn_start,  "clicked", G_CALLBACK(on_btn_start_clicked),  NULL);
    g_signal_connect(btn_stop,   "clicked", G_CALLBACK(on_btn_stop_clicked),   NULL);
    g_signal_connect(btn_reload, "clicked", G_CALLBACK(on_btn_reload_clicked), NULL);
    g_signal_connect(btn_ping,   "clicked", G_CALLBACK(on_btn_ping_clicked),   NULL);
    g_signal_connect(btn_logs,   "clicked", G_CALLBACK(on_btn_logs_clicked),   NULL);
    g_signal_connect(btn_kill,   "clicked", G_CALLBACK(on_btn_kill_clicked),   NULL);
}


/* ------------------------------------------------------------------ */
/* File-change watchers (Phase 5)                                     */

typedef struct {
    ServerDef   *server;
    gchar       *method;
    guint        debounce_id;
} WatchCtx;

static GPtrArray *file_monitors = NULL; /* GPtrArray of GFileMonitor* */
static GPtrArray *watch_ctxs    = NULL; /* GPtrArray of WatchCtx*     */

static gboolean on_debounce_fire(gpointer data)
{
    WatchCtx  *ctx = data;
    ServerDef *s   = ctx->server;
    gchar     *cmd = build_action_cmd(s, ctx->method, ctx->method);
    if (cmd) {
        run_server_command(cmd);
        g_free(cmd);
    }
    ctx->debounce_id = 0;
    return G_SOURCE_REMOVE;
}

static void on_file_changed(G_GNUC_UNUSED GFileMonitor *monitor,
                             G_GNUC_UNUSED GFile *file,
                             G_GNUC_UNUSED GFile *other,
                             GFileMonitorEvent    event_type,
                             gpointer             data)
{
    if (event_type != G_FILE_MONITOR_EVENT_CHANGED &&
        event_type != G_FILE_MONITOR_EVENT_CREATED)
        return;

    WatchCtx *ctx = data;
    if (ctx->debounce_id)
        g_source_remove(ctx->debounce_id);
    ctx->debounce_id = g_timeout_add(500, on_debounce_fire, ctx);
}

static void setup_file_monitors(void)
{
    if (!file_monitors) {
        file_monitors = g_ptr_array_new_with_free_func(g_object_unref);
        watch_ctxs    = g_ptr_array_new_with_free_func(g_free);
    }

    GeanyProject *proj = geany->app->project;
    if (!servers)
        return;

    for (guint i = 0; i < servers->len; i++) {
        ServerDef *s = g_ptr_array_index(servers, i);
        if (!s->changes || !*s->changes)
            continue;

        /* Format: "method path1;path2" */
        gchar **parts = g_strsplit(s->changes, " ", 2);
        if (!parts[0] || !parts[1]) {
            g_strfreev(parts);
            continue;
        }
        gchar *method = parts[0];
        gchar **paths = g_strsplit(parts[1], ";", -1);

        for (gint pi = 0; paths[pi]; pi++) {
            gchar *p = g_strstrip(paths[pi]);
            if (!*p)
                continue;

            gchar *full_path;
            if (g_path_is_absolute(p) || !proj || !proj->base_path) {
                full_path = g_strdup(p);
            } else {
                full_path = g_build_filename(proj->base_path, p, NULL);
            }

            GFile        *gf  = g_file_new_for_path(full_path);
            GFileMonitor *mon = g_file_monitor(gf, G_FILE_MONITOR_NONE,
                                               NULL, NULL);
            g_object_unref(gf);
            g_free(full_path);

            if (!mon)
                continue;

            WatchCtx *ctx    = g_new0(WatchCtx, 1);
            ctx->server      = s;
            ctx->method      = g_strdup(method);
            ctx->debounce_id = 0;
            g_ptr_array_add(watch_ctxs, ctx);

            g_signal_connect(mon, "changed", G_CALLBACK(on_file_changed), ctx);
            g_ptr_array_add(file_monitors, mon);
        }

        g_strfreev(paths);
        g_strfreev(parts);
    }
}

static void teardown_file_monitors(void)
{
    if (watch_ctxs) {
        for (guint i = 0; i < watch_ctxs->len; i++) {
            WatchCtx *ctx = g_ptr_array_index(watch_ctxs, i);
            if (ctx->debounce_id) {
                g_source_remove(ctx->debounce_id);
                ctx->debounce_id = 0;
            }
            g_free(ctx->method);
        }
        g_ptr_array_free(watch_ctxs, TRUE);
        watch_ctxs = NULL;
    }
    if (file_monitors) {
        for (guint i = 0; i < file_monitors->len; i++) {
            GFileMonitor *mon = g_ptr_array_index(file_monitors, i);
            g_file_monitor_cancel(mon);
        }
        g_ptr_array_free(file_monitors, TRUE);
        file_monitors = NULL;
    }
}


/* ------------------------------------------------------------------ */
/* Project lifecycle signals (Phase 5)                                */

static void fire_lifecycle_actions(const gchar *trigger)
{
    if (!servers)
        return;
    for (guint i = 0; i < servers->len; i++) {
        ServerDef *s = g_ptr_array_index(servers, i);
        const gchar *action = (g_strcmp0(trigger, "open") == 0) ? s->on_open : s->on_close;
        if (!action || !*action)
            continue;

        gchar *cmd = build_action_cmd(s, action, action);
        if (cmd) {
            run_server_command(cmd);
            g_free(cmd);
        }
    }
}

static void reload_and_refresh(void)
{
    teardown_file_monitors();
    selected_server = NULL;
    servers_load_config();
    populate_list_store();
    update_button_sensitivity();
    setup_file_monitors();
}

static void on_project_open(G_GNUC_UNUSED GObject *obj,
                             G_GNUC_UNUSED GKeyFile *config,
                             G_GNUC_UNUSED gpointer data)
{
    reload_and_refresh();
    fire_lifecycle_actions("open");
}

static void on_project_close(G_GNUC_UNUSED GObject *obj,
                              G_GNUC_UNUSED gpointer data)
{
    fire_lifecycle_actions("close");
    reload_and_refresh();
}


/* ------------------------------------------------------------------ */
/* Plugin lifecycle                                                    */

static gboolean gs_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer data)
{
    geany_plugin = plugin;
    geany_data   = plugin->geany_data;

    dots_init();
    servers_load_config();

    tab_widget = create_servers_tab();
    connect_button_signals();

    /* Log loaded servers to Messages window */
    if (servers) {
        for (guint i = 0; i < servers->len; i++) {
            ServerDef *s = g_ptr_array_index(servers, i);
            msgwin_msg_add(COLOR_BLACK, -1, NULL, "%s", s->display);
        }
    }

    /* \xf0\x9f\x96\xa5 = 🖥 */
    GtkWidget *label = gtk_label_new("\xf0\x9f\x96\xa5 Servers");
    tab_index = gtk_notebook_append_page(
        GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
        tab_widget, label);

    /* Refresh dots when the tab is focused */
    g_signal_connect(geany_data->main_widgets->message_window_notebook,
                     "switch-page",
                     G_CALLBACK(on_tab_switched), NULL);

    plugin_signal_connect(plugin, geany->object, "project-open",  TRUE,
                          G_CALLBACK(on_project_open),  NULL);
    plugin_signal_connect(plugin, geany->object, "project-close", TRUE,
                          G_CALLBACK(on_project_close), NULL);

    setup_file_monitors();

    return TRUE;
}

static void gs_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin,
                        G_GNUC_UNUSED gpointer data)
{
    /* Cancel in-flight async checks */
    if (check_cancellable) {
        g_cancellable_cancel(check_cancellable);
        g_object_unref(check_cancellable);
        check_cancellable = NULL;
    }

    teardown_file_monitors();

    g_signal_handlers_disconnect_by_func(
        geany_data->main_widgets->message_window_notebook,
        G_CALLBACK(on_tab_switched), NULL);

    if (tab_index >= 0) {
        gtk_notebook_remove_page(
            GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
            tab_index);
        tab_widget = NULL;
        tab_index  = -1;
    }

    /* list_store is owned by the tree view widget, freed with it above */
    list_store      = NULL;
    tree_view       = NULL;
    selected_server = NULL;
    selected_arse   = NULL;
    btn_start = btn_stop = btn_reload = btn_ping = btn_logs = btn_kill = NULL;

    dots_free();
    servers_free_config();
}


/* ------------------------------------------------------------------ */

G_MODULE_EXPORT void geany_load_module(GeanyPlugin *plugin)
{
    plugin->info->name        = "Servers";
    plugin->info->description =
        "Server management tab in the message window. "
        "Reads servertools.conf, shows live status/ping dots, "
        "and provides start/stop/reload/ping/log actions.";
    plugin->info->version     = "0.1";
    plugin->info->author      = "teknopaul";

    plugin->funcs->init    = gs_init;
    plugin->funcs->cleanup = gs_cleanup;

    GEANY_PLUGIN_REGISTER(plugin, 235);
}
