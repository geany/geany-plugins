/*
 * xmlsnippets.c - this file is part of XMLSnippets, a Geany plugin
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "xmlsnippets.h"
#include <ctype.h>
#include <string.h>


typedef struct Info
{
	struct
	{
		const gchar *completion;
		const gchar *tag_name_start; /* start of the first tag within `completion' */
	} snippet;
	struct
	{
		const gchar *tag_name_end;
	} input;
} Info;


static const gchar * skip_xml_tag_name(const gchar * start)
{
	const gchar *iter;

	for (iter = start; strchr(":_-.", *iter) || isalnum(*iter); iter++) ;
	return iter;
}


/* If user entered some attributes, copy them to the first tag within snippet body
 * @return   Newly allocated completion string or @c NULL to indicate
 *           that completion should be aborted
 */
static gchar * merge_attributes(const gchar *sel, gint size, const Info * info)
{
	const gchar *input_iter, *input_iter_end, *iter;

	/* Separately ensure there is at least one space
	 * Needed for the code below which copy attributes */
	input_iter = info->input.tag_name_end;
	if (!isspace(*input_iter))
		goto normal; /* nothing to do */

	input_iter++;
	while (isspace(*input_iter))
		input_iter++;
	if (*input_iter == '>')
		goto normal; /* nothing to do */

	/* Find the end of the attribute string */
	g_assert(sel[size-1] == '>');
	input_iter_end = &sel[size-2];
	while (isspace(*input_iter_end))
		input_iter_end--;
	input_iter_end++;

	/* Ensure the tag within snippet body does not contain attributes */
	iter = info->snippet.tag_name_start;
	iter = skip_xml_tag_name(iter);
	if (*iter != '>')
	{
		g_message("%s",
			"Autocompletion aborted: both of the input string and "
			"the first tag of the snippet body contain attributes");
		goto abort;
	}

	/* Merge */
	{
		GString *completion = g_string_sized_new(20);
		g_string_append_len(completion, info->snippet.completion, iter - info->snippet.completion);

		input_iter -= 1; /* leave one space */
		for (; input_iter != input_iter_end; ++input_iter)
		{
			switch (*input_iter)
			{
				case '{':
					g_string_append(completion, "{ob}");
					break;
				case '}':
					g_string_append(completion, "{cb}");
					break;
				case '%':
					g_string_append(completion, "{pc}");
					break;
				default:
					g_string_append_c(completion, *input_iter);
			}
		}

		g_string_append(completion, iter);
		return g_string_free(completion, FALSE);
	}

	abort:
		return NULL;
	normal:
		return g_strdup(info->snippet.completion);
}


gboolean get_completion(GeanyEditor *editor, const gchar *sel, const gint size,
	CompletionInfo * c, InputInfo * i)
{
	Info info;
	const gchar *str_found, *tagname, *input_iter, *completion, *iter;
	gchar *completion_result;

	g_return_val_if_fail(sel[size-1] == '>', FALSE);
	if (size < 3)
		return FALSE;
	if (sel[size - 2] == '/')
		return FALSE; /* closing tag */

	str_found = utils_find_open_xml_tag_pos(sel, size);
	if (str_found == NULL)
		return FALSE;

	tagname = str_found + 1; /* skip the left bracket */
	input_iter = skip_xml_tag_name(tagname);
	if (input_iter == tagname)
		return FALSE;

	tagname = g_strndup(tagname, input_iter - tagname);
	completion = editor_find_snippet(editor, tagname);
	g_free((gchar *)tagname);
	if (completion == NULL)
		return FALSE;

	/* To prevent insertion of a snippet, which user intended to use,
	 * e.g., in JavaScript, ensure the snippet body starts with a tag.
	 * We don't prevent name clashes by using special snippet names
	 * as it won't allow the plugin to automatically catch up the
	 * snippets supplied with Geany, e.g. "table". */
	iter = completion;
	while (TRUE)
	{
		if (isspace(*iter))
			iter++;
		else if (*iter == '\\' && (*(iter+1) == 'n' || *(iter+1) == 't'))
			iter += 2;
		else
			break;
	}
	if (*iter != '<')
		return FALSE;

	info.snippet.completion = completion;
	info.snippet.tag_name_start = iter+1; /* +1: skip the left bracket */
	info.input.tag_name_end = input_iter;

	completion_result = merge_attributes(sel, size, &info);
	if (completion_result == NULL)
		return FALSE;

	c->completion = completion_result;
	i->tag_start = str_found - sel;
	return TRUE;
}
