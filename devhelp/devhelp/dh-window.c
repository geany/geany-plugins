/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2008 Imendio AB
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Fullscreen mode code adapted from gedit
 *  Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 *  Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 *  Copyright (C) 2002-2005 Paolo Maggi
 */

#include "config.h"
#include <string.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#ifdef GDK_WINDOWING_QUARTZ
#include <ige-mac-integration.h>
#endif

#include "dh-book-tree.h"
#include "dh-book-manager.h"
#include "dh-book.h"
#include "dh-preferences.h"
#include "dh-search.h"
#include "dh-window.h"
#include "dh-util.h"
#include "dh-marshal.h"
#include "dh-enum-types.h"
#include "eggfindbar.h"
#include "ige-conf.h"

#define FULLSCREEN_ANIMATION_SPEED 4

struct _DhWindowPriv {
        DhBase         *base;

        GtkWidget      *main_box;
        GtkWidget      *menu_box;
        GtkWidget      *hpaned;
        GtkWidget      *control_notebook;
        GtkWidget      *book_tree;
        GtkWidget      *search;
        GtkWidget      *notebook;

        GtkWidget      *vbox;
        GtkWidget      *findbar;

        GtkWidget      *fullscreen_controls;
        guint           fullscreen_animation_timeout_id;
        gboolean        fullscreen_animation_enter;

        GtkUIManager   *manager;
        GtkActionGroup *action_group;

        DhLink         *selected_search_link;
        guint           find_source_id;
};

enum {
        OPEN_LINK,
        LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

static guint tab_accel_keys[] = {
        GDK_1, GDK_2, GDK_3, GDK_4, GDK_5,
        GDK_6, GDK_7, GDK_8, GDK_9, GDK_0
};

static const
struct
{
        gchar *name;
        int    level;
}
zoom_levels[] =
{
        { N_("50%"), 70 },
        { N_("75%"), 84 },
        { N_("100%"), 100 },
        { N_("125%"), 119 },
        { N_("150%"), 141 },
        { N_("175%"), 168 },
        { N_("200%"), 200 },
        { N_("300%"), 283 },
        { N_("400%"), 400 }
};

#define ZOOM_MINIMAL    (zoom_levels[0].level)
#define ZOOM_MAXIMAL    (zoom_levels[8].level)
#define ZOOM_DEFAULT    (zoom_levels[2].level)

#if GTK_CHECK_VERSION (2,17,5)
#define ERRORS_IN_INFOBAR
#endif

static void           dh_window_class_init           (DhWindowClass   *klass);
static void           dh_window_init                 (DhWindow        *window);
static void           window_populate                (DhWindow        *window);
static void           window_tree_link_selected_cb   (GObject         *ignored,
                                                      DhLink          *link,
                                                      DhWindow        *window);
static void           window_search_link_selected_cb (GObject         *ignored,
                                                      DhLink          *link,
                                                      DhWindow        *window);
static void           window_check_history           (DhWindow        *window,
                                                      WebKitWebView   *web_view);
static void           window_web_view_tab_accel_cb   (GtkAccelGroup   *accel_group,
                                                      GObject         *object,
                                                      guint            key,
                                                      GdkModifierType  mod,
                                                      DhWindow        *window);
static void           window_find_search_changed_cb  (GObject         *object,
                                                      GParamSpec      *arg1,
                                                      DhWindow        *window);
static void           window_find_case_changed_cb    (GObject         *object,
                                                      GParamSpec      *arg1,
                                                      DhWindow        *window);
static void           window_find_previous_cb        (GtkEntry        *entry,
                                                      DhWindow        *window);
static void           window_find_next_cb            (GtkEntry        *entry,
                                                      DhWindow        *window);
static void           window_findbar_close_cb        (GtkWidget       *widget,
                                                      DhWindow        *window);
static GtkWidget *    window_new_tab_label           (DhWindow        *window,
                                                      const gchar     *label,
                                                      const GtkWidget *parent);
static int            window_open_new_tab            (DhWindow        *window,
                                                      const gchar     *location,
                                                      gboolean         switch_focus);
static WebKitWebView *window_get_active_web_view     (DhWindow        *window);
#ifdef ERRORS_IN_INFOBAR
static GtkWidget *    window_get_active_info_bar     (DhWindow *window);
#endif
static void           window_update_title            (DhWindow        *window,
                                                      WebKitWebView   *web_view,
                                                      const gchar     *title);
static void           window_tab_set_title           (DhWindow        *window,
                                                      WebKitWebView   *web_view,
                                                      const gchar     *title);
static void           window_close_tab               (DhWindow *window,
                                                      gint      page_num);

G_DEFINE_TYPE (DhWindow, dh_window, GTK_TYPE_WINDOW);

#define GET_PRIVATE(instance) G_TYPE_INSTANCE_GET_PRIVATE \
  (instance, DH_TYPE_WINDOW, DhWindowPriv);

static void
window_activate_new_window (GtkAction *action,
                            DhWindow  *window)
{
        DhWindowPriv *priv;
        GtkWidget    *new_window;

        priv = window->priv;

        new_window = dh_base_new_window (priv->base);
        gtk_widget_show (new_window);
}

static void
window_activate_new_tab (GtkAction *action,
                         DhWindow  *window)
{
        window_open_new_tab (window, NULL, TRUE);
}

static void
window_activate_print (GtkAction *action,
                       DhWindow  *window)
{
    WebKitWebView *web_view;

    web_view = window_get_active_web_view (window);
    webkit_web_view_execute_script (web_view, "print();");
}

static void
window_close_tab (DhWindow *window,
                  gint      page_num)
{
        DhWindowPriv *priv;
        gint          pages;

        priv = window->priv;

        gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook), page_num);

        pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));

        if (pages == 0) {
                gtk_widget_destroy (GTK_WIDGET (window));
        }
        else if (pages == 1) {
                gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
        }
}

static void
window_activate_close (GtkAction *action,
                       DhWindow  *window)
{
        gint          page_num;

        page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->priv->notebook));
        window_close_tab (window, page_num);
}

static void
window_activate_quit (GtkAction *action,
                      DhWindow  *window)
{
        dh_base_quit (window->priv->base);
}

static void
window_activate_copy (GtkAction *action,
                      DhWindow  *window)
{
        GtkWidget *widget;
        DhWindowPriv  *priv;

        priv = window->priv;

        widget = gtk_window_get_focus (GTK_WINDOW (window));

        if (GTK_IS_EDITABLE (widget)) {
                gtk_editable_copy_clipboard (GTK_EDITABLE (widget));
        } else if (GTK_IS_TREE_VIEW (widget) &&
                   gtk_widget_is_ancestor (widget, priv->search) &&
                   priv->selected_search_link) {
                GtkClipboard *clipboard;
                clipboard = gtk_widget_get_clipboard (widget, GDK_SELECTION_CLIPBOARD);
                gtk_clipboard_set_text (clipboard,
                                dh_link_get_name(priv->selected_search_link), -1);
        } else {
                WebKitWebView *web_view;

                web_view = window_get_active_web_view (window);
                webkit_web_view_copy_clipboard (web_view);
        }
}

static void
window_activate_find (GtkAction *action,
                      DhWindow  *window)
{
        DhWindowPriv  *priv;
        WebKitWebView *web_view;

        priv = window->priv;
        web_view = window_get_active_web_view (window);

        gtk_widget_show (priv->findbar);
        gtk_widget_grab_focus (priv->findbar);

        webkit_web_view_set_highlight_text_matches (web_view, TRUE);
}

