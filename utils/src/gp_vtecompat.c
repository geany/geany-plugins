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

#include <glib.h>
#include <glib/gstdio.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef G_OS_UNIX
#include <vte/vte.h>
#include <../../utils/src/gp_vtecompat.h>
#endif

/** Set font from string.
 *
 * Compatibility function to replace deprecated vte_terminal_set_font_from_string().
 *
 * @param vte  Pointer to VteTerminal
 * @param font Font specification as string
 *
 **/
void gp_vtecompat_set_font_from_string(VteTerminal *vte, char *font)
{
    PangoFontDescription *font_desc;

    font_desc = pango_font_description_from_string(font);
    vte_terminal_set_font(vte, font_desc);
    pango_font_description_free (font_desc);
}

