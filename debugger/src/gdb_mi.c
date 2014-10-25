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
 * http://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_214.html
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <glib.h>

#include "gdb_mi.h"

#define DEBUG

#define isodigit(c) ((c) >= '0' && (c) <= '7')


#if defined(DEBUG) || defined(TEST)
static void gdb_mi_result_dump(const struct gdb_mi_result *r, gboolean next, gint indent);
#endif
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

#if defined(DEBUG) || defined(TEST)

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

#endif

/* parses: cstring
 * 
 * cstring is defined as:
 * 
 * c-string ==>
 *     """ seven-bit-iso-c-string-content """ 
 * 
 * FIXME: what exactly does "seven-bit-iso-c-string-content" mean?
 *        reading between the lines suggests it's US-ASCII with values >= 0x80
 *        encoded as \NNN (most likely octal), but that's not really clear */
static gchar *parse_cstring(const gchar **p)
{
	GString *str = g_string_new(NULL);

	if (**p == '"')
	{
		(*p)++;
		while (**p != '"')
		{
			int c = **p;
			/* TODO: check expansions here */
			if (c == '\\')
			{
				(*p)++;
				c = **p;
				switch (tolower(c))
				{
					case '\\':
					case '"': break;
					case 'a': c = '\a'; break;
					case 'b': c = '\b'; break;
					case 'n': c = '\n'; break;
					case 'r': c = '\r'; break;
					case 't': c = '\t'; break;
					case 'v': c = '\v'; break;
					default:
						/* two-digit hex escape */
						if (tolower(c) == 'x' && isxdigit((*p)[1]) && isxdigit((*p)[2]))
						{
							c  = (tolower(*++(*p)) - '0') * 16;
							c += (tolower(*++(*p)) - '0');
						}
						/* three-digit octal escape */
						else if (c >= '0' && c <= '3' && isodigit((*p)[1]) && isodigit((*p)[2]))
						{
							c  = (*  (*p) - '0') * 8 * 8;
							c += (*++(*p) - '0') * 8;
							c += (*++(*p) - '0');
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
			g_string_append_c(str, (gchar) c);
			(*p)++;
		}
		if (**p == '"')
			(*p)++;
	}
	return g_string_free(str, FALSE);
}

/* parses: string
 * FIXME: what really is a string?  here it uses [a-zA-Z_][a-zA-Z0-9_-.]* but
 *        the docs aren't clear on this */
static gchar *parse_string(const gchar **p)
{
	GString *str = g_string_new(NULL);

	if (isalpha(**p) || **p == '_')
	{
		g_string_append_c(str, **p);
		for ((*p)++; isalnum(**p) || strchr("-_.", **p); (*p)++)
			g_string_append_c(str, **p);
	}
	return g_string_free(str, FALSE);
}

/* parses: string "=" value */
static gboolean parse_result(struct gdb_mi_result *result, const gchar **p)
{
	result->var = parse_string(p);
	while (isspace(**p)) (*p)++;
	if (**p == '=')
	{
		(*p)++;
		while (isspace(**p)) (*p)++;
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
			while (isspace(**p)) (*p)++;
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
			while (isspace(**p)) (*p)++;
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
	char nl;

#ifdef DEBUG
	fprintf(stderr, "line: %s\n", line);
#endif

	if (sscanf(line, "(gdb) %c", &nl) == 1 && (nl == '\r' || nl == '\n'))
		record->type = GDB_MI_TYPE_PROMPT;
	else
	{
		/* extract token */
		const gchar *token_end = line;
		for (token_end = line; isdigit(*token_end); token_end++)
			;
		if (token_end > line)
		{
			record->token = g_strndup(line, (gsize)(token_end - line));
			line = token_end;
			while (isspace(*line))
				line++;
		}

		/* extract record */
		record->type = *line;
		++line;
		while (isspace(*line)) line++;
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
				 * This adds "raw text" to "c-string", so? */
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
					while (isspace(*line)) line++;
					if (*line != ',')
						break;
					else
					{
						struct gdb_mi_result *res = g_malloc0(sizeof *res);
						line++;
						while (isspace(*line)) line++;
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

#ifdef DEBUG
	if (! (gdb_mi_record_matches(record, '^', "done", NULL) &&
		   gdb_mi_result_var(record->first, "files", GDB_MI_VAL_LIST)))
	gdb_mi_record_dump(record);
#endif

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

/* FIXME: */
const void *gdb_mi_result_var_path(const struct gdb_mi_result *result, const gchar *path, enum gdb_mi_value_type type)
{
	gchar **chunks = g_strsplit(path, "/", -1);
	gchar **p;
	void *value = NULL;

	for (p = chunks; *p; p++)
	{
		const struct gdb_mi_value *val = gdb_mi_result_var_value(result, *p);
		if (! val)
			break;
		if (! p[1])
		{
			if (val->type == type)
			{
				if (val->type == GDB_MI_VAL_STRING)
					value = val->string;
				else if (val->type == GDB_MI_VAL_LIST)
					value = val->list;
			}
		}
		else if (val->type == GDB_MI_VAL_LIST)
			result = val->list;
		else
			break;
	}
	g_strfreev(chunks);
	return value;
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

int main(void)
{
	char buf[256] = {0};

	while (fgets(buf, sizeof buf, stdin))
	{
		struct gdb_mi_record *record = gdb_mi_record_parse(buf);

		gdb_mi_record_dump(record);
		gdb_mi_record_free(record);
	}
	return 0;
}

#endif
