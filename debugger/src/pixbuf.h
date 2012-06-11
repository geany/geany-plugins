/*
 *      pixbufs.h
 *      
 *      Copyright 2011 Alexander Petukhov <devel(at)apetukhov.ru>
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

#ifndef PIXBUF_H
#define PIXBUF_H

#include <gdk-pixbuf/gdk-pixbuf.h>

extern GdkPixbuf *break_pixbuf;
extern GdkPixbuf *break_disabled_pixbuf;
extern GdkPixbuf *break_condition_pixbuf;

extern GdkPixbuf *argument_pixbuf;
extern GdkPixbuf *local_pixbuf;
extern GdkPixbuf *watch_pixbuf;

extern GdkPixbuf *frame_pixbuf;
extern GdkPixbuf *frame_current_pixbuf;
 
void pixbufs_init(void);
void pixbufs_destroy(void);

#endif /* guard */
