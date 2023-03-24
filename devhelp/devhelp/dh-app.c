/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* SPDX-FileCopyrightText: 2002 CodeFactory AB
 * SPDX-FileCopyrightText: 2002 Mikael Hallendal <micke@imendio.com>
 * SPDX-FileCopyrightText: 2004-2008 Imendio AB
 * SPDX-FileCopyrightText: 2012 Aleksander Morgado <aleksander@gnu.org>
 * SPDX-FileCopyrightText: 2017, 2018 Sébastien Wilmet <swilmet@gnome.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"
#include "dh-app.h"
#include <glib/gi18n.h>
#include "dh-assistant.h"
#include "dh-preferences.h"
#include "dh-settings-app.h"
#include "dh-util-app.h"

G_DEFINE_TYPE (DhApp, dh_app, GTK_TYPE_APPLICATION);

static DhAssistant *
get_active_assistant_window (DhApp *app)
{
        GList *windows;
        GList *l;

        windows = gtk_application_get_windows (GTK_APPLICATION (app));

        for (l = windows; l != NULL; l = l->next) {
                GtkWindow *cur_window = GTK_WINDOW (l->data);

                if (DH_IS_ASSISTANT (cur_window))
                        return DH_ASSISTANT (cur_window);
        }

        return NULL;
}

static void
save_active_main_window_gsettings (DhApp *app)
{
        DhWindow *active_window;
        DhSettingsApp *settings;

        active_window = dh_app_get_active_main_window (app, FALSE);
        if (active_window == NULL)
                return;

        settings = dh_settings_app_get_singleton ();
        dh_util_window_settings_save (GTK_WINDOW (active_window),
                                      dh_settings_app_peek_window_settings (settings));
}

static void
save_active_assistant_window_gsettings (DhApp *app)
{
        DhAssistant *active_assistant;
        DhSettingsApp *settings;

        active_assistant = get_active_assistant_window (app);
        if (active_assistant == NULL)
                return;

        settings = dh_settings_app_get_singleton ();
        dh_util_window_settings_save (GTK_WINDOW (active_assistant),
                                      dh_settings_app_peek_assistant_settings (settings));
}

static void
new_window_cb (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
        DhApp *app = DH_APP (user_data);
        GtkWidget *new_window;

        save_active_main_window_gsettings (app);

        new_window = dh_window_new (GTK_APPLICATION (app));
        gtk_widget_show_all (new_window);
}

static void
preferences_cb (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
        DhApp *app = DH_APP (user_data);
        GtkWindow *parent_window;

        parent_window = (GtkWindow *) dh_app_get_active_main_window (app, FALSE);
        dh_preferences_show_dialog (parent_window);
}

static void
help_cb (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       user_data)
{
        DhApp *app = DH_APP (user_data);
        GtkWindow *window;
        GError *error = NULL;

        window = (GtkWindow *) dh_app_get_active_main_window (app, FALSE);

        gtk_show_uri_on_window (window, "help:devhelp", GDK_CURRENT_TIME, &error);

        if (error != NULL) {
                g_warning ("Failed to open the documentation: %s", error->message);
                g_clear_error (&error);
        }
}

