/*
 * test-stubs.c - this file is part of XMLSnippets, a Geany plugin
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "test-stubs.h"
#include <ctype.h>


GHashTable *completions;

void test_stubs_init(void)
{
  completions = g_hash_table_new(g_str_hash, g_str_equal);
}

void test_stubs_finalize(void)
{
  g_hash_table_destroy(completions);
}

const gchar *editor_find_snippet(GeanyEditor *editor, const gchar *snippet_name)
{
  return (const gchar *)g_hash_table_lookup(completions, snippet_name);
}


/** Searches backward through @a size bytes looking for a '<'.
 * @param sel .
 * @param size .
 * @return pointer to '<' of the found opening tag within @a sel, or @c NULL if no opening tag was found.
 */
const gchar *utils_find_open_xml_tag_pos(const gchar sel[], gint size)
{
	/* stolen from anjuta and modified */
	const gchar *begin, *cur;

	if (G_UNLIKELY(size < 3))
	{	/* Smallest tag is "<p>" which is 3 characters */
		return NULL;
	}
	begin = &sel[0];
	cur = &sel[size - 1];

	/* Skip to the character before the closing brace */
	while (cur > begin)
	{
		if (*cur == '>')
			break;
		--cur;
	}
	--cur;
	/* skip whitespace */
	while (cur > begin && isspace(*cur))
		cur--;
	if (*cur == '/')
		return NULL; /* we found a short tag which doesn't need to be closed */
	while (cur > begin)
	{
		if (*cur == '<')
			break;
		/* exit immediately if such non-valid XML/HTML is detected, e.g. "<script>if a >" */
		else if (*cur == '>')
			break;
		--cur;
	}

	/* if the found tag is an opening, not a closing tag or empty <> */
	if (*cur == '<' && *(cur + 1) != '/' && *(cur + 1) != '>')
		return cur;

	return NULL;
}

#endif
