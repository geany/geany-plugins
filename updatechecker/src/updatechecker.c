/*
 *      updatechecker.c
 *
 *      Copyright 2011-2015 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* A little plugin for regular checks for updates of Geany */

#include "libsoup/soup.h"
#include "stdlib.h"

#ifdef HAVE_CONFIG_H
	#include "config.h" /* for the gettext domain */
#endif

#include <geanyplugin.h>

GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;

PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(
    LOCALEDIR,
    GETTEXT_PACKAGE,
    _("Updatechecker"),
    _("Checks whether there are updates for Geany available"),
    VERSION,
    "Frank Lanitz <frank@frank.uvena.de>")

enum {
    UPDATECHECK_MANUAL,
    UPDATECHECK_STARTUP
};

#define UPDATE_CHECK_URL "https://geany.org/service/version/"

#define UPDATECHECKER_TYPE_KEY "updatechecker-type"

static GtkWidget *main_menu_item = NULL;
static void update_check_result_cb(GObject *session,
    GAsyncResult *result, gpointer user_data);

static gboolean check_on_startup = FALSE;

/* Configuration file */
static gchar *config_file = NULL;

static SoupSession *soup_session = NULL;


static struct
{
    GtkWidget *run_on_startup;
}
config_widgets;

typedef struct
{
    gint major;
    gint minor;
    gint mini;
    gchar *extra;
}
version_struct;


static void update_check(gint type)
{
    SoupMessage *msg;
    gchar *user_agent = g_strconcat("Updatechecker ", VERSION, " at Geany ",
                                     GEANY_VERSION, NULL);

    g_message("Checking for updates (querying URL \"%s\")", UPDATE_CHECK_URL);
    if (! soup_session)
        soup_session = soup_session_new_with_options(
                "user-agent", user_agent,
                "timeout", 10,
                NULL);

    g_free(user_agent);

    msg = soup_message_new ("GET", UPDATE_CHECK_URL);
    g_object_set_data(G_OBJECT(msg), UPDATECHECKER_TYPE_KEY, GINT_TO_POINTER(type));

    soup_session_send_and_read_async(soup_session, msg, G_PRIORITY_DEFAULT, NULL,
                                     update_check_result_cb, msg);
}



static void
on_geany_startup_complete(G_GNUC_UNUSED GObject *obj,
                          G_GNUC_UNUSED gpointer user_data)
{
    if (check_on_startup == TRUE)
    {
        update_check(UPDATECHECK_STARTUP);
    }
}


/* Based on some code by Sylpheed project.
 * http://sylpheed.sraoss.jp/en/
 * GPL FTW! */
static void parse_version_string(const gchar *ver, gint *major, gint *minor,
                 gint *micro, gchar **extra)
{
    gchar **vers;
    vers = g_strsplit(ver, ".", 4);
    if (vers[0])
    {
        *major = atoi(vers[0]);
        if (vers[1])
        {
            *minor = atoi(vers[1]);
            if (vers[2])
            {
                *micro = atoi(vers[2]);
                if (vers[3])
                {
                    *extra = g_strdup(vers[3]);
                }
                else
                {
                    *extra = NULL;
                }
            }
            else
            {
                *micro = 0;
            }
        }
        else
        {
            *minor = 0;
        }
    }
    else
    {
        *major = 0;
    }
    g_strfreev(vers);
}


/* Returns TRUE if the version installed is < as the version found
 * on the server. All other cases a causes a FALSE. */
static gboolean
version_compare(const gchar *current_version)
{
    version_struct geany_running;
    version_struct geany_current;

    parse_version_string(GEANY_VERSION, &geany_running.major,
        &geany_running.minor, &geany_running.mini, &geany_running.extra);

    parse_version_string(current_version, &geany_current.major,
        &geany_current.minor, &geany_current.mini, &geany_current.extra);

    if (geany_running.major < geany_current.major)
        return TRUE;
    if (geany_running.major > geany_current.major)
        return FALSE;

    if (geany_running.minor < geany_current.minor)
        return TRUE;
    if (geany_running.minor > geany_current.minor)
        return FALSE;

    if (geany_running.mini < geany_current.mini)
        return TRUE;
    if (geany_running.mini > geany_current.mini)
        return FALSE;

    return FALSE;
}


static gchar *bytes_to_string(GBytes *bytes)
{
    gsize bytes_size = g_bytes_get_size(bytes);
    gchar *str = g_malloc(bytes_size + 1);
    memcpy(str, g_bytes_get_data(bytes, NULL), bytes_size);
    str[bytes_size] = 0;
    return str;
}


