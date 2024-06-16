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
#define PLUGIN_VERSION "0.3"

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

#define PASTEBIN_GROUP_DEFAULTS                     "defaults"
#define PASTEBIN_GROUP_FORMAT                       "format"
#define PASTEBIN_GROUP_LANGUAGES                    "languages"
#define PASTEBIN_GROUP_PARSE                        "parse"
#define PASTEBIN_GROUP_PARSE_KEY_SEARCH             "search"
#define PASTEBIN_GROUP_PARSE_KEY_REPLACE            "replace"
#define PASTEBIN_GROUP_PASTEBIN                     "pastebin"
#define PASTEBIN_GROUP_PASTEBIN_KEY_NAME            "name"
#define PASTEBIN_GROUP_PASTEBIN_KEY_URL             "url"
#define PASTEBIN_GROUP_PASTEBIN_KEY_METHOD          "method"
#define PASTEBIN_GROUP_PASTEBIN_KEY_CONTENT_TYPE    "content-type"

GeanyPlugin *geany_plugin;
GeanyData *geany_data;

static GtkWidget *main_menu_item = NULL;

typedef struct
{
    gchar *name;
    GKeyFile *config;
}
Pastebin;

typedef enum
{
    FORMAT_HTML_FORM_URLENCODED,
    FORMAT_JSON
}
Format;

GSList *pastebins = NULL;

static struct
{
    GtkWidget *combo;
    GtkWidget *check_button;
    GtkWidget *author_entry;
    GtkWidget *check_confirm;
} widgets;

static gchar *config_file = NULL;
static gchar *author_name = NULL;

static gchar *pastebin_selected = NULL;
static gboolean check_button_is_checked = FALSE;
static gboolean confirm_paste = TRUE;

PLUGIN_VERSION_CHECK(236)
PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE, PLUGIN_NAME,
                             _("Paste your code on your favorite pastebin"),
                             PLUGIN_VERSION, "Enrico Trotta <enrico.trt@gmail.com>")


static void show_msgbox(GtkMessageType type, GtkButtonsType buttons,
                        const gchar *main_text,
                        const gchar *secondary_markup, ...) G_GNUC_PRINTF (4, 5);

static void pastebin_free(Pastebin *pastebin)
{
    g_key_file_free(pastebin->config);
    g_free(pastebin->name);
    g_free(pastebin);
}

/* like g_key_file_has_group() but sets error if the group is missing */
static gboolean ensure_keyfile_has_group(GKeyFile *kf,
                                         const gchar *group,
                                         GError **error)
{
    if (g_key_file_has_group(kf, group))
        return TRUE;
    else
    {
        g_set_error(error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND,
                    _("Group \"%s\" not found."), group);
        return FALSE;
    }
}

/* like g_key_file_has_key() but sets error if either the group or the key is
 * missing */
static gboolean ensure_keyfile_has_key(GKeyFile *kf,
                                       const gchar *group,
                                       const gchar *key,
                                       GError **error)
{
    if (! ensure_keyfile_has_group(kf, group, error))
        return FALSE;
    else if (g_key_file_has_key(kf, group, key, NULL))
        return TRUE;
    else
    {
        g_set_error(error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND,
                    _("Group \"%s\" has no key \"%s\"."), group, key);
        return FALSE;
    }
}

static Pastebin *pastebin_new(const gchar  *path,
                              GError      **error)
{
    Pastebin *pastebin = NULL;
    GKeyFile *kf = g_key_file_new();

    if (g_key_file_load_from_file(kf, path, 0, error) &&
        ensure_keyfile_has_key(kf, PASTEBIN_GROUP_PASTEBIN,
                               PASTEBIN_GROUP_PASTEBIN_KEY_NAME, error) &&
        ensure_keyfile_has_key(kf, PASTEBIN_GROUP_PASTEBIN,
                               PASTEBIN_GROUP_PASTEBIN_KEY_URL, error) &&
        ensure_keyfile_has_group(kf, PASTEBIN_GROUP_FORMAT, error))
    {
        pastebin = g_malloc(sizeof *pastebin);

        pastebin->name = g_key_file_get_string(kf, PASTEBIN_GROUP_PASTEBIN,
                                               PASTEBIN_GROUP_PASTEBIN_KEY_NAME, NULL);
        pastebin->config = kf;
    }
    else
        g_key_file_free(kf);

    return pastebin;
}

