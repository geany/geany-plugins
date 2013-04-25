/* gtktreedatalist.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * scptreedata.c
 * Copyright 2013 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gtk/gtk.h>

#include "scptreedata.h"

#if !GLIB_CHECK_VERSION(2, 30, 0)
#define G_VALUE_INIT { 0, { { 0 } } }
#endif

GType scp_tree_data_get_fundamental_type(GType type)
{
	type = G_TYPE_FUNDAMENTAL(type);
	return G_UNLIKELY(type == G_TYPE_INTERFACE && g_type_is_a(type, G_TYPE_OBJECT))
		? G_TYPE_OBJECT : type;
}

void scp_tree_data_free(ScpTreeData *data, GType type)
{
	if (data->v_pointer)
	{
		switch (scp_tree_data_get_fundamental_type(type))
		{
			case G_TYPE_STRING  : g_free(data->v_string); break;
			case G_TYPE_OBJECT  : g_object_unref(data->v_pointer); break;
			case G_TYPE_BOXED   : g_boxed_free(type, data->v_pointer); break;
		#if GLIB_CHECK_VERSION(2, 26, 0)
			case G_TYPE_VARIANT : g_variant_unref(data->v_pointer);
		#endif
		}
	}
}

void scp_tree_data_warn_unsupported_type(const char *prefix, GType type)
{
	g_warning("%s: Unsupported type %s", prefix, g_type_name(type));
}

gboolean scp_tree_data_check_type(GType type)
{
	static const GType types[] =
	{
		G_TYPE_INT,
		G_TYPE_UINT,
		G_TYPE_STRING,
		G_TYPE_BOOLEAN,
		G_TYPE_LONG,
		G_TYPE_ULONG,
		G_TYPE_FLOAT,
		G_TYPE_DOUBLE,
		G_TYPE_CHAR,
		G_TYPE_UCHAR,
		G_TYPE_INT64,
		G_TYPE_UINT64,
		G_TYPE_ENUM,
		G_TYPE_FLAGS,
		G_TYPE_POINTER,
		G_TYPE_OBJECT,
		G_TYPE_BOXED,
	#if GLIB_CHECK_VERSION(2, 26, 0)
		G_TYPE_VARIANT,
	#endif
		G_TYPE_INVALID
	};

	gint i;
	type = scp_tree_data_get_fundamental_type(type);

	for (i = 0; types[i] != G_TYPE_INVALID; i++)
		if (types[i] == type)
			return TRUE;

	return FALSE;
}

void scp_tree_data_to_value(const ScpTreeData *data, GType type, GValue *value)
{
	g_value_init(value, type);

	switch (scp_tree_data_get_fundamental_type(type))
	{
		case G_TYPE_INT     : g_value_set_int(value, data->v_int); break;
		case G_TYPE_UINT    : g_value_set_uint(value, data->v_uint); break;
		case G_TYPE_STRING  : g_value_set_string(value, data->v_string); break;
		case G_TYPE_BOOLEAN : g_value_set_boolean(value, data->v_int); break;
		case G_TYPE_LONG    : g_value_set_long(value, data->v_long); break;
		case G_TYPE_ULONG   : g_value_set_ulong(value, data->v_ulong); break;
		case G_TYPE_FLOAT   : g_value_set_float(value, data->v_float); break;
		case G_TYPE_DOUBLE  : g_value_set_double(value, data->v_double); break;
	#if GLIB_CHECK_VERSION(2, 32, 0)
		case G_TYPE_CHAR    : g_value_set_schar(value, data->v_char); break;
	#else
		case G_TYPE_CHAR    : g_value_set_char(value, data->v_char); break;
	#endif
		case G_TYPE_UCHAR   : g_value_set_uchar(value, data->v_uchar); break;
		case G_TYPE_INT64   : g_value_set_int64(value, data->v_int64); break;
		case G_TYPE_UINT64  : g_value_set_uint64 (value, data->v_uint64); break;
		case G_TYPE_ENUM    : g_value_set_enum(value, data->v_int); break;
		case G_TYPE_FLAGS   : g_value_set_flags(value, data->v_uint); break;
		case G_TYPE_POINTER : g_value_set_pointer(value, data->v_pointer); break;
		case G_TYPE_OBJECT  : g_value_set_object(value, (GObject *) data->v_pointer); break;
		case G_TYPE_BOXED   : g_value_set_boxed(value, data->v_pointer); break;
	#if GLIB_CHECK_VERSION(2, 26, 0)
		case G_TYPE_VARIANT : g_value_set_variant(value, data->v_pointer); break;
	#endif
		default : scp_tree_data_warn_unsupported_type(G_STRFUNC, type);
	}
}

void scp_tree_data_to_pointer(const ScpTreeData *data, GType type, gpointer dest)
{
	switch (scp_tree_data_get_fundamental_type(type))
	{
		case G_TYPE_INT     :
		case G_TYPE_ENUM    : *(gint *) dest = data->v_int; break;
		case G_TYPE_UINT    :
		case G_TYPE_FLAGS   : *(guint *) dest = data->v_uint; break;
		case G_TYPE_BOOLEAN : *(gboolean *) dest = data->v_int != 0; break;
		case G_TYPE_LONG    : *(glong *) dest = data->v_long; break;
		case G_TYPE_ULONG   : *(gulong *) dest = data->v_ulong; break;
		case G_TYPE_FLOAT   : *(gfloat *) dest = data->v_float; break;
		case G_TYPE_DOUBLE  : *(gdouble *) dest = data->v_double; break;
		case G_TYPE_CHAR    : *(gint8 *) dest = data->v_char; break;
		case G_TYPE_UCHAR   : *(guint8 *) dest = data->v_uchar; break;
		case G_TYPE_INT64   : *(gint64 *) dest = data->v_int64; break;
		case G_TYPE_UINT64  : *(guint64 *) dest = data->v_uint64; break;
		case G_TYPE_STRING  : *(char **) dest = data->v_string; break;
		case G_TYPE_POINTER :
		case G_TYPE_OBJECT  :
	#if GLIB_CHECK_VERSION(2, 26, 0)
		case G_TYPE_VARIANT :
	#endif
		case G_TYPE_BOXED   : *(gpointer *) dest = data->v_pointer; break;
		default : scp_tree_data_warn_unsupported_type(G_STRFUNC, type);
	}
}

void scp_tree_data_from_value(ScpTreeData *data, GValue *value, gboolean copy)
{
	switch (scp_tree_data_get_fundamental_type(G_VALUE_TYPE(value)))
	{
		case G_TYPE_INT     : data->v_int = g_value_get_int(value); break;
		case G_TYPE_UINT    : data->v_uint = g_value_get_uint(value); break;
		case G_TYPE_STRING  :
		{
			data->v_string = copy ? g_value_dup_string(value) :
				(char *) g_value_get_string(value);
			break;
		}
		case G_TYPE_BOOLEAN : data->v_int = g_value_get_boolean(value); break;
		case G_TYPE_LONG    : data->v_long = g_value_get_long(value); break;
		case G_TYPE_ULONG   : data->v_ulong = g_value_get_ulong(value); break;
		case G_TYPE_FLOAT   : data->v_float = g_value_get_float(value); break;
		case G_TYPE_DOUBLE  : data->v_double = g_value_get_double(value); break;
	#if GLIB_CHECK_VERSION(2, 32, 0)
		case G_TYPE_CHAR    : data->v_char = g_value_get_schar(value); break;
	#else
		case G_TYPE_CHAR    : data->v_char = g_value_get_char(value); break;
	#endif
		case G_TYPE_UCHAR   : data->v_uchar = g_value_get_uchar(value); break;
		case G_TYPE_INT64   : data->v_int64 = g_value_get_int64(value); break;
		case G_TYPE_UINT64  : data->v_uint64 = g_value_get_uint64(value); break;
		case G_TYPE_ENUM    : data->v_int = g_value_get_enum(value); break;
		case G_TYPE_FLAGS   : data->v_uint = g_value_get_flags(value); break;
		case G_TYPE_POINTER : data->v_pointer = g_value_get_pointer(value); break;
		case G_TYPE_OBJECT  :
		{
			data->v_pointer = copy ? g_value_dup_object(value) :
				g_value_get_object(value);
			break;
		}
		case G_TYPE_BOXED :
		{
			data->v_pointer = copy ? g_value_dup_boxed(value) :
				g_value_get_boxed(value);
			break;
		}
	#if GLIB_CHECK_VERSION(2, 26, 0)
		case G_TYPE_VARIANT :
		{
			data->v_pointer = copy ? g_value_dup_variant(value) :
				g_value_get_variant(value);
			break;
		}
	#endif
		default : scp_tree_data_warn_unsupported_type(G_STRFUNC, G_VALUE_TYPE(value));
	}
}

void scp_tree_data_copy(const ScpTreeData *data, ScpTreeData *new_data, GType type)
{
	switch (scp_tree_data_get_fundamental_type(type))
	{
		case G_TYPE_INT     :
		case G_TYPE_UINT    :
		case G_TYPE_BOOLEAN :
		case G_TYPE_LONG    :
		case G_TYPE_ULONG   :
		case G_TYPE_FLOAT   :
		case G_TYPE_DOUBLE  :
		case G_TYPE_CHAR    :
		case G_TYPE_UCHAR   :
		case G_TYPE_INT64   :
		case G_TYPE_UINT64  :
		case G_TYPE_ENUM    :
		case G_TYPE_FLAGS   : *new_data = *data; break;
		default : scp_tree_data_assign_pointer(new_data, type, data->v_pointer, TRUE);
	}
}

void scp_tree_data_assign_pointer(ScpTreeData *data, GType type, gpointer ptr, gboolean copy)
{
	switch (scp_tree_data_get_fundamental_type(type))
	{
		case G_TYPE_STRING :
		{
			data->v_pointer = copy ? g_strdup((const char *) ptr) : ptr;
			break;
		}
		case G_TYPE_POINTER :
		{
			data->v_pointer = ptr;
			break;
		}
		case G_TYPE_OBJECT :
		{
			data->v_pointer = copy && ptr ? g_object_ref(ptr) : ptr;
			break;
		}
		case G_TYPE_BOXED :
		{
			data->v_pointer = copy && ptr ? g_boxed_copy(type, data->v_pointer) : ptr;
			break;
		}
	#if GLIB_CHECK_VERSION(2, 26, 0)
		case G_TYPE_VARIANT :
		{
			data->v_pointer = copy && ptr ? g_variant_ref(ptr) : ptr;
			break;
		}
	#endif
		default : scp_tree_data_warn_unsupported_type(G_STRFUNC, type);
	}
}

#define scp_compare(a, b) ((a) < (b) ? -1 : (a) > (b))

gint scp_tree_data_compare_func(ScpTreeData *a, ScpTreeData *b, GType type)
{
	switch (scp_tree_data_get_fundamental_type(type))
	{
		case G_TYPE_ENUM    : /* this is somewhat bogus */
		case G_TYPE_INT     : return scp_compare(a->v_int, b->v_int);
		case G_TYPE_FLAGS   : /* this is even more bogus */
		case G_TYPE_UINT    : return scp_compare(a->v_uint, b->v_uint);
		case G_TYPE_STRING  : return g_strcmp0(a->v_string, b->v_string);
		case G_TYPE_BOOLEAN : return (a->v_int != 0) - (b->v_int != 0);
		case G_TYPE_LONG    : return scp_compare(a->v_long, b->v_long);
		case G_TYPE_ULONG   : return scp_compare(a->v_ulong, b->v_ulong);
		case G_TYPE_FLOAT   : return scp_compare(a->v_float, b->v_float);
		case G_TYPE_DOUBLE  : return scp_compare(a->v_double, b->v_double);
		case G_TYPE_CHAR    : return a->v_char - b->v_char;
		case G_TYPE_UCHAR   : return (gint) a->v_uchar - (gint) b->v_uchar;
		case G_TYPE_INT64   : return scp_compare(a->v_int64, b->v_int64);
		case G_TYPE_UINT64  : return scp_compare(a->v_uint64, b->v_uint64);
	}

	scp_tree_data_warn_unsupported_type(G_STRFUNC, type);
	return 0;
}

