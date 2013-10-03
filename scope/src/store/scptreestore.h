/* gtktreestore.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * scptreestore.h
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

#ifndef __SCP_TREE_STORE_H__

G_BEGIN_DECLS

#define SCP_TYPE_TREE_STORE (scp_tree_store_get_type())
#define SCP_TREE_STORE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SCP_TYPE_TREE_STORE, \
	ScpTreeStore))
#define SCP_TREE_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SCP_TYPE_TREE_STORE, \
	ScpTreeStoreClass))
#define SCP_IS_TREE_STORE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SCP_TYPE_TREE_STORE))
#define SCP_IS_TREE_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SCP_TYPE_TREE_STORE))
#define SCP_TREE_STORE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), SCP_TYPE_TREE_STORE, \
	ScpTreeStoreClass))

typedef struct _ScpTreeStorePrivate ScpTreeStorePrivate;

typedef struct _ScpTreeStore
{
	GObject parent;
	ScpTreeStorePrivate *priv;
} ScpTreeStore;

typedef struct _ScpTreeStoreClass
{
	GObjectClass parent_class;
	/* Padding for future expansion */
	void (*_scp_reserved1)(void);
	void (*_scp_reserved2)(void);
	void (*_scp_reserved3)(void);
	void (*_scp_reserved4)(void);
} ScpTreeStoreClass;

/* Store */
GType scp_tree_store_get_type(void);
ScpTreeStore *scp_tree_store_new(gboolean sublevels, gint n_columns, ...);
ScpTreeStore *scp_tree_store_newv(gboolean sublevels, gint n_columns, GType *types);
gboolean scp_tree_store_set_column_types(ScpTreeStore *store, gint n_columns, GType *types);
void scp_tree_store_set_valuesv(ScpTreeStore *store, GtkTreeIter *iter, gint *columns,
	GValue *values, gint n_values);
void scp_tree_store_set_value(ScpTreeStore *store, GtkTreeIter *iter, gint column,
	GValue *value);
void scp_tree_store_set_valist(ScpTreeStore *store, GtkTreeIter *iter, va_list ap);
void scp_tree_store_set(ScpTreeStore *store, GtkTreeIter *iter, ...);
gboolean scp_tree_store_remove(ScpTreeStore *store, GtkTreeIter *iter);
void scp_tree_store_insert(ScpTreeStore *store, GtkTreeIter *iter, GtkTreeIter *parent,
	gint position);
#define scp_tree_store_prepend(store, iter, parent) \
	scp_tree_store_insert((store), (iter), (parent), 0)
#define scp_tree_store_append(store, iter, parent) \
	scp_tree_store_insert((store), (iter), (parent), -1)
void scp_tree_store_insert_with_valuesv(ScpTreeStore *store, GtkTreeIter *iter,
	GtkTreeIter *parent, gint position, gint *columns, GValue *values, gint n_values);
#define scp_tree_store_prepend_with_valuesv(store, iter, parent, columns, values, n_values) \
	scp_tree_store_insert_with_valuesv(store, iter, parent, 0, columns, values, n_values)
#define scp_tree_store_append_with_valuesv(store, iter, parent, columns, values, n_values) \
	scp_tree_store_insert_with_valuesv(store, iter, parent, -1, columns, values, n_values)
void scp_tree_store_insert_with_valist(ScpTreeStore *store, GtkTreeIter *iter, \
	GtkTreeIter *parent, gint position, va_list ap);
#define scp_tree_store_prepend_with_valist(store, iter, parent, ap) \
	scp_tree_store_insert_with_valist(store, iter, parent, 0, ap)
#define scp_tree_store_append_with_valist(store, iter, parent, ap) \
	scp_tree_store_insert_with_valist(store, iter, parent, -1, ap)
void scp_tree_store_insert_with_values(ScpTreeStore *store, GtkTreeIter *iter,
	GtkTreeIter *parent, gint position, ...);
#define scp_tree_store_prepend_with_values(store, iter, parent, ...) \
	scp_tree_store_insert_with_values(store, iter, parent, 0, __VA_ARGS__)
#define scp_tree_store_append_with_values(store, iter, parent, ...) \
	scp_tree_store_insert_with_values(store, iter, parent, -1, __VA_ARGS__)
void scp_tree_store_get_valist(ScpTreeStore *store, GtkTreeIter *iter, va_list var_args);
void scp_tree_store_get(ScpTreeStore *store, GtkTreeIter *iter, ...);
gboolean scp_tree_store_is_ancestor(ScpTreeStore *store, GtkTreeIter *iter,
	GtkTreeIter *descendant);
gint scp_tree_store_iter_depth(ScpTreeStore *store, GtkTreeIter *iter);
void scp_tree_store_clear_children(ScpTreeStore *store, GtkTreeIter *parent,
	gboolean emit_subsignals);