static int
window_get_current_zoom_level_index (DhWindow *window)
{
        WebKitWebView *web_view;
        float zoom_level;
        int zoom_level_as_int = ZOOM_DEFAULT;
        int i;

        web_view = window_get_active_web_view (window);
        if (web_view) {
                g_object_get (web_view, "zoom-level", &zoom_level, NULL);
                zoom_level_as_int = (int)(zoom_level*100);
        }

        for (i=0; zoom_levels[i].level != ZOOM_MAXIMAL; i++) {
                if (zoom_levels[i].level == zoom_level_as_int)
                        return i;
        }
        return i;
}

static void
window_update_zoom_actions_sensitiveness (DhWindow *window)
{
        DhWindowPriv *priv;
        GtkAction *zoom_in, *zoom_out, *zoom_default;
        int zoom_level_idx;

        priv = window->priv;
        zoom_in = gtk_action_group_get_action (priv->action_group, "ZoomIn");
        zoom_out = gtk_action_group_get_action (priv->action_group, "ZoomOut");
        zoom_default = gtk_action_group_get_action (priv->action_group, "ZoomDefault");

        zoom_level_idx = window_get_current_zoom_level_index (window);

        gtk_action_set_sensitive (zoom_in,
                                  zoom_levels[zoom_level_idx].level < ZOOM_MAXIMAL);
        gtk_action_set_sensitive (zoom_out,
                                  zoom_levels[zoom_level_idx].level > ZOOM_MINIMAL);
        gtk_action_set_sensitive (zoom_default,
                                  zoom_levels[zoom_level_idx].level != ZOOM_DEFAULT);
}

static void
window_activate_zoom_in (GtkAction *action,
                         DhWindow  *window)
{
        int zoom_level_idx;

        zoom_level_idx = window_get_current_zoom_level_index (window);
        if (zoom_levels[zoom_level_idx].level < ZOOM_MAXIMAL) {
                WebKitWebView *web_view;

                web_view = window_get_active_web_view (window);
                g_object_set (web_view,
                              "zoom-level", (float)(zoom_levels[zoom_level_idx+1].level)/100,
                              NULL);
                window_update_zoom_actions_sensitiveness (window);
        }

}

static void
window_activate_zoom_out (GtkAction *action,
                          DhWindow  *window)
{
        int zoom_level_idx;

        zoom_level_idx = window_get_current_zoom_level_index (window);
        if (zoom_levels[zoom_level_idx].level > ZOOM_MINIMAL) {
                WebKitWebView *web_view;

                web_view = window_get_active_web_view (window);
                g_object_set (web_view,
                              "zoom-level", (float)(zoom_levels[zoom_level_idx-1].level)/100,
                              NULL);
                window_update_zoom_actions_sensitiveness (window);
        }
}

static void
window_activate_zoom_default (GtkAction *action,
                              DhWindow  *window)
{
        WebKitWebView *web_view;

        web_view = window_get_active_web_view (window);
        g_object_set (web_view, "zoom-level", (float)(ZOOM_DEFAULT)/100, NULL);
        window_update_zoom_actions_sensitiveness (window);
}

static gboolean
run_fullscreen_animation (gpointer data)
{
	DhWindow *window = DH_WINDOW (data);
	GdkScreen *screen;
	GdkRectangle fs_rect;
	gint x, y;

	screen = gtk_window_get_screen (GTK_WINDOW (window));
	gdk_screen_get_monitor_geometry (screen,
					 gdk_screen_get_monitor_at_window (screen,
									   gtk_widget_get_window (GTK_WIDGET (window))),
					 &fs_rect);

	gtk_window_get_position (GTK_WINDOW (window->priv->fullscreen_controls),
				 &x, &y);

	if (window->priv->fullscreen_animation_enter)
	{
		if (y == fs_rect.y)
		{
			window->priv->fullscreen_animation_timeout_id = 0;
			return FALSE;
		}
		else
		{
			gtk_window_move (GTK_WINDOW (window->priv->fullscreen_controls),
					 x, y + 1);
			return TRUE;
		}
	}
	else
	{
		gint w, h;

		gtk_window_get_size (GTK_WINDOW (window->priv->fullscreen_controls),
				     &w, &h);

		if (y == fs_rect.y - h + 1)
		{
			window->priv->fullscreen_animation_timeout_id = 0;
			return FALSE;
		}
		else
		{
			gtk_window_move (GTK_WINDOW (window->priv->fullscreen_controls),
					 x, y - 1);
			return TRUE;
		}
	}
}

static void
show_hide_fullscreen_toolbar (DhWindow *window,
                              gboolean     show,
                              gint         height)
{
        GtkSettings *settings;
        gboolean enable_animations;

        settings = gtk_widget_get_settings (GTK_WIDGET (window));
        g_object_get (G_OBJECT (settings),
                      "gtk-enable-animations",
                      &enable_animations,
                      NULL);

        if (enable_animations)
        {
                window->priv->fullscreen_animation_enter = show;

                if (window->priv->fullscreen_animation_timeout_id == 0)
                {
                        window->priv->fullscreen_animation_timeout_id =
                                g_timeout_add (FULLSCREEN_ANIMATION_SPEED,
                                               (GSourceFunc) run_fullscreen_animation,
                                               window);
                }
        }
        else
        {
                GdkRectangle fs_rect;
                GdkScreen *screen;

                screen = gtk_window_get_screen (GTK_WINDOW (window));
                gdk_screen_get_monitor_geometry (screen,
                                                 gdk_screen_get_monitor_at_window (screen,
                                                                                   gtk_widget_get_window (GTK_WIDGET (window))),
                                                 &fs_rect);

                if (show)
                        gtk_window_move (GTK_WINDOW (window->priv->fullscreen_controls),
                                 fs_rect.x, fs_rect.y);
                else
                        gtk_window_move (GTK_WINDOW (window->priv->fullscreen_controls),
                                         fs_rect.x, fs_rect.y - height + 1);
        }

}


static gboolean
on_fullscreen_controls_enter_notify_event (GtkWidget        *widget,
                                           GdkEventCrossing *event,
                                           DhWindow      *window)
{
        show_hide_fullscreen_toolbar (window, TRUE, 0);

        return FALSE;
}

static gboolean
on_fullscreen_controls_leave_notify_event (GtkWidget        *widget,
                                           GdkEventCrossing *event,
                                           DhWindow      *window)
{
        GdkDisplay *display;
        GdkScreen *screen;
        gint w, h;
        gint x, y;

        display = gdk_display_get_default ();
        screen = gtk_window_get_screen (GTK_WINDOW (window));

        gtk_window_get_size (GTK_WINDOW (window->priv->fullscreen_controls), &w, &h);
        gdk_display_get_pointer (display, &screen, &x, &y, NULL);

        /* gtk seems to emit leave notify when clicking on tool items,
         * work around it by checking the coordinates
         */
        if (y >= h)
        {
                show_hide_fullscreen_toolbar (window, FALSE, h);
        }

        return FALSE;
}

static gboolean
window_is_fullscreen (DhWindow *window)
{
        GdkWindowState  state;

        g_return_val_if_fail (DH_IS_WINDOW (window), FALSE);

#if GTK_CHECK_VERSION (2,14,0)
        state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)));
#else
        state = gdk_window_get_state (GTK_WIDGET (window)->window);
#endif

        return state & GDK_WINDOW_STATE_FULLSCREEN;
}

