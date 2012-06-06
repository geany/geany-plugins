/*
 *      cellrendererbreakicon.c
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
 * 		cell renderer class that renders breakpoint icon, that in turn depends on a hitscount,condition and whether
 * 		a breakpoint is enabled or disabled
 */

#include <string.h>
#include <geanyplugin.h>

#include "cellrendererbreakicon.h"

enum {
  PROP_0,
  PROP_PIXBUF_ENABLED,
  PROP_PIXBUF_DISABLED,
  PROP_PIXBUF_CONDITIONAL,
  PROP_PIXBUF_FILE,
  PROP_ENABLED,
  PROP_CONDITION,
  PROP_HITSCOUNT
};

static gpointer parent_class;
static guint clicked_signal;

/*
 * property getter
 */
static void cell_renderer_break_icon_get_property(GObject *object, guint param_id, GValue *value, GParamSpec *pspec)
{
	CellRendererBreakIcon *cellbreakpoint = CELL_RENDERER_BREAK_ICON(object);
	switch (param_id)
	{
		case PROP_PIXBUF_ENABLED:
			g_value_set_object (value, cellbreakpoint->pixbuf_enabled);
			break;
		case PROP_PIXBUF_DISABLED:
			g_value_set_object (value, cellbreakpoint->pixbuf_disabled);
			break;
		case PROP_PIXBUF_CONDITIONAL:
			  g_value_set_object (value, cellbreakpoint->pixbuf_conditional);
			  break;
		case PROP_PIXBUF_FILE:
			  g_value_set_object (value, cellbreakpoint->pixbuf_file);
			  break;
		case PROP_ENABLED:
			g_value_set_boolean(value, cellbreakpoint->enabled);
			break;
		case PROP_CONDITION:
			g_value_set_string(value, cellbreakpoint->condition);
			break;
		case PROP_HITSCOUNT:
			g_value_set_int(value, cellbreakpoint->hitscount);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/*
 * property setter
 */
static void cell_renderer_break_icon_set_property (GObject *object, guint param_id, const GValue *value, GParamSpec *pspec)
{
	CellRendererBreakIcon *cellbreakpoint = CELL_RENDERER_BREAK_ICON(object);
	switch (param_id)
	{
		case PROP_PIXBUF_ENABLED:
			if (cellbreakpoint->pixbuf_enabled)
			{
				g_object_unref(cellbreakpoint->pixbuf_enabled);
			}
			cellbreakpoint->pixbuf_enabled = (GdkPixbuf*)g_value_dup_object(value);
			break;
		case PROP_PIXBUF_DISABLED:
			if (cellbreakpoint->pixbuf_disabled)
			{
				g_object_unref(cellbreakpoint->pixbuf_disabled);
			}
			cellbreakpoint->pixbuf_disabled = (GdkPixbuf*)g_value_dup_object(value);
			break;
		case PROP_PIXBUF_CONDITIONAL:
			if (cellbreakpoint->pixbuf_conditional)
			{
				g_object_unref(cellbreakpoint->pixbuf_conditional);
			}
			cellbreakpoint->pixbuf_conditional = (GdkPixbuf*)g_value_dup_object(value);
			break;
		case PROP_PIXBUF_FILE:
			if (cellbreakpoint->pixbuf_file)
			{
				g_object_unref(cellbreakpoint->pixbuf_file);
			}
			cellbreakpoint->pixbuf_file = (GdkPixbuf*)g_value_dup_object(value);
			break;
		case PROP_ENABLED:
			cellbreakpoint->enabled = g_value_get_boolean(value);
			break;
		case PROP_CONDITION:
			cellbreakpoint->condition = g_value_get_string(value);
			if (cellbreakpoint->condition)
			{
				cellbreakpoint->condition = g_strdup(cellbreakpoint->condition);
			}
			break;
		case PROP_HITSCOUNT:
			cellbreakpoint->hitscount = g_value_get_int(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/*
 * get size of a cell
 */
static void cell_renderer_break_icon_get_size(GtkCellRenderer *cell, GtkWidget *widget, GdkRectangle *cell_area, 
	gint *x_offset, gint *y_offset, gint *width, gint *height)
{
	CellRendererBreakIcon *cellbreakpoint = (CellRendererBreakIcon *) cell;
	gint pixbuf_width  = 0;
	gint pixbuf_height = 0;
	gint calc_width;
	gint calc_height;
	
	if (cellbreakpoint->pixbuf_enabled)
	{
		pixbuf_width  = gdk_pixbuf_get_width (cellbreakpoint->pixbuf_enabled);
		pixbuf_height = gdk_pixbuf_get_height (cellbreakpoint->pixbuf_enabled);
	}
	if (cellbreakpoint->pixbuf_disabled)
	{
		pixbuf_width  = MAX (pixbuf_width, gdk_pixbuf_get_width (cellbreakpoint->pixbuf_disabled));
		pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (cellbreakpoint->pixbuf_disabled));
	}
	if (cellbreakpoint->pixbuf_conditional)
	{
		pixbuf_width  = MAX (pixbuf_width, gdk_pixbuf_get_width (cellbreakpoint->pixbuf_conditional));
		pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (cellbreakpoint->pixbuf_conditional));
	}
	if (cellbreakpoint->pixbuf_file)
	{
		pixbuf_width  = MAX (pixbuf_width, gdk_pixbuf_get_width (cellbreakpoint->pixbuf_file));
		pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (cellbreakpoint->pixbuf_file));
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
static void cell_renderer_break_icon_render(GtkCellRenderer *cell, GdkDrawable *window, GtkWidget *widget,
	GdkRectangle *background_area, GdkRectangle *cell_area, GdkRectangle *expose_area, GtkCellRendererState flags)
{
	CellRendererBreakIcon *cellbreakpoint = (CellRendererBreakIcon*) cell;
	
	GdkPixbuf *pixbuf = NULL;
	
	GdkRectangle pix_rect;
	GdkRectangle draw_rect;
	cairo_t *cr;
	
	cell_renderer_break_icon_get_size (cell, widget, cell_area,
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
	
	if (cell->is_expander)
	{
		pixbuf = cellbreakpoint->pixbuf_file;
	}
	else if (!cellbreakpoint->enabled)
	{
		pixbuf = cellbreakpoint->pixbuf_disabled;
	}
	else if ((cellbreakpoint->condition && strlen(cellbreakpoint->condition)) || cellbreakpoint->hitscount)
	{
		pixbuf = cellbreakpoint->pixbuf_conditional;
	}
	else
	{
		pixbuf = cellbreakpoint->pixbuf_enabled;
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
 * activate callback
 */
static gint cell_renderer_break_icon_activate(GtkCellRenderer *cell, GdkEvent *event, GtkWidget *widget, const gchar *path,
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
 * init instance
 */
static void cell_renderer_break_icon_init (CellRendererBreakIcon *cell)
{
	cell->enabled = TRUE;
	cell->condition = NULL;
	cell->hitscount = 0;
	
	GtkCellRenderer *cell_renderer = (GtkCellRenderer*)cell;
	
	cell_renderer->mode = GTK_CELL_RENDERER_MODE_ACTIVATABLE;

	cell->pixbuf_enabled = cell->pixbuf_disabled = cell->pixbuf_conditional = cell->pixbuf_file = 0;
}

/*
 * finalizes an instance (frees pixbuffers) 
 */
static void cell_renderer_break_icon_finalize (GObject *object)
{
	CellRendererBreakIcon *cell = (CellRendererBreakIcon*)object;
	
	GdkPixbuf *pixbufs[] = { cell->pixbuf_enabled, cell->pixbuf_disabled, cell->pixbuf_conditional, cell->pixbuf_file };
	int i;
	for(i = 0; i < 4; i++)
	{
		if (pixbufs[i])
		{
			g_object_unref(pixbufs[i]);
		}
	}
	if (cell->condition)
	{
		g_free((void*)cell->condition);
	}
	
	(*G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/*
 * init class
 */
static void cell_renderer_break_icon_class_init (CellRendererBreakIconClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS(class);

	parent_class = g_type_class_peek_parent(class);
	
	object_class->get_property = cell_renderer_break_icon_get_property;
	object_class->set_property = cell_renderer_break_icon_set_property;

	object_class->finalize = cell_renderer_break_icon_finalize;
	
	cell_class->get_size = cell_renderer_break_icon_get_size;
	cell_class->render = cell_renderer_break_icon_render;

	cell_class->activate = cell_renderer_break_icon_activate;
	
	g_object_class_install_property (object_class,
		PROP_PIXBUF_ENABLED,
		g_param_spec_object (
			"pixbuf_enabled",
			"Pixbuf Object",
			"Enabled break image",
			GDK_TYPE_PIXBUF,
			G_PARAM_READWRITE)
	);

	g_object_class_install_property (object_class,
		PROP_PIXBUF_DISABLED,
		g_param_spec_object (
			"pixbuf_disabled",
			"Pixbuf Object",
			"Disabled break image",
			GDK_TYPE_PIXBUF,
			G_PARAM_READWRITE)
	);

	g_object_class_install_property (object_class,
		PROP_PIXBUF_CONDITIONAL,
		g_param_spec_object (
			"pixbuf_conditional",
			"Pixbuf Object",
			"Conditional break image",
			GDK_TYPE_PIXBUF,
			G_PARAM_READWRITE)
	);

	g_object_class_install_property (object_class,
		PROP_PIXBUF_FILE,
		g_param_spec_object (
			"pixbuf_file",
			"Pixbuf Object",
			"File image",
			GDK_TYPE_PIXBUF,
			G_PARAM_READWRITE)
	);

	g_object_class_install_property (object_class,
		PROP_ENABLED,
		g_param_spec_boolean ("enabled", "Activeness", "The active state of the breakpoint", FALSE, G_PARAM_READWRITE)
	);

	g_object_class_install_property (object_class,
		PROP_CONDITION,
		g_param_spec_string ("condition", "Breakpoint condition", "Whether a brealpoint has a condition", FALSE, G_PARAM_READWRITE)
	);

	g_object_class_install_property (object_class,
		PROP_HITSCOUNT,
		g_param_spec_int (
			"hitscount",
			"Breakpoint hitscount",
			"Number of passes to wait until stop at a breakpoint",
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE
		)
	);
  
	clicked_signal = g_signal_new ("clicked",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (CellRendererBreakIconClass, clicked),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);
}

/*
 * registers a type
 */
GType cell_renderer_break_icon_get_type(void)
{
	static GType cell_break_icon_type = 0;
	
	if(0 == cell_break_icon_type)
	{
		if ( (cell_break_icon_type = g_type_from_name("CellRendererBreakIcon")) )
		{
			parent_class = g_type_class_peek_static(g_type_parent(cell_break_icon_type));
			clicked_signal = g_signal_lookup("clicked", cell_break_icon_type);
		}
		else
		{
			static const GTypeInfo cell_break_icon_info =
			{
				sizeof (CellRendererBreakIconClass),
				NULL,                                                     /* base_init */
				NULL,                                                     /* base_finalize */
				(GClassInitFunc) cell_renderer_break_icon_class_init,
				NULL,                                                     /* class_finalize */
				NULL,                                                     /* class_data */
				sizeof (CellRendererBreakIcon),
				0,                                                        /* n_preallocs */
				(GInstanceInitFunc) cell_renderer_break_icon_init,
			};
		
			/* Derive from GtkCellRenderer */
			cell_break_icon_type = g_type_register_static (GTK_TYPE_CELL_RENDERER,
				"CellRendererBreakIcon",
				&cell_break_icon_info,
				0);
		}
	}
	
	return cell_break_icon_type;
}

/*
 * creates new renderer
 */
GtkCellRenderer* cell_renderer_break_icon_new(void)
{
  return g_object_new(TYPE_CELL_RENDERER_BREAK_ICON, NULL);
}
