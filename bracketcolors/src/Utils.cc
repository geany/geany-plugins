/*
 *      Utils.cc
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


/* --------------------------------- INCLUDES ------------------------------- */

#include "Utils.h"

/* ------------------------------ IMPLEMENTATION ---------------------------- */


// -----------------------------------------------------------------------------
    gboolean utils_is_dark(guint32 color)

/*

----------------------------------------------------------------------------- */
{
    guint8 b = color >> 16;
    guint8 g = color >> 8;
    guint8 r = color;

    // https://stackoverflow.com/questions/596216/formula-to-determine-perceived-brightness-of-rgb-color
    guint8 y  = ((r << 1) + r + (g << 2) + b) >> 3;

    if (y < 125) {
        return TRUE;
    }

    return FALSE;
}



// -----------------------------------------------------------------------------
    gboolean utils_parse_color(
        const gchar *spec,
        GdkColor *color
    )
/*

----------------------------------------------------------------------------- */
{
    gchar buf[64] = {0};

    g_return_val_if_fail(spec != NULL, -1);

    if (spec[0] == '0' && (spec[1] == 'x' || spec[1] == 'X'))
    {
        /* convert to # format for GDK to understand it */
        buf[0] = '#';
        strncpy(buf + 1, spec + 2, sizeof(buf) - 2);
        spec = buf;
    }

    return gdk_color_parse(spec, color);
}



// -----------------------------------------------------------------------------
    gint utils_color_to_bgr(const GdkColor *c)
/*

----------------------------------------------------------------------------- */
{
    g_return_val_if_fail(c != NULL, -1);
    return (c->red / 256) | ((c->green / 256) << 8) | ((c->blue / 256) << 16);
}



// -----------------------------------------------------------------------------
    gint utils_parse_color_to_bgr(const gchar *spec)
/*

----------------------------------------------------------------------------- */
{
    GdkColor color;
    if (utils_parse_color(spec, &color)) {
        return utils_color_to_bgr(&color);
    }
    else {
        return -1;
    }
}