#define scp_tree_store_clear(store) scp_tree_store_clear_children((store), NULL, TRUE)
gboolean scp_tree_store_iter_is_valid(ScpTreeStore *store, GtkTreeIter *iter);
void scp_tree_store_reorder(ScpTreeStore *store, GtkTreeIter *parent, gint *new_order);
void scp_tree_store_swap(ScpTreeStore *store, GtkTreeIter *a, GtkTreeIter *b);
void scp_tree_store_move(ScpTreeStore *store, GtkTreeIter *iter, gint position);
gint scp_tree_store_iter_tell(ScpTreeStore *store, GtkTreeIter *iter);

/* Model */
GtkTreeModelFlags scp_tree_store_get_flags(ScpTreeStore *store);
gint scp_tree_store_get_n_columns(ScpTreeStore *store);
GType scp_tree_store_get_column_type(ScpTreeStore *store, gint index);
gboolean scp_tree_store_get_iter(ScpTreeStore *store, GtkTreeIter *iter, GtkTreePath *path);
GtkTreePath *scp_tree_store_get_path(ScpTreeStore *store, GtkTreeIter *iter);
void scp_tree_store_get_value(ScpTreeStore *model, GtkTreeIter *iter, gint column,
	GValue *value);
gboolean scp_tree_store_iter_next(ScpTreeStore *store, GtkTreeIter *iter);
gboolean scp_tree_store_iter_previous(ScpTreeStore *store, GtkTreeIter *iter);
gboolean scp_tree_store_iter_children(ScpTreeStore *store, GtkTreeIter *iter,
	GtkTreeIter *parent);
gboolean scp_tree_store_iter_has_child(ScpTreeStore *store, GtkTreeIter *iter);
gint scp_tree_store_iter_n_children(ScpTreeStore *store, GtkTreeIter *iter);
gboolean scp_tree_store_iter_nth_child(ScpTreeStore *store, GtkTreeIter *iter,
	GtkTreeIter *parent, gint n);
#define scp_tree_store_get_iter_first(store, iter) \
	scp_tree_store_iter_nth_child((store), (iter), NULL, 0)
gboolean scp_tree_store_iter_parent(ScpTreeStore *store, GtkTreeIter *iter,
	GtkTreeIter *child);
void scp_tree_store_foreach(ScpTreeStore *store, GtkTreeModelForeachFunc func, gpointer gdata);

/* DND */
gboolean scp_tree_store_row_draggable(ScpTreeStore *store, GtkTreePath *path);
gboolean scp_tree_store_drag_data_delete(ScpTreeStore *store, GtkTreePath *path);
gboolean scp_tree_store_drag_data_get(ScpTreeStore *store, GtkTreePath *path,
	GtkSelectionData *selection_data);
gboolean scp_tree_store_drag_data_received(ScpTreeStore *store, GtkTreePath *dest,
	GtkSelectionData *selection_data);
gboolean scp_tree_store_row_drop_possible(ScpTreeStore *store, GtkTreePath *dest_path,
	GtkSelectionData *selection_data);

/* Sortable */
gboolean scp_tree_store_get_sort_column_id(ScpTreeStore *store, gint *sort_column_id,
	GtkSortType *order);
void scp_tree_store_set_sort_column_id(ScpTreeStore *store, gint sort_column_id,
	GtkSortType order);
void scp_tree_store_set_sort_func(ScpTreeStore *store, gint sort_column_id,
	GtkTreeIterCompareFunc func, gpointer data, GDestroyNotify destroy);
void scp_tree_store_set_default_sort_func(ScpTreeStore *store, GtkTreeIterCompareFunc func,
	gpointer data, GDestroyNotify destroy);
gboolean scp_tree_store_has_default_sort_func(ScpTreeStore *store);

/* Extra */
void scp_tree_store_set_allocation(ScpTreeStore *store, guint toplevel_reserved,
	guint sublevel_reserved, gboolean sublevel_discard);
void scp_tree_store_set_utf8_collate(ScpTreeStore *store, gint column, gboolean collate);
gboolean scp_tree_store_get_utf8_collate(ScpTreeStore *store, gint column);
gint scp_tree_store_compare_func(ScpTreeStore *store, GtkTreeIter *a, GtkTreeIter *b,
	gpointer data);
gboolean scp_tree_store_iter_seek(ScpTreeStore *store, GtkTreeIter *iter, gint position);
gboolean scp_tree_store_search(ScpTreeStore *store, gboolean sublevels, gboolean linear_order,
	GtkTreeIter *iter, GtkTreeIter *parent, gint column, ...);
typedef gint (*ScpTreeStoreTraverseFunc)(ScpTreeStore *store, GtkTreeIter *iter,
	gpointer gdata);
gboolean scp_tree_store_traverse(ScpTreeStore *store, gboolean sublevels, GtkTreeIter *iter,
	GtkTreeIter *parent, ScpTreeStoreTraverseFunc func, gpointer gdata);
void scp_tree_store_register_dynamic(void);

G_END_DECLS

#define __SCP_TREE_STORE_H__ 1
#endif
