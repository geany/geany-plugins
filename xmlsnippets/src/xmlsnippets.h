/*
 * xmlsnippets.h - this file is part of XMLSnippets, a Geany plugin
 *
 * Copyright 2010 Eugene Arshinov <earshinov(at)gmail(dot)com>
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

#ifndef XMLSNIPPETS_XMLSNIPPETS_H
#define XMLSNIPPETS_XMLSNIPPETS_H

#ifdef TEST
  #include "test-stubs.h"
#else
  #include "plugin.h"
#endif
#include <glib.h>

typedef struct InputInfo
{
	gint tag_start;
} InputInfo;

typedef struct CompletionInfo
{
	gchar *completion;
} CompletionInfo;

gboolean get_completion(GeanyEditor *editor, const gchar *sel, const gint size,
  CompletionInfo * c, InputInfo * i);

#endif
