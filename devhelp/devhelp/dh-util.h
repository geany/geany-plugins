/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2004,2008 Imendio AB
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

#ifndef __DH_UTIL_H__
#define __DH_UTIL_H__

#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include "dh-link.h"

G_BEGIN_DECLS

GtkBuilder * dh_util_builder_get_file             (const gchar *filename,
                                                   const gchar *root,
                                                   const gchar *domain,
                                                   const gchar *first_required_widget,
                                                   ...);
void         dh_util_builder_connect              (GtkBuilder  *gui,
                                                   gpointer     user_data,
                                                   gchar       *first_widget,
                                                   ...);
gchar *      dh_util_build_data_filename          (const gchar *first_part,
                                                   ...);
void         dh_util_state_manage_window          (GtkWindow   *window,
                                                   const gchar *name);
void         dh_util_state_manage_paned           (GtkPaned    *paned,
                                                   const gchar *name);
void         dh_util_state_manage_notebook        (GtkNotebook *notebook,
                                                   const gchar *name,
                                                   const gchar *default_tab);
void         dh_util_state_set_notebook_page_name (GtkWidget   *page,
                                                   const gchar *page_name);
const gchar *dh_util_state_get_notebook_page_name (GtkWidget   *page);
GSList *     dh_util_state_load_books_disabled    (void);
void         dh_util_state_store_books_disabled   (GSList *books_disabled);

void         dh_util_font_get_variable            (gchar        **name,
                                                   gdouble       *size,
                                                   gboolean       use_system_font);
void         dh_util_font_get_fixed               (gchar        **name,
                                                   gdouble       *size,
                                                   gboolean       use_system_font);
void         dh_util_font_add_web_view            (WebKitWebView *view);
gint         dh_util_cmp_book                     (DhLink *a,
                                                   DhLink *b);

G_END_DECLS

#endif /* __DH_UTIL_H__ */