static const Pastebin *find_pastebin_by_name(const gchar *name)
{
    GSList *node;

    g_return_val_if_fail(name != NULL, NULL);

    for (node = pastebins; node; node = node->next)
    {
        Pastebin *pastebin = node->data;

        if (strcmp(pastebin->name, name) == 0)
            return pastebin;
    }

    return NULL;
}

static gint sort_pastebins(gconstpointer a, gconstpointer b)
{
    return utils_str_casecmp(((Pastebin *) a)->name, ((Pastebin *) b)->name);
}

static void load_pastebins_in_dir(const gchar *path)
{
    GError *err = NULL;
    GDir *dir = g_dir_open(path, 0, &err);

    if (! dir && err->code != G_FILE_ERROR_NOENT)
        g_critical("Failed to read directory %s: %s", path, err->message);
    if (err)
        g_clear_error(&err);
    if (dir)
    {
        const gchar *filename;

        while ((filename = g_dir_read_name(dir)))
        {
            if (*filename == '.') /* silently skip dotfiles */
                continue;
            else if (! g_str_has_suffix(filename, ".conf"))
            {
                g_debug("Skipping %s%s%s because it has no .conf extension",
                        path, G_DIR_SEPARATOR_S, filename);
                continue;
            }
            else
            {
                gchar *fpath = g_build_filename(path, filename, NULL);
                Pastebin *pastebin = pastebin_new(fpath, &err);

                if (! pastebin)
                {
                    g_critical("Invalid pastebin configuration file %s: %s",
                               fpath, err->message);
                    g_clear_error(&err);
                }
                else
                {
                    if (! find_pastebin_by_name(pastebin->name))
                        pastebins = g_slist_prepend(pastebins, pastebin);
                    else
                    {
                        g_debug("Skipping duplicate configuration \"%s\" for "
                                "pastebin \"%s\"", fpath, pastebin->name);
                        pastebin_free(pastebin);
                    }
                }

                g_free(fpath);
            }
        }

        g_dir_close(dir);
    }
}

static gchar *get_data_dir_path(const gchar *filename)
{
    gchar *prefix = NULL;
    gchar *path;

#ifdef G_OS_WIN32
    prefix = g_win32_get_package_installation_directory_of_module(NULL);
#elif defined(__APPLE__)
    if (g_getenv("GEANY_PLUGINS_SHARE_PATH"))
        return g_build_filename(g_getenv("GEANY_PLUGINS_SHARE_PATH"), 
                                PLUGIN, filename, NULL);
#endif
    path = g_build_filename(prefix ? prefix : "", PLUGINDATADIR, filename, NULL);
    g_free(prefix);
    return path;
}

static void load_all_pastebins(void)
{
    gchar *paths[] = {
        g_build_filename(geany->app->configdir, "plugins", "geniuspaste",
                         "pastebins", NULL),
        get_data_dir_path("pastebins")
    };
    guint i;

    for (i = 0; i < G_N_ELEMENTS(paths); i++)
    {
        load_pastebins_in_dir(paths[i]);
        g_free(paths[i]);
    }
    pastebins = g_slist_sort(pastebins, sort_pastebins);
}

static void free_all_pastebins(void)
{
    g_slist_free_full(pastebins, (GDestroyNotify) pastebin_free);
    pastebins = NULL;
}

