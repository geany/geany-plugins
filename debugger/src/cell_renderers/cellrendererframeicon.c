/*
 *      cellrendererframeicon.c
 *      
 *      Copyright 2012 Alexander Petukhov <devel(at)apetukhov.ru>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/*
 * 		cell renderer class that renders frame icon, that in turn depends on whether a
 * 		row is the first vhildren (uppermost frame) and whether a renderer is under the cursor
 */

#include <string.h>
#include <geanyplugin.h>

#include "cellrendererframeicon.h"

enum {
  PROP_0,
  PROP_PIXBUF_ACTIVE,
  PROP_PIXBUF_HIGHLIGHTED,
  PROP_ACTIVE_FRAME,
};

static gpointer parent_class;
static guint clicked_signal;

/*
 * activate callback
 */
static gint cell_renderer_frame_icon_activate(GtkCellRenderer *cell, GdkEvent *event, GtkWidget *widget, const gchar *path,
	GdkRectangle *background_area, GdkRectangle *cell_area, GtkCellRendererState  flags)
{
	if (!event ||
		(
			event->button.x >= cell_area->x &&
			event->button.x < (cell_area->x + cell_area->width)
		)
	)
	{
		g_signal_emit (cell, clicked_signal, 0, path);
	}
	return TRUE;
}

/*
 * property getter
 */
