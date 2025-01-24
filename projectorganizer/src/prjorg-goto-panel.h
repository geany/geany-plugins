/*
 * Copyright 2023 Jiri Techet <techet@gmail.com>
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

#ifndef PRJORG_GOTO_PANEL_H
#define PRJORG_GOTO_PANEL_H 1

#include <geanyplugin.h>


typedef struct
{
	gchar *name;
	gchar *file_name;
	gchar *scope;
	gchar *tooltip;
	gint line;
	TMIcon icon;
} PrjorgGotoSymbol;

void prjorg_goto_symbol_free(PrjorgGotoSymbol *symbol);


typedef void (*PrjorgGotoPanelLookupFunction) (const char *);

void prjorg_goto_panel_show(const gchar *query, PrjorgGotoPanelLookupFunction func);
void prjorg_goto_panel_fill(GPtrArray *symbols);
GPtrArray *prjorg_goto_panel_filter(GPtrArray *symbols, const gchar *filter);

#endif  /* PRJORG_GOTO_PANEL_H */
