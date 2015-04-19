/* gtktreestore.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * scptreestore.c
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
#include "scptreestore.h"

#if defined(ENABLE_NLS) && defined(GETTEXT_PACKAGE)
#define P_(String) g_dgettext(GETTEXT_PACKAGE "-properties", (String))
#else
#define P_(String) (String)
#endif

#if !GLIB_CHECK_VERSION(2, 30, 0)
#define G_VALUE_INIT { 0, { { 0 } } }
#endif

#if !GTK_CHECK_VERSION(2, 90, 7)
#define gtk_get_debug_flags() (gtk_debug_flags + 0)
#endif

#ifdef G_DISABLE_CHECKS
#define VALIDATE_ONLY G_GNUC_UNUSED
#else
#define VALIDATE_ONLY
#endif

typedef struct _AElem AElem;

struct _AElem
{
	AElem *parent;
	GPtrArray *children;
	ScpTreeData data[1];
};

struct _ScpTreeStorePrivate
{
	gint stamp;
	AElem *root;
	GPtrArray *roar;
	guint n_columns;
	ScpTreeDataHeader *headers;
	gint sort_column_id;
	GtkSortType sort_order;
	GtkTreeIterCompareFunc sort_func;  /* current */
	gboolean sublevels;
	guint toplevel_reserved;
	guint sublevel_reserved;
	gboolean sublevel_discard;
	gboolean columns_dirty;
};

#define VALID_ITER(iter, store) \
	((iter) && (iter)->user_data && (store)->priv->stamp == (iter)->stamp)
#define VALID_ITER_OR_NULL(iter, store) \
	(!(iter) || ((iter)->user_data && (store)->priv->stamp == (iter)->stamp))
#define SCP_TREE_MODEL(store) ((GtkTreeModel *) store)

#define ITER_ARRAY(iter) ((GPtrArray *) ((iter)->user_data))
#define ITER_INDEX(iter) (GPOINTER_TO_INT((iter)->user_data2))
#define ITER_ELEM(iter) ((AElem *) ITER_ARRAY(iter)->pdata[ITER_INDEX(iter)])
#define ELEM_SIZE(n_columns) (sizeof(AElem) + ((n_columns) - 1) * sizeof(ScpTreeData))

/* Store */

static void scp_ptr_array_insert_val(GPtrArray *array, guint index, gpointer data)
{
	g_ptr_array_set_size(array, array->len + 1);
	memmove(array->pdata + index + 1, array->pdata + index, (array->len - index - 1) *
		sizeof(gpointer));
	array->pdata[index] = data;
}

static void scp_free_element(ScpTreeStore *store, AElem *elem);

static void scp_free_array(ScpTreeStore *store, GPtrArray *array)
{
	if (array)
	{
		guint i;

		for (i = 0; i < array->len; i++)
			scp_free_element(store, (AElem *) array->pdata[i]);
		g_ptr_array_free(array, TRUE);
	}
}

static void scp_free_element(ScpTreeStore *store, AElem *elem)
{
	ScpTreeStorePrivate *priv = store->priv;
	guint i;

	scp_free_array(store, elem->children);

	for (i = 0; i < priv->n_columns; i++)
		scp_tree_data_free(elem->data + i, priv->headers[i].type);

	g_slice_free1(ELEM_SIZE(priv->n_columns), elem);
}

ScpTreeStore *scp_tree_store_new(gboolean sublevels, gint n_columns, ...)
{
	GType *types;
	va_list ap;
	gint i;
	ScpTreeStore *store;

	g_return_val_if_fail(n_columns > 0, NULL);
	types = g_malloc(n_columns * sizeof(GType));

	va_start(ap, n_columns);
	for (i = 0; i < n_columns; i++)
		types[i] = va_arg(ap, GType);
	va_end(ap);

	store = scp_tree_store_newv(sublevels, n_columns, types);
	g_free(types);
	return store;
}

ScpTreeStore *scp_tree_store_newv(gboolean sublevels, gint n_columns, GType *types)
{
	ScpTreeStore *store;

	g_return_val_if_fail(n_columns > 0, NULL);
	store = g_object_new(SCP_TYPE_TREE_STORE, "sublevels", sublevels != FALSE, NULL);

	if (!scp_tree_store_set_column_types(store, n_columns, types))
	{
		g_object_unref(store);
		store = NULL;
	}

	return store;
}

#define scp_tree_model_compare_func ((GtkTreeIterCompareFunc) scp_tree_store_compare_func)

gboolean scp_tree_store_set_column_types(ScpTreeStore *store, gint n_columns, GType *types)
{
	ScpTreeStorePrivate *priv = store->priv;

	g_return_val_if_fail(SCP_IS_TREE_STORE(store), FALSE);
	g_return_val_if_fail(!priv->columns_dirty, FALSE);

	if (priv->headers)
		scp_tree_data_headers_free(priv->n_columns, priv->headers);

	priv->headers = scp_tree_data_headers_new(n_columns, types, scp_tree_model_compare_func);
	priv->n_columns = n_columns;
	return TRUE;
}

static void scp_emit_reordered(ScpTreeStore *store, GtkTreeIter *iter, gint *new_order)
{
	ScpTreeStorePrivate *priv = store->priv;
	GtkTreePath *path;
	GtkTreeIter parent;

	if (iter->user_data == priv->root->children)
	{
		path = gtk_tree_path_new();
		parent.stamp = priv->stamp;
		parent.user_data = store->priv->roar;
		parent.user_data2 = GINT_TO_POINTER(0);
	}
	else
	{
		scp_tree_store_iter_parent(store, &parent, iter);
		path = scp_tree_store_get_path(store, &parent);
	}

	gtk_tree_model_rows_reordered(SCP_TREE_MODEL(store), path, &parent, new_order);
	gtk_tree_path_free(path);
}

static void scp_move_element(ScpTreeStore *store, GPtrArray *array, GtkTreeIter *iter,
	guint new_pos, gboolean emit_reordered)
{
	guint old_pos = ITER_INDEX(iter);

	if (old_pos != new_pos)
	{
		gpointer data = array->pdata[old_pos];

		if (new_pos < old_pos)
		{
			memmove(array->pdata + new_pos + 1, array->pdata + new_pos,
				(old_pos - new_pos) * sizeof(gpointer));
		}
		else
		{
			memmove(array->pdata + old_pos, array->pdata + old_pos + 1,
				(new_pos - old_pos) * sizeof(gpointer));
		}

		array->pdata[new_pos] = data;
		iter->user_data2 = GINT_TO_POINTER(new_pos);

		if (emit_reordered)
		{
			gint *new_order = g_new(gint, array->len);
			guint i;

			for (i = 0; i < array->len; i++)
			{
				if (G_UNLIKELY(i == new_pos))
					new_order[i] = old_pos;
				else if (new_pos < old_pos)
					new_order[i] = i - (i > new_pos && i <= old_pos);
				else
					new_order[i] = i + (i >= old_pos && i < new_pos);
			}

			scp_emit_reordered(store, iter, new_order);
			g_free(new_order);
		}
	}
}

static inline gint scp_fixup_compare_result(ScpTreeStore *store, gint result)
{
	return store->priv->sort_order == GTK_SORT_ASCENDING ? result :
		result > 0 ? -1 : result < 0;
}

#define scp_store_compare(store, func, a, b, data) \
	scp_fixup_compare_result((store), (*(func))(SCP_TREE_MODEL(store), (a), (b), (data)))

static guint scp_search_pos(ScpTreeStore *store, GtkTreeIterCompareFunc func, gpointer data,
	GtkTreeIter *iter, gint low, gint high)
{
	if (low <= high)
	{
		gint mid;
		GtkTreeIter iter1;

		iter1.stamp = iter->stamp;
		iter1.user_data = iter->user_data;

		while (low < high)
		{
			gint result;

			mid = (low + high) / 2;
			iter1.user_data2 = GINT_TO_POINTER(mid);
			result = scp_store_compare(store, func, iter, &iter1, data);

			if (result > 0)
				low = mid + 1;
			else if (result < 0)
				high = mid - 1;
			else
				return mid;
		}

		iter1.user_data2 = GINT_TO_POINTER(low);
		if (scp_store_compare(store, func, iter, &iter1, data) > 0)
			low++;
	}

	return low;
}