static void cell_renderer_frame_icon_get_property(GObject *object, guint param_id, GValue *value, GParamSpec *pspec)
{
	CellRendererFrameIcon *cellframe = CELL_RENDERER_FRAME_ICON(object);
	switch (param_id)
	{
		case PROP_PIXBUF_ACTIVE:
			g_value_set_object (value, cellframe->pixbuf_active);
			break;
		case PROP_PIXBUF_HIGHLIGHTED:
			g_value_set_object (value, cellframe->pixbuf_highlighted);
			break;
		case PROP_ACTIVE_FRAME:
			g_value_set_boolean(value, cellframe->active_frame);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/*
 * property setter
 */
static void cell_renderer_frame_icon_set_property (GObject *object, guint param_id, const GValue *value, GParamSpec *pspec)
{
	CellRendererFrameIcon *cellframe = CELL_RENDERER_FRAME_ICON(object);
	switch (param_id)
	{
		case PROP_PIXBUF_ACTIVE:
			if (cellframe->pixbuf_active)
			{
				g_object_unref(cellframe->pixbuf_active);
			}
			cellframe->pixbuf_active = (GdkPixbuf*)g_value_dup_object(value);
			break;
		case PROP_PIXBUF_HIGHLIGHTED:
			if (cellframe->pixbuf_highlighted)
			{
				g_object_unref(cellframe->pixbuf_highlighted);
			}
			cellframe->pixbuf_highlighted = (GdkPixbuf*)g_value_dup_object(value);
			break;
		case PROP_ACTIVE_FRAME:
		{
			cellframe->active_frame = g_value_get_boolean(value);
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/*
 * get size of a cell
 */
static void cell_renderer_frame_icon_get_size(GtkCellRenderer *cell, GtkWidget *widget, GdkRectangle *cell_area, 
	gint *x_offset, gint *y_offset, gint *width, gint *height)
{
	CellRendererFrameIcon *cellframe = (CellRendererFrameIcon *) cell;
	gint pixbuf_width  = 0;
	gint pixbuf_height = 0;
	gint calc_width;
	gint calc_height;
	
	if (cellframe->pixbuf_active)
	{
		pixbuf_width  = gdk_pixbuf_get_width (cellframe->pixbuf_active);
		pixbuf_height = gdk_pixbuf_get_height (cellframe->pixbuf_active);
	}
	if (cellframe->pixbuf_highlighted)
	{
		pixbuf_width  = MAX (pixbuf_width, gdk_pixbuf_get_width (cellframe->pixbuf_highlighted));
		pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (cellframe->pixbuf_highlighted));
	}
	
	calc_width  = (gint) cell->xpad * 2 + pixbuf_width;
	calc_height = (gint) cell->ypad * 2 + pixbuf_height;
	
	if (cell_area && pixbuf_width > 0 && pixbuf_height > 0)
	{
		if (x_offset)
		{
			*x_offset = (((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
				(1.0 - cell->xalign) : cell->xalign) * 
				(cell_area->width - calc_width));
			*x_offset = MAX (*x_offset, 0);
		}
		if (y_offset)
		{
			*y_offset = (cell->yalign * (cell_area->height - calc_height));
			*y_offset = MAX (*y_offset, 0);
		}
	}
	else
	{
		if (x_offset) *x_offset = 0;
		if (y_offset) *y_offset = 0;
	}
	
	if (width)
		*width = calc_width;
	
	if (height)
		*height = calc_height;
}

/*
 * render a cell
 */
static void cell_renderer_frame_icon_render(GtkCellRenderer *cell, GdkDrawable *window, GtkWidget *widget,
	GdkRectangle *background_area, GdkRectangle *cell_area, GdkRectangle *expose_area, GtkCellRendererState flags)
{
	CellRendererFrameIcon *cellframe = (CellRendererFrameIcon*) cell;
	
	GdkPixbuf *pixbuf = NULL;
	
	GdkRectangle pix_rect;
	GdkRectangle draw_rect;
	cairo_t *cr;
	
	cell_renderer_frame_icon_get_size (cell, widget, cell_area,
		&pix_rect.x,
		&pix_rect.y,
		&pix_rect.width,
		&pix_rect.height);
	
	pix_rect.x += cell_area->x + cell->xpad;
	pix_rect.y += cell_area->y + cell->ypad;
	pix_rect.width  -= cell->xpad * 2;
	pix_rect.height -= cell->ypad * 2;
	
	if (!gdk_rectangle_intersect (cell_area, &pix_rect, &draw_rect) ||
		!gdk_rectangle_intersect (expose_area, &draw_rect, &draw_rect))
		return;
	
	if (cellframe->active_frame)
	{
		pixbuf = cellframe->pixbuf_active;
	}
	else if (flags & GTK_CELL_RENDERER_PRELIT)
	{
		pixbuf = cellframe->pixbuf_highlighted;
	}
	
	if (!pixbuf)
		return;
	
	cr = gdk_cairo_create (window);
	
	gdk_cairo_set_source_pixbuf (cr, pixbuf, pix_rect.x, pix_rect.y);
	gdk_cairo_rectangle (cr, &draw_rect);
	cairo_fill (cr);
	
	cairo_destroy (cr);
}

/*
 * init instance
 */
static void cell_renderer_frame_icon_init (CellRendererFrameIcon *cell)
{
	GtkCellRenderer *cell_renderer = (GtkCellRenderer*)cell;
	
	cell->active_frame = FALSE;
	
	cell_renderer->mode = GTK_CELL_RENDERER_MODE_ACTIVATABLE;

	cell->pixbuf_active = cell->pixbuf_highlighted = 0;
}

/*
 * finalizes an instance (frees pixbuffers) 
 */
static void cell_renderer_frame_icon_finalize (GObject *object)
{
	CellRendererFrameIcon *cell = (CellRendererFrameIcon*)object;
	
	GdkPixbuf *pixbufs[] = { cell->pixbuf_active, cell->pixbuf_highlighted };
	int i;
	for(i = 0; i < 2; i++)
	{
		if (pixbufs[i])
		{
			g_object_unref(pixbufs[i]);
		}
	}
	
	(*G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/*
 * init class
 */
static void cell_renderer_frame_icon_class_init (CellRendererFrameIconClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS(class);

	parent_class = g_type_class_peek_parent(class);
	
	object_class->get_property = cell_renderer_frame_icon_get_property;
	object_class->set_property = cell_renderer_frame_icon_set_property;

	object_class->finalize = cell_renderer_frame_icon_finalize;
	
	cell_class->get_size = cell_renderer_frame_icon_get_size;
	cell_class->render = cell_renderer_frame_icon_render;

	cell_class->activate = cell_renderer_frame_icon_activate;

	g_object_class_install_property (object_class,
		PROP_PIXBUF_ACTIVE,
		g_param_spec_object (
			"pixbuf_active",
			"Pixbuf Object",
			"Active frame image",
			GDK_TYPE_PIXBUF,
			G_PARAM_READWRITE)
	);

	g_object_class_install_property (object_class,
		PROP_PIXBUF_HIGHLIGHTED,
		g_param_spec_object (
			"pixbuf_highlighted",
			"Pixbuf Object",
			"Highlighted frame image",
			GDK_TYPE_PIXBUF,
			G_PARAM_READWRITE)
	);

	g_object_class_install_property (object_class,
		PROP_ACTIVE_FRAME,
		g_param_spec_boolean ("active_frame", "Activeness", "Is a frame active", FALSE, G_PARAM_READWRITE)
	);

	clicked_signal = g_signal_new ("clicked",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (CellRendererFrameIconClass, clicked),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);
}

/*
 * registers a type
 */
GType cell_renderer_frame_icon_get_type(void)
{
	static GType cell_frame_icon_type = 0;
	
	if(0 == cell_frame_icon_type)
	{
		if ( (cell_frame_icon_type = g_type_from_name("CellRendererFrameIcon")) )
		{
			parent_class = g_type_class_peek_static(g_type_parent(cell_frame_icon_type));
			clicked_signal = g_signal_lookup("clicked", cell_frame_icon_type);
		}
		else
		{
			static const GTypeInfo cell_frame_icon_info =
			{
				sizeof (CellRendererFrameIconClass),
				NULL,                                                     /* base_init */
				NULL,                                                     /* base_finalize */
				(GClassInitFunc) cell_renderer_frame_icon_class_init,
				NULL,                                                     /* class_finalize */
				NULL,                                                     /* class_data */
				sizeof (CellRendererFrameIcon),
				0,                                                        /* n_preallocs */
				(GInstanceInitFunc) cell_renderer_frame_icon_init,
			};
	
			/* Derive from GtkCellRenderer */
			cell_frame_icon_type = g_type_register_static (GTK_TYPE_CELL_RENDERER,
				"CellRendererFrameIcon",
				&cell_frame_icon_info,
				0);
		}
	}
	
	return cell_frame_icon_type;
}

/*
 * creates new renderer
 */
GtkCellRenderer* cell_renderer_frame_icon_new(void)
{
  return g_object_new(TYPE_CELL_RENDERER_FRAME_ICON, NULL);
}
