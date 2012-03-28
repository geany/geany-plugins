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
#include "dh-util.h"
#include "dh-book.h"
#include "dh-book-manager.h"
#include "dh-marshal.h"

typedef struct {
        /* The list of all DhBooks found in the system */
        GList *books;
} DhBookManagerPriv;

enum {
        DISABLED_BOOK_LIST_UPDATED,
        LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (DhBookManager, dh_book_manager, G_TYPE_OBJECT);

#define GET_PRIVATE(instance) G_TYPE_INSTANCE_GET_PRIVATE       \
        (instance, DH_TYPE_BOOK_MANAGER, DhBookManagerPriv)

static void    dh_book_manager_init       (DhBookManager      *book_manager);
static void    dh_book_manager_class_init (DhBookManagerClass *klass);

static void    book_manager_add_from_filepath     (DhBookManager *book_manager,
                                                   const gchar   *book_path);
static void    book_manager_add_from_dir          (DhBookManager *book_manager,
                                                   const gchar   *dir_path);

#ifdef GDK_WINDOWING_QUARTZ
static void    book_manager_add_from_xcode_docset (DhBookManager *book_manager,
                                                   const gchar   *dir_path);
#endif

static void
book_manager_finalize (GObject *object)
{
        DhBookManagerPriv *priv;
        GList             *l;

        priv = GET_PRIVATE (object);

        /* Destroy all books */
        for (l = priv->books; l; l = g_list_next (l)) {
                g_object_unref (l->data);
        }
        g_list_free (priv->books);

        G_OBJECT_CLASS (dh_book_manager_parent_class)->finalize (object);
}

static void
dh_book_manager_class_init (DhBookManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = book_manager_finalize;

        signals[DISABLED_BOOK_LIST_UPDATED] =
                g_signal_new ("disabled-book-list-updated",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (DhBookManagerClass, disabled_book_list_updated),
                              NULL, NULL,
                              _dh_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);

	g_type_class_add_private (klass, sizeof (DhBookManagerPriv));
}

static void
dh_book_manager_init (DhBookManager *book_manager)
{
        DhBookManagerPriv *priv = GET_PRIVATE (book_manager);

        priv->books = NULL;
}

static void
book_manager_clean_list_of_books_disabled (GSList *books_disabled)
{
        GSList *sl;

        for (sl = books_disabled; sl; sl = g_slist_next (sl)) {
                g_free (sl->data);
        }
        g_slist_free (sl);
}

static void
book_manager_check_status_from_conf (DhBookManager *book_manager)
{
        GSList *books_disabled, *sl;

        books_disabled = dh_util_state_load_books_disabled ();

        for (sl = books_disabled; sl; sl = g_slist_next (sl)) {
                DhBook *book;

                book = dh_book_manager_get_book_by_name (book_manager,
                                                         (const gchar *)sl->data);
                if (book) {
                        dh_book_set_enabled (book, FALSE);
                }
        }

        book_manager_clean_list_of_books_disabled (books_disabled);
}

static void
book_manager_add_books_in_data_dir (DhBookManager *book_manager,
                                    const gchar   *data_dir)
{
        gchar *dir;

        dir = g_build_filename (data_dir, "gtk-doc", "html", NULL);
        book_manager_add_from_dir (book_manager, dir);
        g_free (dir);

        dir = g_build_filename (data_dir, "devhelp", "books", NULL);
        book_manager_add_from_dir (book_manager, dir);
        g_free (dir);
}

void
dh_book_manager_populate (DhBookManager *book_manager)
{
        const gchar * const * system_dirs;

        book_manager_add_books_in_data_dir (book_manager,
                                            g_get_user_data_dir ());

        system_dirs = g_get_system_data_dirs ();
        while (*system_dirs) {
                book_manager_add_books_in_data_dir (book_manager,
                                                    *system_dirs);
                system_dirs++;
        }

#ifdef GDK_WINDOWING_QUARTZ
        book_manager_add_from_xcode_docset (
                book_manager,
                "/Library/Developer/Shared/Documentation/DocSets");
#endif

        /* Once all books are loaded, check enabled status from conf */
        book_manager_check_status_from_conf (book_manager);
}

static gchar *
book_manager_get_book_path (const gchar *base_path,
                            const gchar *name)
{
        static const gchar *suffixes[] = {
                "devhelp2",
                "devhelp2.gz",
                "devhelp",
                "devhelp.gz",
                NULL
        };
        gchar *tmp;
        gchar *book_path;
        guint  i;

        for (i = 0; suffixes[i]; i++) {
                tmp = g_build_filename (base_path, name, name, NULL);
                book_path = g_strconcat (tmp, ".", suffixes[i], NULL);
                g_free (tmp);

                if (g_file_test (book_path, G_FILE_TEST_EXISTS)) {
                        return book_path;;
                }
                g_free (book_path);
        }
        return NULL;
}

static void
book_manager_add_from_dir (DhBookManager *book_manager,
                           const gchar   *dir_path)
{
        GDir        *dir;
        const gchar *name;

        g_return_if_fail (book_manager);
        g_return_if_fail (dir_path);

        /* Open directory */
        dir = g_dir_open (dir_path, 0, NULL);
        if (!dir) {
                return;
        }

        /* And iterate it */
        while ((name = g_dir_read_name (dir)) != NULL) {
                gchar *book_path;

                book_path = book_manager_get_book_path (dir_path, name);
                if (book_path) {
                        /* Add book from filepath */
                        book_manager_add_from_filepath (book_manager,
                                                        book_path);
                        g_free (book_path);
                }
        }

        g_dir_close (dir);
}

#ifdef GDK_WINDOWING_QUARTZ
static gboolean
seems_docset_dir (const gchar *path)
{
        gchar    *tmp;
        gboolean  seems_like_devhelp = FALSE;

        g_return_val_if_fail (path, FALSE);

        /* Do some sanity checking on the directory first so we don't have
         * to go through several hundreds of files in every docset.
         */
        tmp = g_build_filename (path, "style.css", NULL);
        if (g_file_test (tmp, G_FILE_TEST_EXISTS)) {
                gchar *tmp;

                tmp = g_build_filename (path, "index.sgml", NULL);
                if (g_file_test (tmp, G_FILE_TEST_EXISTS)) {
                        seems_like_devhelp = TRUE;
                }
                g_free (tmp);
        }
        g_free (tmp);

        return seems_like_devhelp;
}

static void
book_manager_add_from_xcode_docset (DhBookManager *book_manager,
                                    const gchar   *dir_path)
{
        GDir        *dir;
        const gchar *name;

        g_return_if_fail (book_manager);
        g_return_if_fail (dir_path);

        if (!seems_docset_dir (dir_path)) {
                return;
        }

        /* Open directory */
        dir = g_dir_open (dir_path, 0, NULL);
        if (!dir) {
                return;
        }

        /* And iterate it, looking for files ending with .devhelp2 */
        while ((name = g_dir_read_name (dir)) != NULL) {
                if (g_strcmp0 (strrchr (name, '.'),
                               ".devhelp2") == 0) {
                        gchar *book_path;

                        book_path = g_build_filename (path, name, NULL);
                        /* Add book from filepath */
                        book_manager_add_from_filepath (book_manager,
                                                        book_path);
                        g_free (book_path);
                }
        }

        g_dir_close (dir);
}
#endif

static void
book_manager_add_from_filepath (DhBookManager *book_manager,
                                const gchar   *book_path)
{
        DhBookManagerPriv *priv;
        DhBook            *book;

        g_return_if_fail (book_manager);
        g_return_if_fail (book_path);

        priv = GET_PRIVATE (book_manager);

        /* Allocate new book struct */
        book = dh_book_new (book_path);

        /* Check if book with same path was already loaded in the manager */
        if (g_list_find_custom (priv->books,
                                book,
                                (GCompareFunc)dh_book_cmp_by_path)) {
                g_object_unref (book);
                return;
        }

        /* Check if book with same bookname was already loaded in the manager
         * (we need to force unique book names) */
        if (g_list_find_custom (priv->books,
                                book,
                                (GCompareFunc)dh_book_cmp_by_name)) {
                g_object_unref (book);
                return;
        }

        /* Add the book to the book list */
        priv->books = g_list_insert_sorted (priv->books,
                                            book,
                                            (GCompareFunc)dh_book_cmp_by_title);
}

GList *
dh_book_manager_get_books (DhBookManager *book_manager)
{
        g_return_val_if_fail (book_manager, NULL);

        return GET_PRIVATE (book_manager)->books;
}

DhBook *
dh_book_manager_get_book_by_name (DhBookManager *book_manager,
                                  const gchar *name)
{
        DhBook *book = NULL;
        GList  *l;

        g_return_val_if_fail (book_manager, NULL);

        for (l = GET_PRIVATE (book_manager)->books;
             l && !book;
             l = g_list_next (l)) {
                if (g_strcmp0 (name,
                               dh_book_get_name (DH_BOOK (l->data))) == 0) {
                        book = l->data;
                }
        }

        return book;
}

void
dh_book_manager_update (DhBookManager *book_manager)
{
        DhBookManagerPriv *priv;
        GSList *books_disabled = NULL;
        GList  *l;

        g_return_if_fail (book_manager);

        priv = GET_PRIVATE (book_manager);

        /* Create list of disabled books */
        for (l = priv->books; l; l = g_list_next (l)) {
                DhBook *book = DH_BOOK (l->data);

                if (!dh_book_get_enabled (book)) {
                        books_disabled = g_slist_append (books_disabled,
                                                         g_strdup (dh_book_get_name (book)));
                }
        }

        /* Store in conf */
        dh_util_state_store_books_disabled (books_disabled);

        /* Emit signal to notify others */
        g_signal_emit (book_manager,
                       signals[DISABLED_BOOK_LIST_UPDATED],
                       0);

        book_manager_clean_list_of_books_disabled (books_disabled);
}

DhBookManager *
dh_book_manager_new (void)
{
        return g_object_new (DH_TYPE_BOOK_MANAGER, NULL);
}