gint scp_tree_data_value_compare_func(GValue *a, GValue *b)
{
	ScpTreeData data_a, data_b;

	scp_tree_data_from_value(&data_a, a, FALSE);
	scp_tree_data_from_value(&data_b, b, FALSE);
	return scp_tree_data_compare_func(&data_a, &data_b, G_VALUE_TYPE(a));
}

gint scp_tree_data_model_compare_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
	gpointer gdata)
{
	gint column = GPOINTER_TO_INT(gdata);
	GValue value_a = G_VALUE_INIT;
	GValue value_b = G_VALUE_INIT;
	gint result;

	gtk_tree_model_get_value(model, a, column, &value_a);
	gtk_tree_model_get_value(model, b, column, &value_b);
	result = scp_tree_data_value_compare_func(&value_a, &value_b);
	g_value_unset(&value_a);
	g_value_unset(&value_b);

	return result;
}

/* Header code */

ScpTreeDataHeader *scp_tree_data_headers_new(gint n_columns, GType *types,
	GtkTreeIterCompareFunc func)
{
	ScpTreeDataHeader *headers = g_new0(ScpTreeDataHeader, n_columns + 1);
	ScpTreeDataHeader *header = headers + 1;
	gint i;

	for (i = 0; i < n_columns; i++, header++)
	{
		header->type = types[i];
		if (!scp_tree_data_check_type(header->type))
			scp_tree_data_warn_unsupported_type(G_STRFUNC, header->type);
		header->utf8_collate = g_type_is_a(header->type, G_TYPE_STRING);
		header->func = func;
		header->data = GINT_TO_POINTER(i);
		header->destroy = NULL;
	}

	return headers + 1;
}

static void scp_destroy_header(ScpTreeDataHeader *header)
{
	if (header->destroy)
	{
		GDestroyNotify destroy = header->destroy;
		header->destroy = NULL;
		destroy(header->data);
	}
}

void scp_tree_data_headers_free(gint n_columns, ScpTreeDataHeader *headers)
{
	gint i;

	for (i = 0; i < n_columns; i++)
		scp_destroy_header(headers + i);

	g_free(headers - 1);
}

void scp_tree_data_set_header(ScpTreeDataHeader *headers, gint sort_column_id,
	GtkTreeIterCompareFunc func, gpointer data, GDestroyNotify destroy)
{
	ScpTreeDataHeader *header = headers + sort_column_id;

	scp_destroy_header(header);
	header->func = func;
	header->data = data;
	header->destroy = destroy;
}
