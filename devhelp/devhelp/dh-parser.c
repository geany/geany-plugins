/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (c) 2002-2003 Mikael Hallendal <micke@imendio.com>
 * Copyright (c) 2002-2003 CodeFactory AB
 * Copyright (C) 2005,2008 Imendio AB
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
#include <errno.h>
#include <zlib.h>
#include <glib/gi18n-lib.h>

#include "dh-error.h"
#include "dh-link.h"
#include "dh-parser.h"

#define NAMESPACE      "http://www.devhelp.net/book"
#define BYTES_PER_READ 4096

typedef struct {
	GMarkupParser       *m_parser;
	GMarkupParseContext *context;

	const gchar         *path;

	/* Top node of book */
	GNode               *book_node;

	/* Current sub section node */
	GNode               *parent;

	gboolean             parsing_chapters;
	gboolean             parsing_keywords;

 	GNode              **book_tree;
	GList              **keywords;

	/* Version 2 uses <keyword> instead of <function>. */
	gint                 version;
} DhParser;

static void
dh_parser_free (DhParser *parser)
{
        // NOTE: priv->book_tree and priv->keywords do not need to be freed
        // because they're only used to store the locations for the return
        // params of dh_parser_read_file()

        g_markup_parse_context_free (parser->context);
        g_free (parser->m_parser);
        g_free (parser);
}

static void
parser_start_node_book (DhParser             *parser,
                        GMarkupParseContext  *context,
                        const gchar          *node_name,
                        const gchar         **attribute_names,
                        const gchar         **attribute_values,
                        GError              **error)
{
        gint         i, j;
        gint         line, col;
        gchar       *title = NULL;
        gchar *base = NULL;
        const gchar *name = NULL;
        const gchar *uri = NULL;
	DhLink      *link;

        if (g_ascii_strcasecmp (node_name, "book") != 0) {
                g_markup_parse_context_get_position (context, &line, &col);
                g_set_error (error,
                             DH_ERROR,
                             DH_ERROR_MALFORMED_BOOK,
                             _("Expected '%s', got '%s' at line %d, column %d"),
                             "book", node_name, line, col);
                return;
        }

        for (i = 0; attribute_names[i]; ++i) {
                const gchar *xmlns;

                if (g_ascii_strcasecmp (attribute_names[i], "xmlns") == 0) {
                        xmlns = attribute_values[i];
                        if (g_ascii_strcasecmp (xmlns, NAMESPACE) != 0) {
                                g_markup_parse_context_get_position (context,
                                                                     &line,
                                                                     &col);
                                g_set_error (error,
                                             DH_ERROR,
                                             DH_ERROR_MALFORMED_BOOK,
                                             _("Invalid namespace '%s' at"
                                               " line %d, column %d"),
                                             xmlns, line, col);
                                return;
                        }
                }
                else if (g_ascii_strcasecmp (attribute_names[i], "name") == 0) {
                        name = attribute_values[i];
                }
                else if (g_ascii_strcasecmp (attribute_names[i], "title") == 0) {
                        title = g_strdup(attribute_values[i]);
                        for (j = 0; title[j]; j++) {
                                if (title[j] == '\n') title[j] = ' ';
                        }
                }
                else if (g_ascii_strcasecmp (attribute_names[i], "base") == 0) {
                        base = g_strdup (attribute_values[i]);
			}
                else if (g_ascii_strcasecmp (attribute_names[i], "link") == 0) {
                        uri = attribute_values[i];
                }
        }

        if (!title || !name || !uri) {
                g_markup_parse_context_get_position (context, &line, &col);
                g_set_error (error,
                             DH_ERROR,
                             DH_ERROR_MALFORMED_BOOK,
                             _("\"title\", \"name\" and \"link\" elements are "
                               "required at line %d, column %d"),
                             line, col);
                g_free (title);
                return;
        }

        if (!base) {
                base = g_path_get_dirname (parser->path);
        }

        link = dh_link_new (DH_LINK_TYPE_BOOK,
                            base,
                            name,
                            title,
                            NULL,
                            NULL,
                            uri);
        g_free (base);

        *parser->keywords = g_list_prepend (*parser->keywords, dh_link_ref (link));

        parser->book_node = g_node_new (dh_link_ref (link));
        *parser->book_tree = parser->book_node;
        parser->parent = parser->book_node;
        g_free (title);
        dh_link_unref (link);
}

