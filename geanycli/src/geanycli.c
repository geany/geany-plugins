/*
 * geanycli.c — Geany plugin: tabbed terminals in the message window
 *
 * Adds a "Cli" tab to the message window.  Inside that tab a second
 * GtkNotebook holds multiple VTE terminal instances.  Each terminal opens
 * a login shell at the current project root (or $HOME when no project is
 * open).  If the shell exits it is automatically restarted.
 *
 * Inter-plugin IPC — two equivalent mechanisms are available:
 *
 *   1) GLib signal (no header required):
 *        g_signal_emit_by_name(geany->object,
 *                              "geanycli-run-command",
 *                              "make", FALSE);
 *
 *   2) Direct symbol call (resolves via GModule at runtime):
 *        typedef void (*GtRunCmdFn)(const gchar *, gboolean);
 *        GtRunCmdFn fn;
 *        if (g_module_symbol(other_plugin->module,
 *                            "geanycli_run_command",
 *                            (gpointer *)&fn))
 *            fn("make", FALSE);
 *
 *   Signal / function parameters:
 *     cmd     – shell command string to execute
 *     new_tab – TRUE  → open a fresh terminal tab then run cmd
 *               FALSE → run cmd in the currently active tab
 *
 * Copyright 2025 teknopaul
 * License: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/wait.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

#include <geanyplugin.h>

#include "geanycli.h"


/* ------------------------------------------------------------------ */

GeanyPlugin  *geany_plugin;
GeanyData    *geany_data;


/* ------------------------------------------------------------------ */
/* Per-tab state                                                       */

typedef struct {
    VteTerminal *vte;
    GtkWidget   *hbox;   /* terminal + scrollbar; used as the notebook page */
    GtkWidget   *label;  /* GtkLabel in the tab header, for renaming */
    GPid         child_pid; /* PID of the shell; 0 when no child is running */
} TermTab;

/* Carries (tab, cmd) across the async spawn boundary */
typedef struct {
    TermTab *tab;
    gchar   *cmd;
} SpawnData;


/* ------------------------------------------------------------------ */
/* Module globals                                                      */

static GtkNotebook *term_nb      = NULL;  /* inner notebook with terminal pages */
static GtkWidget   *outer_widget = NULL;  /* the widget appended to msg notebook */
static gint         outer_idx    = -1;    /* page index in the message notebook   */
static gint         tab_counter  = 0;     /* monotonically-increasing label counter */
static gboolean     active       = FALSE; /* set FALSE during cleanup to stop restarts */

static GKeyFile  *tools_config    = NULL; /* merged user + project filetypetools.conf */
static GtkWidget *tools_menu_item = NULL; /* "File Tools" entry appended to Tools menu */
static gulong     window_key_handler_id  = 0; /* handler ID for main-window key intercept */
static gulong     msg_nb_switch_handler  = 0; /* handler ID for message-notebook page switch */


/* ------------------------------------------------------------------ */
/* Helpers                                                             */

static void on_msg_nb_switch_page(G_GNUC_UNUSED GtkNotebook *nb,
                                  G_GNUC_UNUSED GtkWidget   *page,
                                  guint                      page_num,
                                  G_GNUC_UNUSED gpointer     data)
{
    if ((gint)page_num != outer_idx || !term_nb)
        return;
    gint inner = gtk_notebook_get_current_page(term_nb);
    if (inner < 0)
        return;
    GtkWidget *inner_page = gtk_notebook_get_nth_page(term_nb, inner);
    if (!inner_page)
        return;
    TermTab *tab = g_object_get_data(G_OBJECT(inner_page), "termtab");
    if (tab)
        gtk_widget_grab_focus(GTK_WIDGET(tab->vte));
}

static gchar *get_work_dir(void)
{
    GeanyApp *app = geany->app;
    if (app->project && app->project->base_path && *app->project->base_path)
        return g_strdup(app->project->base_path);
    return g_strdup(g_get_home_dir());
}

static void on_spawn_ready(VteTerminal *vte,
                           GPid         pid,
                           GError      *error,
                           gpointer     user_data)
{
    SpawnData *sd = user_data;
    sd->tab->child_pid = error ? 0 : pid;
    if (!error && sd->cmd && *sd->cmd)
    {
        vte_terminal_feed_child(VTE_TERMINAL(vte), "\x15", 1);
        vte_terminal_feed_child(VTE_TERMINAL(vte), sd->cmd, -1);
        vte_terminal_feed_child(VTE_TERMINAL(vte), "\n", 1);
    }
    g_free(sd->cmd);
    g_free(sd);
}

static void spawn_shell(TermTab *tab, const gchar *cmd)
{
    const gchar *shell = g_getenv("SHELL");
    if (!shell || !*shell)
        shell = "/bin/sh";

    gchar *argv[2] = { (gchar *)shell, NULL };

    gchar *wd = get_work_dir();

    const gchar *exclude[] = { "COLUMNS", "LINES", "TERM", "TERM_PROGRAM", NULL };
    gchar **env = utils_copy_environment(exclude, "TERM", "xterm-256color", NULL);

    SpawnData *sd = g_new0(SpawnData, 1);
    sd->tab = tab;
    sd->cmd = g_strdup(cmd);

    vte_terminal_spawn_async(tab->vte,
                             VTE_PTY_DEFAULT,
                             wd,
                             argv,
                             env,
                             G_SPAWN_SEARCH_PATH,
                             NULL, NULL, NULL,
                             -1, NULL,
                             on_spawn_ready, sd);
    g_strfreev(env);
    g_free(wd);
}


/* ------------------------------------------------------------------ */
/* Signal handlers                                                     */

static void on_child_exited(VteTerminal *vte,
                             G_GNUC_UNUSED gint status,
                             gpointer user_data)
{
    if (!active)
        return;
    TermTab *tab = user_data;
    tab->child_pid = 0;
    vte_terminal_feed(vte, "\r\n[process exited — restarting shell]\r\n", -1);
    spawn_shell(tab, NULL);
}

