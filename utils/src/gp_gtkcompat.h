/*
 * Copyright 2017 LarsGit223
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* Compatibility macros to support different GTK versions */

#ifndef GP_GTKCOMPAT_H
#define GP_GTKCOMPAT_H

G_BEGIN_DECLS

/* Remove gtk_window_set_has_resize_grip() starting from version 3.14 */
#if GTK_CHECK_VERSION(3, 14, 0)
#define gtk_window_set_has_resize_grip(window, value)
#endif

/* Replace calls to gtk_widget_set_state() with call to
   gtk_widget_set_state_flags() and translate States to State-Flags.
   Starting from version 3.0.*/
#if GTK_CHECK_VERSION(3, 0, 0)
#define GTK_STATE_NORMAL       GTK_STATE_FLAG_NORMAL
#define GTK_STATE_ACTIVE       GTK_STATE_FLAG_ACTIVE
#define GTK_STATE_PRELIGHT     GTK_STATE_FLAG_PRELIGHT
#define GTK_STATE_SELECTED     GTK_STATE_FLAG_SELECTED
#define GTK_STATE_INSENSITIVE  GTK_STATE_FLAG_INSENSITIVE
#define GTK_STATE_INCONSISTENT GTK_STATE_FLAG_INCONSISTENT
#define GTK_STATE_FOCUSED      GTK_STATE_FLAG_FOCUSED
#define gtk_widget_set_state(widget, state) \
        gtk_widget_set_state_flags(widget, state, FALSE)
#endif

/* Replace some GTK_STOCK constants with labels.
   Add new ones on-demand. Starting from version 3.10 */
#if GTK_CHECK_VERSION(3, 10, 0)
#undef GTK_STOCK_OPEN
#undef GTK_STOCK_CANCEL
#define GTK_STOCK_OPEN   _("_Open")
#define GTK_STOCK_CANCEL _("_Cancel")
#endif
G_END_DECLS

#endif /* GP_GTKCOMPAT_H */