static void load_settings(void)
{
    GKeyFile *config = g_key_file_new();

    if (config_file)
        g_free(config_file);
    config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
                              "geniuspaste", G_DIR_SEPARATOR_S, "geniuspaste.conf", NULL);
    g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

    if (g_key_file_has_key(config, "geniuspaste", "pastebin", NULL) ||
        ! g_key_file_has_key(config, "geniuspaste", "website", NULL))
    {
        pastebin_selected = utils_get_setting_string(config, "geniuspaste", "pastebin", "pastebin.geany.org");
    }
    else
    {
        /* compatibility for old setting "website" */
        switch (utils_get_setting_integer(config, "geniuspaste", "website", 2))
        {
            case 0: pastebin_selected = g_strdup("codepad.org"); break;
            case 1: pastebin_selected = g_strdup("tinypaste.com"); break;
            default:
            case 2: pastebin_selected = g_strdup("pastebin.geany.org"); break;
            case 3: pastebin_selected = g_strdup("dpaste.de"); break;
            case 4: pastebin_selected = g_strdup("sprunge.us"); break;
        }
    }
    check_button_is_checked = utils_get_setting_boolean(config, "geniuspaste", "open_browser", FALSE);
    author_name = utils_get_setting_string(config, "geniuspaste", "author_name", USERNAME);
    confirm_paste = utils_get_setting_boolean(config, "geniuspaste", "confirm_paste", TRUE);

    g_key_file_free(config);
}

