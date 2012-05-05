/*
 *  geniuspaste - paste your code on your favorite pastebin.
 *
 *  Copyright 2012 Enrico "Enrix835" Trotta <enrico.trt@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "libsoup/soup.h"
#include "stdlib.h"
#include "string.h"

#ifdef HAVE_CONFIG_H
#include "config.h" /* for the gettext domain */
#endif

#include <geanyplugin.h>

#ifdef G_OS_WIN32
#define USERNAME        getenv("USERNAME")
#else
#define USERNAME        getenv("USER")
#endif

#define CODEPAD_ORG 		0
#define PASTEBIN_COM 		1
#define PASTEBIN_GEANY_ORG 	2
#define DPASTE_DE 			3
#define SPRUNGE_US 			4

#define DEFAULT_TYPE_CODEPAD langs_supported_codepad[8];
#define DEFAULT_TYPE_DPASTE langs_supported_dpaste[15];

GeanyPlugin *geany_plugin;
GeanyData *geany_data;
GeanyFunctions *geany_functions;

static GtkWidget *main_menu_item = NULL;

static const gchar *websites[] = {
    "http://codepad.org",
    "http://pastebin.com/api_public.php",
    "http://pastebin.geany.org/api/",
    "http://dpaste.de/api/",
    "http://sprunge.us/",
};

static struct {
    GtkWidget *combo;
    GtkWidget *check_button;
} widgets;

static gint website_selected;
static gboolean check_button_is_checked = FALSE;

PLUGIN_VERSION_CHECK(147)
PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE, "GeniusPaste",
    _("Paste your code on your favorite pastebin"), 
    "0.1", "Enrico Trotta");

static gint indexof(const gchar * string, gchar c)
{
    gchar * occ = strchr(string, c);
    return occ ? occ - string : -1;
}

static gint last_indexof(const gchar * string, gchar c)
{
    gchar * occ = strrchr(string, c);
    return occ ? occ - string : -1;
}