static void on_close_btn_clicked(G_GNUC_UNUSED GtkButton *btn, gpointer user_data)
{
    if (!term_nb)
        return;
    if (gtk_notebook_get_n_pages(term_nb) <= 1)
        return; /* always keep at least one terminal */

    TermTab *tab = user_data;
    g_signal_handlers_disconnect_by_func(tab->vte,
                                         G_CALLBACK(on_child_exited), tab);

    gint page = gtk_notebook_page_num(term_nb, tab->hbox);
    if (page >= 0)
        gtk_notebook_remove_page(term_nb, page);

    g_free(tab);
}


/* ------------------------------------------------------------------ */
/* Right-click context menu                                            */

static void on_copy_activate(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data)
{
#if VTE_CHECK_VERSION(0, 50, 0)
    vte_terminal_copy_clipboard_format(VTE_TERMINAL(user_data), VTE_FORMAT_TEXT);
#else
    vte_terminal_copy_clipboard(VTE_TERMINAL(user_data));
#endif
}

static void on_paste_activate(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data)
{
    vte_terminal_paste_clipboard(VTE_TERMINAL(user_data));
}

static void on_send_to_agent_activate(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data)
{
    const gchar *text = (const gchar *)user_data;
    if (!text || !*text)
        return;
    GType obj_type = G_OBJECT_TYPE(geany->object);
    if (g_signal_lookup("geanyagent-insert-to-agent", obj_type))
        g_signal_emit_by_name(geany->object, "geanyagent-insert-to-agent", text);
}

static void on_rename_tab_activate(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data)
{
    TermTab *tab = user_data;
    if (!tab || !tab->label)
        return;

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        _("Rename Tab"),
        GTK_WINDOW(geany_data->main_widgets->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Rename"), GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry),
                       gtk_label_get_text(GTK_LABEL(tab->label)));
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_add(GTK_CONTAINER(content), entry);
    gtk_widget_show_all(content);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
        if (text && *text)
            gtk_label_set_text(GTK_LABEL(tab->label), text);
    }
    gtk_widget_destroy(dialog);
}

static gboolean on_vte_key_press(GtkWidget *widget,
                                  GdkEventKey *event,
                                  gpointer user_data)
{
    (void)widget;
    (void)user_data;
    /* Per-tab key handler — reserved for future per-tab shortcuts.
     * Ctrl+C / Ctrl+X interception is handled at the window level in
     * on_window_key_press() because Geany's accelerators (Edit>Copy,
     * Edit>Cut) consume these keys before GTK propagates them here. */
    (void)event;
    return FALSE;
}

/* Window-level key handler.  Connected to the main Geany window so we run
 * before the window's own default handler processes accelerators.
 * Only acts when a VTE terminal has keyboard focus. */
static gboolean on_window_key_press(GtkWidget *window,
                                     GdkEventKey *event,
                                     G_GNUC_UNUSED gpointer data)
{
    GtkWidget *focused = gtk_window_get_focus(GTK_WINDOW(window));
    if (!focused || !VTE_IS_TERMINAL(focused))
        return FALSE;

    /* Ctrl+C without Shift → send ETX (interrupt). */
    if ((event->state & GDK_CONTROL_MASK) &&
        !(event->state & (GDK_SHIFT_MASK | GDK_MOD1_MASK)) &&
        (event->keyval == GDK_KEY_c || event->keyval == GDK_KEY_C))
    {
        vte_terminal_feed_child(VTE_TERMINAL(focused), "\x03", 1);
        return TRUE;
    }
    /* Ctrl+X without Shift → send CAN (0x18). */
    if ((event->state & GDK_CONTROL_MASK) &&
        !(event->state & (GDK_SHIFT_MASK | GDK_MOD1_MASK)) &&
        (event->keyval == GDK_KEY_x || event->keyval == GDK_KEY_X))
    {
        vte_terminal_feed_child(VTE_TERMINAL(focused), "\x18", 1);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_vte_button_press(GtkWidget *widget,
                                    GdkEventButton *event,
                                    gpointer data)
{
    if (event->button != 3)
        return FALSE;

    VteTerminal *vte     = VTE_TERMINAL(widget);
    TermTab     *tab     = data;
    gboolean     has_sel = vte_terminal_get_has_selection(vte);

    if (has_sel)
    {
#if VTE_CHECK_VERSION(0, 50, 0)
        vte_terminal_copy_clipboard_format(vte, VTE_FORMAT_TEXT);
#else
        vte_terminal_copy_clipboard(vte);
#endif
    }

    GtkWidget *menu = gtk_menu_new();

    GtkWidget *copy_item = gtk_menu_item_new_with_label(_("Copy"));
    gtk_widget_set_sensitive(copy_item, has_sel);
    g_signal_connect(copy_item, "activate", G_CALLBACK(on_copy_activate), vte);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_item);

    GtkWidget *paste_item = gtk_menu_item_new_with_label(_("Paste"));
    g_signal_connect(paste_item, "activate", G_CALLBACK(on_paste_activate), vte);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste_item);

    if (has_sel)
    {
        GType obj_type = G_OBJECT_TYPE(geany->object);
        if (g_signal_lookup("geanyagent-insert-to-agent", obj_type))
        {
            GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            gchar        *clip_text = gtk_clipboard_wait_for_text(clipboard);
            if (clip_text && *g_strstrip(clip_text))
            {
                GtkWidget *send_item = gtk_menu_item_new_with_label(_("Send to Agent"));
                g_signal_connect_data(send_item, "activate",
                                      G_CALLBACK(on_send_to_agent_activate),
                                      clip_text, (GClosureNotify)g_free, 0);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), send_item);
            }
            else
                g_free(clip_text);
        }
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    GtkWidget *rename_item = gtk_menu_item_new_with_label(_("Rename tab\xe2\x80\xa6"));
    g_signal_connect(rename_item, "activate", G_CALLBACK(on_rename_tab_activate), tab);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), rename_item);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);

    return TRUE;
}


/* ------------------------------------------------------------------ */
/* Tab creation                                                        */

static void on_stop_btn_clicked(G_GNUC_UNUSED GtkButton *btn, gpointer user_data)
{
    TermTab *tab = user_data;
    /* Use the PTY's foreground process group — this is the process actually
     * running in the terminal (not the shell itself, which ignores SIGINT
     * while waiting for a child). */
    VtePty *pty = vte_terminal_get_pty(tab->vte);
    if (!pty)
        return;
    int fd = vte_pty_get_fd(pty);
    if (fd < 0)
        return;
    pid_t fg = tcgetpgrp(fd);
    if (fg > 0)
        kill(-fg, SIGINT);
}

