/*
 *      pixbufs.c
 *      
 *      Copyright 2010 Alexander Petukhov <devel(at)apetukhov.ru>
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
 * 		pixbuffers
 */

#include <gtk/gtk.h>

#include "xpm/breakpoint.xpm"
#include "xpm/breakpoint_disabled.xpm"
#include "xpm/breakpoint_condition.xpm"

#include "xpm/argument.xpm"
#include "xpm/local.xpm"
#include "xpm/watch.xpm"

#include "xpm/frame.xpm"
#include "xpm/frame_current.xpm"

GdkPixbuf *break_pixbuf = NULL;
GdkPixbuf *break_disabled_pixbuf = NULL;
GdkPixbuf *break_condition_pixbuf = NULL;

GdkPixbuf *argument_pixbuf = NULL;
GdkPixbuf *local_pixbuf = NULL;
GdkPixbuf *watch_pixbuf = NULL;
 
GdkPixbuf *frame_pixbuf = NULL;
GdkPixbuf *frame_current_pixbuf = NULL;

/*
 * create pixbuffers
 */
void pixbufs_init(void)
{
	break_pixbuf = gdk_pixbuf_new_from_xpm_data(breakpoint_xpm);
	break_disabled_pixbuf = gdk_pixbuf_new_from_xpm_data(breakpoint_disabled_xpm);
	break_condition_pixbuf = gdk_pixbuf_new_from_xpm_data(breakpoint_condition_xpm);

	argument_pixbuf = gdk_pixbuf_new_from_xpm_data(argument_xpm);
	local_pixbuf = gdk_pixbuf_new_from_xpm_data(local_xpm);
	watch_pixbuf = gdk_pixbuf_new_from_xpm_data(watch_xpm);

	frame_pixbuf = gdk_pixbuf_new_from_xpm_data(frame_xpm);
	frame_current_pixbuf = gdk_pixbuf_new_from_xpm_data(frame_current_xpm);
}

/*
 * free pixbuffers
 */
void pixbufs_destroy(void)
{
	g_object_unref(break_pixbuf);
	g_object_unref(break_disabled_pixbuf);
	g_object_unref(break_condition_pixbuf);

	g_object_unref(argument_pixbuf);
	g_object_unref(local_pixbuf);
	g_object_unref(watch_pixbuf);

	g_object_unref(frame_pixbuf);
	g_object_unref(frame_current_pixbuf);
}