static void
about_cb (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
        DhApp *app = DH_APP (user_data);
        GtkWindow *parent_window;

        const gchar *authors[] = {
                "Mikael Hallendal <micke@imendio.com>",
                "Richard Hult <richard@imendio.com>",
                "Johan Dahlin <johan@gnome.org>",
                "Ross Burton <ross@burtonini.com>",
                "Aleksander Morgado <aleksander@lanedo.com>",
                "Thomas Bechtold <toabctl@gnome.org>",
                "Frédéric Péters <fpeters@0d.be>",
                "Sébastien Wilmet <swilmet@gnome.org>",
                NULL
        };

        parent_window = (GtkWindow *) dh_app_get_active_main_window (app, FALSE);

        gtk_show_about_dialog (parent_window,
                               /* Translators: please don't translate "Devhelp" (it's marked as
                                * translatable for transliteration only).
                                */
                               "program-name", dh_util_is_devel_build ()
                                        ? _("Devhelp (development build)")
                                        : _("Devhelp"),
                               "version", PACKAGE_VERSION,
                               "comments", _("A developer tool for browsing and searching API documentation"),
                               "authors", authors,
                               "translator-credits", _("translator-credits"),
                               "website", "https://wiki.gnome.org/Apps/Devhelp",
                               "website-label", _("Devhelp Website"),
                               "logo-icon-name", APPLICATION_ID,
                               "license-type", GTK_LICENSE_GPL_3_0,
                               "copyright", "Copyright 2001-2021 – the Devhelp team",
                               NULL);
}

static void
quit_cb (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       user_data)
{
        DhApp *app = DH_APP (user_data);

        save_active_main_window_gsettings (app);
        save_active_assistant_window_gsettings (app);

        g_application_quit (G_APPLICATION (app));
}

static void
search_cb (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
        DhApp *app = DH_APP (user_data);
        const gchar *keyword;
        DhWindow *window;

        keyword = g_variant_get_string (parameter, NULL);
        if (keyword == NULL || keyword[0] == '\0') {
                g_warning ("Cannot search in application window: no keyword given.");
                return;
        }

        window = dh_app_get_active_main_window (app, TRUE);
        dh_window_search (window, keyword);
        gtk_window_present (GTK_WINDOW (window));
}

static void
search_assistant_cb (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
        DhApp *app = DH_APP (user_data);
        DhAssistant *assistant;
        const gchar *keyword;

        keyword = g_variant_get_string (parameter, NULL);
        if (keyword == NULL || keyword[0] == '\0') {
                g_warning ("Cannot look for keyword in Search Assistant: no keyword given.");
                return;
        }

        assistant = get_active_assistant_window (app);
        if (assistant == NULL)
                assistant = dh_assistant_new (app);

        dh_assistant_search (assistant, keyword);
        gtk_window_present (GTK_WINDOW (assistant));
}

static void
raise_cb (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
        DhApp *app = DH_APP (user_data);
        GtkWindow *window;

        window = gtk_application_get_active_window (GTK_APPLICATION (app));
        if (window == NULL)
                window = (GtkWindow *) dh_app_get_active_main_window (app, TRUE);

        gtk_window_present (window);
}

static void
add_action_entries (DhApp *app)
{
        const GActionEntry app_entries[] = {
                /* General actions */
                { "new-window", new_window_cb },
                { "preferences", preferences_cb },
                { "help", help_cb },
                { "about", about_cb },
                { "quit", quit_cb },

                /* Additional commandline-specific actions */
                { "search", search_cb, "s" },
                { "search-assistant", search_assistant_cb, "s" },
                { "raise", raise_cb },
        };

        g_action_map_add_action_entries (G_ACTION_MAP (app),
                                         app_entries,
                                         G_N_ELEMENTS (app_entries),
                                         app);
}

static void
setup_go_to_tab_accelerators (GtkApplication *app)
{
        const gchar *accels[] = {NULL, NULL};
        gint key_num;

        for (key_num = 1; key_num <= 9; key_num++) {
                gchar *accel;
                gchar *detailed_action_name;

                accel = g_strdup_printf ("<Alt>%d", key_num);
                accels[0] = accel;

                detailed_action_name = g_strdup_printf ("win.go-to-tab(uint16 %d)", key_num - 1);

                gtk_application_set_accels_for_action (app, detailed_action_name, accels);

                g_free (accel);
                g_free (detailed_action_name);
        }

        /* On a typical keyboard the 0 is after 9, so it's the equivalent of 10
         * (9 starting from 0).
         */
        accels[0] = "<Alt>0";
        gtk_application_set_accels_for_action (app, "win.go-to-tab(uint16 9)", accels);
}

