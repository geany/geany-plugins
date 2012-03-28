/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Imendio AB
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

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "ige-conf-private.h"

typedef struct {
        GString      *text;

        gchar        *current_key;
        gchar        *current_value;
        IgeConfType   current_type;

        GList        *defaults;
} DefaultData;

#define BYTES_PER_READ 4096

static void
parser_start_cb (GMarkupParseContext  *context,
                 const gchar          *node_name,
                 const gchar         **attribute_names,
                 const gchar         **attribute_values,
                 gpointer              user_data,
                 GError              **error)
{
	DefaultData *data = user_data;

        if (g_ascii_strcasecmp (node_name, "applyto") == 0) {
                data->text = g_string_new (NULL);
        }
        else if (g_ascii_strcasecmp (node_name, "type") == 0) {
                data->text = g_string_new (NULL);
        }
        else if (g_ascii_strcasecmp (node_name, "default") == 0) {
                data->text = g_string_new (NULL);
        }
}

static void
parser_end_cb (GMarkupParseContext  *context,
               const gchar          *node_name,
               gpointer              user_data,
               GError              **error)
{
	DefaultData *data = user_data;

        if (g_ascii_strcasecmp (node_name, "schema") == 0) {
                IgeConfDefaultItem *item;

                item = g_slice_new0 (IgeConfDefaultItem);
                item->key = data->current_key;
                item->type = data->current_type;

                switch (item->type) {
                case IGE_CONF_TYPE_INT:
                case IGE_CONF_TYPE_STRING:
                        item->value = g_strdup (data->current_value);
                        break;
                case IGE_CONF_TYPE_BOOLEAN:
                        if (strcmp (data->current_value, "true") == 0) {
                                item->value = g_strdup ("YES");
                        } else {
                                item->value = g_strdup ("NO");
                        }
                        break;
                }

                data->defaults = g_list_prepend (data->defaults, item);

                data->current_key = NULL;

                g_free (data->current_value);
                data->current_value = NULL;
        }
        else if (g_ascii_strcasecmp (node_name, "applyto") == 0) {
                data->current_key = g_string_free (data->text, FALSE);
                data->text = NULL;
        }
        else if (g_ascii_strcasecmp (node_name, "type") == 0) {
                gchar *str;

                str = g_string_free (data->text, FALSE);
                if (strcmp (str, "int") == 0) {
                        data->current_type = IGE_CONF_TYPE_INT;
                }
                else if (strcmp (str, "bool") == 0) {
                        data->current_type = IGE_CONF_TYPE_BOOLEAN;
                }
                else if (strcmp (str, "string") == 0) {
                        data->current_type = IGE_CONF_TYPE_STRING;
                }

                g_free (str);
                data->text = NULL;
        }
        else if (g_ascii_strcasecmp (node_name, "default") == 0) {
                data->current_value = g_string_free (data->text, FALSE);
                data->text = NULL;
        }
}

static void
parser_text_cb (GMarkupParseContext  *context,
                const gchar          *text,
                gsize                 text_len,
                gpointer              user_data,
                GError              **error)
{
        DefaultData *data = user_data;

        if (data->text) {
                g_string_append_len (data->text, text, text_len);
        }
}

static void
parser_error_cb (GMarkupParseContext *context,
		 GError              *error,
		 gpointer             user_data)
{
	g_warning ("Error: %s\n", error->message);
}

GList *
_ige_conf_defaults_read_file (const gchar  *path,
                              GError      **error)
{
        DefaultData          data;
	GMarkupParser       *parser;
	GMarkupParseContext *context;
	GIOChannel          *io = NULL;
	gchar                buf[BYTES_PER_READ];

        io = g_io_channel_new_file (path, "r", error);
        if (!io) {
                return NULL;
        }

	parser = g_new0 (GMarkupParser, 1);

	parser->start_element = parser_start_cb;
	parser->end_element = parser_end_cb;
	parser->text = parser_text_cb;
	parser->error = parser_error_cb;

        memset (&data, 0, sizeof (DefaultData));

	context = g_markup_parse_context_new (parser,
                                              0,
                                              &data,
                                              NULL);

        while (TRUE) {
                GIOStatus io_status;
                gsize     bytes_read;

                io_status = g_io_channel_read_chars (io, buf, BYTES_PER_READ,
                                                     &bytes_read, error);
                if (io_status == G_IO_STATUS_ERROR) {
                        goto exit;
                }
                if (io_status != G_IO_STATUS_NORMAL) {
                        break;
                }

                g_markup_parse_context_parse (context, buf, bytes_read, error);
                if (error != NULL && *error != NULL) {
                        goto exit;
                }

                if (bytes_read < BYTES_PER_READ) {
                        break;
                }
        }

 exit:
        g_io_channel_unref (io);
	g_markup_parse_context_free (context);
	g_free (parser);

	return data.defaults;
}

void
_ige_conf_defaults_free_list (GList *defaults)
{
        GList *l;

        for (l = defaults; l; l = l->next) {
                IgeConfDefaultItem *item = l->data;

                g_free (item->value);
                g_slice_free (IgeConfDefaultItem, item);
        }

        g_list_free (defaults);
}

gchar *
_ige_conf_defaults_get_root (GList *defaults)
{
        GList  *l;
        gchar  *root;
        gchar **strv_prev = NULL;
        gint    i;
        gint    last_common = G_MAXINT;

        for (l = defaults; l; l = l->next) {
                IgeConfDefaultItem  *item = l->data;
                gchar              **strv;

                strv = g_strsplit (item->key, "/", 0);
                if (strv_prev == NULL) {
                        strv_prev = strv;
                        continue;
                }

                i = 0;
                while (strv[i] && strv_prev[i] && i < last_common) {
                        if (strcmp (strv[i], strv_prev[i]) != 0) {
                                last_common = i;
                                break;
                        }
                        i++;
                }

                g_strfreev (strv_prev);
                strv_prev = strv;
        }

        if (strv_prev) {
                GString *str;

                str = g_string_new (NULL);
                i = 0;
                while (strv_prev[i] && i < last_common) {
                        if (strv_prev[i][0] != '\0') {
                                g_string_append_c (str, '/');
                                g_string_append (str, strv_prev[i]);
                        }
                        i++;
                }
                root = g_string_free (str, FALSE);
                g_strfreev (strv_prev);
        } else {
                root = g_strdup ("/");
        }

        return root;
}

static IgeConfDefaultItem *
defaults_get_item (GList       *defaults,
                   const gchar *key)
{
        GList  *l;

        for (l = defaults; l; l = l->next) {
                IgeConfDefaultItem *item = l->data;

                if (strcmp (item->key, key) == 0) {
                        return item;
                }
        }

        return NULL;
}

const gchar *
_ige_conf_defaults_get_string (GList       *defaults,
                               const gchar *key)
{
        IgeConfDefaultItem *item;

        item = defaults_get_item (defaults, key);

        if (item) {
                return item->value;
        }

        return NULL;
}

gint
_ige_conf_defaults_get_int (GList       *defaults,
                            const gchar *key)
{
        IgeConfDefaultItem *item;

        item = defaults_get_item (defaults, key);

        if (item) {
                return strtol (item->value, NULL, 10);
        }

        return 0;
}

gboolean
_ige_conf_defaults_get_bool (GList       *defaults,
                             const gchar *key)
{
        IgeConfDefaultItem *item;

        item = defaults_get_item (defaults, key);

        if (item) {
                if (strcmp (item->value, "false") == 0) {
                        return FALSE;
                }
                else if (strcmp (item->value, "true") == 0) {
                        return TRUE;
                }
        }

        return FALSE;
}