static void save_settings(gboolean interactive)
{
    GKeyFile *config = g_key_file_new();
    gchar *data;
    gchar *config_dir = g_path_get_dirname(config_file);
    gint error = 0;

    g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

    g_key_file_set_string(config, "geniuspaste", "pastebin", pastebin_selected);
    g_key_file_set_boolean(config, "geniuspaste", "open_browser", check_button_is_checked);
    g_key_file_set_string(config, "geniuspaste", "author_name", author_name);
    g_key_file_set_boolean(config, "geniuspaste", "confirm_paste", confirm_paste);

    if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) &&
        (error = utils_mkdir(config_dir, TRUE)) != 0)
    {
        gchar *message;

        message = g_strdup_printf(_("Plugin configuration directory could not be created: %s"),
                                  g_strerror(error));

        if (interactive)
            dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", message);
        else
            msgwin_status_add_string(message);
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

static gchar *get_paste_text(GeanyDocument *doc)
{
    gchar *paste_text;

    if (sci_has_selection(doc->editor->sci))
    {
        paste_text = sci_get_selection_contents(doc->editor->sci);
    }
    else
    {
        gint len = sci_get_length(doc->editor->sci) + 1;
        paste_text = sci_get_contents(doc->editor->sci, len);
    }

    return paste_text;
}

static gchar *pastebin_get_default(const Pastebin *pastebin,
                                   const gchar *key,
                                   const gchar *def)
{
    return utils_get_setting_string(pastebin->config, PASTEBIN_GROUP_DEFAULTS,
                                    key, def);
}

static gchar *pastebin_get_language(const Pastebin *pastebin,
                                    const gchar *geany_ft_name)
{
    gchar *lang = g_key_file_get_string(pastebin->config, PASTEBIN_GROUP_LANGUAGES,
                                        geany_ft_name, NULL);

    /* cppcheck-suppress memleak symbolName=lang
     * obvious cppcheck bug */
    return lang ? lang : pastebin_get_default(pastebin, "language", "");
}

/* append replacement for placeholder @placeholder to @str
 * returns %TRUE on success, %FALSE if the placeholder didn't exist
 *
 * don't prepare replacements because they are unlikely to happen more than
 * once for each paste */
static gboolean append_placeholder(GString         *str,
                                   const gchar     *placeholder,
                                   /* some replacement sources */
                                   const Pastebin  *pastebin,
                                   GeanyDocument   *doc,
                                   const gchar     *contents)
{
    /* special builtin placeholders */
    if (strcmp("contents", placeholder) == 0)
        g_string_append(str, contents);
    else if (strcmp("language", placeholder) == 0)
    {
        gchar *language = pastebin_get_language(pastebin, doc->file_type->name);

        g_string_append(str, language);
        g_free(language);
    }
    else if (strcmp("title", placeholder) == 0)
    {
        gchar *title = g_path_get_basename(DOC_FILENAME(doc));

        g_string_append(str, title);
        g_free(title);
    }
    else if (strcmp("user", placeholder) == 0)
        g_string_append(str, author_name);
    /* non-builtins (ones from [defaults] alone) */
    else
    {
        gchar *val = pastebin_get_default(pastebin, placeholder, NULL);

        if (val)
        {
            g_string_append(str, val);
            g_free(val);
        }
        else
        {
            g_warning("non-existing placeholder \"%%%s%%\"", placeholder);
            return FALSE;
        }
    }

    return TRUE;
}

/* expands "%name%" placeholders in @string
 *
 * placeholders are of the form:
 *      "%" [a-zA-Z0-9_]+ "%"
 * Only valid and supported placeholders are replaced; everything else is left
 * as-is. */
static gchar *expand_placeholders(const gchar    *string,
                                  const Pastebin *pastebin,
                                  GeanyDocument  *doc,
                                  const gchar    *contents)
{
    GString *str = g_string_new(NULL);
    const gchar *p;

    for (; (p = strchr(string, '%')); string = p + 1)
    {
        const gchar *k = p + 1;
        gchar *key = NULL;
        gsize key_len = 0;

        g_string_append_len(str, string, p - string);

        while (g_ascii_isalnum(k[key_len]) || k[key_len] == '_')
            key_len++;

        if (key_len > 0 && k[key_len] == '%')
            key = g_strndup(k, key_len);

        if (! key)
            g_string_append_len(str, p, k + key_len - p);
        else if (! append_placeholder(str, key, pastebin, doc, contents))
            g_string_append_len(str, p, k + key_len + 1 - p);

        if (key) /* skip % suffix too */
            key_len++;

        g_free(key);

        p = k + key_len - 1;
    }
    g_string_append(str, string);

    return g_string_free(str, FALSE);
}

/* Matches @pattern on @haystack and perform match substitutions in @replace */
static gchar *regex_replace(const gchar  *pattern,
                            const gchar  *haystack,
                            const gchar  *replace,
                            GError      **error)
{
    GRegex *re = g_regex_new(pattern, (G_REGEX_DOTALL |
                                       G_REGEX_DOLLAR_ENDONLY |
                                       G_REGEX_RAW), 0, error);
    GMatchInfo *info = NULL;
    gchar *result = NULL;

    if (re && g_regex_match(re, haystack, 0, &info))
    {
        GString *str = g_string_new(NULL);
        const gchar *p;

        for (; (p = strchr(replace, '\\')); replace = p + 1)
        {
            gint digit = ((gint) p[1]) - '0';

            g_string_append_len(str, replace, p - replace);
            if (digit >= 0 && digit <= 9 &&
                digit < g_match_info_get_match_count(info))
            {
                gchar *match = g_match_info_fetch(info, digit);

                g_string_append(str, match);
                g_free(match);
                p++;
            }
            else
                g_string_append_c(str, *p);
        }
        g_string_append(str, replace);

        result = g_string_free(str, FALSE);
    }

    if (info)
        g_match_info_free(info);

    return result;
}

static Format pastebin_get_format(const Pastebin *pastebin)
{
    static const struct
    {
        const gchar *name;
        Format       format;
    } formats[] = {
        { "application/x-www-form-urlencoded",  FORMAT_HTML_FORM_URLENCODED },
        { "application/json",                   FORMAT_JSON }
    };
    Format result = FORMAT_HTML_FORM_URLENCODED;
    gchar *format = utils_get_setting_string(pastebin->config, PASTEBIN_GROUP_PASTEBIN,
                                             PASTEBIN_GROUP_PASTEBIN_KEY_CONTENT_TYPE, NULL);

    if (format)
    {
        for (guint i = 0; i < G_N_ELEMENTS(formats); i++)
        {
            if (strcmp(formats[i].name, format) == 0)
            {
                result = formats[i].format;
                break;
            }
        }

        g_free(format);
    }

    return result;
}

/* Appends a JSON string.  See:
 * http://www.ecma-international.org/publications/files/ECMA-ST/ECMA-404.pdf */
static void append_json_string(GString *str, const gchar *value)
{
    g_string_append_c(str, '"');
    for (; *value; value++)
    {
        if (*value == '"' || *value == '\\')
        {
            g_string_append_c(str, '\\');
            g_string_append_c(str, *value);
        }
        else if (*value == '\b')
            g_string_append(str, "\\b");
        else if (*value == '\f')
            g_string_append(str, "\\f");
        else if (*value == '\n')
            g_string_append(str, "\\n");
        else if (*value == '\r')
            g_string_append(str, "\\r");
        else if (*value == '\t')
            g_string_append(str, "\\t");
        else if (*value >= 0x00 && *value <= 0x1F)
            g_string_append_printf(str, "\\u%04d", *value);
        else
            g_string_append_c(str, *value);
    }
    g_string_append_c(str, '"');
}

static void append_json_data_item(GQuark id, gpointer data, gpointer user_data)
{
    GString *str = user_data;

    if (str->len > 1) /* if there's more than the first "{" */
        g_string_append_c(str, ',');
    append_json_string(str, g_quark_to_string(id));
    g_string_append_c(str, ':');
    append_json_string(str, data);
}

static SoupMessage *json_request_new(const gchar *method,
                                     const gchar *url,
                                     GData **fields)
{
    SoupMessage  *msg = soup_message_new(method, url);
    GString      *str = g_string_new(NULL);
    GBytes       *bytes;

    g_string_append_c(str, '{');
    g_datalist_foreach(fields, append_json_data_item, str);
    g_string_append_c(str, '}');
    bytes = g_bytes_new_take(str->str, str->len);
    (void) g_string_free(str, FALSE); /* buffer already taken above */
    soup_message_set_request_body_from_bytes(msg, "application/json", bytes);
    g_bytes_unref(bytes);

    return msg;
}

/* sends data to @pastebin and returns the raw response */
static SoupMessage *pastebin_soup_message_new(const Pastebin  *pastebin,
                                              GeanyDocument   *doc,
                                              const gchar     *contents)
{
    SoupMessage *msg;
    gchar *url;
    gchar *method;
    Format format;
    gsize n_fields;
    gchar **fields;
    GData *data;

    g_return_val_if_fail(pastebin != NULL, NULL);
    g_return_val_if_fail(contents != NULL, NULL);

    url = utils_get_setting_string(pastebin->config, PASTEBIN_GROUP_PASTEBIN,
                                   PASTEBIN_GROUP_PASTEBIN_KEY_URL, NULL);
    method = utils_get_setting_string(pastebin->config, PASTEBIN_GROUP_PASTEBIN,
                                      PASTEBIN_GROUP_PASTEBIN_KEY_METHOD, "POST");
    format = pastebin_get_format(pastebin);
    /* prepare the form data */
    fields = g_key_file_get_keys(pastebin->config, PASTEBIN_GROUP_FORMAT, &n_fields, NULL);
    g_datalist_init(&data);
    for (gsize i = 0; fields && i < n_fields; i++)
    {
        gchar *value = g_key_file_get_string(pastebin->config, PASTEBIN_GROUP_FORMAT,
                                             fields[i], NULL);

        SETPTR(value, expand_placeholders(value, pastebin, doc, contents));
        g_datalist_set_data_full(&data, fields[i], value, g_free);
    }
    g_strfreev(fields);
    switch (format)
    {
        case FORMAT_JSON:
            msg = json_request_new(method, url, &data);
            break;

        default:
        case FORMAT_HTML_FORM_URLENCODED:
            msg = soup_message_new_from_encoded_form(method, url,
                                                     soup_form_encode_datalist(&data));
            break;
    }
    g_datalist_clear(&data);

    return msg;
}

static gchar *bytes_to_string(GBytes *bytes)
{
    gsize bytes_size = g_bytes_get_size(bytes);
    gchar *str = g_malloc(bytes_size + 1);
    memcpy(str, g_bytes_get_data(bytes, NULL), bytes_size);
    str[bytes_size] = 0;
    return str;
}

/* parses @response and returns the URL of the paste, or %NULL on parse error
 * or if the URL couldn't be found.
 * @warning: it may return NULL even if @error is not set */
static gchar *pastebin_parse_response(const Pastebin  *pastebin,
                                      SoupMessage     *msg,
                                      GBytes          *response_body,
                                      GeanyDocument   *doc,
                                      const gchar     *contents,
                                      GError         **error)
{
    gchar *url = NULL;
    gchar *search;
    gchar *replace;

    g_return_val_if_fail(pastebin != NULL, NULL);
    g_return_val_if_fail(msg != NULL, NULL);

    if (! g_key_file_has_group(pastebin->config, PASTEBIN_GROUP_PARSE))
    {
        /* by default, use the response URI (redirect) */
        url = g_uri_to_string_partial(soup_message_get_uri(msg),
                                      G_URI_HIDE_PASSWORD);
    }
    else
    {
        search = utils_get_setting_string(pastebin->config, PASTEBIN_GROUP_PARSE,
                                          PASTEBIN_GROUP_PARSE_KEY_SEARCH,
                                          "^[[:space:]]*(.+?)[[:space:]]*$");
        replace = utils_get_setting_string(pastebin->config, PASTEBIN_GROUP_PARSE,
                                           PASTEBIN_GROUP_PARSE_KEY_REPLACE, "\\1");
        SETPTR(replace, expand_placeholders(replace, pastebin, doc, contents));

        gchar *response_str = bytes_to_string(response_body);
        url = regex_replace(search, response_str, replace, error);
        g_free(response_str);

        g_free(search);
        g_free(replace);
    }

    return url;
}

static gboolean message_dialog_label_link_activated(GtkLabel *label, gchar *uri, gpointer user_data)
{
    utils_open_browser(uri);
    return TRUE;
}

static void message_dialog_label_set_url_hook(GtkWidget *widget, gpointer data)
{
    if (GTK_IS_LABEL(widget))
    {
        g_signal_connect(widget,
                         "activate-link",
                         G_CALLBACK(message_dialog_label_link_activated),
                         NULL);
    }
}

static void show_msgbox(GtkMessageType type, GtkButtonsType buttons,
                        const gchar *main_text,
                        const gchar *secondary_markup, ...)
{
    GtkWidget *dlg;
    GtkWidget *dlg_vbox;
    va_list ap;
    gchar *markup;

    va_start(ap, secondary_markup);
    markup = g_markup_vprintf_escaped(secondary_markup, ap);
    va_end(ap);

    dlg = g_object_new(GTK_TYPE_MESSAGE_DIALOG,
                       "message-type", type,
                       "buttons", buttons,
                       "transient-for", geany->main_widgets->window,
                       "modal", TRUE,
                       "destroy-with-parent", TRUE,
                       "text", main_text,
                       "secondary-text", markup,
                       "secondary-use-markup", TRUE,
                       NULL);
    /* fetch the message area of the dialog and attach a custom URL hook to the labels */
    dlg_vbox = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(dlg));
    gtk_container_foreach(GTK_CONTAINER(dlg_vbox),
                          message_dialog_label_set_url_hook,
                          NULL);
    /* run the dialog */
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    g_free(markup);
    /* cppcheck-suppress memleak symbolName=dlg */
}

