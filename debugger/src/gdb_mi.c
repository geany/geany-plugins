/*
 *      gdb_mi.c
 *      
 *      Copyright 2014 Colomban Wendling <colomban@geany.org>
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
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/* 
 * Parses GDB/MI records
 * https://sourceware.org/gdb/current/onlinedocs/gdb/GDB_002fMI-Output-Syntax.html
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "gdb_mi.h"


#define ascii_isodigit(c) (((guchar) (c)) >= '0' && ((guchar) (c)) <= '7')


static struct gdb_mi_value *parse_value(const gchar **p);


void gdb_mi_value_free(struct gdb_mi_value *val)
{
	if (! val)
		return;
	switch (val->type)
	{
		case GDB_MI_VAL_STRING:
			g_free(val->string);
			g_warn_if_fail(val->list == NULL);
			break;

		case GDB_MI_VAL_LIST:
			gdb_mi_result_free(val->list, TRUE);
			g_warn_if_fail(val->string == NULL);
			break;
	}
	g_free(val);
}

void gdb_mi_result_free(struct gdb_mi_result *res, gboolean next)
{
	if (! res)
		return;
	g_free(res->var);
	gdb_mi_value_free(res->val);
	if (next)
		gdb_mi_result_free(res->next, next);
	g_free(res);
}

void gdb_mi_record_free(struct gdb_mi_record *record)
{
	if (! record)
		return;
	g_free(record->token);
	g_free(record->klass);
	gdb_mi_result_free(record->first, TRUE);
	g_free(record);
}

/* parses: cstring
 * 
 * cstring is defined as:
 * 
 * c-string ==>
 *     """ seven-bit-iso-c-string-content """ 
 * 
 * FIXME: what exactly does "seven-bit-iso-c-string-content" mean?
 *        reading between the lines suggests it's US-ASCII with values >= 0x80
 *        encoded as \NNN (most likely octal), but that's not really clear --
 *        although it parses everything I encountered
 * FIXME: this does NOT convert to UTF-8.  should it? */
static gchar *parse_cstring(const gchar **p)
{
	GString *str = g_string_new(NULL);

	if (**p == '"')
	{
		(*p)++;
		while (**p != '"')
		{
			gchar c = **p;
			/* TODO: check expansions here */
			if (c == '\\')
			{
				(*p)++;
				c = **p;
				switch (g_ascii_tolower(c))
				{
					case '\\':
					case '"': break;
					case 'a': c = '\a'; break;
					case 'b': c = '\b'; break;
					case 'f': c = '\f'; break;
					case 'n': c = '\n'; break;
					case 'r': c = '\r'; break;
					case 't': c = '\t'; break;
					case 'v': c = '\v'; break;
					default:
						/* hex escape, 1-2 digits (\xN or \xNN)
						 * 
						 * FIXME: is this useful?  Is this right?
						 * the original dbm_gdb.c:unescape_hex_values() used to
						 * read escapes of the form \xNNN and treat them as wide
						 * characters numbers, but  this looks weird for a C-like
						 * escape.
						 * Also, note that this doesn't seem to be referenced anywhere
						 * in GDB/MI syntax.  Only reference in GDB manual is about
						 * keybindings, which use the syntax implemented here */
						if (g_ascii_tolower(**p) == 'x' && g_ascii_isxdigit((*p)[1]))
						{
							c = (gchar) g_ascii_xdigit_value(*++(*p));
							if (g_ascii_isxdigit((*p)[1]))
								c = (gchar) ((c * 16) + g_ascii_xdigit_value(*++(*p)));
						}
						/* octal escape, 1-3 digits (\N, \NN or \NNN) */
						else if (ascii_isodigit(**p))
						{
							int i, v;
							v = g_ascii_digit_value(**p);
							for (i = 0; ascii_isodigit((*p)[1]) && i < 2; i++)
								v = (v * 8) + g_ascii_digit_value(*++(*p));
							if (v <= 0xff)
								c = (gchar) v;
							else
							{
								*p = *p - 3; /* put the whole sequence back */
								c = **p;
								g_warning("Octal escape sequence out of range: %.4s", *p);
							}
						}
						else
						{
							g_warning("Unkown escape \"\\%c\"", **p);
							(*p)--; /* put the \ back */
							c = **p;
						}
						break;
				}
			}
			if (**p == '\0')
				break;
			g_string_append_c(str, c);
			(*p)++;
		}
		if (**p == '"')
			(*p)++;
	}
	return g_string_free(str, FALSE);
}

/* parses: string
 * FIXME: what really is a string?  here it uses [a-zA-Z_-.][a-zA-Z0-9_-.]* but
 *        the docs aren't clear on this */