static void scp_sort_element(ScpTreeStore *store, GtkTreeIter *iter, gboolean emit_reordered)
{
	ScpTreeStorePrivate *priv = store->priv;
	GPtrArray *array = ITER_ARRAY(iter);
	gint index = ITER_INDEX(iter);
	GtkTreeIter iter1;
	gpointer data = priv->headers[priv->sort_column_id].data;

	iter1.stamp = priv->stamp;
	iter1.user_data = array;

	if (index > 0)
	{
		iter1.user_data2 = GINT_TO_POINTER(index - 1);

		if (scp_store_compare(store, priv->sort_func, iter, &iter1, data) < 0)
		{
			scp_move_element(store, array, iter, scp_search_pos(store, priv->sort_func,
				data, iter, 0, index - 2), emit_reordered);
			return;
		}
	}

	if (index < (gint) array->len - 1)
	{
		iter1.user_data2 = GINT_TO_POINTER(index + 1);

		if (scp_store_compare(store, priv->sort_func, iter, &iter1, data) > 0)
		{
			scp_move_element(store, array, iter, scp_search_pos(store, priv->sort_func,
				data, iter, index + 2, array->len - 1) - 1, emit_reordered);
		}
	}
}

static gboolean scp_set_value(ScpTreeStore *store, AElem *elem, gint column, GValue *value)
{
	ScpTreeStorePrivate *priv = store->priv;
	GType dest_type = priv->headers[column].type;

	g_return_val_if_fail((guint) column < priv->n_columns, FALSE);

	if (g_type_is_a(G_VALUE_TYPE(value), dest_type))
		scp_tree_data_from_value(elem->data + column, value, TRUE);
	else
	{
		GValue real_value = G_VALUE_INIT;
		g_value_init(&real_value, dest_type);

		if (!g_value_transform(value, &real_value))
		{
			g_warning("%s: Unable to make conversion from %s to %s\n", G_STRFUNC,
				g_type_name(G_VALUE_TYPE(value)), g_type_name(dest_type));
			return FALSE;
		}

		scp_tree_data_from_value(elem->data + column, &real_value, TRUE);
		g_value_unset(&real_value);
	}

	return TRUE;
}

static void scp_set_vector(ScpTreeStore *store, AElem *elem, gboolean *changed,
	gboolean *sort_changed, gint *columns, GValue *values, gint n_values)
{
	ScpTreeStorePrivate *priv = store->priv;
	gint i;

	if (priv->sort_func && priv->sort_func != scp_tree_model_compare_func)
		*sort_changed = TRUE;

	for (i = 0; i < n_values; i++)
	{
		gint column = columns[i];

		if ((guint) column >= priv->n_columns)
		{
			g_warning("%s: Invalid column number %d added to iter (remember to end "
				"your list of columns with a -1)", G_STRFUNC, column);
			break;
		}

		if (scp_set_value(store, elem, column, values + i))
			*changed = TRUE;

		if (column == priv->sort_column_id)
			*sort_changed = TRUE;
	}
}

static void scp_set_valist(ScpTreeStore *store, AElem *elem, gboolean *changed,
	gboolean *sort_changed, va_list ap)
{
	ScpTreeStorePrivate *priv = store->priv;
	gint column;

	if (priv->sort_func && priv->sort_func != scp_tree_model_compare_func)
		*sort_changed = TRUE;

	while ((column = va_arg(ap, int)) != -1)
	{
		if ((guint) column >= priv->n_columns)
		{
			g_warning("%s: Invalid column number %d added to iter (remember to end "
				"your list of columns with a -1)", G_STRFUNC, column);
			break;
		}

		scp_tree_data_from_stack(elem->data + column, priv->headers[column].type, ap, TRUE);
		*changed = TRUE;

		if (column == priv->sort_column_id)
			*sort_changed = TRUE;
	}
}

static void scp_set_values_signals(ScpTreeStore *store, GtkTreeIter *iter, gboolean changed,
	gboolean sort_changed)
{
	if (sort_changed)
		scp_sort_element(store, iter, TRUE);

	if (changed)
	{
		GtkTreePath *path;

		path = scp_tree_store_get_path(store, iter);
		gtk_tree_model_row_changed(SCP_TREE_MODEL(store), path, iter);
		gtk_tree_path_free(path);
	}
}

void scp_tree_store_set_valuesv(ScpTreeStore *store, GtkTreeIter *iter, gint *columns,
	GValue *values, gint n_values)
{
	gboolean changed = FALSE;
	gboolean sort_changed = FALSE;

	g_return_if_fail(SCP_IS_TREE_STORE(store));
	g_return_if_fail(VALID_ITER(iter, store));

	scp_set_vector(store, ITER_ELEM(iter), &changed, &sort_changed, columns, values, n_values);
	scp_set_values_signals(store, iter, changed, sort_changed);
}

void scp_tree_store_set_value(ScpTreeStore *store, GtkTreeIter *iter, gint column,
	GValue *value)
{
	scp_tree_store_set_valuesv(store, iter, &column, value, 1);
}

void scp_tree_store_set_valist(ScpTreeStore *store, GtkTreeIter *iter, va_list ap)
{
	gboolean changed = FALSE;
	gboolean sort_changed = FALSE;

	g_return_if_fail(SCP_IS_TREE_STORE(store));
	g_return_if_fail(VALID_ITER(iter, store));

	scp_set_valist(store, ITER_ELEM(iter), &changed, &sort_changed, ap);
	scp_set_values_signals(store, iter, changed, sort_changed);
}

void scp_tree_store_set(ScpTreeStore *store, GtkTreeIter *iter, ...)
{
	va_list ap;

	va_start(ap, iter);
	scp_tree_store_set_valist(store, iter, ap);
	va_end(ap);
}

gboolean scp_tree_store_remove(ScpTreeStore *store, GtkTreeIter *iter)
{
	ScpTreeStorePrivate *priv = store->priv;
	GPtrArray *array;
	guint index;
	GtkTreePath *path;
	AElem *elem, *parent;

	g_return_val_if_fail(SCP_IS_TREE_STORE(store), FALSE);
	g_return_val_if_fail(VALID_ITER(iter, store), FALSE);

	array = ITER_ARRAY(iter);
	index = ITER_INDEX(iter);
	elem = (AElem *) array->pdata[index];
	parent = elem->parent;

	path = scp_tree_store_get_path(store, iter);
	scp_free_element(store, elem);
	g_ptr_array_remove_index(array, index);
	gtk_tree_model_row_deleted(SCP_TREE_MODEL(store), path);

	if (index == array->len)
	{
		if (array->len == 0 && parent != priv->root)
		{
			if (priv->sublevel_discard)
			{
				g_ptr_array_free(array, TRUE);
				parent->children = NULL;
			}
			/* child_toggled */
			iter->user_data = parent->parent->children;
			gtk_tree_path_up(path);
			iter->user_data2 = GINT_TO_POINTER(gtk_tree_path_get_indices(path)
				[gtk_tree_path_get_depth(path) - 1]);
			gtk_tree_model_row_has_child_toggled(SCP_TREE_MODEL(store), path, iter);
		}
		/* invalidate */
		iter->stamp = 0;
	}

	gtk_tree_path_free(path);
	return iter->stamp != 0;
}

static void validate_elem(AElem *parent, AElem *elem)
{
	GPtrArray *array = elem->children;
	g_assert(elem->parent == parent);

	if (array)
	{
		guint i;

		for (i = 0; i < array->len; i++)
			validate_elem(elem, (AElem *) array->pdata[i]);
	}
}

static void validate_store(ScpTreeStore *store)
{
	if (gtk_get_debug_flags() & GTK_DEBUG_TREE)
		validate_elem(NULL, (store)->priv->root);
}

static gboolean scp_insert_element(ScpTreeStore *store, GtkTreeIter *iter, AElem *elem,
	gint position, GtkTreeIter *parent_iter)
{
	ScpTreeStorePrivate *priv = store->priv;
	AElem *parent = parent_iter ? ITER_ELEM(parent_iter) : priv->root;
	GPtrArray *array = parent->children;
	GtkTreePath *path;

	g_return_val_if_fail(SCP_IS_TREE_STORE(store), FALSE);
	g_return_val_if_fail(iter != NULL, FALSE);
	g_return_val_if_fail(priv->sublevels == TRUE || parent_iter == NULL, FALSE);
	g_return_val_if_fail(VALID_ITER_OR_NULL(parent_iter, store), FALSE);

	if (array)
	{
		if (position == -1)
			position = array->len;
		else
			g_return_val_if_fail((guint) position <= array->len, FALSE);
	}
	else
	{
		g_return_val_if_fail(position == 0 || position == -1, FALSE);
		position = 0;
		parent->children = array = g_ptr_array_sized_new(parent_iter ?
			priv->sublevel_reserved : priv->toplevel_reserved);
	}

	elem->parent = parent;
	scp_ptr_array_insert_val(array, position, elem);
	iter->stamp = priv->stamp;
	iter->user_data = array;
	iter->user_data2 = GINT_TO_POINTER(position);

	if (priv->sort_func)
		scp_sort_element(store, iter, FALSE);

	priv->columns_dirty = TRUE;
	path = scp_tree_store_get_path(store, iter);
	gtk_tree_model_row_inserted(SCP_TREE_MODEL(store), path, iter);

	if (parent_iter && array->len == 1)
	{
		gtk_tree_path_up(path);
		gtk_tree_model_row_has_child_toggled(SCP_TREE_MODEL(store), path, parent_iter);
	}

	gtk_tree_path_free(path);
	validate_store(store);
	return TRUE;
}