static void debug_logger(SoupLogger *logger,
                         SoupLoggerLogLevel level,
                         char direction,
                         const char *data,
                         gpointer user_data)
{
    const char *prefix;
    switch (direction)
    {
        case '>': prefix = "Request: "; break;
        case '<': prefix = "Response: "; break;
        default: prefix = ""; break;
    }

    msgwin_msg_add(COLOR_BLUE, -1, NULL, "[geniuspaste] %s%s", prefix, data);
}

static gboolean ask_paste_confirmation(GeanyDocument *doc, const gchar *website)
{
    GtkWidget *dialog;
    GtkWidget *check_confirm;
    gboolean ret = FALSE;

    dialog = gtk_message_dialog_new(GTK_WINDOW(geany->main_widgets->window),
                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                    _("Are you sure you want to paste %s to %s?"),
                                    sci_has_selection(doc->editor->sci)
                                    ? _("the selection")
                                    : _("the whole document"),
                                    website);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                             _("The paste might be public, "
                                               "and it might not be possible "
                                               "to delete it."));
    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
                           _("_Cancel"), GTK_RESPONSE_CANCEL,
                           _("_Paste it"), GTK_RESPONSE_ACCEPT,
                           NULL);
    check_confirm = gtk_check_button_new_with_mnemonic(_("Do not _ask again"));
    gtk_widget_set_tooltip_text(check_confirm, _("Whether to ask this question "
                                                 "again for future pastes. "
                                                 "This can be changed at any "
                                                 "time in the plugin preferences."));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_confirm), ! confirm_paste);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       check_confirm, FALSE, TRUE, 0);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_style_context_add_class(gtk_widget_get_style_context(gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog),
                                                                                                GTK_RESPONSE_ACCEPT)),
                                GTK_STYLE_CLASS_SUGGESTED_ACTION);

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        confirm_paste = ! gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_confirm));
        save_settings(FALSE);
        ret = TRUE;
    }

    gtk_widget_destroy(dialog);

    return ret;
}