/* name=NULL → auto-label "Term N"; non-NULL → use that string as tab label. */
static TermTab *create_tab(const gchar *cmd, const gchar *name)
{
    TermTab *tab = g_new0(TermTab, 1);

    /* Terminal widget */
    GtkWidget *vte_widget = vte_terminal_new();
    tab->vte = VTE_TERMINAL(vte_widget);

    vte_terminal_set_size(tab->vte, 80, 24);
    vte_terminal_set_mouse_autohide(tab->vte, TRUE);
    vte_terminal_set_scrollback_lines(tab->vte, 10000);
    gtk_widget_set_size_request(vte_widget, 10, 10);

    g_signal_connect(tab->vte, "child-exited",
                     G_CALLBACK(on_child_exited), tab);
    g_signal_connect(vte_widget, "key-press-event",
                     G_CALLBACK(on_vte_key_press), tab);
    g_signal_connect(vte_widget, "button-press-event",
                     G_CALLBACK(on_vte_button_press), tab);

    /* Scrollbar */
    GtkWidget *scrollbar = gtk_scrollbar_new(
        GTK_ORIENTATION_VERTICAL,
        gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(vte_widget)));
    gtk_widget_set_can_focus(scrollbar, FALSE);

    /* Page widget */
    tab->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab->hbox), vte_widget, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(tab->hbox), scrollbar, FALSE, FALSE, 0);
    gtk_widget_show_all(tab->hbox);

    /* Store back-pointer so cleanup can find the TermTab from the page widget */
    g_object_set_data(G_OBJECT(tab->hbox), "termtab", tab);

    gchar *title = name ? g_strdup(name)
                        : (tab_counter++, g_strdup_printf("Term %d", tab_counter));

    GtkWidget *lbl_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *label    = gtk_label_new(title);
    tab->label = label;
    GtkWidget *close_btn = gtk_button_new();
    GtkWidget *close_img = gtk_image_new_from_icon_name("window-close-symbolic",
                                                          GTK_ICON_SIZE_MENU);
    g_free(title);

    gtk_button_set_image(GTK_BUTTON(close_btn), close_img);
    gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click(GTK_BUTTON(close_btn), FALSE);
    g_signal_connect(close_btn, "clicked",
                     G_CALLBACK(on_close_btn_clicked), tab);

    GtkWidget *stop_btn = gtk_button_new();
    GtkWidget *stop_img = gtk_image_new_from_icon_name("process-stop-symbolic",
                                                        GTK_ICON_SIZE_MENU);
    gtk_button_set_image(GTK_BUTTON(stop_btn), stop_img);
    gtk_button_set_relief(GTK_BUTTON(stop_btn), GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click(GTK_BUTTON(stop_btn), FALSE);
    g_signal_connect(stop_btn, "clicked",
                     G_CALLBACK(on_stop_btn_clicked), tab);

    gtk_box_pack_start(GTK_BOX(lbl_hbox), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(lbl_hbox), stop_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(lbl_hbox), close_btn, FALSE, FALSE, 0);
    gtk_widget_show_all(lbl_hbox);

    gtk_notebook_append_page(term_nb, tab->hbox, lbl_hbox);
    gtk_notebook_set_tab_reorderable(term_nb, tab->hbox, TRUE);

    spawn_shell(tab, cmd);
    return tab;
}


/* ------------------------------------------------------------------ */
/* IPC implementation                                                  */

static void do_run_command(const gchar *cmd, gboolean new_tab)
{
    if (!cmd || !*cmd || !term_nb)
        return;

    /* Bring Terminal tab to front in the message window */
    if (outer_idx >= 0)
        gtk_notebook_set_current_page(
            GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
            outer_idx);

    if (new_tab) {
        create_tab(cmd, NULL);
        gtk_notebook_set_current_page(term_nb,
                                       gtk_notebook_get_n_pages(term_nb) - 1);
        return;  /* command will be fed once the shell spawns */
    }

    gint page = gtk_notebook_get_current_page(term_nb);
    if (page < 0)
        return;

    GtkWidget *page_widget = gtk_notebook_get_nth_page(term_nb, page);
    if (!page_widget)
        return;

    TermTab *tab = g_object_get_data(G_OBJECT(page_widget), "termtab");
    if (!tab)
        return;

    /* Ctrl-U clears any partial input already on the prompt line */
    vte_terminal_feed_child(tab->vte, "\x15", 1);
    vte_terminal_feed_child(tab->vte, cmd, -1);
    vte_terminal_feed_child(tab->vte, "\n", 1);
}

/* Signal handler — other plugins emit "geanycli-run-command" on geany->object */
static void on_ipc_signal(G_GNUC_UNUSED GObject *obj,
                          const gchar *cmd,
                          gboolean new_tab,
                          G_GNUC_UNUSED gpointer data)
{
    do_run_command(cmd, new_tab);
}

/* Exported symbol — other plugins can call this via GModule symbol lookup */
G_MODULE_EXPORT void geanycli_run_command(const gchar *cmd, gboolean new_tab)
{
    do_run_command(cmd, new_tab);
}

/* Feed cmd to every open terminal tab (used for env-var export propagation) */
static void do_run_all_tabs(const gchar *cmd)
{
    if (!cmd || !*cmd || !term_nb)
        return;
    gint n = gtk_notebook_get_n_pages(term_nb);
    for (gint i = 0; i < n; i++) {
        GtkWidget *pg = gtk_notebook_get_nth_page(term_nb, i);
        if (!pg) continue;
        TermTab *tab = g_object_get_data(G_OBJECT(pg), "termtab");
        if (!tab || !tab->child_pid) continue;
        vte_terminal_feed_child(tab->vte, "\x15", 1);
        vte_terminal_feed_child(tab->vte, cmd, -1);
        vte_terminal_feed_child(tab->vte, "\n", 1);
    }
}

/* Signal handler — other plugins emit "geanycli-run-all-tabs" on geany->object */
static void on_ipc_all_signal(G_GNUC_UNUSED GObject *obj,
                               const gchar *cmd,
                               G_GNUC_UNUSED gpointer data)
{
    do_run_all_tabs(cmd);
}

