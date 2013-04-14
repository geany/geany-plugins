/* gtktreedatalist.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * scptreedata.h
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

#ifndef __SCP_TREE_DATA_H__

G_BEGIN_DECLS

typedef union _ScpTreeData
{
	gint8    v_char;
	guint8   v_uchar;
	gint     v_int;
	guint    v_uint;
	glong    v_long;
	gulong   v_ulong;
	gint64   v_int64;
	guint64  v_uint64;
	gfloat   v_float;
	gdouble  v_double;
	gpointer v_pointer;
	char    *v_string;
} ScpTreeData;

GType scp_tree_data_get_fundamental_type(GType type);
void scp_tree_data_free(ScpTreeData *data, GType type);
void scp_tree_data_warn_unsupported_type(const char *prefix, GType type);
gboolean scp_tree_data_check_type(GType type);
void scp_tree_data_to_value(const ScpTreeData *data, GType type, GValue *value);
void scp_tree_data_to_pointer(const ScpTreeData *data, GType type, gpointer dest);
void scp_tree_data_from_value(ScpTreeData *data, GValue *value, gboolean copy);
void scp_tree_data_copy(const ScpTreeData *data, ScpTreeData *new_data, GType type);
void scp_tree_data_assign_pointer(ScpTreeData *data, GType type, gpointer ptr, gboolean copy);

#if GLIB_CHECK_VERSION(2, 26, 0)
#define SCP_TREE_DATA_CASE_VARIANT case G_TYPE_VARIANT :
#else
#define SCP_TREE_DATA_CASE_VARIANT
#endif

#define scp_tree_data_from_stack(data, type, ap, copy) \
	G_STMT_START \
	{ \
		const GType type1 = (type); \
	\
		switch (scp_tree_data_get_fundamental_type(type1)) \
		{ \
			case G_TYPE_INT     : \
			case G_TYPE_ENUM    : (data)->v_int = va_arg((ap), int); break; \
			case G_TYPE_UINT    : \
			case G_TYPE_FLAGS   : (data)->v_uint = va_arg((ap), unsigned); break; \
			case G_TYPE_BOOLEAN : (data)->v_int = va_arg((ap), int) != 0; break; \
			case G_TYPE_LONG    : (data)->v_long = va_arg((ap), long); break; \
			case G_TYPE_ULONG   : (data)->v_ulong = va_arg((ap), unsigned long); break; \
			case G_TYPE_FLOAT   : (data)->v_float = va_arg((ap), double); break; \
			case G_TYPE_DOUBLE  : (data)->v_double = va_arg((ap), double); break; \
			case G_TYPE_CHAR    : (data)->v_char = va_arg((ap), int); break; \
			case G_TYPE_UCHAR   : (data)->v_uchar = va_arg((ap), unsigned); break; \
			case G_TYPE_INT64   : (data)->v_int64 = va_arg((ap), gint64); break; \
			case G_TYPE_UINT64  : (data)->v_uint64 = va_arg((ap), guint64); break; \
			case G_TYPE_POINTER : \
			case G_TYPE_STRING  : \
			case G_TYPE_OBJECT  : \
			case G_TYPE_BOXED   : \
			SCP_TREE_DATA_CASE_VARIANT \
			{ \
				scp_tree_data_assign_pointer((data), type1, va_arg((ap), gpointer), \
					(copy)); \
				break; \
			} \
			default : scp_tree_data_warn_unsupported_type(G_STRFUNC, type1); \
		} \
	} G_STMT_END

gint scp_tree_data_compare_func(ScpTreeData *a, ScpTreeData *b, GType type);
gint scp_tree_data_value_compare_func(GValue *a, GValue *b);
gint scp_tree_data_model_compare_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
	gpointer gdata);

/* Header code */
typedef struct _ScpTreeDataHeader
{
	GType type;
	gboolean utf8_collate;
	GtkTreeIterCompareFunc func;
	gpointer data;
	GDestroyNotify destroy;
} ScpTreeDataHeader;

ScpTreeDataHeader *scp_tree_data_headers_new(gint n_columns, GType *types,
	GtkTreeIterCompareFunc func);
void scp_tree_data_headers_free(gint n_columns, ScpTreeDataHeader *headers);
void scp_tree_data_set_header(ScpTreeDataHeader *headers, gint sort_column_id,
	GtkTreeIterCompareFunc func, gpointer gdata, GDestroyNotify destroy);

G_END_DECLS

#define __SCP_TREE_DATA_H__ 1
#endif