static void paste(GeanyDocument * doc, const gchar * website)
{
    const Pastebin *pastebin;
    gchar *f_content;
    SoupSession *session;
    SoupMessage *msg;
    gchar *user_agent = NULL;
    guint status;
    GError *err = NULL;
    GBytes *response;

    g_return_if_fail(doc && doc->is_valid);

    /* find the pastebin */
    pastebin = find_pastebin_by_name(website);
    if (! pastebin)
    {
        show_msgbox(GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                    _("Invalid pastebin service."),
                    _("Unknown pastebin service \"%s\". "
                      "Select an existing pastebin service in the preferences "
                      "or create an appropriate pastebin configuration and "
                      "retry."),
                    website);
        return;
    }

    /* get the contents */
    f_content = get_paste_text(doc);
    if (f_content == NULL || f_content[0] == '\0')
    {
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Refusing to create blank paste"));
        return;
    }

    if (confirm_paste && ! ask_paste_confirmation(doc, website))
        return;

    msg = pastebin_soup_message_new(pastebin, doc, f_content);

    user_agent = g_strconcat(PLUGIN_NAME, " ", PLUGIN_VERSION, " / Geany ", GEANY_VERSION, NULL);
    session = soup_session_new_with_options("user-agent", user_agent, NULL);
    if (geany->app->debug_mode)
    {
        SoupLogger *logger = soup_logger_new(SOUP_LOGGER_LOG_BODY);
        soup_logger_set_printer(logger, debug_logger, NULL, NULL);
        soup_session_add_feature(session, SOUP_SESSION_FEATURE(logger));
        g_object_unref(logger);
    }
    g_free(user_agent);

    response = soup_session_send_and_read(session, msg, NULL, &err);
    g_object_unref(session);

    status = soup_message_get_status(msg);
    if (err || ! SOUP_STATUS_IS_SUCCESSFUL(status))
    {
        show_msgbox(GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                    _("Failed to paste the code"),
                    _("Error pasting the code to the pastebin service %s.\n"
                      "Error code: %u (%s).\n\n%s"),
                    pastebin->name, status,
                    err ? err->message : soup_message_get_reason_phrase(msg),
                    (err
                     ? _("Check your connection or the pastebin configuration and retry.")
                     : SOUP_STATUS_IS_SERVER_ERROR(status)
                     ? _("Wait for the service to come back and retry, or retry "
                         "with another pastebin service.")
                     : _("Check the pastebin configuration and retry.")));
        g_clear_error(&err);
    }
    else
    {
        gchar *p_url = pastebin_parse_response(pastebin, msg, response, doc,
                                               f_content, &err);

        if (err || ! p_url)
        {
            show_msgbox(GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                        _("Failed to obtain paste URL."),
                        _("The code was successfully pasted on %s, but an "
                          "error occurred trying to obtain its URL: %s\n\n%s"),
                        pastebin->name,
                        (err ? err->message
                         : _("Unexpected response from the pastebin service.")),
                        _("Check the pastebin configuration and retry."));

            if (err)
                g_error_free(err);
        }
        else if (check_button_is_checked)
        {
            utils_open_browser(p_url);
        }
        else
        {
            show_msgbox(GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                        _("Paste Successful"),
                        _("Your paste can be found here:\n<a href=\"%s\" "
                          "title=\"Click to open the paste in your browser\">%s</a>"),
                        p_url, p_url);
        }

        g_free(p_url);
    }

    if (response)
        g_bytes_unref(response);
    g_object_unref(msg);
    g_free(f_content);
}

