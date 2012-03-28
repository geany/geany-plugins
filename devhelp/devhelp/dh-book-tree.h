/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Mikael Hallendal <micke@imendio.com>
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

#ifndef __DH_BOOK_TREE_H__
#define __DH_BOOK_TREE_H__

#include <gtk/gtk.h>
#include "dh-link.h"
#include "dh-book-manager.h"

G_BEGIN_DECLS

#define DH_TYPE_BOOK_TREE            (dh_book_tree_get_type ())
#define DH_BOOK_TREE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), DH_TYPE_BOOK_TREE, DhBookTree))
#define DH_BOOK_TREE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), DH_TYPE_BOOK_TREE, DhBookTreeClass))
#define DH_IS_BOOK_TREE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DH_TYPE_BOOK_TREE))
#define DH_IS_BOOK_TREE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), DH_TYPE_BOOK_TREE))

typedef struct _DhBookTree      DhBookTree;
typedef struct _DhBookTreeClass DhBookTreeClass;

struct _DhBookTree {
        GtkTreeView parent_instance;
};

struct _DhBookTreeClass {
        GtkTreeViewClass parent_class;
};

GType        dh_book_tree_get_type                (void) G_GNUC_CONST;
GtkWidget *  dh_book_tree_new                     (DhBookManager *book_manager);
void         dh_book_tree_select_uri              (DhBookTree    *book_tree,
                                                   const gchar   *uri);
const gchar *dh_book_tree_get_selected_book_title (DhBookTree    *tree);

G_END_DECLS

#endif /* __DH_BOOK_TREE_H__ */