static void
parser_start_node_chapter (DhParser             *parser,
                           GMarkupParseContext  *context,
                           const gchar          *node_name,
                           const gchar         **attribute_names,
                           const gchar         **attribute_values,
                           GError              **error)
{
        gint         i;
        gint         line, col;
        const gchar *name = NULL;
        const gchar *uri = NULL;
	DhLink      *link;
        GNode       *node;

        if (g_ascii_strcasecmp (node_name, "sub") != 0) {
                g_markup_parse_context_get_position (context, &line, &col);
                g_set_error (error,
                             DH_ERROR,
                             DH_ERROR_MALFORMED_BOOK,
                             _("Expected '%s', got '%s' at line %d, column %d"),
                             "sub", node_name, line, col);
                return;
        }

        for (i = 0; attribute_names[i]; ++i) {
                if (g_ascii_strcasecmp (attribute_names[i], "name") == 0) {
                        name = attribute_values[i];
                }
                else if (g_ascii_strcasecmp (attribute_names[i], "link") == 0) {
                        uri = attribute_values[i];
                }
        }

        if (!name || !uri) {
                g_markup_parse_context_get_position (context, &line, &col);
                g_set_error (error,
                             DH_ERROR,
                             DH_ERROR_MALFORMED_BOOK,
                             _("\"name\" and \"link\" elements are required "
                               "inside <sub> on line %d, column %d"),
                             line, col);
                return;
        }

        link = dh_link_new (DH_LINK_TYPE_PAGE,
                            NULL,
                            NULL,
                            name,
                            parser->book_node->data,
                            NULL,
                            uri);

        *parser->keywords = g_list_prepend (*parser->keywords, link);

        node = g_node_new (link);
        g_node_prepend (parser->parent, node);
        parser->parent = node;
}

