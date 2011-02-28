//      geanypg.c
//
//      Copyright 2011 Hans Alves <alves.h88@gmail.com>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.

#include "geanypg.h"

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;

/* Check that the running Geany supports the plugin API version used below, and check
 * for binary compatibility. */
PLUGIN_VERSION_CHECK(201)

/* All plugins must set name, description, version and author. */
PLUGIN_SET_INFO(_("GeanyPG"),
                _("gpg encryption plugin for geany"),
                "0.1",
                _("Hans Alves <alves.h88@gmail.com>"))

static GtkWidget * main_menu_item = NULL;

static gpgme_error_t geanypg_init_gpgme(void)
{
    // Initialize the locale environment.
    setlocale(LC_ALL, "");
    fprintf(stderr, "GEANYPG: Using libgpgme version: %s\n",
            gpgme_check_version("1.1.0"));
    gpgme_set_locale(NULL, LC_CTYPE, setlocale(LC_CTYPE, NULL));
#ifdef LC_MESSAGES // only necessary for portability to W32 systems
    gpgme_set_locale(NULL, LC_MESSAGES, setlocale(LC_MESSAGES, NULL));
#endif
    return gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP);
}

gpgme_error_t geanypg_show_err_msg(gpgme_error_t err)
{
    gchar const * msg = (gchar const *)gpgme_strerror(err);
    gchar const * src = (gchar const *)gpgme_strsource(err);
    dialogs_show_msgbox(GTK_MESSAGE_ERROR, "Error from %s: %s\n.", src, msg);
    fprintf(stderr, "GEANYPG: %s\nFrom %s.\n", msg, src);
    return err;
}

void plugin_init(GeanyData *data)
{
    gpgme_error_t err = geanypg_init_gpgme();
    if (err)
    {
        geanypg_show_err_msg(err);
        return;
    }
    // Create a new menu item and show it
    main_menu_item = gtk_menu_item_new_with_mnemonic("GeanyPG");
    gtk_widget_show(main_menu_item);

    GtkWidget * submenu = gtk_menu_new();
    gtk_widget_show(submenu);
    GtkWidget * encrypt = gtk_menu_item_new_with_mnemonic("Encrypt");
    GtkWidget * sign = gtk_menu_item_new_with_mnemonic("Sign");
    GtkWidget * decrypt = gtk_menu_item_new_with_mnemonic("Decrypt / Verify");
    GtkWidget * verify = gtk_menu_item_new_with_mnemonic("Verify detached signature");

    gtk_widget_show(encrypt);
    gtk_widget_show(sign);
    gtk_widget_show(decrypt);
    gtk_widget_show(verify);

    gtk_menu_append(GTK_MENU(submenu), encrypt);
    gtk_menu_append(GTK_MENU(submenu), sign);
    gtk_menu_append(GTK_MENU(submenu), decrypt);
    gtk_menu_append(GTK_MENU(submenu), verify);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_menu_item), submenu);

    // Attach the new menu item to the Tools menu
    gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),
        main_menu_item);

    // Connect the menu item with a callback function
    // which is called when the item is clicked
    g_signal_connect(encrypt, "activate", G_CALLBACK(geanypg_encrypt_cb), NULL);
    g_signal_connect(sign,    "activate", G_CALLBACK(geanypg_sign_cb), NULL);
    g_signal_connect(decrypt, "activate", G_CALLBACK(geanypg_decrypt_cb), NULL);
    g_signal_connect(verify, "activate", G_CALLBACK(geanypg_verify_cb), NULL);
}


void plugin_cleanup(void)
{
    if (main_menu_item)
        gtk_widget_destroy(main_menu_item);
}
