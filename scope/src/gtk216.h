/*
 *  gtk216.h
 *
 *  Copyright 2012 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GTK216_H

#if !GLIB_CHECK_VERSION(2, 22, 0)
GArray *scope_array_sized_new(gboolean zero_terminated, gboolean clear, guint element_size,
	guint reserved_size);
#define g_array_sized_new(zero_terminated, clear, element_size, reserved_size) \
	scope_array_sized_new((zero_terminated), (clear), (element_size), (reserved_size))
#define g_array_new(zero_terminated, clear, elt_size) \
	g_array_sized_new((zero_terminated), (clear), (elt_size), 0)
guint scope_array_get_element_size(GArray *array);
#define g_array_get_element_size(array) scope_array_get_element_size(array)
gchar *scope_array_free(GArray *array, gboolean free_segment);
#define g_array_free(array, free_segment) scope_array_free((array), (free_segment))
#endif  /* GLIB 2.22.0 */

#if !GTK_CHECK_VERSION(2, 18, 0)
#define gtk_widget_get_visible(widget) GTK_WIDGET_VISIBLE(widget)
#define gtk_widget_get_sensitive(widget) GTK_WIDGET_SENSITIVE(widget)
void gtk_widget_set_visible(GtkWidget *widget, gboolean visible);
#endif

void gtk216_init(void);
void gtk216_finalize(void);

#define GTK216_H 1
#endif