void scp_tree_store_insert(ScpTreeStore *store, GtkTreeIter *iter, GtkTreeIter *parent,
	gint position)
{
	AElem *elem = g_slice_alloc0(ELEM_SIZE(store->priv->n_columns));

	if (!scp_insert_element(store, iter, elem, position, parent))
		g_slice_free1(ELEM_SIZE(store->priv->n_columns), elem);
}

void scp_tree_store_insert_with_valuesv(ScpTreeStore *store, GtkTreeIter *iter,
	GtkTreeIter *parent, gint position, gint *columns, GValue *values, gint n_values)
{
	AElem *elem = g_slice_alloc0(ELEM_SIZE(store->priv->n_columns));
	gboolean changed, sort_changed;
	GtkTreeIter iter1;

	scp_set_vector(store, elem, &changed, &sort_changed, columns, values, n_values);

	if (!scp_insert_element(store, iter ? iter : &iter1, elem, position, parent))
		scp_free_element(store, elem);
}

void scp_tree_store_insert_with_valist(ScpTreeStore *store, GtkTreeIter *iter, GtkTreeIter
	*parent, gint position, va_list ap)
{
	AElem *elem = g_slice_alloc0(ELEM_SIZE(store->priv->n_columns));
	gboolean changed, sort_changed;
	GtkTreeIter iter1;

	scp_set_valist(store, elem, &changed, &sort_changed, ap);

	if (!scp_insert_element(store, iter ? iter : &iter1, elem, position, parent))
		scp_free_element(store, elem);
}

void scp_tree_store_insert_with_values(ScpTreeStore *store, GtkTreeIter *iter,
	GtkTreeIter *parent, gint position, ...)
{
	va_list ap;

	va_start(ap, position);
	scp_tree_store_insert_with_valist(store, iter, parent, position, ap);
	va_end(ap);
}

void scp_tree_store_get_valist(ScpTreeStore *store, GtkTreeIter *iter, va_list ap)
{
	ScpTreeStorePrivate *priv = store->priv;
	AElem *elem = ITER_ELEM(iter);
	gint column;

	g_return_if_fail(SCP_IS_TREE_STORE(store));
	g_return_if_fail(VALID_ITER(iter, store));

	while ((column = va_arg(ap, int)) != -1)
	{
		gpointer dest;

		if ((guint) column >= priv->n_columns)
		{
			g_warning("%s: Invalid column number %d added to iter (remember to end "
				"your list of columns with a -1)", G_STRFUNC, column);
			break;
		}

		dest = va_arg(ap, gpointer);
		scp_tree_data_to_pointer(elem->data + column, priv->headers[column].type, dest);
	}
}

void scp_tree_store_get(ScpTreeStore *store, GtkTreeIter *iter, ...)
{
	va_list ap;

	va_start(ap, iter);
	scp_tree_store_get_valist(store, iter, ap);
	va_end(ap);
}

static gboolean scp_tree_contains(GPtrArray *array, AElem *elem)
{
	if (array)
	{
		guint i;

		for (i = 0; i < array->len; i++)
		{
			AElem *elem1 = (AElem *) array->pdata[i];
			if (elem1 == elem || scp_tree_contains(elem1->children, elem))
				return TRUE;
		}
	}

	return FALSE;
}

gboolean scp_tree_store_is_ancestor(VALIDATE_ONLY ScpTreeStore *store, GtkTreeIter *iter,
	GtkTreeIter *descendant)
{
	g_return_val_if_fail(SCP_IS_TREE_STORE(store), FALSE);
	g_return_val_if_fail(VALID_ITER(iter, store), FALSE);
	g_return_val_if_fail(VALID_ITER(descendant, store), FALSE);

	return scp_tree_contains(ITER_ELEM(iter)->children, ITER_ELEM(descendant));
}

gint scp_tree_store_iter_depth(VALIDATE_ONLY ScpTreeStore *store, GtkTreeIter *iter)
{
	gint depth = 0;
	AElem *elem;

	g_return_val_if_fail(SCP_IS_TREE_STORE(store), 0);
	g_return_val_if_fail(VALID_ITER(iter, store), 0);

	for (elem = ITER_ELEM(iter); elem->parent; depth++, elem = elem->parent);
	return depth;
}

static void scp_clear_array(ScpTreeStore *store, GPtrArray *array, gboolean emit_subsignals)
{
	if (array)
	{
		GtkTreeIter iter;
		gint i;

		iter.user_data = array;

		for (i = (gint) array->len - 1; i >= 0; i--)
		{
			if (emit_subsignals)
				scp_clear_array(store, ((AElem *) array->pdata[i])->children, TRUE);

			iter.stamp = store->priv->stamp;
			iter.user_data2 = GINT_TO_POINTER(i);
			scp_tree_store_remove(store, &iter);
		}
	}
}

void scp_tree_store_clear_children(ScpTreeStore *store, GtkTreeIter *parent,
	gboolean emit_subsignals)
{
	g_return_if_fail(SCP_IS_TREE_STORE(store));
	g_return_if_fail(VALID_ITER_OR_NULL(parent, store));

	if (parent)
		scp_clear_array(store, ITER_ELEM(parent)->children, emit_subsignals);
	else
	{
		scp_clear_array(store, store->priv->root->children, emit_subsignals);
		while (++store->priv->stamp == 0);
	}
}

gboolean scp_tree_store_iter_is_valid(ScpTreeStore *store, GtkTreeIter *iter)
{
	g_return_val_if_fail(SCP_IS_TREE_STORE(store), FALSE);
	g_return_val_if_fail(VALID_ITER(iter, store), FALSE);
	return scp_tree_contains(store->priv->root->children, ITER_ELEM(iter));
}

static void scp_reorder_array(ScpTreeStore *store, GtkTreeIter *parent, GPtrArray *array,
	gint *new_order)
{
	gpointer *pdata = g_new(gpointer, array->len);
	GtkTreePath *path;
	guint i;

	for (i = 0; i < array->len; i++)
		pdata[i] = array->pdata[new_order[i]];

	memcpy(array->pdata, pdata, array->len * sizeof(gpointer));
	g_free(pdata);
	/* emit signal */
	path = parent ? scp_tree_store_get_path(store, parent) : gtk_tree_path_new();
	gtk_tree_model_rows_reordered(SCP_TREE_MODEL(store), path, parent, new_order);
	gtk_tree_path_free(path);
}

void scp_tree_store_reorder(ScpTreeStore *store, GtkTreeIter *parent, gint *new_order)
{
	GPtrArray *array;

	g_return_if_fail(SCP_IS_TREE_STORE(store));
	g_return_if_fail(store->priv->sort_func == NULL);
	g_return_if_fail(VALID_ITER_OR_NULL(parent, store));
	g_return_if_fail(new_order != NULL);

	if ((array = (parent ? ITER_ELEM(parent) : store->priv->root)->children) != NULL)
		scp_reorder_array(store, parent, array, new_order);
}

void scp_tree_store_swap(ScpTreeStore *store, GtkTreeIter *a, GtkTreeIter *b)
{
	GPtrArray *array = ITER_ARRAY(a);
	guint index_a = ITER_INDEX(a);
	guint index_b = ITER_INDEX(b);

	g_return_if_fail(SCP_IS_TREE_STORE(store));
	g_return_if_fail(VALID_ITER(a, store));
	g_return_if_fail(VALID_ITER(b, store));

	if (ITER_ARRAY(b) != array)
	{
		g_warning("%s: Given children don't have a common parent\n", G_STRFUNC);
		return;
	}

	if (index_a != index_b)
	{
		AElem *swap = (AElem *) array->pdata[index_a];
		gint *new_order = g_new(gint, array->len);
		guint i;

		array->pdata[index_a] = array->pdata[index_b];
		array->pdata[index_b] = swap;

		for (i = 0; i < array->len; i++)
			new_order[i] = i == index_a ? index_b : i == index_b ? index_a : i;

		scp_emit_reordered(store, a, new_order);
		g_free(new_order);
	}
}