static void
setup_accelerators (GtkApplication *app)
{
        const gchar *accels[] = {NULL, NULL, NULL, NULL};

        setup_go_to_tab_accelerators (app);

        accels[0] = "<Control>0";
        gtk_application_set_accels_for_action (app, "win.zoom-default", accels);

        accels[0] = "<Control>minus";
        gtk_application_set_accels_for_action (app, "win.zoom-out", accels);

        /* For "<Control>equal": Epiphany also has this keyboard shortcut for
         * zoom-in. On keyboards the = and + are usually on the same key, but +
         * is less convenient to type because Shift must be pressed too.
         * Apparently it's usual on Windows to press Ctrl+= to zoom in.
         * https://bugzilla.gnome.org/show_bug.cgi?id=743704
         */
        accels[0] = "<Control>plus";
        accels[1] = "<Control>equal";
        gtk_application_set_accels_for_action (app, "win.zoom-in", accels);
        accels[1] = NULL;

        accels[0] = "<Control>f";
        gtk_application_set_accels_for_action (app, "win.find", accels);

        accels[0] = "<Control>c";
        gtk_application_set_accels_for_action (app, "win.copy", accels);

        accels[0] = "<Control>p";
        gtk_application_set_accels_for_action (app, "win.print", accels);

        accels[0] = "<Control>t";
        gtk_application_set_accels_for_action (app, "win.new-tab", accels);

        accels[0] = "<Control>n";
        gtk_application_set_accels_for_action (app, "app.new-window", accels);

        accels[0] = "<Control>Page_Down";
        gtk_application_set_accels_for_action (app, "win.next-tab", accels);

        accels[0] = "<Control>Page_Up";
        gtk_application_set_accels_for_action (app, "win.prev-tab", accels);

        accels[0] = "<Control>w";
        gtk_application_set_accels_for_action (app, "win.close-tab", accels);

        accels[0] = "<Control>q";
        gtk_application_set_accels_for_action (app, "app.quit", accels);

        accels[0] = "F1";
        gtk_application_set_accels_for_action (app, "app.help", accels);

        accels[0] = "F9";
        gtk_application_set_accels_for_action (app, "win.show-sidebar", accels);

        accels[0] = "F10";
        gtk_application_set_accels_for_action (app, "win.show-window-menu", accels);

        accels[0] = "<Alt>Right";
        accels[1] = "Forward";
        gtk_application_set_accels_for_action (app, "win.go-forward", accels);

        accels[0] = "<Alt>Left";
        accels[1] = "Back";
        gtk_application_set_accels_for_action (app, "win.go-back", accels);

        accels[0] = "<Control>k";
        accels[1] = "<Control>s";
        accels[2] = "<Control>l";
        gtk_application_set_accels_for_action (app, "win.focus-search", accels);
}

static void
dh_app_startup (GApplication *application)
{
        DhApp *app = DH_APP (application);

        g_application_set_resource_base_path (application, "/org/gnome/devhelp");

        if (G_APPLICATION_CLASS (dh_app_parent_class)->startup != NULL)
                G_APPLICATION_CLASS (dh_app_parent_class)->startup (application);

        add_action_entries (app);
        setup_accelerators (GTK_APPLICATION (app));
}

static void
dh_app_activate (GApplication *app)
{
        g_action_group_activate_action (G_ACTION_GROUP (app), "new-window", NULL);
}

static gboolean option_version;

