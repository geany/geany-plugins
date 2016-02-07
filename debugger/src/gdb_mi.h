/*
 *      gdb_mi.h
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

#ifndef GDB_MI_H
#define GDB_MI_H

#include <glib.h>

enum gdb_mi_value_type
{
	GDB_MI_VAL_STRING,
	GDB_MI_VAL_LIST
};

struct gdb_mi_result;
struct gdb_mi_value
{
	enum gdb_mi_value_type type;
	union {
		gchar *string;
		struct gdb_mi_result *list;
	} v;
};

struct gdb_mi_result
{
	gchar *var;
	struct gdb_mi_value *val;
	struct gdb_mi_result *next;
};

enum gdb_mi_record_type
{
	GDB_MI_TYPE_PROMPT = 0,
	GDB_MI_TYPE_RESULT = '^',
	GDB_MI_TYPE_EXEC_ASYNC = '*',
	GDB_MI_TYPE_STATUS_ASYNC = '+',
	GDB_MI_TYPE_NOTIFY_ASYNC = '=',
	GDB_MI_TYPE_CONSOLE_STREAM = '~',
	GDB_MI_TYPE_TARGET_STREAM = '@',
	GDB_MI_TYPE_LOG_STREAM = '&'
};

struct gdb_mi_record
{
	enum gdb_mi_record_type type;
	gchar *token;
	gchar *klass; /*< contains the async record class or the stream output */
	struct gdb_mi_result *first; /*< pointer to the first result (if any) */
};


void gdb_mi_value_free(struct gdb_mi_value *val);
void gdb_mi_result_free(struct gdb_mi_result *res, gboolean next);
void gdb_mi_record_free(struct gdb_mi_record *record);
struct gdb_mi_record *gdb_mi_record_parse(const gchar *line);
const void *gdb_mi_result_var(const struct gdb_mi_result *result, const gchar *name, enum gdb_mi_value_type type);
gboolean gdb_mi_record_matches(const struct gdb_mi_record *record, enum gdb_mi_record_type type, const gchar *klass, ...) G_GNUC_NULL_TERMINATED;

#define gdb_mi_result_foreach(node_, result_) \
	for ((node_) = (result_); (node_); (node_) = (node_)->next)

#define gdb_mi_result_foreach_matched(node_, result_, name_, type_) \
	gdb_mi_result_foreach ((node_), (result_)) \
		if (((name_) != NULL && (! (node_)->var || strcmp((node_)->var, (name_) ? (name_) : "") != 0)) || \
			((type_) >= 0 && (node_)->val->type != (type_))) \
			continue; \
		else

#endif /* guard */
