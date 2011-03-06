/*
 * main-notebook.h - Part of the Geany Devhelp Plugin
 * 
 * Copyright 2010 Matthew Brush <mbrush@leftclick.ca>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef MAIN_NOTEBOOK_H
#define MAIN_NOTEBOOK_H

#include <gtk/gtk.h>

/* 
 * When you need the main_notebook, you call main_notebook_get() which 
 * will get an existing main_notebook or create one if it doesn't exist.  
 * When you are done with the main_notebook, call main_notebook_destroy() 
 * and it will be destroyed and the UI will be put back to normal only 
 * if no other plugins are still using it.
 * 
 * See main-notebook.c for documentation for these functions
 */

gboolean main_notebook_exists(void);
GtkWidget *main_notebook_get(void);
gboolean main_notebook_needs_destroying(void);
void main_notebook_destroy(void);

#endif
