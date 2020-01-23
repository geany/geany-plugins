/*
 *      cellrenderertoggle.h
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

#ifndef __CELL_RENDERER_TOGGLE_H__
#define __CELL_RENDERER_TOGGLE_H__

#include <gtk/gtkcellrenderertoggle.h>

G_BEGIN_DECLS

#define TYPE_CELL_RENDERER_TOGGLE				(cell_renderer_toggle_get_type ())
#define CELL_RENDERER_TOGGLE(obj)				(G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CELL_RENDERER_TOGGLE, CellRendererToggle))
#define CELL_RENDERER_TOGGLE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_CELL_RENDERER_TOGGLE, CellRendererToggleClass))
#define IS_CELL_RENDERER_TOGGLE(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CELL_RENDERER_TOGGLE))
#define IS_CELL_RENDERER_TOGGLE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_CELL_RENDERER_TOGGLE))
#define CELL_RENDERER_TOGGLE_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_CELL_RENDERER_TOGGLE, CellRendererToggleClass))

typedef struct _CellRendererToggle CellRendererToggle;
typedef struct _CellRendererToggleClass CellRendererToggleClass;

GType					cell_renderer_toggle_get_type(void);
GtkCellRenderer*		cell_renderer_toggle_new (void);

G_END_DECLS

#endif /* __CELL_RENDERER_TOGGLE_H__ */