void scp_tree_store_move(ScpTreeStore *store, GtkTreeIter *iter, gint position)
{
	GPtrArray *array = ITER_ARRAY(iter);

	g_return_if_fail(SCP_IS_TREE_STORE(store));
	g_return_if_fail(store->priv->sort_func == NULL);
	g_return_if_fail(VALID_ITER(iter, store));

	if (position == -1)
	{
		g_return_if_fail(array->len > 0);
		position = array->len - 1;
	}
	else
		g_return_if_fail((guint) position < array->len);

	scp_move_element(store, array, iter, position, TRUE);
}

gint scp_tree_store_iter_tell(VALIDATE_ONLY ScpTreeStore *store, GtkTreeIter *iter)
{
	g_return_val_if_fail(SCP_IS_TREE_STORE(store), -1);
	g_return_val_if_fail(VALID_ITER(iter, store), -1);
	g_return_val_if_fail((guint) ITER_INDEX(iter) < ITER_ARRAY(iter)->len, -1);

	return ITER_INDEX(iter);
}

/* Model */

static gint scp_ptr_array_find(GPtrArray *array, AElem *elem)
{
	guint i;

	for (i = 0; i < array->len; i++)
		if (array->pdata[i] == elem)
			return i;

	return -1;
}

GtkTreeModelFlags scp_tree_store_get_flags(ScpTreeStore *store)
{
	return store->priv->sublevels ? 0 : GTK_TREE_MODEL_LIST_ONLY;
}

gint scp_tree_store_get_n_columns(ScpTreeStore *store)
{
	store->priv->columns_dirty = TRUE;
	return store->priv->n_columns;
}

GType scp_tree_store_get_column_type(ScpTreeStore *store, gint index)
{
	ScpTreeStorePrivate *priv = store->priv;

	g_return_val_if_fail((guint) index < priv->n_columns, G_TYPE_INVALID);
	priv->columns_dirty = TRUE;
	return priv->headers[index].type;
}

gboolean scp_tree_store_get_iter(ScpTreeStore *store, GtkTreeIter *iter, GtkTreePath *path)
{
	ScpTreeStorePrivate *priv = store->priv;
	GPtrArray *array = priv->root->children;
	const gint *indices;
	gint depth, i;

	priv->columns_dirty = TRUE;
	indices = gtk_tree_path_get_indices(path);
	depth = gtk_tree_path_get_depth(path);
	g_return_val_if_fail(depth > 0, FALSE);

	for (i = 0; ; i++)
	{
		if (!array || (guint) indices[i] >= array->len)
		{
			iter->stamp = 0;
			return FALSE;
		}

		if (i == depth - 1)
			break;

		array = ((AElem *) array->pdata[indices[i]])->children;
	}

	iter->stamp = priv->stamp;
	iter->user_data = array;
	iter->user_data2 = GINT_TO_POINTER(indices[depth - 1]);
	return TRUE;
}

GtkTreePath *scp_tree_store_get_path(VALIDATE_ONLY ScpTreeStore *store, GtkTreeIter *iter)
{
	AElem *elem = ITER_ELEM(iter);
	GtkTreePath *path;

	g_return_val_if_fail(VALID_ITER(iter, store), NULL);
	path = gtk_tree_path_new();

	if (elem->parent)
	{
		gtk_tree_path_append_index(path, ITER_INDEX(iter));

		while ((elem = elem->parent), elem->parent)
		{
			gint index = scp_ptr_array_find(elem->parent->children, elem);

			if (index == -1)
			{
				/* We couldn't find node, meaning it's prolly not ours */
				/* Perhaps I should do a g_return_if_fail here. */
				gtk_tree_path_free(path);
				return NULL;
			}

			gtk_tree_path_prepend_index(path, index);
		}
	}

	return path;
}

void scp_tree_store_get_value(ScpTreeStore *store, GtkTreeIter *iter, gint column,
	GValue *value)
{
	ScpTreeStorePrivate *priv = store->priv;

	g_return_if_fail((guint) column < priv->n_columns);
	g_return_if_fail(VALID_ITER(iter, store));
	scp_tree_data_to_value(ITER_ELEM(iter)->data + column, priv->headers[column].type, value);
}

gboolean scp_tree_store_iter_next(VALIDATE_ONLY ScpTreeStore *store, GtkTreeIter *iter)
{
	gint index = ITER_INDEX(iter);

	g_return_val_if_fail(VALID_ITER(iter, store), FALSE);

	if (index < (gint) ITER_ARRAY(iter)->len - 1)
	{
		iter->user_data2 = GINT_TO_POINTER(index + 1);
		return TRUE;
	}

	iter->stamp = 0;
	return FALSE;
}

gboolean scp_tree_store_iter_previous(VALIDATE_ONLY ScpTreeStore *store, GtkTreeIter *iter)
{
	gint index = ITER_INDEX(iter);

	g_return_val_if_fail(VALID_ITER(iter, store), FALSE);

	if (index > 0)
	{
		iter->user_data2 = GINT_TO_POINTER(index - 1);
		return TRUE;
	}

	iter->stamp = 0;
	return FALSE;
}

gboolean scp_tree_store_iter_children(ScpTreeStore *store, GtkTreeIter *iter,
	GtkTreeIter *parent)
{
	ScpTreeStorePrivate *priv = store->priv;
	GPtrArray *array;

	g_return_val_if_fail(VALID_ITER_OR_NULL(parent, store), FALSE);
	array = (parent ? ITER_ELEM(parent) : priv->root)->children;

	if (array && array->len)
	{
		iter->stamp = priv->stamp;
		iter->user_data = array;
		iter->user_data2 = GINT_TO_POINTER(0);
		return TRUE;
	}

	iter->stamp = 0;
	return FALSE;
}

gboolean scp_tree_store_iter_has_child(VALIDATE_ONLY ScpTreeStore *store, GtkTreeIter *iter)
{
	g_return_val_if_fail(VALID_ITER(iter, store), FALSE);
	return ITER_ELEM(iter)->children && ITER_ELEM(iter)->children->len;
}

gint scp_tree_store_iter_n_children(ScpTreeStore *store, GtkTreeIter *iter)
{
	GPtrArray *array;

	g_return_val_if_fail(VALID_ITER_OR_NULL(iter, store), 0);
	array = (iter ? ITER_ELEM(iter) : store->priv->root)->children;
	return array ? array->len : 0;
}

gboolean scp_tree_store_iter_nth_child(ScpTreeStore *store, GtkTreeIter *iter,
	GtkTreeIter *parent, gint n)
{
	ScpTreeStorePrivate *priv = store->priv;
	GPtrArray *array;

	g_return_val_if_fail(VALID_ITER_OR_NULL(parent, store), FALSE);
	array = (parent ? ITER_ELEM(parent) : priv->root)->children;

	if (array && (guint) n < array->len)
	{
		iter->stamp = priv->stamp;
		iter->user_data = array;
		iter->user_data2 = GINT_TO_POINTER(n);
		return TRUE;
	}

	iter->stamp = 0;
	return FALSE;
}

gboolean scp_tree_store_iter_parent(ScpTreeStore *store, GtkTreeIter *iter,
	GtkTreeIter *child)
{
	ScpTreeStorePrivate *priv = store->priv;
	AElem *parent;

	g_return_val_if_fail(iter != NULL, FALSE);
	g_return_val_if_fail(VALID_ITER(child, store), FALSE);
	parent = ITER_ELEM(child)->parent;
	g_assert(parent != NULL);

	if (parent->parent)
	{
		gint index = scp_ptr_array_find(parent->parent->children, parent);

		if (index != -1)
		{
			iter->stamp = priv->stamp;
			iter->user_data = parent->parent->children;
			iter->user_data2 = GINT_TO_POINTER(index);
			return TRUE;
		}
	}

	iter->stamp = 0;
	return FALSE;
}

static gboolean scp_foreach(ScpTreeStore *store, GPtrArray *array, GtkTreePath *path,
	GtkTreeModelForeachFunc func, gpointer gdata)
{
	if (array)
	{
		guint i;
		GtkTreeIter iter;

		gtk_tree_path_down(path);
		iter.stamp = store->priv->stamp;
		iter.user_data = array;

		for (i = 0; i < array->len; i++)
		{
			iter.user_data2 = GINT_TO_POINTER(i);

			if (func((GtkTreeModel *) store, path, &iter, gdata) || scp_foreach(store,
				((AElem *) array->pdata[i])->children, path, func, gdata))
			{
				return TRUE;
			}
			gtk_tree_path_next(path);
		}

		gtk_tree_path_up(path);
	}

	return FALSE;
}

void scp_tree_store_foreach(ScpTreeStore *store, GtkTreeModelForeachFunc func, gpointer gdata)
{
	GtkTreePath *path;
	
	g_return_if_fail(SCP_IS_TREE_STORE(store));
	g_return_if_fail(func != NULL);

	path = gtk_tree_path_new();
	scp_foreach(store, store->priv->root->children, path, func, gdata);
	gtk_tree_path_free(path);
}