static void paste(const gchar * website)
{
    SoupSession *session = soup_session_async_new();
    SoupMessage *msg = NULL;

    GeanyDocument *doc = document_get_current();

    if(doc == NULL) {
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, "There are no opened documents. Open one and retry.\n");
        return;
    }

    GeanyFiletype *ft = doc->file_type;

    GError *error;

    gchar *f_content;
    gchar *f_type = g_strdup(ft->name);
    gchar *f_path = doc->real_path;
    gchar *f_name = doc->file_name;
    gchar *f_title;
    gchar *p_url;
    gchar *formdata = NULL;
    gchar *temp_body;
    gchar **tokens_array;

    const gchar *langs_supported_codepad[] = { "C", "C++", "D", "Haskell",
            "Lua", "OCaml", "PHP", "Perl", "Plain Text",
            "Python", "Ruby", "Scheme", "Tcl"
    };

    const gchar *langs_supported_dpaste[] = { "Bash", "C", "CSS", "Diff",
            "Django/Jinja", "HTML", "IRC logs", "JavaScript", "PHP",
            "Python console session", "Python Traceback", "Python",
            "Python3", "Restructured Text", "SQL", "Text only"
    };

    gint occ_position;
    gint i;
    guint status;
    gsize f_lenght;
    gboolean result;

    occ_position = last_indexof(f_name, G_DIR_SEPARATOR);
    f_title = f_name + occ_position + 1;

    switch (website_selected) {

    case CODEPAD_ORG:

        for (i = 0; i < G_N_ELEMENTS(langs_supported_codepad); i++) {
            if (g_strcmp0(f_type, langs_supported_codepad[i]) == 0)
                break;
            else
                f_type = DEFAULT_TYPE_CODEPAD;
        }

        result = g_file_get_contents(f_path, &f_content, &f_lenght, &error);
        if(result == FALSE) {
            dialogs_show_msgbox(GTK_MESSAGE_ERROR, "Unable to the the content of the file");
            g_error_free(error);
            return;
        }

        msg = soup_message_new("POST", website);
        formdata = soup_form_encode("lang", f_type, "code", f_content,
                                    "submit", "Submit", NULL);

        break;

    case PASTEBIN_COM:

        result = g_file_get_contents(f_path, &f_content, &f_lenght, &error);
        if(result == FALSE) {
            dialogs_show_msgbox(GTK_MESSAGE_ERROR, "Unable to the the content of the file");
            g_error_free(error);
            return;
        }

        msg = soup_message_new("POST", website);
        formdata = soup_form_encode("paste_code", f_content, "paste_format",
                                    f_type, "paste_name", f_title, NULL);

        break;


    case DPASTE_DE:

        for (i = 0; i < G_N_ELEMENTS(langs_supported_dpaste); i++) {
            if (g_strcmp0(f_type, langs_supported_dpaste[i]) == 0)
                break;
            else
                f_type = DEFAULT_TYPE_DPASTE;
        }

        result = g_file_get_contents(f_path, &f_content, &f_lenght, &error);
        if(result == FALSE) {
            dialogs_show_msgbox(GTK_MESSAGE_ERROR, "Unable to the the content of the file");
            g_error_free(error);
            return;
        }

        msg = soup_message_new("POST", website);
        /* apparently dpaste.de detects automatically the syntax of the
         * pasted code so 'lexer' should be unneeded
         */
        formdata = soup_form_encode("content", f_content, "title", f_title,
                                    "lexer", f_type, NULL);

        break;

    case SPRUNGE_US:

        result = g_file_get_contents(f_path, &f_content, &f_lenght, &error);
        if(result == FALSE) {
            dialogs_show_msgbox(GTK_MESSAGE_ERROR, "Unable to the the content of the file");
            g_error_free(error);
            return;
        }

        msg = soup_message_new("POST", website);
        formdata = soup_form_encode("sprunge", f_content, NULL);

        break;

    case PASTEBIN_GEANY_ORG:

        result = g_file_get_contents(f_path, &f_content, &f_lenght, &error);
        if(result == FALSE) {
            dialogs_show_msgbox(GTK_MESSAGE_ERROR, "Unable to the the content of the file");
            g_error_free(error);
            return;
        }

        msg = soup_message_new("POST", website);
        formdata = soup_form_encode("content", f_content, "author", USERNAME,
                                    "title", f_title, "lexer", f_type, NULL);

        break;

    }

    soup_message_set_request(msg, "application/x-www-form-urlencoded",
                             SOUP_MEMORY_COPY, formdata, strlen(formdata));

    status = soup_session_send_message(session, msg);
    p_url = g_strdup(msg->response_body->data);
    g_free(f_content);
    
    if(status == SOUP_STATUS_OK) {

        /*
         * codepad.org doesn't return only the url of the new snippet pasted
         * but an html page. This minimal parser will get the bare url.
         */

        if (website_selected == CODEPAD_ORG) {
            temp_body = g_strdup(p_url);
            tokens_array = g_strsplit(temp_body, "<a href=\"", 0);

            /* cuts the string when it finds the first occurrence of '/'
             * It shoud work even if codepad would change its url.
             */

            p_url = g_strdup(tokens_array[5]);
            occ_position = indexof(tokens_array[5], '\"');
            
            g_free(temp_body);
            g_strfreev(tokens_array);
            
            if(occ_position != -1) {
                p_url[occ_position] = '\0';
            } else {
                dialogs_show_msgbox(GTK_MESSAGE_ERROR, "Unable to paste the code on codepad.org\n"
                                    "Retry or select another pastebin.");
                g_free(p_url);
                return;
            }

        } else if(website_selected == DPASTE_DE) {
            p_url = g_strndup(p_url + 1, strlen(p_url) - 2);

        } else if(website_selected == SPRUNGE_US) {

            /* in order to enable the syntax highlightning on sprunge.us
             * it is necessary to append at the returned url a question
             * mark '?' followed by the file type.
             *
             * e.g. sprunge.us/xxxx?c
             */
            p_url[strlen(p_url) - 1] = '\0';
            f_type[0] = g_ascii_tolower(f_type[0]);
            temp_body = g_strdup_printf("?%s", f_type);
            g_strlcat(p_url + 1, temp_body, -1);
            p_url = g_strchomp(p_url);
            g_free(temp_body);
        }

        if (check_button_is_checked) {
            utils_open_browser(p_url);
        } else {
            dialogs_show_msgbox(GTK_MESSAGE_INFO, "%s", p_url);
        }

    } else {
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, "Unable to paste the code. Check your connection and retry.\n"
                            "Error code: %d\n", status);
    }

    g_free(p_url);
}

static void item_activate(GtkMenuItem * menuitem, gpointer gdata)
{
    paste(websites[website_selected]);
}

static void on_configure_response(GtkDialog * dialog, gint response, gpointer * user_data)
{
    if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
        website_selected =  gtk_combo_box_get_active(GTK_COMBO_BOX(widgets.combo));

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.check_button))) {
            check_button_is_checked = TRUE;
        }
    }
}

GtkWidget *plugin_configure(GtkDialog * dialog)
{
    gint i;
    GtkWidget *label, *vbox;

    vbox = gtk_vbox_new(FALSE, 6);

    label = gtk_label_new(_("Select a pastebin:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    widgets.combo = gtk_combo_box_text_new();

    for (i = 0; i < G_N_ELEMENTS(websites); i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets.combo), websites[i]);

    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets.combo), 0);

    widgets.check_button = gtk_check_button_new_with_label("show your paste in a new browser tab");

    gtk_container_add(GTK_CONTAINER(vbox), label);
    gtk_container_add(GTK_CONTAINER(vbox), widgets.combo);
    gtk_container_add(GTK_CONTAINER(vbox), widgets.check_button);

    gtk_widget_show_all(vbox);

    g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);

    return vbox;
}

static void add_menu_item()
{
    GtkWidget *paste_item;

    paste_item = gtk_menu_item_new_with_mnemonic(_("_Paste it!"));
    gtk_widget_show(paste_item);
    gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),
                      paste_item);
    g_signal_connect(paste_item, "activate", G_CALLBACK(item_activate),
                     NULL);

    main_menu_item = paste_item;
}

void plugin_init(GeanyData * data)
{
    add_menu_item();
}


void plugin_cleanup(void)
{
    gtk_widget_destroy(main_menu_item);
}
