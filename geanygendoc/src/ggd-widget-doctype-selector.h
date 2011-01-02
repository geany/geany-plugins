/*
 *  
 *  GeanyGenDoc, a Geany plugin to ease generation of source code documentation
 *  Copyright (C) 2010-2011  Colomban Wendling <ban@herbesfolles.org>
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 */

#ifndef H_GGD_WIDGET_DOCTYPE_SELECTOR
#define H_GGD_WIDGET_DOCTYPE_SELECTOR

#include <gtk/gtk.h>

#include "ggd-macros.h"

G_BEGIN_DECLS
GGD_BEGIN_PLUGIN_API


#define GGD_DOCTYPE_SELECTOR_TYPE            (ggd_doctype_selector_get_type ())
#define GGD_DOCTYPE_SELECTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GGD_DOCTYPE_SELECTOR_TYPE, GgdDoctypeSelector))
#define GGD_DOCTYPE_SELECTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GGD_DOCTYPE_SELECTOR_TYPE, GgdDoctypeSelectorClass))
#define GGD_IS_DOCTYPE_SELECTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GGD_DOCTYPE_SELECTOR_TYPE))
#define GGD_IS_DOCTYPE_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GGD_DOCTYPE_SELECTOR_TYPE))


typedef struct _GgdDoctypeSelector        GgdDoctypeSelector;
typedef struct _GgdDoctypeSelectorClass   GgdDoctypeSelectorClass;
typedef struct _GgdDoctypeSelectorPrivate GgdDoctypeSelectorPrivate;

struct _GgdDoctypeSelector
{
  GtkScrolledWindow parent;
  
  GgdDoctypeSelectorPrivate *priv;
};

struct _GgdDoctypeSelectorClass
{
  GtkScrolledWindowClass parent_class;
};


GType         ggd_doctype_selector_get_type         (void) G_GNUC_CONST;
GtkWidget    *ggd_doctype_selector_new              (void);
gboolean      ggd_doctype_selector_set_doctype      (GgdDoctypeSelector *self,
                                                     guint               language_id,
                                                     const gchar        *doctype);
gchar        *ggd_doctype_selector_get_doctype      (GgdDoctypeSelector *self,
                                                     guint               language_id);


GGD_END_PLUGIN_API
G_END_DECLS

#endif /* guard */
