/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2005-2008 Imendio AB
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

#ifndef _DH_BOOK_H_
#define _DH_BOOK_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _DhBook      DhBook;
typedef struct _DhBookClass DhBookClass;

#define DH_TYPE_BOOK         (dh_book_get_type ())
#define DH_BOOK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), DH_TYPE_BOOK, DhBook))
#define DH_BOOK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), DH_TYPE_BOOK, DhBookClass))
#define DH_IS_BOOK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), DH_TYPE_BOOK))
#define DH_IS_BOOK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), DH_TYPE_BOOK))
#define DH_BOOK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DH_TYPE_BOOK, DhBookClass))

struct _DhBook {
        GObject parent_instance;
};

struct _DhBookClass {
        GObjectClass parent_class;
};

GType        dh_book_get_type     (void) G_GNUC_CONST;
DhBook      *dh_book_new          (const gchar  *book_path);
GList       *dh_book_get_keywords (DhBook *book);
GNode       *dh_book_get_tree     (DhBook *book);
const gchar *dh_book_get_name     (DhBook *book);
const gchar *dh_book_get_title    (DhBook *book);
gboolean     dh_book_get_enabled  (DhBook *book);
void         dh_book_set_enabled  (DhBook *book,
                                   gboolean enabled);
gint         dh_book_cmp_by_path  (const DhBook *a,
                                   const DhBook *b);
gint         dh_book_cmp_by_name  (const DhBook *a,
                                   const DhBook *b);
gint         dh_book_cmp_by_title (const DhBook *a,
                                   const DhBook *b);

G_END_DECLS

#endif /* _DH_BOOK_H_ */
