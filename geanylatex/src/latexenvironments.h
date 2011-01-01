/*
 *      latexenvironments.h
 *
 *      Copyright 2009-2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

#ifndef LATEXENVIRONMENTS_H
#define LATEXENVIRONMENTS_H

#include "geanylatex.h"
#include <string.h>

enum {
    ENVIRONMENT_CAT_DUMMY = 0,
    ENVIRONMENT_CAT_FORMAT,
    ENVIRONMENT_CAT_STRUCTURE,
    ENVIRONMENT_CAT_LISTS,
    ENVIRONMENT_CAT_MATH,
    ENVIRONMENT_CAT_MAX
};

enum {
    GLATEX_LIST_DESCRIPTION = 0,
    GLATEX_LIST_ENUMERATE,
    GLATEX_LIST_ITEMIZE,
    GLATEX_LIST_END
};

enum {
    GLATEX_ENVIRONMENT_TYPE_NONE = 0,
    GLATEX_ENVIRONMENT_TYPE_LIST
};

extern SubMenuTemplate glatex_environment_array[];

extern CategoryName glatex_environment_cat_names[];

void glatex_insert_environment(const gchar *environment, gint type);

void
glatex_insert_environment_dialog(G_GNUC_UNUSED GtkMenuItem *menuitem,
                                 G_GNUC_UNUSED gpointer gdata);

void
glatex_environment_insert_activated (G_GNUC_UNUSED GtkMenuItem *menuitem,
                              		 G_GNUC_UNUSED gpointer gdata);

void glatex_insert_list_environment(gint type);
#endif