static void
window_fullscreen_controls_build (DhWindow *window)
{
        GtkWidget *toolbar;
        GtkAction *action;
        DhWindowPriv  *priv;

        priv = window->priv;
        if (priv->fullscreen_controls != NULL)
                return;

        priv->fullscreen_controls = gtk_window_new (GTK_WINDOW_POPUP);
        gtk_window_set_transient_for (GTK_WINDOW (priv->fullscreen_controls),
                                      GTK_WINDOW (window));

        toolbar = gtk_ui_manager_get_widget (priv->manager, "/FullscreenToolBar");
        gtk_container_add (GTK_CONTAINER (priv->fullscreen_controls),
                           toolbar);
        action = gtk_action_group_get_action (priv->action_group,
                                              "LeaveFullscreen");
        g_object_set (action, "is-important", TRUE, NULL);

        /* Set the toolbar style */
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar),
                               GTK_TOOLBAR_BOTH_HORIZ);

        g_signal_connect (priv->fullscreen_controls, "enter-notify-event",
                          G_CALLBACK (on_fullscreen_controls_enter_notify_event),
                          window);
        g_signal_connect (priv->fullscreen_controls, "leave-notify-event",
                          G_CALLBACK (on_fullscreen_controls_leave_notify_event),
                          window);
}

static void
window_fullscreen_controls_show (DhWindow *window)
{
        GdkScreen *screen;
        GdkRectangle fs_rect;
        gint w, h;

        screen = gtk_window_get_screen (GTK_WINDOW (window));
        gdk_screen_get_monitor_geometry (screen,
                        gdk_screen_get_monitor_at_window (
                                screen,
                                gtk_widget_get_window (GTK_WIDGET (window))),
                        &fs_rect);

        gtk_window_get_size (GTK_WINDOW (window->priv->fullscreen_controls), &w, &h);

        gtk_window_resize (GTK_WINDOW (window->priv->fullscreen_controls),
                           fs_rect.width, h);

        gtk_window_move (GTK_WINDOW (window->priv->fullscreen_controls),
                         fs_rect.x, fs_rect.y - h + 1);

        gtk_widget_show_all (window->priv->fullscreen_controls);
}

static void
window_fullscreen (DhWindow *window)
{
        if (window_is_fullscreen (window))
                return;

        gtk_window_fullscreen (GTK_WINDOW (window));
        gtk_widget_hide (gtk_ui_manager_get_widget (window->priv->manager, "/MenuBar"));
        gtk_widget_hide (gtk_ui_manager_get_widget (window->priv->manager, "/Toolbar"));

        window_fullscreen_controls_build (window);
        window_fullscreen_controls_show (window);
}

static void
window_unfullscreen (DhWindow *window)
{
        if (! window_is_fullscreen (window))
                return;

        gtk_window_unfullscreen (GTK_WINDOW (window));
        gtk_widget_show (gtk_ui_manager_get_widget (window->priv->manager, "/MenuBar"));
        gtk_widget_show (gtk_ui_manager_get_widget (window->priv->manager, "/Toolbar"));

        gtk_widget_hide (window->priv->fullscreen_controls);
}


static void
window_toggle_fullscreen_mode (GtkAction *action,
                               DhWindow  *window)
{
        if (window_is_fullscreen (window)) {
                window_unfullscreen (window);
        } else {
                window_fullscreen (window);
        }
}

static void
window_leave_fullscreen_mode (GtkAction *action,
                              DhWindow *window)
{
        GtkAction *view_action;

        view_action = gtk_action_group_get_action (window->priv->action_group,
                                                   "ViewFullscreen");
        g_signal_handlers_block_by_func (view_action,
                        G_CALLBACK (window_toggle_fullscreen_mode),
                        window);
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (view_action),
                                      FALSE);
        window_unfullscreen (window);
        g_signal_handlers_unblock_by_func (view_action,
                        G_CALLBACK (window_toggle_fullscreen_mode),
                        window);
}

static void
window_activate_preferences (GtkAction *action,
                             DhWindow  *window)
{
        dh_preferences_show_dialog (GTK_WINDOW (window));
}

static void
window_activate_back (GtkAction *action,
                      DhWindow  *window)
{
        DhWindowPriv  *priv;
        WebKitWebView *web_view;
        GtkWidget     *frame;

        priv = window->priv;

        frame = gtk_notebook_get_nth_page (
                GTK_NOTEBOOK (priv->notebook),
                gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook)));
        web_view = g_object_get_data (G_OBJECT (frame), "web_view");

        webkit_web_view_go_back (web_view);
}

static void
window_activate_forward (GtkAction *action,
                         DhWindow  *window)
{
        DhWindowPriv  *priv;
        WebKitWebView *web_view;
        GtkWidget     *frame;

        priv = window->priv;

        frame = gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->notebook),
                                           gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook))
                                          );
        web_view = g_object_get_data (G_OBJECT (frame), "web_view");

        webkit_web_view_go_forward (web_view);
}

static void
window_activate_show_contents (GtkAction *action,
                               DhWindow  *window)
{
        DhWindowPriv *priv;

        priv = window->priv;

        gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->control_notebook), 0);
        gtk_widget_grab_focus (priv->book_tree);
}

static void
window_activate_show_search (GtkAction *action,
                             DhWindow  *window)
{
        DhWindowPriv *priv;

        priv = window->priv;

        gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->control_notebook), 1);
        gtk_widget_grab_focus (priv->search);
}

static void
window_activate_about (GtkAction *action,
                       DhWindow  *window)
{
        const gchar  *authors[] = {
                "Mikael Hallendal <micke@imendio.com>",
                "Richard Hult <richard@imendio.com>",
                "Johan Dahlin <johan@gnome.org>",
                "Ross Burton <ross@burtonini.com>",
                "Aleksander Morgado <aleksander@lanedo.com>",
                NULL
        };
        const gchar **documenters = NULL;
        const gchar  *translator_credits = _("translator_credits");

        /* i18n: Please don't translate "Devhelp" (it's marked as translatable
         * for transliteration only) */
        gtk_show_about_dialog (GTK_WINDOW (window),
                               "name", _("Devhelp"),
                               "version", PACKAGE_VERSION,
                               "comments", _("A developers' help browser for GNOME"),
                               "authors", authors,
                               "documenters", documenters,
                               "translator-credits",
                               strcmp (translator_credits, "translator_credits") != 0 ?
                               translator_credits : NULL,
                               "website", "http://live.gnome.org/devhelp",
                               "logo-icon-name", "devhelp",
                               NULL);
}

static void
window_open_link_cb (DhWindow *window,
                     const char *location,
                     DhOpenLinkFlags flags)
{
        DhWindowPriv *priv;
        priv = window->priv;

        if (flags & DH_OPEN_LINK_NEW_TAB) {
                window_open_new_tab (window, location, FALSE);
        }
        else if (flags & DH_OPEN_LINK_NEW_WINDOW) {
                GtkWidget *new_window;
                new_window = dh_base_new_window (priv->base);
                gtk_widget_show (new_window);
        }
}