/* Exported symbol — feed cmd to every terminal tab */
G_MODULE_EXPORT void geanycli_run_all_tabs(const gchar *cmd)
{
    do_run_all_tabs(cmd);
}

/* Feed text to the active terminal tab without appending a newline. */
static void do_feed_text(const gchar *text)
{
    if (!text || !*text || !term_nb)
        return;

    gint page = gtk_notebook_get_current_page(term_nb);
    if (page < 0)
        return;

    GtkWidget *page_widget = gtk_notebook_get_nth_page(term_nb, page);
    if (!page_widget)
        return;

    TermTab *tab = g_object_get_data(G_OBJECT(page_widget), "termtab");
    if (!tab)
        return;

    if (outer_idx >= 0)
        gtk_notebook_set_current_page(
            GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
            outer_idx);

    vte_terminal_feed_child(tab->vte, text, -1);
    gtk_widget_grab_focus(GTK_WIDGET(tab->vte));
}

static void on_ipc_feed_signal(G_GNUC_UNUSED GObject *obj,
                               const gchar *text,
                               G_GNUC_UNUSED gpointer data)
{
    do_feed_text(text);
}

G_MODULE_EXPORT void geanycli_feed_text(const gchar *text)
{
    do_feed_text(text);
}

/* ------------------------------------------------------------------ */
/* Named-tab routing                                                   */

/* Find an existing inner tab by its label text; returns NULL if not found. */
static TermTab *find_tab_by_name(const gchar *name)
{
    if (!term_nb || !name)
        return NULL;
    gint n = gtk_notebook_get_n_pages(term_nb);
    for (gint i = 0; i < n; i++) {
        GtkWidget *page = gtk_notebook_get_nth_page(term_nb, i);
        TermTab   *tab  = g_object_get_data(G_OBJECT(page), "termtab");
        if (tab && tab->label) {
            const gchar *lbl = gtk_label_get_text(GTK_LABEL(tab->label));
            if (g_strcmp0(lbl, name) == 0)
                return tab;
        }
    }
    return NULL;
}

/* Run cmd in a tab whose label matches name; create one if it doesn't exist. */
static void do_run_in_named_tab(const gchar *cmd, const gchar *name)
{
    if (!cmd || !*cmd || !term_nb)
        return;

    if (outer_idx >= 0)
        gtk_notebook_set_current_page(
            GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
            outer_idx);

    TermTab *tab = find_tab_by_name(name);
    if (tab) {
        gint page = gtk_notebook_page_num(term_nb, tab->hbox);
        if (page >= 0)
            gtk_notebook_set_current_page(term_nb, page);
        vte_terminal_feed_child(tab->vte, "\x15", 1);
        vte_terminal_feed_child(tab->vte, cmd, -1);
        vte_terminal_feed_child(tab->vte, "\n", 1);
    } else {
        create_tab(cmd, name);
        gtk_notebook_set_current_page(term_nb, gtk_notebook_get_n_pages(term_nb) - 1);
    }
}


/* ------------------------------------------------------------------ */
/* Captured-output routing (_Compiler, _Messages, _Status)            */

static gboolean on_compiler_io(GIOChannel *ch, GIOCondition cond,
                                G_GNUC_UNUSED gpointer data)
{
    if (cond & G_IO_IN) {
        gchar *line = NULL;
        while (g_io_channel_read_line(ch, &line, NULL, NULL, NULL) == G_IO_STATUS_NORMAL) {
            g_strchomp(line);
            msgwin_compiler_add_string(COLOR_BLACK, line);
            g_free(line);
            line = NULL;
        }
    }
    return !(cond & (G_IO_HUP | G_IO_ERR));
}

static void run_in_compiler(const gchar *cmd)
{
    gchar *argv[]  = { (gchar *)"/bin/sh", (gchar *)"-c", (gchar *)cmd, NULL };
    gint   out_fd  = -1;
    GError *err    = NULL;

    msgwin_clear_tab(MSG_COMPILER);
    msgwin_switch_tab(MSG_COMPILER, TRUE);
    msgwin_compiler_add_string(COLOR_BLUE, cmd);

    if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                                   NULL, NULL, NULL,
                                   NULL, &out_fd, NULL, &err)) {
        msgwin_compiler_add_string(COLOR_RED, err->message);
        g_error_free(err);
        return;
    }

    GIOChannel *ch = g_io_channel_unix_new(out_fd);
    g_io_channel_set_flags(ch, G_IO_FLAG_NONBLOCK, NULL);
    g_io_channel_set_close_on_unref(ch, TRUE);
    g_io_add_watch(ch, G_IO_IN | G_IO_HUP | G_IO_ERR, on_compiler_io, NULL);
    g_io_channel_unref(ch);
}

/* Shared state for async _Messages jobs. */
typedef struct {
    GString  *output;
    GPid      pid;
    gboolean  io_done;
    gboolean  child_done;
    gint      exit_status;
    gchar    *cmd_display;
} MessagesJob;

static void messages_job_finish(MessagesJob *job)
{
    if (!job->io_done || !job->child_done)
        return;

    gchar **lines = g_strsplit(job->output->str, "\n", -1);
    gint    n     = g_strv_length(lines);
    gint    last[2] = { -1, -1 };
    for (gint i = 0; i < n; i++) {
        if (*g_strstrip(lines[i])) {
            last[0] = last[1];
            last[1] = i;
        }
    }
    msgwin_switch_tab(MSG_MESSAGE, TRUE);
    for (gint j = 0; j < 2; j++) {
        if (last[j] >= 0)
            msgwin_msg_add_string(COLOR_BLACK, -1, NULL, lines[last[j]]);
    }
    g_strfreev(lines);

    gboolean ok  = WIFEXITED(job->exit_status) && WEXITSTATUS(job->exit_status) == 0;
    const gchar *dot = ok ? "\xf0\x9f\x9f\xa2" : "\xf0\x9f\x9f\xa5";  /* 🟢 🔴 */
    gchar *summary = g_strdup_printf("%s %s", dot, job->cmd_display ? job->cmd_display : "");
    msgwin_msg_add_string(ok ? COLOR_BLACK : COLOR_RED, -1, NULL, summary);
    g_free(summary);

    g_string_free(job->output, TRUE);
    g_free(job->cmd_display);
    g_spawn_close_pid(job->pid);
    g_free(job);
}

