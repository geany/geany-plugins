/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2004-2008 Imendio AB
 * Copyright (C) 2010 Lanedo GmbH
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

#include "dh-link.h"
#include "dh-parser.h"
#include "dh-book.h"

/* Structure defining basic contents to store about every book */
typedef struct {
        /* File path of the book */
        gchar    *path;
        /* Enable or disabled? */
        gboolean  enabled;
        /* Book name */
        gchar    *name;
        /* Book title */
        gchar    *title;
        /* Generated book tree */
        GNode    *tree;
        /* Generated list of keywords in the book */
        GList    *keywords;
} DhBookPriv;

G_DEFINE_TYPE (DhBook, dh_book, G_TYPE_OBJECT);

#define GET_PRIVATE(instance) G_TYPE_INSTANCE_GET_PRIVATE       \
        (instance, DH_TYPE_BOOK, DhBookPriv)

static void    dh_book_init       (DhBook      *book);
static void    dh_book_class_init (DhBookClass *klass);

static void    unref_node_link    (GNode *node,
                                   gpointer data);

static void
book_finalize (GObject *object)
{
        DhBookPriv *priv;

        priv = GET_PRIVATE (object);

        if (priv->tree) {
                g_node_traverse (priv->tree,
                                 G_IN_ORDER,
                                 G_TRAVERSE_ALL,
                                 -1,
                                 (GNodeTraverseFunc)unref_node_link,
                                 NULL);
                g_node_destroy (priv->tree);
        }

        if (priv->keywords) {
                g_list_foreach (priv->keywords, (GFunc)dh_link_unref, NULL);
                g_list_free (priv->keywords);
        }

        g_free (priv->title);

        g_free (priv->path);

        G_OBJECT_CLASS (dh_book_parent_class)->finalize (object);
}

static void
dh_book_class_init (DhBookClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = book_finalize;

	g_type_class_add_private (klass, sizeof (DhBookPriv));
}

static void
dh_book_init (DhBook *book)
{
        DhBookPriv *priv = GET_PRIVATE (book);

        priv->name = NULL;
        priv->path = NULL;
        priv->title = NULL;
        priv->enabled = TRUE;
        priv->tree = NULL;
        priv->keywords = NULL;
}

static void
unref_node_link (GNode *node,
                 gpointer data)
{
        dh_link_unref (node->data);
}

DhBook *
dh_book_new (const gchar  *book_path)
{
        DhBookPriv *priv;
        DhBook     *book;
        GError     *error = NULL;

        g_return_val_if_fail (book_path, NULL);

        book = g_object_new (DH_TYPE_BOOK, NULL);
        priv = GET_PRIVATE (book);

        /* Parse file storing contents in the book struct */
        if (!dh_parser_read_file  (book_path,
                                   &priv->tree,
                                   &priv->keywords,
                                   &error)) {
                g_warning ("Failed to read '%s': %s",
                           priv->path, error->message);
                g_error_free (error);

                /* Deallocate the book, as we are not going to add it
                 *  in the manager */
                g_object_unref (book);
                return NULL;
        }

        /* Store path */
        priv->path = g_strdup (book_path);

        /* Setup title */
        priv->title = g_strdup (dh_link_get_name ((DhLink *)priv->tree->data));

        /* Setup name */
        priv->name = g_strdup (dh_link_get_book_id ((DhLink *)priv->tree->data));

        return book;
}

GList *
dh_book_get_keywords (DhBook *book)
{
        DhBookPriv *priv;

        g_return_val_if_fail (DH_IS_BOOK (book), NULL);

        priv = GET_PRIVATE (book);

        return priv->enabled ? priv->keywords : NULL;
}

GNode *
dh_book_get_tree (DhBook *book)
{
        DhBookPriv *priv;

        g_return_val_if_fail (DH_IS_BOOK (book), NULL);

        priv = GET_PRIVATE (book);

        return priv->enabled ? priv->tree : NULL;
}

const gchar *
dh_book_get_name (DhBook *book)
{
        DhBookPriv *priv;

        g_return_val_if_fail (DH_IS_BOOK (book), NULL);

        priv = GET_PRIVATE (book);

        return priv->name;
}

const gchar *
dh_book_get_title (DhBook *book)
{
        DhBookPriv *priv;

        g_return_val_if_fail (DH_IS_BOOK (book), NULL);

        priv = GET_PRIVATE (book);

        return priv->title;
}

gboolean
dh_book_get_enabled (DhBook *book)
{
        g_return_val_if_fail (DH_IS_BOOK (book), FALSE);

        return GET_PRIVATE (book)->enabled;
}

void
dh_book_set_enabled (DhBook *book,
                     gboolean enabled)
{
        g_return_if_fail (DH_IS_BOOK (book));

        GET_PRIVATE (book)->enabled = enabled;
}

gint
dh_book_cmp_by_path (const DhBook *a,
                     const DhBook *b)
{
        return ((a && b) ?
                g_strcmp0 (GET_PRIVATE (a)->path, GET_PRIVATE (b)->path) :
                -1);
}

gint
dh_book_cmp_by_name (const DhBook *a,
                     const DhBook *b)
{
        return ((a && b) ?
                g_ascii_strcasecmp (GET_PRIVATE (a)->name, GET_PRIVATE (b)->name) :
                -1);
}

gint
dh_book_cmp_by_title (const DhBook *a,
                      const DhBook *b)
{
        return ((a && b) ?
                g_utf8_collate (GET_PRIVATE (a)->title, GET_PRIVATE (b)->title) :
                -1);
}