static const GtkActionEntry actions[] = {
        { "FileMenu", NULL, N_("_File"), NULL, NULL, NULL },
        { "EditMenu", NULL, N_("_Edit"), NULL, NULL, NULL },
        { "ViewMenu", NULL, N_("_View"), NULL, NULL, NULL },
        { "GoMenu",   NULL, N_("_Go"), NULL, NULL, NULL },
        { "HelpMenu", NULL, N_("_Help"), NULL, NULL, NULL },

        /* File menu */
        { "NewWindow", GTK_STOCK_NEW, N_("_New Window"), "<control>N", NULL,
          G_CALLBACK (window_activate_new_window) },
        { "NewTab", GTK_STOCK_NEW, N_("New _Tab"), "<control>T", NULL,
          G_CALLBACK (window_activate_new_tab) },
        { "Print", GTK_STOCK_PRINT, N_("_Print…"), "<control>P", NULL,
          G_CALLBACK (window_activate_print) },
        { "Close", GTK_STOCK_CLOSE, NULL, NULL, NULL,
          G_CALLBACK (window_activate_close) },
        { "Quit", GTK_STOCK_QUIT, NULL, NULL, NULL,
          G_CALLBACK (window_activate_quit) },

        /* Edit menu */
        { "Copy", GTK_STOCK_COPY, NULL, "<control>C", NULL,
          G_CALLBACK (window_activate_copy) },
        { "Find", GTK_STOCK_FIND, NULL, "<control>F", NULL,
          G_CALLBACK (window_activate_find) },
        { "Find Next", GTK_STOCK_GO_FORWARD, N_("Find Next"), "<control>G", NULL,
          G_CALLBACK (window_find_next_cb) },
        { "Find Previous", GTK_STOCK_GO_BACK, N_("Find Previous"), "<shift><control>G", NULL,
          G_CALLBACK (window_find_previous_cb) },
        { "Preferences", GTK_STOCK_PREFERENCES, NULL, NULL, NULL,
          G_CALLBACK (window_activate_preferences) },

        /* Go menu */
        { "Back", GTK_STOCK_GO_BACK, NULL, "<alt>Left",
          N_("Go to the previous page"),
          G_CALLBACK (window_activate_back) },
        { "Forward", GTK_STOCK_GO_FORWARD, NULL, "<alt>Right",
          N_("Go to the next page"),
          G_CALLBACK (window_activate_forward) },

        { "ShowContentsTab", NULL, N_("_Contents Tab"), "<ctrl>B", NULL,
          G_CALLBACK (window_activate_show_contents) },

        { "ShowSearchTab", NULL, N_("_Search Tab"), "<ctrl>S", NULL,
          G_CALLBACK (window_activate_show_search) },

        /* View menu */
        { "ZoomIn", GTK_STOCK_ZOOM_IN, N_("_Larger Text"), "<ctrl>plus",
          N_("Increase the text size"),
          G_CALLBACK (window_activate_zoom_in) },
        { "ZoomOut", GTK_STOCK_ZOOM_OUT, N_("S_maller Text"), "<ctrl>minus",
          N_("Decrease the text size"),
          G_CALLBACK (window_activate_zoom_out) },
        { "ZoomDefault", GTK_STOCK_ZOOM_100, N_("_Normal Size"), "<ctrl>0",
          N_("Use the normal text size"),
          G_CALLBACK (window_activate_zoom_default) },

        /* About menu */
        { "About", GTK_STOCK_ABOUT, NULL, NULL, NULL,
          G_CALLBACK (window_activate_about) },

        /* Fullscreen toolbar */
        { "LeaveFullscreen", GTK_STOCK_LEAVE_FULLSCREEN, NULL,
          NULL, N_("Leave fullscreen mode"),
          G_CALLBACK (window_leave_fullscreen_mode) }
};

static const GtkToggleActionEntry always_sensitive_toggle_menu_entries[] =
{
        { "ViewFullscreen", GTK_STOCK_FULLSCREEN, NULL, "F11",
          N_("Display in full screen"),
          G_CALLBACK (window_toggle_fullscreen_mode), FALSE },
};

static const gchar* important_actions[] = {
        "Back",
        "Forward"
};

static void
window_finalize (GObject *object)
{
        DhWindowPriv *priv = GET_PRIVATE (object);

        g_object_unref (priv->base);

        G_OBJECT_CLASS (dh_window_parent_class)->finalize (object);
}

static void
dh_window_class_init (DhWindowClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = window_finalize;

        signals[OPEN_LINK] =
                g_signal_new ("open-link",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (DhWindowClass, open_link),
                              NULL, NULL,
                              _dh_marshal_VOID__STRING_FLAGS,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_STRING,
                              DH_TYPE_OPEN_LINK_FLAGS);

        gtk_rc_parse_string ("style \"devhelp-tab-close-button-style\"\n"
                             "{\n"
                             "GtkWidget::focus-padding = 0\n"
                             "GtkWidget::focus-line-width = 0\n"
                             "xthickness = 0\n"
                             "ythickness = 0\n"
                             "}\n"
                             "widget \"*.devhelp-tab-close-button\" "
                             "style \"devhelp-tab-close-button-style\"");

        g_type_class_add_private (klass, sizeof (DhWindowPriv));
}

static void
dh_window_init (DhWindow *window)
{
        DhWindowPriv  *priv;
        GtkAction     *action;
        GtkAccelGroup *accel_group;
        GClosure      *closure;
        guint           i;

        priv = GET_PRIVATE (window);
        window->priv = priv;

        priv->selected_search_link = NULL;

        priv->manager = gtk_ui_manager_new ();

        accel_group = gtk_ui_manager_get_accel_group (priv->manager);
        gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

        priv->main_box = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (priv->main_box);

        priv->menu_box = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (priv->menu_box);
        gtk_container_set_border_width (GTK_CONTAINER (priv->menu_box), 0);
        gtk_box_pack_start (GTK_BOX (priv->main_box), priv->menu_box,
                            FALSE, TRUE, 0);

        gtk_container_add (GTK_CONTAINER (window), priv->main_box);

        g_signal_connect (window,
                          "open-link",
                          G_CALLBACK (window_open_link_cb),
                          window);

        priv->action_group = gtk_action_group_new ("MainWindow");

        gtk_action_group_set_translation_domain (priv->action_group,
                                                 GETTEXT_PACKAGE);

        gtk_action_group_add_actions (priv->action_group,
                                      actions,
                                      G_N_ELEMENTS (actions),
                                      window);
        gtk_action_group_add_toggle_actions (priv->action_group,
                                             always_sensitive_toggle_menu_entries,
                                             G_N_ELEMENTS (always_sensitive_toggle_menu_entries),
                                             window);

        for (i = 0; i < G_N_ELEMENTS (important_actions); i++) {
                action = gtk_action_group_get_action (priv->action_group,
                                                      important_actions[i]);
                g_object_set (action, "is-important", TRUE, NULL);
        }

        gtk_ui_manager_insert_action_group (priv->manager,
                                            priv->action_group,
                                            0);

        action = gtk_action_group_get_action (priv->action_group,
                                              "Back");
        g_object_set (action, "sensitive", FALSE, NULL);

        action = gtk_action_group_get_action (priv->action_group,
                                              "Forward");
        g_object_set (action, "sensitive", FALSE, NULL);

        action = gtk_action_group_get_action (priv->action_group, "ZoomIn");
        /* Translators: This refers to text size */
        g_object_set (action, "short_label", _("Larger"), NULL);
        action = gtk_action_group_get_action (priv->action_group, "ZoomOut");
        /* Translators: This refers to text size */
        g_object_set (action, "short_label", _("Smaller"), NULL);

        accel_group = gtk_accel_group_new ();
        gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

        for (i = 0; i < G_N_ELEMENTS (tab_accel_keys); i++) {
                closure =  g_cclosure_new (G_CALLBACK (window_web_view_tab_accel_cb),
                                           window,
                                           NULL);
                gtk_accel_group_connect (accel_group,
                                         tab_accel_keys[i],
                                         GDK_MOD1_MASK,
                                         0,
                                         closure);
        }
}

/* The ugliest hack. When switching tabs, the selection and cursor is changed
 * for the tree view so the web_view content is changed. Block the signal during
 * switch.
 */