static gboolean on_messages_io(GIOChannel *ch, GIOCondition cond, gpointer data)
{
    MessagesJob *job = data;
    if (cond & G_IO_IN) {
        gchar *line = NULL;
        while (g_io_channel_read_line(ch, &line, NULL, NULL, NULL) == G_IO_STATUS_NORMAL) {
            g_string_append(job->output, line);
            g_free(line);
            line = NULL;
        }
    }
    if (cond & (G_IO_HUP | G_IO_ERR)) {
        job->io_done = TRUE;
        messages_job_finish(job);
        return FALSE;
    }
    return TRUE;
}

static void on_messages_child_exit(GPid pid, gint status, gpointer data)
{
    MessagesJob *job   = data;
    job->exit_status   = status;
    job->child_done    = TRUE;
    g_spawn_close_pid(pid);
    messages_job_finish(job);
}

static void run_in_messages(const gchar *cmd)
{
    gchar *argv[]  = { (gchar *)"/bin/sh", (gchar *)"-c", (gchar *)cmd, NULL };
    gint   out_fd  = -1;
    GPid   pid;
    GError *err    = NULL;

    if (!g_spawn_async_with_pipes(NULL, argv, NULL,
                                   G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                   NULL, NULL, &pid,
                                   NULL, &out_fd, NULL, &err)) {
        msgwin_msg_add_string(COLOR_RED, -1, NULL, err->message);
        g_error_free(err);
        return;
    }

    MessagesJob *job = g_new0(MessagesJob, 1);
    job->output      = g_string_new(NULL);
    job->pid         = pid;
    job->cmd_display = g_strdup(cmd);

    GIOChannel *ch = g_io_channel_unix_new(out_fd);
    g_io_channel_set_flags(ch, G_IO_FLAG_NONBLOCK, NULL);
    g_io_channel_set_close_on_unref(ch, TRUE);
    g_io_add_watch(ch, G_IO_IN | G_IO_HUP | G_IO_ERR, on_messages_io, job);
    g_io_channel_unref(ch);

    g_child_watch_add(pid, on_messages_child_exit, job);
}

typedef struct { gchar *cmd_display; } StatusJob;

static void on_status_child_exit(GPid pid, gint status, gpointer data)
{
    StatusJob *job   = data;
    gboolean   ok    = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    const gchar *dot = ok ? "\xf0\x9f\x9f\xa2" : "\xf0\x9f\x9f\xa5";
    gchar *msg = g_strdup_printf("%s %s", dot, job->cmd_display);
    msgwin_status_add_string(msg);
    g_free(msg);
    g_free(job->cmd_display);
    g_free(job);
    g_spawn_close_pid(pid);
}

static void run_in_status(const gchar *cmd)
{
    gchar *argv[] = { (gchar *)"/bin/sh", (gchar *)"-c", (gchar *)cmd, NULL };
    GPid   pid;
    GError *err   = NULL;

    if (!g_spawn_async(NULL, argv, NULL,
                       G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD |
                       G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                       NULL, NULL, &pid, &err)) {
        msgwin_status_add_string(err->message);
        g_error_free(err);
        return;
    }

    StatusJob *job     = g_new0(StatusJob, 1);
    job->cmd_display   = g_strdup(cmd);
    g_child_watch_add(pid, on_status_child_exit, job);
}

/* Central router: dispatch cmd based on tab_spec value from config. */
static void do_run_routed(const gchar *cmd, const gchar *tab_spec, const gchar *name)
{
    if (!cmd || !*cmd)
        return;

    if (!tab_spec || g_strcmp0(tab_spec, "_new") == 0) {
        do_run_in_named_tab(cmd, name && *name ? name : NULL);
    } else if (g_strcmp0(tab_spec, "_current") == 0) {
        do_run_command(cmd, FALSE);
    } else if (g_strcmp0(tab_spec, "_Compiler") == 0) {
        run_in_compiler(cmd);
    } else if (g_strcmp0(tab_spec, "_Messages") == 0) {
        run_in_messages(cmd);
    } else if (g_strcmp0(tab_spec, "_Status") == 0) {
        run_in_status(cmd);
    } else {
        /* Any other string (including _Terminal): treat as named tab */
        do_run_in_named_tab(cmd, tab_spec);
    }
}


/* ------------------------------------------------------------------ */
/* New-tab button                                                      */

static void on_new_tab_clicked(G_GNUC_UNUSED GtkButton *btn,
                               G_GNUC_UNUSED gpointer data)
{
    create_tab(NULL, NULL);
    gtk_notebook_set_current_page(term_nb,
                                   gtk_notebook_get_n_pages(term_nb) - 1);
}


/* ------------------------------------------------------------------ */
/* File-type tools                                                     */

#define TOOLS_CONFIG_FILENAME "filetypetools.conf"

/* Load ~/.config/geany/filetypetools.conf, then overlay
 * <project-root>/filetypetools.conf on top. */
static void tools_config_load(void)
{
    if (tools_config)
        g_key_file_free(tools_config);
    tools_config = g_key_file_new();

    gchar *user_path = g_build_filename(geany->app->configdir,
                                         TOOLS_CONFIG_FILENAME, NULL);
    g_key_file_load_from_file(tools_config, user_path, G_KEY_FILE_NONE, NULL);
    g_free(user_path);

    GeanyProject *proj = geany->app->project;
    if (!proj || !proj->base_path)
        return;

    gchar *proj_path = g_build_filename(proj->base_path,
                                         TOOLS_CONFIG_FILENAME, NULL);
    GKeyFile *pkf = g_key_file_new();
    if (g_key_file_load_from_file(pkf, proj_path, G_KEY_FILE_NONE, NULL)) {
        gsize ng;
        gchar **groups = g_key_file_get_groups(pkf, &ng);
        for (gsize gi = 0; gi < ng; gi++) {
            gsize nk;
            gchar **keys = g_key_file_get_keys(pkf, groups[gi], &nk, NULL);
            for (gsize ki = 0; ki < nk; ki++) {
                gchar *val = g_key_file_get_string(pkf, groups[gi],
                                                    keys[ki], NULL);
                g_key_file_set_string(tools_config, groups[gi],
                                      keys[ki], val);
                g_free(val);
            }
            g_strfreev(keys);
        }
        g_strfreev(groups);
    }
    g_key_file_free(pkf);
    g_free(proj_path);
}

