/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
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

#ifndef __DH_BOOK_MANAGER_H__
#define __DH_BOOK_MANAGER_H__

#include <gtk/gtk.h>

#include "dh-book.h"

G_BEGIN_DECLS

typedef struct _DhBookManager      DhBookManager;
typedef struct _DhBookManagerClass DhBookManagerClass;

#define DH_TYPE_BOOK_MANAGER         (dh_book_manager_get_type ())
#define DH_BOOK_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), DH_TYPE_BOOK_MANAGER, DhBookManager))
#define DH_BOOK_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), DH_TYPE_BOOK_MANAGER, DhBookManagerClass))
#define DH_IS_BOOK_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), DH_TYPE_BOOK_MANAGER))
#define DH_IS_BOOK_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), DH_TYPE_BOOK_MANAGER))
#define DH_BOOK_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DH_TYPE_BOOK_MANAGER, DhBookManagerClass))

struct _DhBookManager {
        GObject parent_instance;
};

struct _DhBookManagerClass {
        GObjectClass parent_class;

        /* Signals */
        void (* disabled_book_list_updated) (DhBookManager *book_manager);
};

GType          dh_book_manager_get_type         (void) G_GNUC_CONST;
DhBookManager *dh_book_manager_new              (void);
void           dh_book_manager_populate         (DhBookManager *book_manager);
GList         *dh_book_manager_get_books        (DhBookManager *book_manager);
DhBook        *dh_book_manager_get_book_by_name (DhBookManager *book_manager,
                                                 const gchar *name);
void           dh_book_manager_update           (DhBookManager *book_manager);

G_END_DECLS

#endif /* __DH_BOOK_MANAGER_H__ */
