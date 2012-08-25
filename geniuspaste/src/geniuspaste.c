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

#include <libsoup/soup.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifdef HAVE_CONFIG_H
#include "config.h" /* for the gettext domain */
#endif

#include <geanyplugin.h>


#define PLUGIN_NAME "GeniusPaste"
#define PLUGIN_VERSION "0.2"

#ifdef G_OS_WIN32
#define USERNAME        g_getenv("USERNAME")
#else
#define USERNAME        g_getenv("USER")
#endif

/* stay compatible with GTK < 2.24 */
#if !GTK_CHECK_VERSION(2,24,0)
#define gtk_combo_box_text_new         gtk_combo_box_new_text
#define gtk_combo_box_text_append_text gtk_combo_box_append_text
#define GTK_COMBO_BOX_TEXT             GTK_COMBO_BOX
#endif

#define CODEPAD_ORG 		0
#define TINYPASTE_COM 		1
#define PASTEBIN_GEANY_ORG 	2
#define DPASTE_DE 			3
#define SPRUNGE_US 			4

#define DEFAULT_TYPE_CODEPAD langs_supported_codepad[8];
#define DEFAULT_TYPE_DPASTE langs_supported_dpaste[15];

GeanyPlugin *geany_plugin;
GeanyData *geany_data;
GeanyFunctions *geany_functions;

static GtkWidget *main_menu_item = NULL;

static const gchar *websites[] =
{
    "codepad.org",
    "tinypaste.com",
    "pastebin.geany.org",
    "dpaste.de",
    "sprunge.us",
};

static const gchar *websites_api[] =
{
    "http://codepad.org/",
    "http://tinypaste.com/api/create.xml",
    "http://pastebin.geany.org/api/",
    "http://dpaste.de/api/",
    "http://sprunge.us/",
};

static struct
{
    GtkWidget *combo;
    GtkWidget *check_button;
    GtkWidget *author_entry;
} widgets;

static gchar *config_file = NULL;
static gchar *author_name = NULL;

static gint website_selected;
static gboolean check_button_is_checked = FALSE;

PLUGIN_VERSION_CHECK(147)
PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE, PLUGIN_NAME,
                             _("Paste your code on your favorite pastebin"),
                             PLUGIN_VERSION, "Enrico Trotta <enrico.trt@gmail.com>")

static gint indexof(const gchar * string, gchar c)
{
    gchar * occ = strchr(string, c);
    return occ ? occ - string : -1;
}

static void load_settings(void)
{
    GKeyFile *config = g_key_file_new();

    config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
                              "geniuspaste", G_DIR_SEPARATOR_S, "geniuspaste.conf", NULL);
    g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);
    
    website_selected = utils_get_setting_integer(config, "geniuspaste", "website", PASTEBIN_GEANY_ORG);
    check_button_is_checked = utils_get_setting_boolean(config, "geniuspaste", "open_browser", FALSE);
    author_name = utils_get_setting_string(config, "geniuspaste", "author_name", USERNAME);
    
    g_key_file_free(config);
}

static void save_settings(void)
{
    GKeyFile *config = g_key_file_new();
    gchar *data;
    gchar *config_dir = g_path_get_dirname(config_file);

    g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

    g_key_file_set_integer(config, "geniuspaste", "website", website_selected);
    g_key_file_set_boolean(config, "geniuspaste", "open_browser", check_button_is_checked);
    g_key_file_set_string(config, "geniuspaste", "author_name", author_name);

    if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0)
    {
        dialogs_show_msgbox(GTK_MESSAGE_ERROR,
                            _("Plugin configuration directory could not be created."));
    }
    else
    {
        data = g_key_file_to_data(config, NULL, NULL);
        utils_write_file(config_file, data);
        g_free(data);
    }
    
    g_free(config_dir);
    g_key_file_free(config);
}

static gchar *get_paste_text(GeanyDocument *doc, gsize *text_len)
{
    gsize len;
    gchar *paste_text;

    if (sci_has_selection(doc->editor->sci))
    {
        len = sci_get_selected_text_length(doc->editor->sci) + 1;
        paste_text = sci_get_selection_contents(doc->editor->sci);
    }
    else
    {
        len = sci_get_length(doc->editor->sci) + 1;
        paste_text = sci_get_contents(doc->editor->sci, len);
    }

    if (text_len)
        *text_len = len;

    return paste_text;
}