static void
window_control_switch_page_cb (GtkWidget       *notebook,
                               gpointer         page,
                               guint            page_num,
                               DhWindow        *window)
{
        DhWindowPriv *priv;

        priv = window->priv;

        g_signal_handlers_block_by_func (priv->book_tree,
                                         window_tree_link_selected_cb,
                                         window);
}

static void
window_control_after_switch_page_cb (GtkWidget       *notebook,
                                     gpointer         page,
                                     guint            page_num,
                                     DhWindow        *window)
{
        DhWindowPriv *priv;

        priv = window->priv;

        g_signal_handlers_unblock_by_func (priv->book_tree,
                                           window_tree_link_selected_cb,
                                           window);
}

static void
window_web_view_switch_page_cb (GtkNotebook     *notebook,
                                gpointer         page,
                                guint            new_page_num,
                                DhWindow        *window)
{
        DhWindowPriv *priv;
        GtkWidget    *new_page;

        priv = window->priv;

        new_page = gtk_notebook_get_nth_page (notebook, new_page_num);
        if (new_page) {
                WebKitWebView  *new_web_view;
                WebKitWebFrame *web_frame;
                const gchar    *location;

                new_web_view = g_object_get_data (G_OBJECT (new_page), "web_view");

                /* Sync the book tree. */
                web_frame = webkit_web_view_get_main_frame (new_web_view);
                location = webkit_web_frame_get_uri (web_frame);

                if (location) {
                        dh_book_tree_select_uri (DH_BOOK_TREE (priv->book_tree),
                                                 location);
                }
                window_check_history (window, new_web_view);

                window_update_title (window, new_web_view, NULL);
        } else {
                /* i18n: Please don't translate "Devhelp" (it's marked as translatable
                 * for transliteration only) */
                gtk_window_set_title (GTK_WINDOW (window), _("Devhelp"));
                window_check_history (window, NULL);
        }
}

static void
window_web_view_switch_page_after_cb (GtkNotebook     *notebook,
                                      gpointer         page,
                                      guint            new_page_num,
                                      DhWindow        *window)
{
        window_update_zoom_actions_sensitiveness (window);
}

static void
window_populate (DhWindow *window)
{
        DhWindowPriv  *priv;
        gchar         *path;
        GtkWidget     *book_tree_sw;
        DhBookManager *book_manager;
        GtkWidget     *menubar;
        GtkWidget     *toolbar;

        priv = window->priv;

        path = dh_util_build_data_filename ("devhelp", "ui", "window.ui", NULL);
        gtk_ui_manager_add_ui_from_file (priv->manager,
                                         path,
                                         NULL);
        g_free (path);
        gtk_ui_manager_ensure_update (priv->manager);

        menubar = gtk_ui_manager_get_widget (priv->manager, "/MenuBar");
        gtk_box_pack_start (GTK_BOX (priv->menu_box), menubar,
                            FALSE, FALSE, 0);
        toolbar = gtk_ui_manager_get_widget (priv->manager, "/Toolbar");
        gtk_box_pack_start (GTK_BOX (priv->menu_box), toolbar,
                            FALSE, FALSE, 0);

#ifdef GDK_WINDOWING_QUARTZ
        {
                GtkWidget       *widget;
                IgeMacMenuGroup *group;

                /* Hide toolbar labels. */
                gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);

                /* Setup menubar. */
                ige_mac_menu_set_menu_bar (GTK_MENU_SHELL (menubar));
                gtk_widget_hide (menubar);

                widget = gtk_ui_manager_get_widget (priv->manager, "/MenuBar/FileMenu/Quit");
                ige_mac_menu_set_quit_menu_item (GTK_MENU_ITEM (widget));

                group =  ige_mac_menu_add_app_menu_group ();
                widget = gtk_ui_manager_get_widget (priv->manager, "/MenuBar/HelpMenu/About");
                ige_mac_menu_add_app_menu_item (group, GTK_MENU_ITEM (widget),
                                                /* i18n: please don't translate
                                                 * "Devhelp", it's a name, not a
                                                 * generic word. */
                                                _("About Devhelp"));

                group =  ige_mac_menu_add_app_menu_group ();
                widget = gtk_ui_manager_get_widget (priv->manager, "/MenuBar/EditMenu/Preferences");
                ige_mac_menu_add_app_menu_item (group, GTK_MENU_ITEM (widget),
                                                _("Preferences…"));

                ige_mac_menu_set_global_key_handler_enabled (TRUE);
        }
#endif

        priv->hpaned = gtk_hpaned_new ();

        gtk_box_pack_start (GTK_BOX (priv->main_box), priv->hpaned, TRUE, TRUE, 0);

        /* Search and contents notebook. */
        priv->control_notebook = gtk_notebook_new ();

        gtk_paned_add1 (GTK_PANED (priv->hpaned), priv->control_notebook);

        g_signal_connect (priv->control_notebook,
                          "switch-page",
                          G_CALLBACK (window_control_switch_page_cb),
                          window);

        g_signal_connect_after (priv->control_notebook,
                                "switch-page",
                                G_CALLBACK (window_control_after_switch_page_cb),
                                window);

        book_tree_sw = gtk_scrolled_window_new (NULL, NULL);

        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (book_tree_sw),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (book_tree_sw),
                                             GTK_SHADOW_IN);
        gtk_container_set_border_width (GTK_CONTAINER (book_tree_sw), 2);

        book_manager = dh_base_get_book_manager (priv->base);

        priv->book_tree = dh_book_tree_new (book_manager);
        gtk_container_add (GTK_CONTAINER (book_tree_sw),
                           priv->book_tree);
        dh_util_state_set_notebook_page_name (book_tree_sw, "content");
        gtk_notebook_append_page (GTK_NOTEBOOK (priv->control_notebook),
                                  book_tree_sw,
                                  gtk_label_new (_("Contents")));
        g_signal_connect (priv->book_tree,
                          "link-selected",
                          G_CALLBACK (window_tree_link_selected_cb),
                          window);

        priv->search = dh_search_new (book_manager);
        dh_util_state_set_notebook_page_name (priv->search, "search");
        gtk_notebook_append_page (GTK_NOTEBOOK (priv->control_notebook),
                                  priv->search,
                                  gtk_label_new (_("Search")));
        g_signal_connect (priv->search,
                          "link-selected",
                          G_CALLBACK (window_search_link_selected_cb),
                          window);

        priv->vbox = gtk_vbox_new (FALSE, 0);
        gtk_paned_add2 (GTK_PANED (priv->hpaned), priv->vbox);

        /* HTML tabs notebook. */
        priv->notebook = gtk_notebook_new ();
        gtk_container_set_border_width (GTK_CONTAINER (priv->notebook), 0);
        gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
        gtk_notebook_set_scrollable (GTK_NOTEBOOK (priv->notebook), TRUE);
        gtk_box_pack_start (GTK_BOX (priv->vbox), priv->notebook, TRUE, TRUE, 0);

        g_signal_connect (priv->notebook,
                          "switch-page",
                          G_CALLBACK (window_web_view_switch_page_cb),
                          window);
        g_signal_connect_after (priv->notebook,
                                "switch-page",
                                G_CALLBACK (window_web_view_switch_page_after_cb),
                                window);


        /* Create findbar. */
        priv->findbar = egg_find_bar_new ();
        gtk_widget_set_no_show_all (priv->findbar, TRUE);
        gtk_box_pack_start (GTK_BOX (priv->vbox), priv->findbar, FALSE, FALSE, 0);

        g_signal_connect (priv->findbar,
                          "notify::search-string",
                          G_CALLBACK(window_find_search_changed_cb),
                          window);
        g_signal_connect (priv->findbar,
                          "notify::case-sensitive",
                          G_CALLBACK (window_find_case_changed_cb),
                          window);
        g_signal_connect (priv->findbar,
                          "previous",
                          G_CALLBACK (window_find_previous_cb),
                          window);
        g_signal_connect (priv->findbar,
                          "next",
                          G_CALLBACK (window_find_next_cb),
                          window);
        g_signal_connect (priv->findbar,
                          "close",
                          G_CALLBACK (window_findbar_close_cb),
                          window);

        gtk_widget_show_all (priv->hpaned);

        window_update_zoom_actions_sensitiveness (window);
        window_open_new_tab (window, NULL, TRUE);
}