static void
parser_start_node_keyword (DhParser             *parser,
                           GMarkupParseContext  *context,
                           const gchar          *node_name,
                           const gchar         **attribute_names,
                           const gchar         **attribute_values,
                           GError              **error)
{
        gint         i;
        gint         line, col;
        const gchar *name = NULL;
        const gchar *uri = NULL;
        const gchar *type = NULL;
        const gchar *deprecated = NULL;
        DhLinkType   link_type;
	DhLink      *link;
        gchar       *tmp;

        if (parser->version == 2 &&
            g_ascii_strcasecmp (node_name, "keyword") != 0) {
                g_markup_parse_context_get_position (context, &line, &col);
                g_set_error (error,
                             DH_ERROR,
                             DH_ERROR_MALFORMED_BOOK,
                             _("Expected '%s', got '%s' at line %d, column %d"),
                             "keyword", node_name, line, col);
                return;
        }
        else if (parser->version == 1 &&
            g_ascii_strcasecmp (node_name, "function") != 0) {
                g_markup_parse_context_get_position (context, &line, &col);
                g_set_error (error,
                             DH_ERROR,
                             DH_ERROR_MALFORMED_BOOK,
                             _("Expected '%s', got '%s' at line %d, column %d"),
                             "function", node_name, line, col);
                return;
        }

        for (i = 0; attribute_names[i]; ++i) {
                if (g_ascii_strcasecmp (attribute_names[i], "type") == 0) {
                        type = attribute_values[i];
                }
                else if (g_ascii_strcasecmp (attribute_names[i], "name") == 0) {
                        name = attribute_values[i];
                }
                else if (g_ascii_strcasecmp (attribute_names[i], "link") == 0) {
                        uri = attribute_values[i];
                }
                else if (g_ascii_strcasecmp (attribute_names[i], "deprecated") == 0) {
                        deprecated = attribute_values[i];
                }
        }

        if (!name || !uri) {
                g_markup_parse_context_get_position (context, &line, &col);
                g_set_error (error,
                             DH_ERROR,
                             DH_ERROR_MALFORMED_BOOK,
                             _("\"name\" and \"link\" elements are required "
                               "inside '%s' on line %d, column %d"),
                             parser->version == 2 ? "keyword" : "function",
                             line, col);
                return;
        }

        if (parser->version == 2 && !type) {
                /* Required */
                g_markup_parse_context_get_position (context, &line, &col);
                g_set_error (error,
                             DH_ERROR,
                             DH_ERROR_MALFORMED_BOOK,
                             _("\"type\" element is required "
                               "inside <keyword> on line %d, column %d"),
                             line, col);
                return;
        }

        if (parser->version == 2) {
                if (strcmp (type, "function") == 0) {
                        link_type = DH_LINK_TYPE_FUNCTION;
                }
                else if (strcmp (type, "struct") == 0) {
                        link_type = DH_LINK_TYPE_STRUCT;
                }
                else if (strcmp (type, "macro") == 0) {
                        link_type = DH_LINK_TYPE_MACRO;
                }
                else if (strcmp (type, "enum") == 0) {
                        link_type = DH_LINK_TYPE_ENUM;
                }
                else if (strcmp (type, "typedef") == 0) {
                        link_type = DH_LINK_TYPE_TYPEDEF;
                } else {
                        link_type = DH_LINK_TYPE_KEYWORD;
                }
        } else {
                link_type = DH_LINK_TYPE_KEYWORD;
        }

        /* Strip out trailing " () or "()". */
        if (g_str_has_suffix (name, " ()")) {
                tmp = g_strndup (name, strlen (name) - 3);

                if (link_type == DH_LINK_TYPE_KEYWORD) {
                        link_type = DH_LINK_TYPE_FUNCTION;
                }
                name = tmp;
        }
        else if (g_str_has_suffix (name, "()")) {
                tmp = g_strndup (name, strlen (name) - 2);

                /* With old devhelp format, take a guess that this is a
                 * macro.
                 */
                if (link_type == DH_LINK_TYPE_KEYWORD) {
                        link_type = DH_LINK_TYPE_MACRO;
                }
                name = tmp;
        } else {
                tmp = NULL;
        }

        /* Strip out prefixing "struct", "union", "enum", to make searching
         * easier. Also fix up the link type (only applies for old devhelp
         * format).
         */
        if (g_str_has_prefix (name, "struct ")) {
                name = name + 7;
                if (link_type == DH_LINK_TYPE_KEYWORD) {
                        link_type = DH_LINK_TYPE_STRUCT;
                }
        }
        else if (g_str_has_prefix (name, "union ")) {
                name = name + 6;
                if (link_type == DH_LINK_TYPE_KEYWORD) {
                        link_type = DH_LINK_TYPE_STRUCT;
                }
        }
        else if (g_str_has_prefix (name, "enum ")) {
                name = name + 5;
                if (link_type == DH_LINK_TYPE_KEYWORD) {
                        link_type = DH_LINK_TYPE_ENUM;
                }
        }

        link = dh_link_new (link_type,
                            NULL,
                            NULL,
                            name,
                            parser->book_node->data,
                            parser->parent->data,
                            uri);

        g_free (tmp);

        if (deprecated) {
                dh_link_set_flags (
                        link,
                        dh_link_get_flags (link) | DH_LINK_FLAGS_DEPRECATED);
        }

        *parser->keywords = g_list_prepend (*parser->keywords, link);
}

static void
parser_start_node_cb (GMarkupParseContext  *context,
		      const gchar          *node_name,
		      const gchar         **attribute_names,
		      const gchar         **attribute_values,
		      gpointer              user_data,
		      GError              **error)
{
	DhParser *parser = user_data;

        if (parser->parsing_keywords) {
                parser_start_node_keyword (parser,
                                           context,
                                           node_name,
                                           attribute_names,
                                           attribute_values,
                                           error);
                return;
        }
        else if (parser->parsing_chapters) {
                parser_start_node_chapter (parser,
                                           context,
                                           node_name,
                                           attribute_names,
                                           attribute_values,
                                           error);
                return;
        }
	else if (g_ascii_strcasecmp (node_name, "functions") == 0) {
		parser->parsing_keywords = TRUE;
	}
	else if (g_ascii_strcasecmp (node_name, "chapters") == 0) {
		parser->parsing_chapters = TRUE;
	}
	if (!parser->book_node) {
                parser_start_node_book (parser,
                                        context,
                                        node_name,
                                        attribute_names,
                                        attribute_values,
                                        error);
		return;
	}
}

