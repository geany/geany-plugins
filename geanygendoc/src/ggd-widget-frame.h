/*
 *  
 *  GeanyGenDoc, a Geany plugin to ease generation of source code documentation
 *  Copyright (C) 2010  Colomban Wendling <ban@herbesfolles.org>
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

#ifndef H_GGD_WIDGET_FRAME
#define H_GGD_WIDGET_FRAME

#include <gtk/gtk.h>

#include "ggd-macros.h"

G_BEGIN_DECLS
GGD_BEGIN_PLUGIN_API


#define GGD_FRAME_TYPE            (ggd_frame_get_type ())
#define GGD_FRAME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GGD_FRAME_TYPE, GgdFrame))
#define GGD_FRAME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GGD_FRAME_TYPE, GgdFrameClass))
#define GGD_IS_FRAME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GGD_FRAME_TYPE))
#define GGD_IS_FRAME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GGD_FRAME_TYPE))


typedef struct _GgdFrame        GgdFrame;
typedef struct _GgdFrameClass   GgdFrameClass;
typedef struct _GgdFramePrivate GgdFramePrivate;

struct _GgdFrame
{
  GtkFrame parent;
  
  GgdFramePrivate *priv;
};

struct _GgdFrameClass
{
  GtkFrameClass parent_class;
};


GType         ggd_frame_get_type        (void);
GtkWidget    *ggd_frame_new             (const gchar *label);


GGD_END_PLUGIN_API
G_END_DECLS

#endif /* guard */