static gchar *
find_library_equivalent (DhWindow    *window,
                         const gchar *uri)
{
        DhWindowPriv *priv;
        gchar **components;
        GList *iter;
        DhLink *link;
        DhBookManager *book_manager;
        gchar *book_id;
        gchar *filename;
        gchar *local_uri = NULL;
        GList *books;

        components = g_strsplit (uri, "/", 0);
        book_id = components[4];
        filename = components[6];

        priv = window->priv;
        book_manager = dh_base_get_book_manager (priv->base);

        /* use list pointer to iterate */
        for (books = dh_book_manager_get_books (book_manager);
             !local_uri && books;
             books = g_list_next (books)) {
                DhBook *book = DH_BOOK (books->data);

                for (iter = dh_book_get_keywords (book);
                     iter;
                     iter = g_list_next (iter)) {
                        link = iter->data;
                        if (g_strcmp0 (dh_link_get_book_id (link), book_id) != 0) {
                                continue;
                        }
                        if (g_strcmp0 (dh_link_get_file_name (link), filename) != 0) {
                                continue;
                        }
                        local_uri = dh_link_get_uri (link);
                        break;
                }
        }

        g_strfreev (components);

        return local_uri;
}


static gboolean
window_web_view_navigation_policy_decision_requested (WebKitWebView             *web_view,
                                                      WebKitWebFrame            *frame,
                                                      WebKitNetworkRequest      *request,
                                                      WebKitWebNavigationAction *navigation_action,
                                                      WebKitWebPolicyDecision   *policy_decision,
                                                      DhWindow                  *window)
{
        DhWindowPriv *priv;
        const char   *uri;

        priv = window->priv;

        uri = webkit_network_request_get_uri (request);

#ifdef ERRORS_IN_INFOBAR
        /* make sure to hide the info bar on page change */
        gtk_widget_hide (window_get_active_info_bar (window));
#endif

        if (webkit_web_navigation_action_get_button (navigation_action) == 2) { /* middle click */
                webkit_web_policy_decision_ignore (policy_decision);
                g_signal_emit (window, signals[OPEN_LINK], 0, uri, DH_OPEN_LINK_NEW_TAB);
                return TRUE;
        }

        if (strcmp (uri, "about:blank") == 0) {
                return FALSE;
        }

        if (strncmp (uri, "http://library.gnome.org/devel/", 31) == 0) {
                gchar *local_uri = find_library_equivalent (window, uri);
                if (local_uri) {
                        webkit_web_policy_decision_ignore (policy_decision);
                        _dh_window_display_uri (window, local_uri);
                        g_free (local_uri);
                        return TRUE;
                }
        }

        if (strncmp (uri, "file://", 7) != 0) {
                webkit_web_policy_decision_ignore (policy_decision);
                gtk_show_uri (NULL, uri, GDK_CURRENT_TIME, NULL);
                return TRUE;
        }

        if (web_view == window_get_active_web_view (window)) {
                dh_book_tree_select_uri (DH_BOOK_TREE (priv->book_tree), uri);
                window_check_history (window, web_view);
        }

        return FALSE;
}


static gboolean
window_web_view_load_error_cb (WebKitWebView  *web_view,
                               WebKitWebFrame *frame,
                               gchar          *uri,
                               gpointer       *web_error,
                               DhWindow       *window)
{
#ifdef ERRORS_IN_INFOBAR
        GtkWidget *info_bar;
        GtkWidget *content_area;
        GtkWidget *message_label;
        GList     *children;
        gchar     *markup;

        info_bar = window_get_active_info_bar (window);
        markup = g_strdup_printf ("<b>%s</b>",
                       _("Error opening the requested link."));
        message_label = gtk_label_new (markup);
        gtk_misc_set_alignment (GTK_MISC (message_label), 0, 0.5);
        gtk_label_set_use_markup (GTK_LABEL (message_label), TRUE);
        content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar));
        children = gtk_container_get_children (GTK_CONTAINER (content_area));
        if (children) {
                gtk_container_remove (GTK_CONTAINER (content_area), children->data);
                g_list_free (children);
        }
        gtk_container_add (GTK_CONTAINER (content_area), message_label);
        gtk_widget_show (message_label);

        gtk_widget_show (info_bar);
        g_free (markup);

        return TRUE;
#else
	return FALSE;
#endif
}

static void
window_tree_link_selected_cb (GObject  *ignored,
                              DhLink   *link,
                              DhWindow *window)
{
        WebKitWebView *view;
        gchar         *uri;

        view = window_get_active_web_view (window);

        uri = dh_link_get_uri (link);
        webkit_web_view_load_uri (view, uri);
        g_free (uri);

        window_check_history (window, view);
}

static void
window_search_link_selected_cb (GObject  *ignored,
                                DhLink   *link,
                                DhWindow *window)
{
        DhWindowPriv  *priv;
        WebKitWebView *view;
        gchar         *uri;

        priv = window->priv;

        priv->selected_search_link = link;

        view = window_get_active_web_view (window);

        uri = dh_link_get_uri (link);
        webkit_web_view_load_uri (view, uri);
        g_free (uri);

        window_check_history (window, view);
}

static void
window_check_history (DhWindow      *window,
                      WebKitWebView *web_view)
{
        DhWindowPriv *priv;
        GtkAction    *action;

        priv = window->priv;

        action = gtk_action_group_get_action (priv->action_group, "Forward");
        g_object_set (action,
                      "sensitive", web_view ? webkit_web_view_can_go_forward (web_view) : FALSE,
                      NULL);

        action = gtk_action_group_get_action (priv->action_group, "Back");
        g_object_set (action,
                      "sensitive", web_view ? webkit_web_view_can_go_back (web_view) : FALSE,
                      NULL);
}

static void
window_web_view_title_changed_cb (WebKitWebView  *web_view,
                                  WebKitWebFrame *web_frame,
                                  const gchar    *title,
                                  DhWindow       *window)
{
        if (web_view == window_get_active_web_view (window)) {
                window_update_title (window, web_view, title);
        }

        window_tab_set_title (window, web_view, title);
}

static gboolean
window_web_view_button_press_event_cb (WebKitWebView  *web_view,
                                       GdkEventButton *event,
                                       DhWindow       *window)
{
        if (event->button == 3) {
                return TRUE;
        }

        return FALSE;
}

static gboolean
do_search (DhWindow *window)
{
        DhWindowPriv  *priv = window->priv;
        WebKitWebView *web_view;

        priv->find_source_id = 0;

        web_view = window_get_active_web_view (window);

        webkit_web_view_unmark_text_matches (web_view);
        webkit_web_view_mark_text_matches (
                web_view,
                egg_find_bar_get_search_string (EGG_FIND_BAR (priv->findbar)),
                egg_find_bar_get_case_sensitive (EGG_FIND_BAR (priv->findbar)), 0);
        webkit_web_view_set_highlight_text_matches (web_view, TRUE);

        webkit_web_view_search_text (
                web_view, egg_find_bar_get_search_string (EGG_FIND_BAR (priv->findbar)),
                egg_find_bar_get_case_sensitive (EGG_FIND_BAR (priv->findbar)),
                TRUE, TRUE);

	return FALSE;
}