static void paste(GeanyDocument * doc, const gchar * website)
{
    SoupSession *session;
    SoupMessage *msg = NULL;

    gchar *f_content;
    gchar const *f_type;
    gchar *f_title;
    gchar *p_url;
    gchar *formdata = NULL;
    gchar *user_agent = NULL;
    gchar **tokens_array;

    const gchar *langs_supported_codepad[] =
    {
        "C", "C++", "D", "Haskell",
        "Lua", "OCaml", "PHP", "Perl", "Plain Text",
        "Python", "Ruby", "Scheme", "Tcl"
    };

    const gchar *langs_supported_dpaste[] =
    {
        "Bash", "C", "CSS", "Diff",
        "Django/Jinja", "HTML", "IRC logs", "JavaScript", "PHP",
        "Python console session", "Python Traceback", "Python",
        "Python3", "Restructured Text", "SQL", "Text only"
    };

    gint occ_position;
    gint i;
    guint status;
    gsize f_length;

    g_return_if_fail(doc && doc->is_valid);

    f_type = doc->file_type->name;

    if (doc->file_name == NULL)
        f_title = document_get_basename_for_display(doc, -1);
    else
        f_title = g_path_get_basename(doc->file_name);

    load_settings();
    
    f_content = get_paste_text(doc, &f_length);
    if (f_content == NULL || f_content[0] == '\0')
    {
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Refusing to create blank paste"));
        return;
    }

    switch (website_selected)
    {

    case CODEPAD_ORG:

        for (i = 0; i < G_N_ELEMENTS(langs_supported_codepad); i++)
        {
            if (g_strcmp0(f_type, langs_supported_codepad[i]) == 0)
                break;
            else
                f_type = DEFAULT_TYPE_CODEPAD;
        }

        msg = soup_message_new("POST", website);
        formdata = soup_form_encode("lang", f_type,
                                    "code", f_content,
                                    "submit", "Submit",
                                    NULL);

        break;

    case TINYPASTE_COM:

        msg = soup_message_new("POST", website);
        formdata = soup_form_encode("paste", f_content, 
                                    "title", f_title,
                                    "is_code", g_strcmp0(f_type, "None") == 0 ? "0" : "1",
                                    NULL);

        break;


    case DPASTE_DE:

        for (i = 0; i < G_N_ELEMENTS(langs_supported_dpaste); i++)
        {
            if (g_strcmp0(f_type, langs_supported_dpaste[i]) == 0)
                break;
            else
                f_type = DEFAULT_TYPE_DPASTE;
        }

        msg = soup_message_new("POST", website);
        /* apparently dpaste.de detects automatically the syntax of the
         * pasted code so 'lexer' should be unneeded
         */
        formdata = soup_form_encode("content", f_content,
                                    "title", f_title,
                                    "lexer", f_type,
                                    NULL);

        break;

    case SPRUNGE_US:

        msg = soup_message_new("POST", website);
        formdata = soup_form_encode("sprunge", f_content, NULL);

        break;

    case PASTEBIN_GEANY_ORG:

        msg = soup_message_new("POST", website);
        formdata = soup_form_encode("content", f_content,
                                    "author", author_name,
                                    "title", f_title,
                                    "lexer", f_type,
                                    NULL);

        break;

    }

    g_free(f_content);

    user_agent = g_strconcat(PLUGIN_NAME, " ", PLUGIN_VERSION, " / Geany ", GEANY_VERSION, NULL);
    session = soup_session_async_new_with_options(SOUP_SESSION_USER_AGENT, user_agent, NULL);
    g_free(user_agent);

    soup_message_set_request(msg, "application/x-www-form-urlencoded",
                             SOUP_MEMORY_TAKE, formdata, strlen(formdata));

    status = soup_session_send_message(session, msg);
    p_url = g_strdup(msg->response_body->data);

    g_object_unref(session);
    g_object_unref(msg);

    if(status == SOUP_STATUS_OK)
    {

        /*
         * codepad.org doesn't return only the url of the new snippet pasted
         * but an html page. This minimal parser will get the bare url.
         */

        if (website_selected == CODEPAD_ORG)
        {
            tokens_array = g_strsplit(p_url, "<a href=\"", 0);

            /* cuts the string when it finds the first occurrence of '/'
             * It shoud work even if codepad would change its url.
             */

            SETPTR(p_url, g_strdup(tokens_array[5]));
            occ_position = indexof(tokens_array[5], '\"');

            g_strfreev(tokens_array);

            if(occ_position != -1)
            {
                p_url[occ_position] = '\0';
            }
            else
            {
                dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Unable to paste the code on codepad.org\n"
                                    "Retry or select another pastebin."));
                g_free(p_url);
                return;
            }

        }
        else if(website_selected == TINYPASTE_COM)
        {
            /* tinypaste.com returns a XML response which looks
             * like this:
             * 
             * <?xml version="1.0" encoding="utf-8"?>
             * <result>
             *      <response>xxxxx</response>
             * </result>
             */
            tokens_array = g_strsplit_set(p_url, "<>", 0);
            
            SETPTR(p_url, g_strdup_printf("http://%s/%s", websites[TINYPASTE_COM], tokens_array[6]));
            
            g_strfreev(tokens_array);
        }
            
        else if(website_selected == DPASTE_DE)
        {
            SETPTR(p_url, g_strndup(p_url + 1, strlen(p_url) - 2));

        }
        else if(website_selected == SPRUNGE_US)
        {

            /* in order to enable the syntax highlightning on sprunge.us
             * it is necessary to append at the returned url a question
             * mark '?' followed by the file type.
             *
             * e.g. sprunge.us/xxxx?c
             */
            gchar *ft_tmp = g_ascii_strdown(f_type, -1);
            g_strstrip(p_url);
            SETPTR(p_url, g_strdup_printf("%s?%s", p_url, ft_tmp));
            g_free(ft_tmp);
        }

        if (check_button_is_checked)
        {
            utils_open_browser(p_url);
        }
        else
        {
            GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(geany->main_widgets->window),
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                _("Paste Successful"));
            gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(dlg),
                _("Your paste can be found here:\n<a href=\"%s\" "
                "title=\"Click to open the paste in your browser\">%s</a>"), p_url, p_url);
            gtk_dialog_run(GTK_DIALOG(dlg));
            gtk_widget_destroy(dlg);
        }
    }
    else
    {
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Unable to paste the code. Check your connection and retry.\n"
                            "Error code: %d\n"), status);
    }

    g_free(p_url);
}

