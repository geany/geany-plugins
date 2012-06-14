/*
 *      gtkcellrendererfile.c
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
 * 		cell renderer that inherits from toggle
 * 		the only difference - checks for a x position in "activate" signal
 * 		to ensure the "toggled" signal is sent only when clicked on a particulary cell renderer
 */

#include <geanyplugin.h>

#include "cellrenderertoggle.h"

/*
 * handles an activation and sends a toggle signal is its cell renderer has been clicked
 */
static gint cell_renderer_toggle_activate(GtkCellRenderer *cell, GdkEvent *event, GtkWidget *widget, const gchar *path,
	GdkRectangle *background_area, GdkRectangle *cell_area, GtkCellRendererState  flags)
{
	if (!event ||
		(
			event->button.x >= cell_area->x &&
			event->button.x < (cell_area->x + cell_area->width)
		)
	)
	{
		g_signal_emit_by_name(cell, "toggled", path);
	}
	return TRUE;
}

/*
 * instance init
 */
static void cell_renderer_toggle_init (CellRendererToggle *cell)
{
	GtkCellRenderer *cell_renderer = (GtkCellRenderer*)cell;
	cell_renderer->mode = GTK_CELL_RENDERER_MODE_ACTIVATABLE;
}

/*
 * class init
 */
static void cell_renderer_toggle_class_init (GtkCellRendererToggleClass *class)
{
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS(class);
	cell_class->activate = cell_renderer_toggle_activate;
}

/*
 * registers a type
 */
GType cell_renderer_toggle_get_type(void)
{
	static GType cell_break_toggle_type = 0;
	
	if(0 == cell_break_toggle_type && !(cell_break_toggle_type = g_type_from_name("CellRendererToggle")))
	{
		static const GTypeInfo cell_file_info =
		{
			sizeof (CellRendererToggleClass),
			NULL,                                                     /* base_init */
			NULL,                                                     /* base_finalize */
			(GClassInitFunc) cell_renderer_toggle_class_init,
			NULL,                                                     /* class_finalize */
			NULL,                                                     /* class_data */
			sizeof (CellRendererToggle),
			0,                                                        /* n_preallocs */
			(GInstanceInitFunc) cell_renderer_toggle_init,
		};

		/* Derive from GtkToggleCellRenderer */
		cell_break_toggle_type = g_type_register_static (GTK_TYPE_CELL_RENDERER_TOGGLE,
			"CellRendererToggle",
			&cell_file_info,
			0);
	}
	
	return cell_break_toggle_type;
}

/*
 * creates new renderer
 */
GtkCellRenderer* cell_renderer_toggle_new(void)
{
  return g_object_new(TYPE_CELL_RENDERER_TOGGLE, NULL);
}