static void
window_find_search_changed_cb (GObject    *object,
                               GParamSpec *pspec,
                               DhWindow   *window)
{
        DhWindowPriv *priv = window->priv;

        if (priv->find_source_id != 0) {
                g_source_remove (priv->find_source_id);
                priv->find_source_id = 0;
        }

        priv->find_source_id = g_timeout_add (300, (GSourceFunc)do_search, window);
}

static void
window_find_case_changed_cb (GObject    *object,
                             GParamSpec *pspec,
                             DhWindow   *window)
{
        DhWindowPriv  *priv = window->priv;;
        WebKitWebView *view;
        const gchar   *string;
        gboolean       case_sensitive;

        view = window_get_active_web_view (window);

        string = egg_find_bar_get_search_string (EGG_FIND_BAR (priv->findbar));
        case_sensitive = egg_find_bar_get_case_sensitive (EGG_FIND_BAR (priv->findbar));

        webkit_web_view_unmark_text_matches (view);
        webkit_web_view_mark_text_matches (view, string, case_sensitive, 0);
        webkit_web_view_set_highlight_text_matches (view, TRUE);
}

static void
window_find_next_cb (GtkEntry *entry,
                     DhWindow *window)
{
        DhWindowPriv  *priv = window->priv;
        WebKitWebView *view;
        const gchar   *string;
        gboolean       case_sensitive;

        view = window_get_active_web_view (window);

        gtk_widget_show (priv->findbar);

        string = egg_find_bar_get_search_string (EGG_FIND_BAR (priv->findbar));
        case_sensitive = egg_find_bar_get_case_sensitive (EGG_FIND_BAR (priv->findbar));

        webkit_web_view_search_text (view, string, case_sensitive, TRUE, TRUE);
}

static void
window_find_previous_cb (GtkEntry *entry,
                         DhWindow *window)
{
        DhWindowPriv  *priv = window->priv;
        WebKitWebView *view;
        const gchar   *string;
        gboolean       case_sensitive;

        view = window_get_active_web_view (window);

        gtk_widget_show (priv->findbar);

        string = egg_find_bar_get_search_string (EGG_FIND_BAR (priv->findbar));
        case_sensitive = egg_find_bar_get_case_sensitive (EGG_FIND_BAR (priv->findbar));

        webkit_web_view_search_text (view, string, case_sensitive, FALSE, TRUE);
}

static void
window_findbar_close_cb (GtkWidget *widget,
                         DhWindow  *window)
{
        DhWindowPriv  *priv = window->priv;
        WebKitWebView *view;

        view = window_get_active_web_view (window);

        gtk_widget_hide (priv->findbar);

        webkit_web_view_set_highlight_text_matches (view, FALSE);
}

#if 0
static void
window_web_view_open_new_tab_cb (WebKitWebView *web_view,
                                 const gchar   *location,
                                 DhWindow      *window)
{
        window_open_new_tab (window, location);
}
#endif

static void
window_web_view_tab_accel_cb (GtkAccelGroup   *accel_group,
                              GObject         *object,
                              guint            key,
                              GdkModifierType  mod,
                              DhWindow        *window)
{
        DhWindowPriv *priv;
        gint          i, num;

        priv = window->priv;

        num = -1;
        for (i = 0; i < (gint) G_N_ELEMENTS (tab_accel_keys); i++) {
                if (tab_accel_keys[i] == key) {
                        num = i;
                        break;
                }
        }

        if (num != -1) {
                gtk_notebook_set_current_page (
                        GTK_NOTEBOOK (priv->notebook), num);
        }
}

static int
window_open_new_tab (DhWindow    *window,
                     const gchar *location,
                     gboolean     switch_focus)
{
        DhWindowPriv *priv;
        GtkWidget    *view;
        GtkWidget    *vbox;
        GtkWidget    *scrolled_window;
        GtkWidget    *label;
        gint          num;
#ifdef ERRORS_IN_INFOBAR
        GtkWidget    *info_bar;
#endif

        priv = window->priv;

        /* Prepare the web view */
        view = webkit_web_view_new ();
        gtk_widget_show (view);
        dh_util_font_add_web_view (WEBKIT_WEB_VIEW (view));

#ifdef ERRORS_IN_INFOBAR
        /* Prepare the info bar */
        info_bar = gtk_info_bar_new ();
        gtk_widget_set_no_show_all (info_bar, TRUE);
        gtk_info_bar_add_button (GTK_INFO_BAR (info_bar),
                                 GTK_STOCK_CLOSE, GTK_RESPONSE_OK);
        gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar),
                                       GTK_MESSAGE_ERROR);
        g_signal_connect (info_bar, "response",
                          G_CALLBACK (gtk_widget_hide), NULL);
#endif

#if 0
        /* Leave this in for now to make it easier to experiment. */
        {
                WebKitWebSettings *settings;
                settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (view));

                g_object_set (settings,
                              "user-stylesheet-uri", "file://" DATADIR "/devhelp/devhelp.css",
                              NULL);
        }
#endif

        vbox = gtk_vbox_new (0, FALSE);
        gtk_widget_show (vbox);

        /* XXX: Really it would be much better to use real structures */
        g_object_set_data (G_OBJECT (vbox), "web_view", view);
#ifdef ERRORS_IN_INFOBAR
        g_object_set_data (G_OBJECT (vbox), "info_bar", info_bar);

        gtk_box_pack_start (GTK_BOX(vbox), info_bar, FALSE, TRUE, 0);
#endif

        scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        gtk_container_add (GTK_CONTAINER (scrolled_window), view);
        gtk_widget_show (scrolled_window);
        gtk_box_pack_start (GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

        label = window_new_tab_label (window, _("Empty Page"), vbox);
        gtk_widget_show_all (label);

        g_signal_connect (view, "title-changed",
                          G_CALLBACK (window_web_view_title_changed_cb),
                          window);
        g_signal_connect (view, "button-press-event",
                          G_CALLBACK (window_web_view_button_press_event_cb),
                          window);
        g_signal_connect (view, "navigation-policy-decision-requested",
                          G_CALLBACK (window_web_view_navigation_policy_decision_requested),
                          window);
        g_signal_connect (view, "load-error",
                          G_CALLBACK (window_web_view_load_error_cb),
                          window);

        num = gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
                                        vbox, NULL);

        gtk_notebook_set_tab_label (GTK_NOTEBOOK (priv->notebook),
                                    vbox, label);

        if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook)) > 1) {
                gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), TRUE);
        } else {
                gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
        }

        if (location) {
                webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view), location);
        } else {
                webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view), "about:blank");
        }

        if (switch_focus) {
                gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), num);
        }

        return num;
}

#ifndef GDK_WINDOWING_QUARTZ
static void
close_button_clicked_cb (GtkButton *button,
                         DhWindow  *window)
{
        GtkWidget *parent_tab;
        gint       pages;
        gint       i;

        parent_tab = g_object_get_data (G_OBJECT (button), "parent_tab");
        pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->priv->notebook));
        for (i=0; i<pages; i++) {
                if (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->priv->notebook), i) == parent_tab) {
                        window_close_tab (window, i);
                        break;
                }
        }
}