static void update_check_result_cb(GObject *session,
    GAsyncResult *result, gpointer user_data)
{
    SoupMessage *msg = user_data;
    gpointer type_ptr = g_object_get_data(G_OBJECT(msg), UPDATECHECKER_TYPE_KEY);
    gint type = GPOINTER_TO_INT(type_ptr);
    GError *err = NULL;

    GBytes *bytes = soup_session_send_and_read_finish(SOUP_SESSION(session), result, &err);

    /* Checking whether we did get a valid (200) result */
    if (bytes && soup_message_get_status(msg) == 200)
    {
        gchar *remote_version = bytes_to_string(bytes);
        if (version_compare(remote_version) == TRUE)
        {
            gchar *update_msg = g_strdup_printf(
                _("There is a more recent version of Geany available: %s"),
                remote_version);
            dialogs_show_msgbox(GTK_MESSAGE_INFO, "%s", update_msg);
            g_message("%s", update_msg);
            g_free(update_msg);
        }
        else
        {
            const gchar *no_update_msg = _("No newer Geany version available.");
            if (type == UPDATECHECK_MANUAL)
            {
                dialogs_show_msgbox(GTK_MESSAGE_INFO, "%s", no_update_msg);
            }
            else
            {
                msgwin_status_add("%s", no_update_msg);
            }
            g_message("%s", no_update_msg);
        }
        g_free(remote_version);
    }
    else if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        ; /* nothing to do */
    else
    {
        gchar *error_message = g_strdup_printf(
            _("Unable to perform version check.\nError code: %d \nError message: %s"),
            soup_message_get_status(msg),
            err ? err->message : soup_message_get_reason_phrase(msg));
        if (type == UPDATECHECK_MANUAL)
        {
            dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", error_message);
        }
        else
        {
            msgwin_status_add("%s", error_message);
        }
        g_warning(
            "Connection error: Code: %d; Message: %s",
            soup_message_get_status(msg),
            err ? err->message : soup_message_get_reason_phrase(msg));
        g_free(error_message);
    }

    g_clear_error(&err);
    g_bytes_unref(bytes);
}

static void manual_check_activated_cb(GtkMenuItem *menuitem, gpointer gdata)
{
    update_check(UPDATECHECK_MANUAL);
}


static void
on_configure_response(G_GNUC_UNUSED GtkDialog *dialog, gint response,
                      G_GNUC_UNUSED gpointer user_data)
{
    if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
    {
        GKeyFile *config = g_key_file_new();
        gchar *data;
        gchar *config_dir = g_path_get_dirname(config_file);

        /* Crabbing options that has been set */
        check_on_startup =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_widgets.run_on_startup));

        /* write stuff to file */
        g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

        g_key_file_set_boolean(config, "general", "check_for_updates_on_startup",
            check_on_startup);

        if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR)
            && utils_mkdir(config_dir, TRUE) != 0)
        {
            dialogs_show_msgbox(GTK_MESSAGE_ERROR,
                _("Plugin configuration directory could not be created."));
        }
        else
        {
            /* write config to file */
            data = g_key_file_to_data(config, NULL, NULL);
            utils_write_file(config_file, data);
            g_free(data);
        }

        g_free(config_dir);
        g_key_file_free(config);
    }
}


GtkWidget *
plugin_configure(GtkDialog * dialog)
{
    GtkWidget   *vbox;
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    config_widgets.run_on_startup = gtk_check_button_new_with_label(
        _("Run updatecheck on startup"));

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_widgets.run_on_startup),
        check_on_startup);

    gtk_box_pack_start(GTK_BOX(vbox), config_widgets.run_on_startup, FALSE, FALSE, 2);
    gtk_widget_show_all(vbox);
    g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);
    return vbox;
}


/* Registering of callbacks for Geany events */
PluginCallback plugin_callbacks[] =
{
    { "geany-startup-complete", (GCallback) &on_geany_startup_complete, FALSE, NULL },
    { NULL, NULL, FALSE, NULL }
};


static void init_configuration(void)
{
    GKeyFile *config = g_key_file_new();

    /* loading configurations from file ...*/
    config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S,
    "plugins", G_DIR_SEPARATOR_S,
    "updatechecker", G_DIR_SEPARATOR_S, "general.conf", NULL);

    /* ... and Initialising options from config file */
    g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

    check_on_startup = utils_get_setting_boolean(config, "general",
        "check_for_updates_on_startup", FALSE);

    g_key_file_free(config);
}


void plugin_init(GeanyData *data)
{
    init_configuration();

    main_menu_item = gtk_menu_item_new_with_mnemonic(_("Check for Updates"));
    gtk_widget_show(main_menu_item);
    gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),
        main_menu_item);
    g_signal_connect(main_menu_item, "activate",
        G_CALLBACK(manual_check_activated_cb), NULL);
}

void plugin_cleanup(void)
{
    if (soup_session)
    {
        soup_session_abort(soup_session);
        g_clear_object(&soup_session);
    }
    gtk_widget_destroy(main_menu_item);
    g_free(config_file);
}