/* DND */
gboolean scp_tree_store_row_draggable(G_GNUC_UNUSED ScpTreeStore *store,
	G_GNUC_UNUSED GtkTreePath *path)
{
	return TRUE;
}

gboolean scp_tree_store_drag_data_delete(ScpTreeStore *store, GtkTreePath *path)
{
	GtkTreeIter iter;

	if (scp_tree_store_get_iter(store, &iter, path))
	{
		scp_tree_store_remove(store, &iter);
		return TRUE;
	}

	return FALSE;
}

gboolean scp_tree_store_drag_data_get(ScpTreeStore *store, GtkTreePath *path,
	GtkSelectionData *selection_data)
{
	/* Note that we don't need to handle the GTK_TREE_MODEL_ROW
	 * target, because the default handler does it for us, but
	 * we do anyway for the convenience of someone maybe overriding the
	 * default handler.
	 */
	return gtk_tree_set_row_drag_data(selection_data, SCP_TREE_MODEL(store), path);
}

static void scp_copy_element(ScpTreeStore *store, GtkTreeIter *src_iter,
	GtkTreeIter *dest_iter)
{
	ScpTreeStorePrivate *priv = store->priv;
	AElem *elem = ITER_ELEM(src_iter);
	AElem *dest = ITER_ELEM(dest_iter);
	GtkTreePath *path = scp_tree_store_get_path(store, dest_iter);
	guint i;

	for (i = 0; i < priv->n_columns; i++)
		scp_tree_data_copy(elem->data + i, dest->data + i, priv->headers[i].type);
	gtk_tree_model_row_changed(SCP_TREE_MODEL(store), path, dest_iter);
	gtk_tree_path_free(path);

	if (elem->children)
	{
		GtkTreeIter child;

		child.stamp = priv->stamp;
		child.user_data = elem->children;

		for (i = 0; i < elem->children->len; i++)
		{
			GtkTreeIter copy;

			child.user_data2 = GINT_TO_POINTER(i);
			scp_tree_store_append(store, &copy, dest_iter);
			scp_copy_element(store, &child, &copy);
		}
	}
}

gboolean scp_tree_store_drag_data_received(ScpTreeStore *store, GtkTreePath *dest_path,
	GtkSelectionData *selection_data)
{
	GtkTreeModel *src_model = NULL;
	GtkTreePath *src_path = NULL;
	GtkTreeIter src_iter;
	gboolean result = FALSE;

	validate_store(store);

	if (gtk_tree_get_row_drag_data(selection_data, &src_model, &src_path) &&
		src_model == SCP_TREE_MODEL(store) && /* can only receive from ourselves */
		scp_tree_store_get_iter(store, &src_iter, src_path))
	{
		GtkTreeIter parent_iter1;
		GtkTreeIter *parent_iter;
		gint depth = gtk_tree_path_get_depth(dest_path);
		gint src_index = GPOINTER_TO_INT(src_iter.user_data2);
		GtkTreeIter dest_iter;

		if (depth == 1)
			parent_iter = NULL;
		else
		{
			GtkTreePath *parent_path = gtk_tree_path_copy(dest_path);

			gtk_tree_path_up(parent_path);
			scp_tree_store_get_iter(store, &parent_iter1, parent_path);
			parent_iter = &parent_iter1;
			gtk_tree_path_free(parent_path);
		}

		scp_tree_store_insert(store, &dest_iter, parent_iter,
			gtk_tree_path_get_indices(dest_path)[depth - 1]);

		if (src_iter.user_data == dest_iter.user_data &&
			ITER_INDEX(&dest_iter) <= src_index)
		{
			/* inserting dest moved src, so adjust iter */
			src_iter.user_data2 = GINT_TO_POINTER(src_index + 1);
		}

		scp_copy_element(store, &src_iter, &dest_iter);
		result = TRUE;
	}

	if (src_path)
		gtk_tree_path_free(src_path);

	return result;
}

gboolean scp_tree_store_row_drop_possible(ScpTreeStore *store, GtkTreePath *dest_path,
	GtkSelectionData *selection_data)
{
	GtkTreeModel *src_model = NULL;
	GtkTreePath *src_path = NULL;
	gboolean result = FALSE;

	if (!store->priv->sort_func &&  /* reject drops if sorted */
		gtk_tree_get_row_drag_data(selection_data, &src_model, &src_path) &&
		src_model == SCP_TREE_MODEL(store) &&  /* can only drag to ourselves */
		!gtk_tree_path_is_ancestor(src_path, dest_path))  /* Can't drop into ourself. */
	{
		/* Can't drop if dest_path's parent doesn't exist */
		GtkTreeIter iter;
		GtkTreePath *parent_path = gtk_tree_path_copy(dest_path);

		gtk_tree_path_up(parent_path);
		result = gtk_tree_path_get_depth(parent_path) == 0 ||
			scp_tree_store_get_iter(store, &iter, parent_path);
		gtk_tree_path_free(parent_path);
	}

	if (src_path)
		gtk_tree_path_free(src_path);

	return result;
}

/* Sortable */

gboolean scp_tree_store_get_sort_column_id(ScpTreeStore *store, gint *sort_column_id,
	GtkSortType *order)
{
	ScpTreeStorePrivate *priv = store->priv;

	if (sort_column_id)
		*sort_column_id = priv->sort_column_id;

	if (order)
		*order = priv->sort_order;

	return priv->sort_column_id != GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID &&
		priv->sort_column_id != GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID;
}

typedef struct _ScpSortData
{
	ScpTreeStore *store;
	GPtrArray *array;
} ScpSortData;

static gint scp_index_compare(gint *a, gint *b, ScpSortData *sort_data)
{
	ScpTreeStorePrivate *priv = sort_data->store->priv;
	GtkTreeIter iter_a, iter_b;

	iter_a.stamp = iter_b.stamp = priv->stamp;
	iter_a.user_data = iter_b.user_data = sort_data->array;
	iter_a.user_data2 = GINT_TO_POINTER(*a);
	iter_b.user_data2 = GINT_TO_POINTER(*b);

	return scp_store_compare(sort_data->store, priv->sort_func, &iter_a, &iter_b,
		priv->headers[priv->sort_column_id].data);
}

static void scp_sort_children(ScpTreeStore *store, GtkTreeIter *parent)
{
	GPtrArray *array = (parent ? ITER_ELEM(parent) : store->priv->root)->children;

	if (array && array->len)
	{
		gint *new_order = g_new(gint, array->len);
		ScpSortData sort_data = { store, array };
		GtkTreeIter iter;
		guint i;

		for (i = 0; i < array->len; i++)
			new_order[i] = i;

		g_qsort_with_data(new_order, array->len, sizeof(gint),
			(GCompareDataFunc) scp_index_compare, &sort_data);
		scp_reorder_array(store, parent, array, new_order);
		g_free(new_order);

		iter.stamp = store->priv->stamp;
		iter.user_data = array;

		for (i = 0; i < array->len; i++)
		{
			iter.user_data2 = GINT_TO_POINTER(i);
			scp_sort_children(store, &iter);
		}
	}
}

static void scp_store_sort(ScpTreeStore *store)
{
	if (store->priv->sort_func)
		scp_sort_children(store, NULL);
}

