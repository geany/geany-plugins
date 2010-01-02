/*
 * gdb-lex.h - A GLib-based parser for GNU debugger machine interface output.
 * Copyright 2008 Jeff Pohlmeyer <yetanothergeek(at)gmail(dot)com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */




/*
  Simply put, the GDB/MI output is parsed into one of three types:
  anything inside quotes is a string; anything inside curly brackets
  is a hash table; and anything inside square braces is a list.
  In some cases list elements are produced by GDB as key-value pairs
  with duplicate key names for each element. For these cases the
  key names are safely and silently ignored, only the values are kept.
*/


typedef enum
{ vt_STRING, vt_HASH, vt_LIST } GdbLxValueType;

typedef struct
{
	GdbLxValueType type;
	union
	{
		gpointer data;
		gchar *string;
		GHashTable *hash;
		GSList *list;
	};
} GdbLxValue;



GHashTable *gdblx_parse_results(gchar * resutls);

/*
  The gdblx_lookup_* functions below return NULL if their hash is NULL,
  if the key is not found, or if its value is not of the expected
  return type.
*/
gchar *gdblx_lookup_string(GHashTable * hash, gchar * key);
GHashTable *gdblx_lookup_hash(GHashTable * hash, gchar * key);
GSList *gdblx_lookup_list(GHashTable * hash, gchar * key);

/*
  Returns TRUE if hash is not NULL, key exists, and key type is vt_STRING,
  and the key's value matches 'expected'.
*/
gboolean gdblx_check_keyval(GHashTable * hash, gchar * key, gchar * expected);


/* Dumps a pretty-printed representation of the hash table to stderr */
void gdblx_dump_table(GHashTable * hash);



/*
  The global scanner object is automatically intialized as soon
  as it is needed, but it must be explicitly destroyed.
*/
void gdblx_scanner_done();
