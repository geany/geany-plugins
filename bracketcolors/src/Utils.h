/*
 *      Utils.h
 *
 *      Copyright 2023 Asif Amin <asifamin@utexas.edu>
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

/* --------------------------------- INCLUDES ------------------------------- */

#include <array>
#include <string>

#include <glib.h>
#include <gdk/gdk.h>

#define BC_NUM_COLORS 3

/* ----------------------------------- TYPES -------------------------------- */

    typedef std::array<std::string, BC_NUM_COLORS> BracketColorArray;

/* --------------------------------- CONSTANTS ------------------------------ */

    /*
     * These were copied from VS Code
     */

    const BracketColorArray sDarkBackgroundColors = {
        "#FF00FF", "#FFFF00", "#00FFFF"
    };

    const BracketColorArray sLightBackgroundColors = {
        "#008000", "#000080", "#800000"
    };

/* --------------------------------- PROTOTYPES ----------------------------- */

    gboolean utils_is_dark(guint32 color);

    gboolean utils_parse_color(const gchar *spec, GdkColor *color);

    gint utils_color_to_bgr(const GdkColor *c);

    gint utils_parse_color_to_bgr(const gchar *spec);

#endif