/* Returns a pointer into filepath for the matching section key, e.g.
 * ".c.snip" or ".snip", trying longest suffix first.
 * Returns ".*" (literal) for the wildcard section, NULL for no match. */
static const gchar *tools_find_section(const gchar *filepath)
{
    if (!tools_config || !filepath)
        return NULL;

    const gchar *base = strrchr(filepath, G_DIR_SEPARATOR);
    base = base ? base + 1 : filepath;

    /* Walk from first dot rightward — longest match first */
    const gchar *dot = strchr(base, '.');
    while (dot) {
        if (g_key_file_has_group(tools_config, dot))
            return dot;
        dot = strchr(dot + 1, '.');
    }
    return g_key_file_has_group(tools_config, ".*") ? ".*" : NULL;
}

/* Expand %p %d %f %e %l %r %F in tmpl using filepath. Caller frees result.
 * When filepath is a directory, %d expands to the directory itself (not its parent). */
static gchar *tools_format_cmd(const gchar *tmpl, const gchar *filepath)
{
    GString *out  = g_string_sized_new(256);
    gboolean is_dir = g_file_test(filepath, G_FILE_TEST_IS_DIR);
    gchar   *dir  = is_dir ? g_strdup(filepath) : g_path_get_dirname(filepath);
    gchar   *base = g_path_get_basename(filepath);
    const gchar *first_dot = strchr(base, '.');
    gchar   *stem = first_dot ? g_strndup(base, (gsize)(first_dot - base))
                              : g_strdup(base);

    gint line = 1;
    GeanyDocument *doc = document_get_current();
    if (doc && doc->file_name && strcmp(doc->file_name, filepath) == 0)
        line = sci_get_current_line(doc->editor->sci) + 1;

    gchar *root = get_work_dir();

    /* %F: filepath relative to project root; falls back to absolute path */
    const gchar *rel = filepath;
    gsize root_len = strlen(root);
    if (g_str_has_prefix(filepath, root) && filepath[root_len] == '/')
        rel = filepath + root_len + 1;

    for (const gchar *p = tmpl; *p; p++) {
        if (*p != '%' || !*(p + 1)) {
            g_string_append_c(out, *p);
            continue;
        }
        p++;
        switch (*p) {
            case 'p': g_string_append(out, filepath);          break;
            case 'd': g_string_append(out, dir);               break;
            case 'f': g_string_append(out, base);              break;
            case 'e': g_string_append(out, stem);              break;
            case 'l': g_string_append_printf(out, "%d", line); break;
            case 'r': g_string_append(out, root);              break;
            case 'F': g_string_append(out, rel);               break;
            case '%': g_string_append_c(out, '%');             break;
            default:
                g_string_append_c(out, '%');
                g_string_append_c(out, *p);
        }
    }

    g_free(root);
    g_free(dir);
    g_free(base);
    g_free(stem);
    return g_string_free(out, FALSE);
}

/* Looks up filepath in filetypetools.conf, expands tool_0 for its section,
 * and runs the result.  Returns TRUE if a command was found and dispatched. */
G_MODULE_EXPORT gboolean geanycli_run_file(const gchar *filepath, gboolean new_tab)
{
    if (!filepath || !tools_config)
        return FALSE;
    const gchar *section = tools_find_section(filepath);
    if (!section)
        return FALSE;
    gchar *tmpl = g_key_file_get_string(tools_config, section, "tool_0", NULL);
    if (!tmpl)
        return FALSE;
    gchar *cmd = tools_format_cmd(tmpl, filepath);
    g_free(tmpl);
    do_run_command(cmd, new_tab);
    g_free(cmd);
    return TRUE;
}

static void on_run_file_signal(G_GNUC_UNUSED GObject *obj,
                               const gchar *filepath,
                               gboolean     new_tab,
                               G_GNUC_UNUSED gpointer data)
{
    geanycli_run_file(filepath, new_tab);
}

static void on_tool_item_activate(GtkMenuItem *item,
                                   G_GNUC_UNUSED gpointer data)
{
    const gchar *cmd  = g_object_get_data(G_OBJECT(item), "tool-cmd");
    const gchar *tab  = g_object_get_data(G_OBJECT(item), "tool-tab");
    const gchar *name = g_object_get_data(G_OBJECT(item), "tool-name");
    if (cmd)
        do_run_routed(cmd, tab, name);
}

/* Append tool items from one filetypetools.conf section to submenu.
 * Reads tool_N, name_N, and tab_N keys.  tab_N controls routing (default: _new).
 * Returns TRUE if at least one item was added. */
static gboolean tools_append_section_items(GtkWidget *submenu,
                                            const gchar *section,
                                            const gchar *path)
{
    gboolean any = FALSE;
    for (gint i = 0; ; i++) {
        gchar *tool_key = g_strdup_printf("tool_%d", i);
        gchar *tmpl     = g_key_file_get_string(tools_config, section, tool_key, NULL);
        g_free(tool_key);
        if (!tmpl)
            break;

        gchar *name_key = g_strdup_printf("name_%d", i);
        gchar *name     = g_key_file_get_string(tools_config, section, name_key, NULL);
        g_free(name_key);

        gchar *tab_key  = g_strdup_printf("tab_%d", i);
        gchar *tab      = g_key_file_get_string(tools_config, section, tab_key, NULL);
        g_free(tab_key);

        gchar     *cmd = tools_format_cmd(tmpl, path);
        GtkWidget *mi  = gtk_menu_item_new_with_label(name ? name : tmpl);
        gtk_widget_set_tooltip_text(mi, cmd);
        g_object_set_data_full(G_OBJECT(mi), "tool-cmd",  cmd,                          g_free);
        g_object_set_data_full(G_OBJECT(mi), "tool-tab",  tab  ? tab  : g_strdup("_new"), g_free);
        g_object_set_data_full(G_OBJECT(mi), "tool-name", name ? g_strdup(name) : g_strdup(tmpl), g_free);
        g_signal_connect(mi, "activate", G_CALLBACK(on_tool_item_activate), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), mi);
        gtk_widget_show(mi);

        g_free(tmpl);
        g_free(name);
        any = TRUE;
    }
    return any;
}

