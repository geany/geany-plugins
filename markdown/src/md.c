/*
 * md.c - Part of the Geany Markdown plugin
 *
 * Copyright 2012 Matthew Brush <mbrush@codebrainz.ca>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <string.h>
#include <stdio.h>
#include "markdown.h"
#include "md.h"

gchar *markdown_to_html(const gchar *md_text)
{
  Document *md;
  gchar *result = NULL;
  
  if (!md_text)
    return g_strdup("");

  md = mkd_string(md_text, strlen(md_text), 0);
  if (md) {
    if (mkd_compile(md, 0)) {
      gchar *res = NULL;
      mkd_document(md, &res);
      result = g_strdup(res);
    }
    mkd_cleanup(md);
  }

  return result;
}