static GOptionEntry options[] = {
        { "new-window", 'n',
          0, G_OPTION_ARG_NONE, NULL,
          N_("Opens a new Devhelp window"),
          NULL
        },
        { "search", 's',
          0, G_OPTION_ARG_STRING, NULL,
          N_("Search for a keyword"),
          N_("KEYWORD")
        },
        { "search-assistant", 'a',
          0, G_OPTION_ARG_STRING, NULL,
          N_("Search and display any hit in the assistant window"),
          N_("KEYWORD")
        },
        { "version", 'v',
          0, G_OPTION_ARG_NONE, &option_version,
          N_("Display the version and exit"),
          NULL
        },
        { "quit", 'q',
          0, G_OPTION_ARG_NONE, NULL,
          N_("Quit any running Devhelp"),
          NULL
        },
        { NULL }
};

static gint
dh_app_handle_local_options (GApplication *app,
                             GVariantDict *local_options)
{
        if (option_version) {
                g_print ("%s %s\n", g_get_application_name (), PACKAGE_VERSION);
                return 0;
        }

        return -1;
}

static gint
dh_app_command_line (GApplication            *g_app,
                     GApplicationCommandLine *command_line)
{
        DhApp *app = DH_APP (g_app);
        GVariantDict *options_dict;
        gboolean option_new_window = FALSE;
        const gchar *option_search = NULL;
        const gchar *option_search_assistant = NULL;
        gboolean option_quit = FALSE;

        options_dict = g_application_command_line_get_options_dict (command_line);

        g_variant_dict_lookup (options_dict, "new-window", "b", &option_new_window);
        g_variant_dict_lookup (options_dict, "search", "&s", &option_search);
        g_variant_dict_lookup (options_dict, "search-assistant", "&s", &option_search_assistant);
        g_variant_dict_lookup (options_dict, "quit", "b", &option_quit);

        if (option_quit) {
                g_action_group_activate_action (G_ACTION_GROUP (app), "quit", NULL);
                return 0;
        }

        if (option_new_window)
                g_action_group_activate_action (G_ACTION_GROUP (app), "new-window", NULL);

        if (option_search != NULL)
                g_action_group_activate_action (G_ACTION_GROUP (app),
                                                "search",
                                                g_variant_new_string (option_search));

        if (option_search_assistant != NULL)
                g_action_group_activate_action (G_ACTION_GROUP (app),
                                                "search-assistant",
                                                g_variant_new_string (option_search_assistant));

        g_action_group_activate_action (G_ACTION_GROUP (app), "raise", NULL);

        return 0;
}

static void
dh_app_class_init (DhAppClass *klass)
{
        GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

        application_class->startup = dh_app_startup;
        application_class->activate = dh_app_activate;
        application_class->handle_local_options = dh_app_handle_local_options;
        application_class->command_line = dh_app_command_line;
}

static void
dh_app_init (DhApp *app)
{
        /* Translators: please don't translate "Devhelp" (it's marked as
         * translatable for transliteration only).
         */
        g_set_application_name (_("Devhelp"));
        gtk_window_set_default_icon_name (APPLICATION_ID);

        g_application_add_main_option_entries (G_APPLICATION (app), options);
}

DhApp *
dh_app_new (void)
{
        return g_object_new (DH_TYPE_APP,
                             "application-id", APPLICATION_ID,
                             "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                             NULL);
}

/* Returns: (transfer none) (nullable). */
DhWindow *
dh_app_get_active_main_window (DhApp    *app,
                               gboolean  create_if_none)
{
        GList *windows;
        GList *l;

        g_return_val_if_fail (DH_IS_APP (app), NULL);

        windows = gtk_application_get_windows (GTK_APPLICATION (app));

        for (l = windows; l != NULL; l = l->next) {
                GtkWindow *cur_window = GTK_WINDOW (l->data);

                if (DH_IS_WINDOW (cur_window))
                        return DH_WINDOW (cur_window);
        }

        if (create_if_none) {
                g_action_group_activate_action (G_ACTION_GROUP (app), "new-window", NULL);

                /* Look again, but with create_if_none = FALSE to avoid an
                 * infinite recursion in case creating a new main window fails.
                 */
                return dh_app_get_active_main_window (app, FALSE);
        }

        return NULL;
}
