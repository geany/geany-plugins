/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 CodeFactory AB
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@imendio.com>
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

#ifndef __DH_SEARCH_H__
#define __DH_SEARCH_H__

#include <gtk/gtk.h>
#include "dh-link.h"
#include "dh-book-manager.h"

G_BEGIN_DECLS

#define DH_TYPE_SEARCH           (dh_search_get_type ())
#define DH_SEARCH(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), DH_TYPE_SEARCH, DhSearch))
#define DH_SEARCH_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass), DH_TYPE_SEARCH, DhSearchClass))
#define DH_IS_SEARCH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DH_TYPE_SEARCH))
#define DH_IS_SEARCH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), DH_TYPE_SEARCH))

typedef struct _DhSearch      DhSearch;
typedef struct _DhSearchClass DhSearchClass;

struct _DhSearch {
        GtkVBox parent_instance;
};

struct _DhSearchClass {
        GtkVBoxClass parent_class;

        /* Signals */
        void (*link_selected) (DhSearch          *search,
                               DhLink            *link);
};

GType      dh_search_get_type          (void);
GtkWidget *dh_search_new               (DhBookManager *book_manager);
void       dh_search_set_search_string (DhSearch      *search,
                                        const gchar   *str,
                                        const gchar   *book_id);

G_END_DECLS

#endif /* __DH_SEARCH_H__ */