/* Returns a newly-allocated list of matching section-name strings for a directory.
 * Checks [dir:marker] sections (marker file must exist in dirpath) then [dir].
 * Caller must g_free each element and g_slist_free the list. */
static GSList *tools_find_dir_sections(const gchar *dirpath)
{
    GSList *result = NULL;
    if (!tools_config || !dirpath)
        return NULL;

    gsize   ng;
    gchar **groups = g_key_file_get_groups(tools_config, &ng);
    for (gsize gi = 0; gi < ng; gi++) {
        const gchar *group = groups[gi];
        if (g_str_has_prefix(group, "dir:")) {
            const gchar *marker      = group + 4;
            gchar       *marker_path = g_build_filename(dirpath, marker, NULL);
            if (g_file_test(marker_path, G_FILE_TEST_EXISTS))
                result = g_slist_append(result, g_strdup(group));
            g_free(marker_path);
        } else if (g_strcmp0(group, "dir") == 0) {
            result = g_slist_append(result, g_strdup(group));
        }
    }
    g_strfreev(groups);
    return result;
}

/* Signal handler: populate a caller-supplied GtkMenu with tools for path.
 * For files, matches by extension; for directories, matches [dir:marker] sections. */
static void on_append_tools_signal(G_GNUC_UNUSED GObject *obj,
                                   const gchar *path,
                                   gboolean     is_dir,
                                   gpointer     submenu_ptr,
                                   G_GNUC_UNUSED gpointer data)
{
    GtkWidget *submenu = GTK_WIDGET(submenu_ptr);
    if (!tools_config || !path || !submenu)
        return;

    if (is_dir) {
        GSList *sections = tools_find_dir_sections(path);
        for (GSList *s = sections; s; s = s->next) {
            tools_append_section_items(submenu, (const gchar *)s->data, path);
            g_free(s->data);
        }
        g_slist_free(sections);
    } else {
        const gchar *section = tools_find_section(path);
        if (section)
            tools_append_section_items(submenu, section, path);
    }
}

/* Rebuild the "File Tools" submenu for the given filepath.
 * Pass NULL to clear and disable the menu. */
static void tools_menu_populate(const gchar *filepath)
{
    if (!tools_menu_item)
        return;

    /* Use GTK's own mechanism to detach and release the old submenu.
     * gtk_widget_destroy() alone does not clear the menu item's pointer,
     * which prevents the replacement submenu from appearing. */
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(tools_menu_item), NULL);

    if (!filepath || !tools_config) {
        gtk_widget_set_sensitive(tools_menu_item, FALSE);
        return;
    }

    const gchar *section = tools_find_section(filepath);
    if (!section) {
        gtk_widget_set_sensitive(tools_menu_item, FALSE);
        return;
    }

    GtkWidget *submenu = gtk_menu_new();

    if (tools_append_section_items(submenu, section, filepath)) {
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(tools_menu_item), submenu);
        gtk_widget_set_sensitive(tools_menu_item, TRUE);
        gtk_widget_show(submenu);
    } else {
        gtk_widget_destroy(submenu);
        gtk_widget_set_sensitive(tools_menu_item, FALSE);
    }
}

static void on_doc_activate(G_GNUC_UNUSED GObject *obj,
                             GeanyDocument *doc,
                             G_GNUC_UNUSED gpointer data)
{
    tools_menu_populate(doc ? doc->file_name : NULL);
}

static void on_project_open(G_GNUC_UNUSED GObject *obj,
                             G_GNUC_UNUSED GKeyFile *config,
                             G_GNUC_UNUSED gpointer data)
{
    tools_config_load();
    GeanyDocument *doc = document_get_current();
    tools_menu_populate(doc ? doc->file_name : NULL);

    gchar *wd = get_work_dir();
    if (!wd)
        return;

    TermTab *root_tab = find_tab_by_name("./");
    if (root_tab) {
        gchar *cmd = g_strdup_printf("cd '%s'\n", wd);
        vte_terminal_feed_child(root_tab->vte, cmd, -1);
        g_free(cmd);
    } else {
        create_tab(NULL, "./");
    }
    g_free(wd);
}

static void on_project_close(G_GNUC_UNUSED GObject *obj,
                              G_GNUC_UNUSED gpointer data)
{
    tools_config_load();
    GeanyDocument *doc = document_get_current();
    tools_menu_populate(doc ? doc->file_name : NULL);
}


/* ------------------------------------------------------------------ */
/* Plugin lifecycle                                                    */

