/*
 *      cellrendererbreakicon.h
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

#ifndef __CELL_RENDERER_BREAK_ICON_H__
#define __CELL_RENDERER_BREAK_ICON_H__

#include <gtk/gtkcellrenderer.h>

G_BEGIN_DECLS

#define TYPE_CELL_RENDERER_BREAK_ICON				(cell_renderer_break_icon_get_type ())
#define CELL_RENDERER_BREAK_ICON(obj)				(G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CELL_RENDERER_BREAK_ICON, CellRendererBreakIcon))
#define CELL_RENDERER_BREAK_ICON_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_CELL_RENDERER_BREAK_ICON, CellRendererBreakIconClass))
#define IS_CELL_RENDERER_BREAK_ICON(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CELL_RENDERER_BREAK_ICON))
#define IS_CELL_RENDERER_BREAK_ICON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_CELL_RENDERER_BREAK_ICON))
#define CELL_RENDERER_BREAK_ICON_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_CELL_RENDERER_BREAK_ICON, CellRendererBreakIconClass))

typedef struct _CellRendererBreakIcon CellRendererBreakIcon;
typedef struct _CellRendererBreakIconClass CellRendererBreakIconClass;

GType					cell_renderer_break_icon_get_type(void);
GtkCellRenderer*		cell_renderer_break_icon_new (void);

G_END_DECLS

#endif /* __CELL_RENDERER_BREAK_ICON_H__ */