static gchar *parse_string(const gchar **p)
{
	GString *str = g_string_new(NULL);

	if (g_ascii_isalpha(**p) || strchr("-_.", **p))
	{
		g_string_append_c(str, **p);
		for ((*p)++; g_ascii_isalnum(**p) || strchr("-_.", **p); (*p)++)
			g_string_append_c(str, **p);
	}
	return g_string_free(str, FALSE);
}

/* parses: string "=" value */
static gboolean parse_result(struct gdb_mi_result *result, const gchar **p)
{
	result->var = parse_string(p);
	while (g_ascii_isspace(**p)) (*p)++;
	if (**p == '=')
	{
		(*p)++;
		while (g_ascii_isspace(**p)) (*p)++;
		result->val = parse_value(p);
	}
	return result->var && result->val;
}

/* parses: cstring | list | tuple
 * Actually, this is more permissive and allows mixed tuples/lists */
static struct gdb_mi_value *parse_value(const gchar **p)
{
	struct gdb_mi_value *val = g_malloc0(sizeof *val);
	if (**p == '"')
	{
		val->type = GDB_MI_VAL_STRING;
		val->string = parse_cstring(p);
	}
	else if (**p == '{' || **p == '[')
	{
		struct gdb_mi_result *prev = NULL;
		val->type = GDB_MI_VAL_LIST;
		gchar end = **p == '{' ? '}' : ']';
		(*p)++;
		while (**p && **p != end)
		{
			struct gdb_mi_result *item = g_malloc0(sizeof *item);
			while (g_ascii_isspace(**p)) (*p)++;
			if ((item->val = parse_value(p)) ||
				parse_result(item, p))
			{
				if (prev)
					prev->next = item;
				else
					val->list = item;
				prev = item;
			}
			else
			{
				gdb_mi_result_free(item, TRUE);
				break;
			}
			while (g_ascii_isspace(**p)) (*p)++;
			if (**p != ',') break;
			(*p)++;
		}
		if (**p == end)
			(*p)++;
	}
	else
	{
		gdb_mi_value_free(val);
		val = NULL;
	}
	return val;
}

static gboolean is_prompt(const gchar *p)
{
	if (strncmp("(gdb)", p, 5) == 0)
	{
		p += 5;
		while (g_ascii_isspace(*p)) p++;
	}
	return *p == 0;
}

/* parses: async-record | stream-record | result-record
 * note: post-value data is ignored.
 * 
 * FIXME: that's NOT exactly what the GDB docs call an output, and that's not
 *        exactly what a line could be.  The GDB docs state that a line can
 *        contain more than one stream-record, as it's not terminated by a
 *        newline, and as it defines:
 * 
 *        output ==> 
 *            ( out-of-band-record )* [ result-record ] "(gdb)" nl
 *        out-of-band-record ==>
 *            async-record | stream-record
 *        stream-record ==>
 *            console-stream-output | target-stream-output | log-stream-output
 *        console-stream-output ==>
 *            "~" c-string
 *        target-stream-output ==>
 *            "@" c-string
 *        log-stream-output ==>
 *            "&" c-string
 * 
 *        so as none of the stream-outputs are terminated by a newline, and the
 *        parser here only extracts the first record it will fail with combined
 *        records in one line.
 */
struct gdb_mi_record *gdb_mi_record_parse(const gchar *line)
{
	struct gdb_mi_record *record = g_malloc0(sizeof *record);

	/* FIXME: prompt detection should not really be useful, especially not as a
	 * special case, as the prompt should always follow an (optional) record */
	if (is_prompt(line))
		record->type = GDB_MI_TYPE_PROMPT;
	else
	{
		/* extract token */
		const gchar *token_end = line;
		for (token_end = line; g_ascii_isdigit(*token_end); token_end++)
			;
		if (token_end > line)
		{
			record->token = g_strndup(line, (gsize)(token_end - line));
			line = token_end;
			while (g_ascii_isspace(*line)) line++;
		}

		/* extract record */
		record->type = *line;
		if (*line) ++line;
		while (g_ascii_isspace(*line)) line++;
		switch (record->type)
		{
			case '~':
			case '@':
			case '&':
				/* FIXME: although the syntax description in the docs are clear,
				 * the "GDB/MI Stream Records" section does not agree with it,
				 * widening the input to:
				 * 
				 * > [string-output] is either raw text (with an implicit new
				 * > line) or a quoted C string (which does not contain an
				 * > implicit newline).
				 * 
				 * This adds "raw text" to "c-string"... so? */
				record->klass = parse_cstring(&line);
				break;
			case '^':
			case '*':
			case '+':
			case '=':
			{
				struct gdb_mi_result *prev = NULL;
				record->klass = parse_string(&line);
				while (*line)
				{
					while (g_ascii_isspace(*line)) line++;
					if (*line != ',')
						break;
					else
					{
						struct gdb_mi_result *res = g_malloc0(sizeof *res);
						line++;
						while (g_ascii_isspace(*line)) line++;
						if (!parse_result(res, &line))
						{
							g_warning("failed to parse result");
							gdb_mi_result_free(res, TRUE);
							break;
						}
						if (prev)
							prev->next = res;
						else
							record->first = res;
						prev = res;
					}
				}
				break;
			}
			default:
				/* FIXME: what to do with invalid prefix? */
				record->type = GDB_MI_TYPE_PROMPT;
		}
	}