static gboolean gt_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer data)
{
    geany_plugin = plugin;
    geany_data   = plugin->geany_data;
    active       = TRUE;

    /* Inner notebook */
    term_nb = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_notebook_set_scrollable(term_nb, TRUE);
    gtk_notebook_set_show_border(term_nb, FALSE);

    /* "+" button appended to the inner tab bar */
    GtkWidget *add_btn = gtk_button_new_from_icon_name("list-add-symbolic",
                                                        GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_button_set_relief(GTK_BUTTON(add_btn), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(add_btn, _("New terminal tab"));
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_new_tab_clicked), NULL);
    gtk_widget_show(add_btn);
    gtk_notebook_set_action_widget(term_nb, add_btn, GTK_PACK_END);

    outer_widget = GTK_WIDGET(term_nb);
    gtk_widget_show(outer_widget);

    GtkWidget *tab_label = gtk_label_new("🗔 Cli");
    outer_idx = gtk_notebook_append_page(
        GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
        outer_widget, tab_label);

    /* First terminal tab — always named "./" so project-open can find it */
    create_tab(NULL, "./");

    /* File-type tools menu */
    tools_config_load();
    tools_menu_item = gtk_menu_item_new_with_label(_("File Tools"));
    gtk_widget_show(tools_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(geany_data->main_widgets->tools_menu),
                          tools_menu_item);
    GeanyDocument *cur = document_get_current();
    tools_menu_populate(cur ? cur->file_name : NULL);

    plugin_signal_connect(plugin, geany->object, "document-activate", FALSE,
                          G_CALLBACK(on_doc_activate), NULL);
    plugin_signal_connect(plugin, geany->object, "document-open", FALSE,
                          G_CALLBACK(on_doc_activate), NULL);
    plugin_signal_connect(plugin, geany->object, "project-open", FALSE,
                          G_CALLBACK(on_project_open), NULL);
    plugin_signal_connect(plugin, geany->object, "project-close", FALSE,
                          G_CALLBACK(on_project_close), NULL);

    /* Auto-focus the active VTE terminal when the CLI tab is switched to */
    msg_nb_switch_handler = g_signal_connect(
        geany_data->main_widgets->message_window_notebook,
        "switch-page",
        G_CALLBACK(on_msg_nb_switch_page),
        NULL);

    /* Intercept Ctrl+C / Ctrl+X at the window level so we run before Geany's
     * accelerators (Edit>Copy, Edit>Cut) consume these keystrokes.
     * We only act when a VTE terminal widget has keyboard focus. */
    window_key_handler_id = g_signal_connect(
        geany_data->main_widgets->window,
        "key-press-event",
        G_CALLBACK(on_window_key_press),
        NULL);

    /* Register IPC signals on the Geany application object.
     * g_signal_lookup guards against duplicate registration on reload. */
    GType obj_type = G_OBJECT_TYPE(geany->object);
    if (!g_signal_lookup("geanycli-run-command", obj_type))
        g_signal_new("geanycli-run-command",
                     obj_type,
                     G_SIGNAL_RUN_LAST,
                     0, NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE,
                     2,
                     G_TYPE_STRING,   /* cmd     */
                     G_TYPE_BOOLEAN); /* new_tab */

    plugin_signal_connect(plugin, geany->object,
                          "geanycli-run-command", FALSE,
                          G_CALLBACK(on_ipc_signal), NULL);

    /* geanycli-run-all-tabs: feed a command to every open terminal tab */
    if (!g_signal_lookup("geanycli-run-all-tabs", obj_type))
        g_signal_new("geanycli-run-all-tabs",
                     obj_type,
                     G_SIGNAL_RUN_LAST,
                     0, NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_STRING); /* cmd */

    plugin_signal_connect(plugin, geany->object,
                          "geanycli-run-all-tabs", FALSE,
                          G_CALLBACK(on_ipc_all_signal), NULL);

    /* geanycli-run-file: look up filepath in filetypetools.conf and run tool_0 */
    if (!g_signal_lookup("geanycli-run-file", obj_type))
        g_signal_new("geanycli-run-file",
                     obj_type,
                     G_SIGNAL_RUN_LAST,
                     0, NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE,
                     2,
                     G_TYPE_STRING,   /* filepath */
                     G_TYPE_BOOLEAN); /* new_tab  */

    plugin_signal_connect(plugin, geany->object,
                          "geanycli-run-file", FALSE,
                          G_CALLBACK(on_run_file_signal), NULL);

    /* geanycli-append-tools: populate a caller-supplied GtkMenu with file/dir tools */
    if (!g_signal_lookup("geanycli-append-tools", obj_type))
        g_signal_new("geanycli-append-tools",
                     obj_type,
                     G_SIGNAL_RUN_LAST,
                     0, NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE,
                     3,
                     G_TYPE_STRING,   /* path   */
                     G_TYPE_BOOLEAN,  /* is_dir */
                     G_TYPE_POINTER); /* GtkWidget *submenu */

    plugin_signal_connect(plugin, geany->object,
                          "geanycli-append-tools", FALSE,
                          G_CALLBACK(on_append_tools_signal), NULL);

    /* geanycli-feed-text: insert text into the active terminal without executing */
    if (!g_signal_lookup("geanycli-feed-text", obj_type))
        g_signal_new("geanycli-feed-text",
                     obj_type,
                     G_SIGNAL_RUN_LAST,
                     0, NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_STRING); /* text */

    plugin_signal_connect(plugin, geany->object,
                          "geanycli-feed-text", FALSE,
                          G_CALLBACK(on_ipc_feed_signal), NULL);

    return TRUE;
}

static void gt_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin,
                       G_GNUC_UNUSED gpointer data)
{
    active = FALSE;

    /* Disconnect the window-level key interceptor. */
    if (window_key_handler_id) {
        g_signal_handler_disconnect(geany_data->main_widgets->window,
                                    window_key_handler_id);
        window_key_handler_id = 0;
    }

    if (msg_nb_switch_handler) {
        g_signal_handler_disconnect(
            geany_data->main_widgets->message_window_notebook,
            msg_nb_switch_handler);
        msg_nb_switch_handler = 0;
    }

    /* Disconnect child-exited signals and free TermTab structs before the
     * notebook widget tree is destroyed by gtk_notebook_remove_page. */
    if (term_nb) {
        gint n = gtk_notebook_get_n_pages(term_nb);
        for (gint i = 0; i < n; i++) {
            GtkWidget *page = gtk_notebook_get_nth_page(term_nb, i);
            TermTab *tab = g_object_get_data(G_OBJECT(page), "termtab");
            if (tab) {
                g_signal_handlers_disconnect_by_func(tab->vte,
                                                     G_CALLBACK(on_child_exited),
                                                     tab);
                g_free(tab);
            }
        }
    }

    if (outer_idx >= 0) {
        gtk_notebook_remove_page(
            GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
            outer_idx);
        outer_widget = NULL;
        term_nb      = NULL;
        outer_idx    = -1;
    }

    if (tools_menu_item) {
        gtk_widget_destroy(tools_menu_item);
        tools_menu_item = NULL;
    }
    if (tools_config) {
        g_key_file_free(tools_config);
        tools_config = NULL;
    }

    tab_counter = 0;
}


/* ------------------------------------------------------------------ */

G_MODULE_EXPORT void geany_load_module(GeanyPlugin *plugin)
{
    plugin->info->name        = "Terminal";
    plugin->info->description =
        "Tabbed VTE terminals in the message window. "
        "Opens at the project root. "
        "Other plugins can run commands by emitting the "
        "\"geanycli-run-command\" signal on geany->object "
        "or by calling geanycli_run_command() via GModule.";
    plugin->info->version     = "0.1";
    plugin->info->author      = "teknopaul";

    plugin->funcs->init    = gt_init;
    plugin->funcs->cleanup = gt_cleanup;

    GEANY_PLUGIN_REGISTER(plugin, 235);
}