static void item_activate(GtkMenuItem * menuitem, gpointer gdata)
{
    GeanyDocument *doc = document_get_current();

    if(!DOC_VALID(doc))
    {
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("There are no opened documents. Open one and retry.\n"));
        return;
    }

    paste(doc, pastebin_selected);
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
            SETPTR(pastebin_selected, gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widgets.combo)));
            check_button_is_checked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.check_button));
            SETPTR(author_name, g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets.author_entry))));
            confirm_paste = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.check_confirm));
            save_settings(TRUE);
        }
    }
}

GtkWidget *plugin_configure(GtkDialog * dialog)
{
    GSList *node;
    gint i;
    GtkWidget *label, *vbox, *author_label;

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    label = gtk_label_new(_("Select a pastebin:"));
    gtk_label_set_xalign(GTK_LABEL(label), 0);

    author_label = gtk_label_new(_("Enter the author name:"));
    gtk_label_set_xalign(GTK_LABEL(author_label), 0);

    widgets.author_entry = gtk_entry_new();

    if(author_name == NULL)
        author_name = g_strdup(USERNAME);

    gtk_entry_set_text(GTK_ENTRY(widgets.author_entry), author_name);

    widgets.combo = gtk_combo_box_text_new();

    for (i = 0, node = pastebins; node; node = node->next, i++)
    {
        Pastebin *pastebin = node->data;

        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets.combo), pastebin->name);
        if (pastebin_selected && strcmp(pastebin_selected, pastebin->name) == 0)
            gtk_combo_box_set_active(GTK_COMBO_BOX(widgets.combo), i);
    }

    widgets.check_button = gtk_check_button_new_with_label(_("Show your paste in a new browser tab"));
    widgets.check_confirm = gtk_check_button_new_with_label(_("Confirm before pasting"));

    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), widgets.combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), author_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), widgets.author_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), widgets.check_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), widgets.check_confirm, FALSE, FALSE, 0);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.check_button), check_button_is_checked);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.check_confirm), confirm_paste);

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
    load_all_pastebins();
    load_settings();
    add_menu_item();
}


void plugin_cleanup(void)
{
    g_free(author_name);
    gtk_widget_destroy(main_menu_item);
    free_all_pastebins();
}
