/*
 * tests.c - this file is part of XMLSnippets, a Geany plugin
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
#include "xmlsnippets.h"
#include <string.h>


static void init(void);
static void finalize(void);
static gboolean test(gint ordinal, const gchar *input, const gchar *completion_needed);
static void fill_completions(void);
static gboolean run_tests(void);


static void init(void)
{
  test_stubs_init();
  fill_completions();
}

static void finalize(void)
{
  test_stubs_finalize();
}


static gboolean test(gint ordinal, const gchar *input, const gchar *completion_needed)
{
	CompletionInfo c;
	InputInfo i;
	gboolean success;

	input = g_strconcat(input, ">", NULL);
	success = get_completion(NULL, (gchar *)input, strlen(input), &c, &i);
	g_free((gchar *)input);
	input = NULL;

	if (completion_needed && !success)
	{
		g_warning(
			"Test %d: FAIL\n"
			"Completion is expected, but did not occur",
			ordinal);
		return FALSE;
	}
	else if (!completion_needed && success)
	{
		g_warning(
			"Test %d: FAIL\n"
			"Unexpected completion returned: '%s'",
			ordinal, c.completion);
		g_free(c.completion);
		return FALSE;
	}
	else if (!completion_needed && !success)
		return TRUE;

	if (strcmp(c.completion, completion_needed) != 0)
	{
		g_warning(
			"Test %d: FAIL\n"
			"Expected completion do not match:\n"
			"got '%s'\n"
			"instead of '%s'",
			ordinal, c.completion, completion_needed);
		g_free(c.completion);
		return FALSE;
	}

	g_free(c.completion);
	return TRUE;
}


static void fill_completions(void)
{
  g_hash_table_insert(completions, "ai", "<a><img/></a>");
  g_hash_table_insert(completions, "empty", "");
  g_hash_table_insert(completions, "spaces", "  ");
  g_hash_table_insert(completions, "tagname", "a");
  g_hash_table_insert(completions, "rbrace", ">");
  g_hash_table_insert(completions, "closing", "</a>");
  g_hash_table_insert(completions, "closed", "<a/>");
  g_hash_table_insert(completions, "attr", "<a href='#'>");
  g_hash_table_insert(completions, "attr-closing", "</a href='#'>"); /* invalid HTML */
  g_hash_table_insert(completions, "attr-closed", "<a href='#' />");
  g_hash_table_insert(completions, "cursor", "<cursor%cursor%>");
}

static gboolean run_tests(void)
{
	gboolean success = TRUE;

	/* Correct snippet, various input */
	success = test(0, "",      NULL) && success;
	success = test(1, "a",     NULL) && success;
	success = test(2, "  ",    NULL) && success;
	success = test(3, "  <",   NULL) && success;
	success = test(4, " ai",   NULL) && success;
	success = test(5, "< ai",  NULL) && success;
	success = test(6, "</ai",  NULL) && success;
	success = test(7, "<ai/",  NULL) && success;
	success = test(8, "<ai /", NULL) && success;
	success = test(9, "<a",    NULL) && success;

	/* Correct snippet, excessive right bracket in source */
	success = test(10, ">",    NULL) && success;
	success = test(11, "<>",   NULL) && success;
	success = test(12, "<ai>", NULL) && success;

	/* Correct snippet, correct input */
	success = test(20, "<ai", "<a><img/></a>") && success;
	success = test(21, "<ai   ", "<a><img/></a>") && success;
	success = test(22, "<ai alt='...'", "<a alt='...'><img/></a>") && success;
	success = test(23, "<ai   alt='...' ", "<a alt='...'><img/></a>") && success;

	/* Various snippets, correct input */
	success = test(30, "<empty", NULL) && success;
	success = test(31, "<spaces", NULL) && success;
	success = test(32, "<tagname", NULL) && success;
	success = test(33, "<rbrace", NULL) && success;
		/* NOTE: We don't intentionally restrict users to not use closing/closed
		 * tags in the beginning of a snippet, so they are by default allowed */
	success = test(34, "<closing", "</a>") && success;
	success = test(35, "<closed", "<a/>") && success;

	/* Snippets with attributes, input without attributes */
	success = test(40, "<attr", "<a href='#'>") && success;
	success = test(41, "<attr-closing", "</a href='#'>") && success;
	success = test(42, "<attr-closed", "<a href='#' />") && success;

	/* Various snippets, input with attributes */
	success = test(50, "<closing x=x", NULL);
		/* DRAWBACK: We don't copy attributes to closed tag */
	success = test(51, "<closed x=x", NULL);
	success = test(52, "<attr x=x", NULL) && success;
	success = test(53, "<attr-closing x=x", NULL) && success;
	success = test(54, "<attr-closed x=x", NULL) && success;

	/* Handling of space around attributes */
	success = test(60, "<ai  alt='...'   href='#' ", "<a alt='...'   href='#'><img/></a>") && success;
		/* DRAWBACK: Code interprets spaces within the first tag of the snippet body
		 * as attributes and refuses to copy the attributes from input */
	success = test(61, "<ai-spaced  alt='...'   href='#' ", NULL) && success;

	/* Other tests */
		/* %cursor% in tag name is treated normally */
	success = test(70, "<cursor", "<cursor%cursor%>") && success;

	return success;
}


int main(int argc, char ** argv)
{
  init();
	gboolean success = run_tests();
  finalize();
  return success ? 0 : 1;
}

#endif