	return record;
}

/* Extracts a variable value from a result
 * @res may be NULL */
static const struct gdb_mi_value *gdb_mi_result_var_value(const struct gdb_mi_result *result, const gchar *name)
{
	g_return_val_if_fail(name != NULL, NULL);

	for (; result; result = result->next)
	{
		if (result->var && strcmp(result->var, name) == 0)
			return result->val;
	}
	return NULL;
}

/* Extracts a variable value from a record
 * @param res a first result, or NULL
 * @param name the variable name
 * @param type the expected type of the value
 * @returns the value of @p name variable (type depending on @p type), or NULL
 */
const void *gdb_mi_result_var(const struct gdb_mi_result *result, const gchar *name, enum gdb_mi_value_type type)
{
	const struct gdb_mi_value *val = gdb_mi_result_var_value(result, name);
	if (! val || val->type != type)
		return NULL;
	else if (val->type == GDB_MI_VAL_STRING)
		return val->string;
	else if (val->type == GDB_MI_VAL_LIST)
		return val->list;
	return NULL;
}

/* checks whether a record matches, possibly including some string values
 * @param record a record
 * @param type the expected type of the record
 * @param klass the expected class of the record
 * @param ... a NULL-terminated name/return location pairs for string results
 * @returns TRUE if record matched, FALSE otherwise
 * 
 * Usage example
 * @{
 *     const gchar *id;
 *     if (gdb_mi_record_matches(record, '=', 'thread-created', "id", &id, NULL))
 *         // here record matched and `id` is present and a string
 * @}
 */
gboolean gdb_mi_record_matches(const struct gdb_mi_record *record, enum gdb_mi_record_type type, const gchar *klass, ...)
{
	va_list ap;
	const gchar *name;
	gboolean success = TRUE;

	g_return_val_if_fail(record != NULL, FALSE);

	if (record->type != type || strcmp(record->klass, klass) != 0)
		return FALSE;

	va_start(ap, klass);
	while ((name = va_arg(ap, const gchar *)) != NULL && success)
	{
		const gchar **out = va_arg(ap, const gchar **);

		g_return_val_if_fail(out != NULL, FALSE);

		*out = gdb_mi_result_var(record->first, name, GDB_MI_VAL_STRING);
		success = *out != NULL;
	}
	va_end(ap);
	return success;
}


#ifdef TEST

static void gdb_mi_result_dump(const struct gdb_mi_result *r, gboolean next, gint indent);

static void gdb_mi_value_dump(const struct gdb_mi_value *v, gint indent)
{
	fprintf(stderr, "%*stype = %d\n", indent * 2, "", v->type);
	switch (v->type)
	{
		case GDB_MI_VAL_STRING:
			fprintf(stderr, "%*sstring = %s\n", indent * 2, "", v->string);
			break;
		case GDB_MI_VAL_LIST:
			fprintf(stderr, "%*slist =>\n", indent * 2, "");
			if (v->list)
				gdb_mi_result_dump(v->list, TRUE, indent + 1);
			break;
	}
}

static void gdb_mi_result_dump(const struct gdb_mi_result *r, gboolean next, gint indent)
{
	fprintf(stderr, "%*svar = %s\n", indent * 2, "", r->var);
	fprintf(stderr, "%*sval =>\n", indent * 2, "");
	gdb_mi_value_dump(r->val, indent + 1);
	if (next && r->next)
		gdb_mi_result_dump(r->next, next, indent);
}

static void gdb_mi_record_dump(const struct gdb_mi_record *record)
{
	fprintf(stderr, "record =>\n");
	fprintf(stderr, "  type = '%c' (%d)\n", record->type ? record->type : '0', record->type);
	fprintf(stderr, "  token = %s\n", record->token);
	fprintf(stderr, "  class = %s\n", record->klass);
	fprintf(stderr, "  results =>\n");
	if (record->first)
		gdb_mi_result_dump(record->first, TRUE, 2);
}

int main(void)
{
	char buf[1024] = {0};

	while (fgets(buf, sizeof buf, stdin))
	{
		struct gdb_mi_record *record = gdb_mi_record_parse(buf);

		gdb_mi_record_dump(record);
		gdb_mi_record_free(record);
	}
	return 0;
}

#endif