static void
parser_end_node_cb (GMarkupParseContext  *context,
		    const gchar          *node_name,
		    gpointer              user_data,
		    GError              **error)
{
	DhParser *parser = user_data;

        if (parser->parsing_keywords) {
                if (g_ascii_strcasecmp (node_name, "functions") == 0) {
			parser->parsing_keywords = FALSE;
		}
	}
	else if (parser->parsing_chapters) {
		g_node_reverse_children (parser->parent);
		if (g_ascii_strcasecmp (node_name, "sub") == 0) {
			parser->parent = parser->parent->parent;
			/* Move up in the tree */
		}
		else if (g_ascii_strcasecmp (node_name, "chapters") == 0) {
			parser->parsing_chapters = FALSE;
		}
	}
}

static void
parser_error_cb (GMarkupParseContext *context,
		 GError              *error,
		 gpointer             user_data)
{
	DhParser *parser = user_data;

	g_markup_parse_context_free (parser->context);
 	parser->context = NULL;
}

static gboolean
parser_read_gz_file (DhParser     *parser,
                     const gchar  *path,
		     GError      **error)
{
	gchar  buf[BYTES_PER_READ];
	gzFile file;

	file = gzopen (path, "r");
	if (!file) {
		g_set_error (error,
			     DH_ERROR,
			     DH_ERROR_FILE_NOT_FOUND,
			     "%s", g_strerror (errno));
		return FALSE;
	}

	while (TRUE) {
		gssize bytes_read;

		bytes_read = gzread (file, buf, BYTES_PER_READ);
		if (bytes_read == -1) {
			gint         err;
			const gchar *message;

			message = gzerror (file, &err);
			g_set_error (error,
				     DH_ERROR,
				     DH_ERROR_INTERNAL_ERROR,
				     _("Cannot uncompress book '%s': %s"),
				     path, message);
			return FALSE;
		}

		g_markup_parse_context_parse (parser->context, buf,
					      bytes_read, error);
		if (error != NULL && *error != NULL) {
			return FALSE;
		}
		if (bytes_read < BYTES_PER_READ) {
			break;
		}
	}

	gzclose (file);

	return TRUE;
}

gboolean
dh_parser_read_file (const gchar  *path,
		     GNode       **book_tree,
		     GList       **keywords,
		     GError      **error)
{
	DhParser   *parser;
        gboolean    gz;
	GIOChannel *io = NULL;
	gchar       buf[BYTES_PER_READ];
	gboolean    result = TRUE;

	parser = g_new0 (DhParser, 1);

	if (g_str_has_suffix (path, ".devhelp2")) {
		parser->version = 2;
                gz = FALSE;
        }
        else if (g_str_has_suffix (path, ".devhelp")) {
		parser->version = 1;
                gz = FALSE;
        }
        else if (g_str_has_suffix (path, ".devhelp2.gz")) {
		parser->version = 2;
                gz = TRUE;
        } else {
		parser->version = 1;
                gz = TRUE;
        }

	parser->m_parser = g_new0 (GMarkupParser, 1);

	parser->m_parser->start_element = parser_start_node_cb;
	parser->m_parser->end_element = parser_end_node_cb;
	parser->m_parser->error = parser_error_cb;

	parser->context = g_markup_parse_context_new (parser->m_parser, 0,
						      parser, NULL);

	parser->path = path;
	parser->book_tree = book_tree;
	parser->keywords = keywords;

        if (gz) {
                if (!parser_read_gz_file (parser,
                                          path,
                                          error)) {
                        result = FALSE;
                }
                goto exit;
	} else {
                io = g_io_channel_new_file (path, "r", error);
                if (!io) {
                        result = FALSE;
                        goto exit;
                }

                while (TRUE) {
                        GIOStatus io_status;
                        gsize     bytes_read;

                        io_status = g_io_channel_read_chars (io, buf, BYTES_PER_READ,
                                                             &bytes_read, error);
                        if (io_status == G_IO_STATUS_ERROR) {
                                result = FALSE;
                                goto exit;
                        }
                        if (io_status != G_IO_STATUS_NORMAL) {
                                break;
                        }

                        g_markup_parse_context_parse (parser->context, buf,
                                                      bytes_read, error);
                        if (error != NULL && *error != NULL) {
                                result = FALSE;
                                goto exit;
                        }

                        if (bytes_read < BYTES_PER_READ) {
                                break;
                        }
                }
        }

 exit:
	if (io) {
                g_io_channel_unref (io);
        }
	dh_parser_free (parser);

	return result;
}