static void
tab_label_style_set_cb (GtkWidget *hbox,
                        GtkStyle  *previous_style,
                        gpointer   user_data)
{
        PangoFontMetrics *metrics;
        PangoContext     *context;
        GtkWidget        *button;
        GtkStyle         *style;
        gint              char_width;
        gint              h, w;

        context = gtk_widget_get_pango_context (hbox);
        style = gtk_widget_get_style (hbox);
        metrics = pango_context_get_metrics (context,
                                             style->font_desc,
                                             pango_context_get_language (context));

        char_width = pango_font_metrics_get_approximate_digit_width (metrics);
        pango_font_metrics_unref (metrics);

        gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (hbox),
                                           GTK_ICON_SIZE_MENU, &w, &h);

        gtk_widget_set_size_request (hbox, 15 * PANGO_PIXELS (char_width) + 2 * w, -1);

        button = g_object_get_data (G_OBJECT (hbox), "close-button");
        gtk_widget_set_size_request (button, w + 2, h + 2);
}
#endif

/* Don't create a close button on quartz, it looks very much out of
 * place.
 */
static GtkWidget*
window_new_tab_label (DhWindow        *window,
                      const gchar     *str,
                      const GtkWidget *parent)
{
        GtkWidget *label;
#ifndef GDK_WINDOWING_QUARTZ
        GtkWidget *hbox;
        GtkWidget *close_button;
        GtkWidget *image;

        hbox = gtk_hbox_new (FALSE, 4);

        label = gtk_label_new (str);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

        close_button = gtk_button_new ();
        gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
        gtk_button_set_focus_on_click (GTK_BUTTON (close_button), FALSE);
        gtk_widget_set_name (close_button, "devhelp-tab-close-button");
        g_object_set_data (G_OBJECT (close_button), "parent_tab", (gpointer) parent);

        image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
        g_signal_connect (close_button, "clicked",
                          G_CALLBACK (close_button_clicked_cb),
                          window);
        gtk_container_add (GTK_CONTAINER (close_button), image);

        gtk_box_pack_start (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);

        /* Set minimal size */
        g_signal_connect (hbox, "style-set",
                          G_CALLBACK (tab_label_style_set_cb),
                          NULL);

        g_object_set_data (G_OBJECT (hbox), "label", label);
        g_object_set_data (G_OBJECT (hbox), "close-button", close_button);

        return hbox;
#else
        label = gtk_label_new (str);
        g_object_set_data (G_OBJECT (label), "label", label);

        return label;
#endif
}

static WebKitWebView *
window_get_active_web_view (DhWindow *window)
{
        DhWindowPriv *priv;
        gint          page_num;
        GtkWidget    *page;

        priv = window->priv;

        page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
        if (page_num == -1) {
                return NULL;
        }

        page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->notebook), page_num);

        return g_object_get_data (G_OBJECT (page), "web_view");
}

#ifdef ERRORS_IN_INFOBAR
static GtkWidget *
window_get_active_info_bar (DhWindow *window)
{
        DhWindowPriv *priv;
        gint          page_num;
        GtkWidget    *page;

        priv = window->priv;

        page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
        if (page_num == -1) {
                return NULL;
        }

        page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->notebook), page_num);

        return g_object_get_data (G_OBJECT (page), "info_bar");
}
#endif

static void
window_update_title (DhWindow      *window,
                     WebKitWebView *web_view,
                     const gchar   *web_view_title)
{
        DhWindowPriv *priv;
        const gchar  *book_title;

        priv = window->priv;

        if (!web_view_title) {
                WebKitWebFrame *web_frame;

                web_frame = webkit_web_view_get_main_frame (web_view);
                web_view_title = webkit_web_frame_get_title (web_frame);
        }

        if (web_view_title && *web_view_title == '\0') {
                web_view_title = NULL;
        }

        book_title = dh_book_tree_get_selected_book_title (DH_BOOK_TREE (priv->book_tree));

        /* Don't use both titles if they are the same. */
        if (book_title && web_view_title && strcmp (book_title, web_view_title) == 0) {
                web_view_title = NULL;
        }

        if (!book_title) {
                /* i18n: Please don't translate "Devhelp" (it's marked as translatable
                 * for transliteration only) */
                book_title = _("Devhelp");
        }

        if (web_view_title) {
                gchar *full_title;
                full_title = g_strdup_printf ("%s - %s", book_title, web_view_title);
                gtk_window_set_title (GTK_WINDOW (window), full_title);
                g_free (full_title);
        } else {
                gtk_window_set_title (GTK_WINDOW (window), book_title);
        }
}

static void
window_tab_set_title (DhWindow      *window,
                      WebKitWebView *web_view,
                      const gchar   *title)
{
        DhWindowPriv *priv;
        gint          num_pages, i;
        GtkWidget    *page;
        GtkWidget    *hbox;
        GtkWidget    *label;
        GtkWidget    *page_web_view;

        priv = window->priv;

        if (!title || title[0] == '\0') {
                title = _("Empty Page");
        }

        num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));
        for (i = 0; i < num_pages; i++) {
                page = gtk_notebook_get_nth_page (
                        GTK_NOTEBOOK (priv->notebook), i);
                page_web_view = g_object_get_data (G_OBJECT (page), "web_view");

                /* The web_view widget is inside a frame. */
                if (page_web_view == GTK_WIDGET (web_view)) {
                        hbox = gtk_notebook_get_tab_label (
                                GTK_NOTEBOOK (priv->notebook), page);

                        if (hbox) {
                                label = g_object_get_data (G_OBJECT (hbox), "label");
                                gtk_label_set_text (GTK_LABEL (label), title);
                        }
                        break;
                }
        }
}

GtkWidget *
dh_window_new (DhBase *base)
{
        DhWindow     *window;
        DhWindowPriv *priv;

        window = g_object_new (DH_TYPE_WINDOW, NULL);
        priv = window->priv;

        priv->base = g_object_ref (base);

        window_populate (window);

        gtk_window_set_icon_name (GTK_WINDOW (window), "devhelp");

        dh_util_state_manage_window (GTK_WINDOW (window), "main/window");
        dh_util_state_manage_paned (GTK_PANED (priv->hpaned), "main/paned");
        dh_util_state_manage_notebook (GTK_NOTEBOOK (priv->control_notebook),
                                       "main/search_notebook",
                                       "content");

        return GTK_WIDGET (window);
}

void
dh_window_search (DhWindow    *window,
                  const gchar *str,
                  const gchar *book_id)
{
        DhWindowPriv *priv;

        g_return_if_fail (DH_IS_WINDOW (window));

        priv = window->priv;

        gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->control_notebook), 1);
        dh_search_set_search_string (DH_SEARCH (priv->search), str, book_id);
}

void
dh_window_focus_search (DhWindow *window)
{
        DhWindowPriv *priv;

        g_return_if_fail (DH_IS_WINDOW (window));

        priv = window->priv;

        gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->control_notebook), 1);
        gtk_widget_grab_focus (priv->search);
}

/* Only call this with a URI that is known to be in the docs. */
void
_dh_window_display_uri (DhWindow    *window,
                        const gchar *uri)
{
        DhWindowPriv  *priv;
        WebKitWebView *web_view;

        g_return_if_fail (DH_IS_WINDOW (window));
        g_return_if_fail (uri != NULL);

        priv = window->priv;

        web_view = window_get_active_web_view (window);
        webkit_web_view_load_uri (web_view, uri);
        dh_book_tree_select_uri (DH_BOOK_TREE (priv->book_tree), uri);
}