static void item_activate(GtkMenuItem * menuitem, gpointer gdata)
{
    GeanyDocument *doc = document_get_current();

    if(!DOC_VALID(doc))
    {
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("There are no opened documents. Open one and retry.\n"));
        return;
    }

    paste(doc, websites_api[website_selected]);
}

static void on_configure_response(GtkDialog * dialog, gint response, gpointer * user_data)
{
    if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
    {
        if(g_strcmp0(gtk_entry_get_text(GTK_ENTRY(widgets.author_entry)), "") == 0)
        {
            dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("The author name field is empty!"));
        }
        else
        {
            website_selected = gtk_combo_box_get_active(GTK_COMBO_BOX(widgets.combo));
            check_button_is_checked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.check_button));
            SETPTR(author_name, g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets.author_entry))));
            save_settings();
        }
    }
}

GtkWidget *plugin_configure(GtkDialog * dialog)
{
    gint i;
    GtkWidget *label, *vbox, *author_label;

    vbox = gtk_vbox_new(FALSE, 6);

    label = gtk_label_new(_("Select a pastebin:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    author_label = gtk_label_new(_("Enter the author name:"));
    gtk_misc_set_alignment(GTK_MISC(author_label), 0, 0.5);

    widgets.author_entry = gtk_entry_new();

    if(author_name == NULL)
        author_name = g_strdup(USERNAME);

    gtk_entry_set_text(GTK_ENTRY(widgets.author_entry), author_name);

    widgets.combo = gtk_combo_box_text_new();

    for (i = 0; i < G_N_ELEMENTS(websites); i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets.combo), websites[i]);

    widgets.check_button = gtk_check_button_new_with_label(_("Show your paste in a new browser tab"));

    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), widgets.combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), author_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), widgets.author_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), widgets.check_button, FALSE, FALSE, 0);

    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets.combo), website_selected);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.check_button), check_button_is_checked);

    gtk_widget_show_all(vbox);

    g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);

    return vbox;
}

static void add_menu_item(void)
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
    load_settings();
    main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
    add_menu_item();
}


void plugin_cleanup(void)
{
    g_free(author_name);
    gtk_widget_destroy(main_menu_item);
}
