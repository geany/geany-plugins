/*
 * test-stubs.h - this file is part of XMLSnippets, a Geany plugin
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

#ifdef TEST
#ifndef XMLSNIPPETS_TEST_STUBS_H
#define XMLSNIPPETS_TEST_STUBS_H

#include <glib.h>

#define GeanyEditor void

extern GHashTable *completions;

void test_stubs_init(void);

const gchar *editor_find_snippet(GeanyEditor *editor, const gchar *snippet_name);

const gchar *utils_find_open_xml_tag_pos(const gchar sel[], gint size);

void test_stubs_finalize(void);

#endif
#endif