void scp_tree_store_set_sort_column_id(ScpTreeStore *store, gint sort_column_id,
	GtkSortType order)
{
	ScpTreeStorePrivate *priv = store->priv;

	if (priv->sort_column_id != sort_column_id || priv->sort_order != order)
	{

		if (sort_column_id == GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
			priv->sort_func = NULL;
		else
		{
			g_return_if_fail((guint) (sort_column_id + 1) < priv->n_columns + 1);
			g_return_if_fail(priv->headers[sort_column_id].func != NULL);
			priv->sort_func = priv->headers[sort_column_id].func;
		}

		priv->sort_column_id = sort_column_id;
		priv->sort_order = order;
		gtk_tree_sortable_sort_column_changed(GTK_TREE_SORTABLE(store));
		scp_store_sort(store);
	}
}

void scp_tree_store_set_sort_func(ScpTreeStore *store, gint sort_column_id,
	GtkTreeIterCompareFunc func, gpointer data, GDestroyNotify destroy)
{
	ScpTreeStorePrivate *priv = store->priv;

	scp_tree_data_set_header(priv->headers, sort_column_id, func, data, destroy);

	if (priv->sort_column_id == sort_column_id)
	{
		priv->sort_func = func;
		scp_store_sort(store);
	}
}

void scp_tree_store_set_default_sort_func(ScpTreeStore *store, GtkTreeIterCompareFunc func,
	gpointer data, GDestroyNotify destroy)
{
	ScpTreeStorePrivate *priv = store->priv;

	scp_tree_data_set_header(priv->headers, GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, func,
		data, destroy);

	if (priv->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
	{
		priv->sort_func = func;
		scp_store_sort(store);
	}
}

gboolean scp_tree_store_has_default_sort_func(ScpTreeStore *store)
{
	return store->priv->headers[GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID].func != NULL;
}

/* Buildable */

/* GtkBuildable custom tags implementation
 *
 * <columns>
 *   <column type="..."/>
 *   <column type="string" utf8_collate="false"/>
 * </columns>
 */
typedef struct _GSListSubParserData
{
	GtkBuilder *builder;
	GObject *object;
	const gchar *name;
	GArray *types;
	GArray *collates;
} GSListSubParserData;

static void tree_model_start_element(G_GNUC_UNUSED GMarkupParseContext *context,
	G_GNUC_UNUSED const gchar *element_name, const gchar **names, const gchar **values,
	gpointer user_data, G_GNUC_UNUSED GError **error)
{
	guint i;
	GSListSubParserData *data = (GSListSubParserData *) user_data;
	gboolean type_processed = FALSE;

	for (i = 0; names[i]; i++)
	{
		if (!strcmp(names[i], "type"))
		{
			GType type = gtk_builder_get_type_from_name(data->builder, values[i]);
			gboolean collate = g_type_is_a(type, G_TYPE_STRING);

			if (type == G_TYPE_INVALID)
			{
				g_warning("%s: Unknown type %s specified in treemodel %s", G_STRFUNC,
					values[i], data->name);
			}

			g_array_append_val(data->types, type);
			g_array_append_val(data->collates, collate);
			type_processed = TRUE;
		}
		else if (!strcmp(names[i], "utf8_collate"))
		{
			GValue value = G_VALUE_INIT;
			GError *error = NULL;

			if (!type_processed)
			{
				g_warning("%s: Attribute %s must be preceded by type in treemodel %s",
					G_STRFUNC, names[i], data->name);
			}
			else if (!gtk_builder_value_from_string_type(data->builder, G_TYPE_BOOLEAN,
				values[i], &value, &error))
			{
				g_warning("%s: %s for %s in treemodel %s", G_STRFUNC, error->message,
					names[i], data->name);
				g_error_free(error);
			}
			else
			{
				g_array_index(data->collates, gboolean, data->collates->len - 1) =
					g_value_get_boolean(&value);
				g_value_unset(&value);
			}
		}
	}
}

static void tree_model_end_element(G_GNUC_UNUSED GMarkupParseContext *context,
	const gchar *element_name, gpointer user_data, G_GNUC_UNUSED GError **error)
{
	GSListSubParserData *data = (GSListSubParserData *) user_data;

	g_assert(data->builder);

	if (!strcmp(element_name, "columns"))
	{
		guint i;

		scp_tree_store_set_column_types(SCP_TREE_STORE(data->object), data->types->len,
			(GType *) data->types->data);

		for (i = 0; i < data->collates->len; i++)
			if (g_array_index(data->collates, gboolean, i))
				scp_tree_store_set_utf8_collate(SCP_TREE_STORE(data->object), i, TRUE);
	}
}

static const GMarkupParser tree_model_parser =
{
	tree_model_start_element,
	tree_model_end_element,
	NULL, NULL, NULL
};

static gboolean scp_tree_store_buildable_custom_tag_start(GtkBuildable *buildable,
	GtkBuilder *builder, GObject *child, const gchar *tagname, GMarkupParser *parser,
	gpointer *user_data)
{
	if (!child && !strcmp(tagname, "columns"))
	{
		GSListSubParserData *parser_data = g_slice_new0(GSListSubParserData);

		parser_data->builder = builder;
		parser_data->object = G_OBJECT(buildable);
		parser_data->name = gtk_buildable_get_name(buildable);
		parser_data->types = g_array_new(FALSE, FALSE, sizeof(GType));
		parser_data->collates = g_array_new(FALSE, FALSE, sizeof(gboolean));
		*parser = tree_model_parser;
		*user_data = parser_data;
		return TRUE;
	}

	return FALSE;
}

static void scp_tree_store_buildable_custom_finished(G_GNUC_UNUSED GtkBuildable *buildable,
	G_GNUC_UNUSED GtkBuilder *builder, G_GNUC_UNUSED GObject *child, const gchar *tagname,
	gpointer user_data)
{
	if (!strcmp(tagname, "columns"))
	{
		GSListSubParserData *data = (GSListSubParserData *) user_data;

		g_array_free(data->types, TRUE);
		g_array_free(data->collates, TRUE);
		g_slice_free(GSListSubParserData, data);
	}
}

/* Extra */

void scp_tree_store_set_allocation(ScpTreeStore *store, guint toplevel_reserved,
	guint sublevel_reserved, gboolean sublevel_discard)
{
	g_object_set(G_OBJECT(store), "sublevel-discard", sublevel_discard,
		"sublevel_reserved", sublevel_reserved, toplevel_reserved ?
		"toplevel-reserved" : NULL, toplevel_reserved, NULL);
}

void scp_tree_store_set_utf8_collate(ScpTreeStore *store, gint column, gboolean collate)
{
	ScpTreeStorePrivate *priv = store->priv;

	g_return_if_fail(SCP_IS_TREE_STORE(store));
	g_return_if_fail((guint) column < priv->n_columns);

	if (g_type_is_a(priv->headers[column].type, G_TYPE_STRING))
	{
		if (priv->headers[column].utf8_collate != collate)
		{
			priv->headers[column].utf8_collate = collate;

			if (priv->sort_func && (priv->sort_column_id == column ||
				priv->sort_func != scp_tree_model_compare_func))
			{
				scp_store_sort(store);
			}
		}
	}
	else if (collate)
		g_warning("%s: Attempt to set uft8_collate for a non-string type\n", G_STRFUNC);
}

gboolean scp_tree_store_get_utf8_collate(ScpTreeStore *store, gint column)
{
	ScpTreeStorePrivate *priv = store->priv;

	g_return_val_if_fail(SCP_IS_TREE_STORE(store), FALSE);
	g_return_val_if_fail((guint) column < priv->n_columns, FALSE);
	return priv->headers[column].utf8_collate;
}

#define scp_data_string(data) ((data)->v_string ? (data)->v_string : "")

gint scp_tree_store_compare_func(ScpTreeStore *store, GtkTreeIter *a, GtkTreeIter *b,
	gpointer data)
{
	ScpTreeStorePrivate *priv = store->priv;
	gint column = GPOINTER_TO_INT(data);
	GType type = priv->headers[column].type;
	ScpTreeData data_a, data_b;

	scp_tree_store_get(store, a, column, &data_a, -1);
	scp_tree_store_get(store, b, column, &data_b, -1);
	return priv->headers[column].utf8_collate ?
		g_utf8_collate(scp_data_string(&data_a), scp_data_string(&data_b)) :
		scp_tree_data_compare_func(&data_a, &data_b, type);
}

gboolean scp_tree_store_iter_seek(VALIDATE_ONLY ScpTreeStore *store, GtkTreeIter *iter,
	gint position)
{
	GPtrArray *array = ITER_ARRAY(iter);

	g_return_val_if_fail(SCP_IS_TREE_STORE(store), FALSE);
	g_return_val_if_fail(VALID_ITER(iter, store), FALSE);

	if (position == -1)
		iter->user_data2 = GINT_TO_POINTER(array->len - 1);
	else if ((guint) position < array->len)
		iter->user_data2 = GINT_TO_POINTER(position);
	else
	{
		iter->stamp = 0;
		return FALSE;
	}

	return TRUE;
}

static gint scp_collate_data(const gchar *key, const gchar *data)
{
	gchar *key1 = g_utf8_collate_key(data, -1);
	gint result = strcmp(key, key1);

	g_free(key1);
	return result;
}

#define scp_compare_data(a, b, type) \
	((type) == G_TYPE_NONE ? scp_collate_data((a)->v_string, scp_data_string(b)) : \
	scp_tree_data_compare_func((a), (b), (type)))

static gboolean scp_linear_search(GPtrArray *array, gint column, ScpTreeData *data, GType type,
	GtkTreeIter *iter, gboolean sublevels)
{
	if (array)
	{
		guint i;

		for (i = 0; i < array->len; i++)
		{
			AElem *elem = ((AElem *) (array)->pdata[i]);

			if (!scp_compare_data(data, elem->data + column, type))
			{
				iter->user_data = array;
				iter->user_data2 = GINT_TO_POINTER(i);
				return TRUE;
			}

			if (sublevels)
				if (scp_linear_search(elem->children, column, data, type, iter, TRUE))
					return TRUE;
		}
	}

	return FALSE;
}

static gboolean scp_binary_search(GPtrArray *array, gint column, ScpTreeData *data, GType type,
	GtkTreeIter *iter, gboolean sublevels)
{
	if (array)
	{
		gint low = 0, high = array->len - 1;

		while (low <= high)
		{
			gint mid = (low + high) / 2;
			AElem *elem = ((AElem *) (array)->pdata[mid]);
			gint result = scp_compare_data(data, elem->data + column, type);

			if (!result)
			{
				iter->user_data = array;
				iter->user_data2 = GINT_TO_POINTER(mid);
				return TRUE;
			}

			if (result > 0)
				low = mid + 1;
			else
				high = mid - 1;
		}

		if (sublevels)
		{
			guint i;

			for (i = 0; i < array->len; i++)
			{
				AElem *elem = ((AElem *) (array)->pdata[i]);

				if (scp_binary_search(elem->children, column, data, type, iter, TRUE))
					return TRUE;
			}
		}
	}

	return FALSE;
}

gboolean scp_tree_store_search(ScpTreeStore *store, gboolean sublevels, gboolean linear_order,
	GtkTreeIter *iter, GtkTreeIter *parent, gint column, ...)
{
	ScpTreeStorePrivate *priv = store->priv;
	GPtrArray *array;
	GType type;
	va_list ap;
	ScpTreeData data;
	gboolean found;

	g_return_val_if_fail(SCP_IS_TREE_STORE(store), FALSE);
	g_return_val_if_fail(VALID_ITER_OR_NULL(parent, store), FALSE);
	g_return_val_if_fail((guint) column < priv->n_columns, FALSE);
	g_return_val_if_fail(sublevels == FALSE || priv->sublevels == TRUE, FALSE);

	array = (parent ? ITER_ELEM(parent) : priv->root)->children;
	type = priv->headers[column].type;
	iter->stamp = priv->stamp;
	iter->user_data = NULL;
	va_start(ap, column);
	scp_tree_data_from_stack(&data, type, ap, FALSE);
	va_end(ap);

	if (priv->headers[column].utf8_collate)
	{
		type = G_TYPE_NONE;
		data.v_string = g_utf8_collate_key(scp_data_string(&data), -1);
	}

	found = !linear_order && column == priv->sort_column_id &&
		priv->sort_func == scp_tree_model_compare_func ?
		scp_binary_search(array, column, &data, type, iter, sublevels) :
		scp_linear_search(array, column, &data, type, iter, sublevels);

	if (type == G_TYPE_NONE)
		g_free(data.v_string);

	return found;
}

static gboolean scp_traverse(ScpTreeStore *store, GPtrArray *array, GtkTreeIter *iter,
	gboolean sublevels, ScpTreeStoreTraverseFunc func, gpointer gdata)
{
	if (array)
	{
		guint i = 0;

		iter->user_data = array;
		iter->user_data2 = GINT_TO_POINTER(0);

		while (i < array->len)
		{
			gint result = func(store, iter, gdata);

			if (result > 0)
				return TRUE;

			if (!result)
			{
				if (sublevels)
				{
					if (scp_traverse(store, ((AElem *) array->pdata[i])->children,
						iter, TRUE, func, gdata) > 0)
					{
						return TRUE;
					}

					iter->user_data = array;
				}

				iter->user_data2 = GINT_TO_POINTER(++i);
			}
			else
				scp_tree_store_remove(store, iter);
		}
	}

	return FALSE;
}

gboolean scp_tree_store_traverse(ScpTreeStore *store, gboolean sublevels, GtkTreeIter *iter,
	GtkTreeIter *parent, ScpTreeStoreTraverseFunc func, gpointer gdata)
{
	ScpTreeStorePrivate *priv = store->priv;
	GtkTreeIter iter1;
	
	g_return_val_if_fail(SCP_IS_TREE_STORE(store), FALSE);
	g_return_val_if_fail(VALID_ITER_OR_NULL(parent, store), FALSE);
	g_return_val_if_fail(sublevels == FALSE || priv->sublevels == TRUE, FALSE);
	g_return_val_if_fail(func != NULL, FALSE);

	if (!iter)
		iter = &iter1;

	iter->stamp = priv->stamp;

	if (!scp_traverse(store, (parent ? ITER_ELEM(parent) : priv->root)->children, iter,
		sublevels, func, gdata))
	{
		iter->stamp = 0;
		return FALSE;
	}

	return TRUE;
}

/* Class */

static void scp_tree_store_tree_model_init(GtkTreeModelIface *iface)
{
	iface->get_flags = (GtkTreeModelFlags (*)(GtkTreeModel *)) scp_tree_store_get_flags;
	iface->get_n_columns = (gint (*)(GtkTreeModel *)) scp_tree_store_get_n_columns;
	iface->get_column_type = (GType (*)(GtkTreeModel *, gint)) scp_tree_store_get_column_type;
	iface->get_iter = (gboolean (*)(GtkTreeModel *, GtkTreeIter *, GtkTreePath *))
		scp_tree_store_get_iter;
	iface->get_path = (GtkTreePath *(*)(GtkTreeModel *, GtkTreeIter *)) scp_tree_store_get_path;
	iface->get_value = (void (*)(GtkTreeModel *, GtkTreeIter *, gint, GValue *))
		scp_tree_store_get_value;
	iface->iter_next = (gboolean (*)(GtkTreeModel *, GtkTreeIter *)) scp_tree_store_iter_next;
#if GTK_CHECK_VERSION(3, 0, 0)
	iface->iter_previous = (gboolean (*)(GtkTreeModel *, GtkTreeIter *))
		scp_tree_store_iter_previous;
#endif
	iface->iter_children = (gboolean (*)(GtkTreeModel *, GtkTreeIter *, GtkTreeIter *))
		scp_tree_store_iter_children;
	iface->iter_has_child = (gboolean (*)(GtkTreeModel *, GtkTreeIter *))
		scp_tree_store_iter_has_child;
	iface->iter_n_children = (gint (*)(GtkTreeModel *, GtkTreeIter *))
		scp_tree_store_iter_n_children;
	iface->iter_nth_child = (gboolean (*)(GtkTreeModel *, GtkTreeIter *, GtkTreeIter *,
		gint)) scp_tree_store_iter_nth_child;
	iface->iter_parent = (gboolean (*)(GtkTreeModel *model, GtkTreeIter *, GtkTreeIter *))
		scp_tree_store_iter_parent;
}

static void scp_tree_store_drag_source_init(GtkTreeDragSourceIface *iface)
{
	iface->row_draggable = (gboolean (*)(GtkTreeDragSource *, GtkTreePath *))
		scp_tree_store_row_draggable;
	iface->drag_data_delete = (gboolean (*)(GtkTreeDragSource *, GtkTreePath *))
		scp_tree_store_drag_data_delete;
	iface->drag_data_get = (gboolean (*)(GtkTreeDragSource *, GtkTreePath *,
		GtkSelectionData *)) scp_tree_store_drag_data_get;
}

static void scp_tree_store_drag_dest_init(GtkTreeDragDestIface *iface)
{
	iface->drag_data_received = (gboolean (*)(GtkTreeDragDest *, GtkTreePath *,
		GtkSelectionData *)) scp_tree_store_drag_data_received;
	iface->row_drop_possible = (gboolean (*)(GtkTreeDragDest *, GtkTreePath *,
		GtkSelectionData *)) scp_tree_store_row_drop_possible;
}

static void scp_tree_store_sortable_init(GtkTreeSortableIface *iface)
{
	iface->get_sort_column_id = (gboolean (*)(GtkTreeSortable *, gint *, GtkSortType *))
		scp_tree_store_get_sort_column_id;
	iface->set_sort_column_id = (void (*)(GtkTreeSortable *, gint, GtkSortType))
		scp_tree_store_set_sort_column_id;
	iface->set_sort_func = (void (*)(GtkTreeSortable *, gint, GtkTreeIterCompareFunc,
		gpointer, GDestroyNotify)) scp_tree_store_set_sort_func;
	iface->set_default_sort_func = (void (*)(GtkTreeSortable *, GtkTreeIterCompareFunc,
		gpointer, GDestroyNotify)) scp_tree_store_set_default_sort_func;
	iface->has_default_sort_func = (gboolean (*)(GtkTreeSortable *))
		scp_tree_store_has_default_sort_func;
}

static void scp_tree_store_buildable_init(GtkBuildableIface *iface)
{
	iface->custom_tag_start = scp_tree_store_buildable_custom_tag_start;
	iface->custom_finished = scp_tree_store_buildable_custom_finished;
}

enum
{
	PROP_0,
	PROP_SUBLEVELS,
	PROP_TOPLEVEL_RESERVED,
	PROP_SUBLEVEL_RESERVED,
	PROP_SUBLEVEL_DISCARD,
	PROP_UTF8_COLLATE
};

static gpointer scp_tree_store_parent_class = NULL;

static GObject *scp_tree_store_constructor(GType type, guint n_construct_properties,
	GObjectConstructParam *construct_properties)
{
	GObject *object = G_OBJECT_CLASS(scp_tree_store_parent_class)->constructor(type,
		n_construct_properties, construct_properties);
	ScpTreeStorePrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE(object, SCP_TYPE_TREE_STORE,
		ScpTreeStorePrivate);

	((ScpTreeStore *) object)->priv = priv;
	while ((priv->stamp = g_random_int()) == 0);  /* we mark invalid iters with stamp 0 */
	priv->root = g_new0(AElem, 1);
	priv->roar = g_ptr_array_new();
	scp_ptr_array_insert_val(priv->roar, 0, priv->root);
	priv->headers = NULL;
	priv->n_columns = 0;
	priv->sort_column_id = GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID;
	priv->sort_order = GTK_SORT_ASCENDING;
	priv->sort_func = NULL;
	priv->toplevel_reserved = 0;
	priv->sublevel_reserved = 0;
	priv->sublevel_discard = FALSE;
	priv->columns_dirty = FALSE;
	return object;
}

static void scp_tree_store_get_property(GObject *object, guint prop_id, GValue *value,
	GParamSpec *pspec)
{
	ScpTreeStorePrivate *priv = SCP_TREE_STORE(object)->priv;

	switch (prop_id)
	{
		case PROP_SUBLEVELS :
		{
			g_value_set_boolean(value, priv->sublevels);
			break;
		}
		case PROP_TOPLEVEL_RESERVED :
		{
			g_value_set_uint(value, priv->toplevel_reserved);
			break;
		}
		case PROP_SUBLEVEL_RESERVED :
		{
			g_value_set_uint(value, priv->sublevel_reserved);
			break;
		}
		case PROP_SUBLEVEL_DISCARD :
		{
			g_value_set_boolean(value, priv->sublevel_reserved);
			break;
		}
		default : G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

static void scp_tree_store_set_property(GObject *object, guint prop_id, const GValue *value,
	GParamSpec *pspec)
{
	ScpTreeStorePrivate *priv = SCP_TREE_STORE(object)->priv;

	switch (prop_id)
	{
		case PROP_SUBLEVELS :
		{
			priv = G_TYPE_INSTANCE_GET_PRIVATE(object, SCP_TYPE_TREE_STORE,
				ScpTreeStorePrivate);  /* ctor, store->priv is not assigned */
			priv->sublevels = g_value_get_boolean(value);
			break;
		}
		case PROP_TOPLEVEL_RESERVED :
		{
			g_return_if_fail(priv->root->children == NULL);
			priv->toplevel_reserved = g_value_get_uint(value);
			break;
		}
		case PROP_SUBLEVEL_RESERVED :
		{
			g_return_if_fail(priv->sublevels);
			priv->sublevel_reserved = g_value_get_uint(value);
			break;
		}
		case PROP_SUBLEVEL_DISCARD :
		{
			g_return_if_fail(priv->sublevels);
			priv->sublevel_discard = g_value_get_boolean(value);
			break;
		}
		default :
		{
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			return;
		}
	}

#if GLIB_CHECK_VERSION(2, 26, 0)
	g_object_notify_by_pspec(object, pspec);
#else
	g_object_notify(object, pspec->name);
#endif
}

static void scp_tree_store_finalize(GObject *object)
{
	ScpTreeStore *store = SCP_TREE_STORE(object);
	ScpTreeStorePrivate *priv = store->priv;

	scp_free_array(store, priv->root->children);
	g_free(priv->root);
	g_ptr_array_free(priv->roar, TRUE);

	if (priv->headers)
		scp_tree_data_headers_free(priv->n_columns, priv->headers);

	G_OBJECT_CLASS(scp_tree_store_parent_class)->finalize(object);
}

static void scp_tree_store_gobject_init(GObjectClass *class)
{
	scp_tree_store_parent_class = g_type_class_peek_parent(class);
	class->constructor = scp_tree_store_constructor;
	class->finalize = scp_tree_store_finalize;
	class->get_property = scp_tree_store_get_property;
	class->set_property = scp_tree_store_set_property;
}

static void scp_tree_store_class_init(GObjectClass *class)
{
	scp_tree_store_gobject_init(class);
	g_type_class_add_private(class, sizeof(ScpTreeStorePrivate));
	g_assert(GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID == -1);  /* headers[-1] = default */

	g_object_class_install_property(class, PROP_SUBLEVELS,
		g_param_spec_boolean("sublevels", P_("Sublevels"),
		P_("Supports sublevels (as opposed to flat list)"), TRUE,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(class, PROP_TOPLEVEL_RESERVED,
		g_param_spec_uint("toplevel-reserved", P_("Toplevel reserved"),
		P_("Number of pointers preallocated for top-level elements"),
		0, G_MAXINT32, 0, G_PARAM_READWRITE));

	g_object_class_install_property(class, PROP_SUBLEVEL_RESERVED,
		g_param_spec_uint("sublevel-reserved", P_("Sublevel reserved"),
		P_("Number of pointers preallocated for sublevel elements"),
		0, G_MAXINT32, 0, G_PARAM_READWRITE));

	g_object_class_install_property(class, PROP_SUBLEVEL_DISCARD,
		g_param_spec_boolean("sublevel-discard", P_("Sublevel discard"),
		P_("Free sublevel arrays when their last element is removed"),
		FALSE, G_PARAM_READWRITE));
}

static volatile gsize scp_tree_store_type_id_volatile = 0;

GType scp_tree_store_get_type(void)
{
	if (g_once_init_enter(&scp_tree_store_type_id_volatile))
	{
		GType type = g_type_register_static_simple(G_TYPE_OBJECT,
			g_intern_string("ScpTreeStore"), sizeof(ScpTreeStoreClass),
			(GClassInitFunc) scp_tree_store_class_init, sizeof(ScpTreeStore), NULL, 0);
		GInterfaceInfo iface_info = { (GInterfaceInitFunc) scp_tree_store_tree_model_init,
			NULL, NULL };

		g_type_add_interface_static(type, GTK_TYPE_TREE_MODEL, &iface_info);
		iface_info.interface_init = (GInterfaceInitFunc) scp_tree_store_drag_source_init;
		g_type_add_interface_static(type, GTK_TYPE_TREE_DRAG_SOURCE, &iface_info);
		iface_info.interface_init = (GInterfaceInitFunc) scp_tree_store_drag_dest_init;
		g_type_add_interface_static(type, GTK_TYPE_TREE_DRAG_DEST, &iface_info);
		iface_info.interface_init = (GInterfaceInitFunc) scp_tree_store_sortable_init;
		g_type_add_interface_static(type, GTK_TYPE_TREE_SORTABLE, &iface_info);
		iface_info.interface_init = (GInterfaceInitFunc) scp_tree_store_buildable_init;
		g_type_add_interface_static(type, GTK_TYPE_BUILDABLE, &iface_info);
		g_once_init_leave(&scp_tree_store_type_id_volatile, type);
	}

	return scp_tree_store_type_id_volatile;
}

void scp_tree_store_register_dynamic(void)
{
	GType type = g_type_from_name("ScpTreeStore");

	if (!type)
	{
		type = scp_tree_store_get_type();
		g_type_class_unref(g_type_class_ref(type));  /* force class creation */
	}
	else if (!scp_tree_store_type_id_volatile)
	{
		/* external registration, repair */
		gpointer class = g_type_class_peek(type);
		gpointer iface = g_type_interface_peek(class, GTK_TYPE_TREE_MODEL);

		scp_tree_store_gobject_init((GObjectClass *) class);
		scp_tree_store_tree_model_init((GtkTreeModelIface *) iface);
		iface = g_type_interface_peek(class, GTK_TYPE_TREE_DRAG_SOURCE);
		scp_tree_store_drag_source_init((GtkTreeDragSourceIface *) iface);
		iface = g_type_interface_peek(class, GTK_TYPE_TREE_DRAG_DEST);
		scp_tree_store_drag_dest_init((GtkTreeDragDestIface *) iface);
		iface = g_type_interface_peek(class, GTK_TYPE_TREE_SORTABLE);
		scp_tree_store_sortable_init((GtkTreeSortableIface *) iface);
		iface = g_type_interface_peek(class, GTK_TYPE_BUILDABLE);
		scp_tree_store_buildable_init((GtkBuildableIface *) iface);
		scp_tree_store_type_id_volatile = type;
	}
}
