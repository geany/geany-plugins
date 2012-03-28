/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2003 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2003      CodeFactory AB
 * Copyright (C) 2008      Imendio AB
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "dh-marshal.h"
#include "dh-book-tree.h"
#include "dh-book.h"

typedef struct {
        const gchar *uri;
        gboolean     found;
        GtkTreeIter  iter;
        GtkTreePath *path;
} FindURIData;

typedef struct {
        GtkTreeStore  *store;
        DhBookManager *book_manager;
        DhLink        *selected_link;
} DhBookTreePriv;

static void dh_book_tree_class_init        (DhBookTreeClass  *klass);
static void dh_book_tree_init              (DhBookTree       *tree);
static void book_tree_add_columns          (DhBookTree       *tree);
static void book_tree_setup_selection      (DhBookTree       *tree);
static void book_tree_populate_tree        (DhBookTree       *tree);
static void book_tree_insert_node          (DhBookTree       *tree,
                                            GNode            *node,
                                            GtkTreeIter      *parent_iter);
static void book_tree_selection_changed_cb (GtkTreeSelection *selection,
                                            DhBookTree       *tree);

enum {
        LINK_SELECTED,
        LAST_SIGNAL
};

enum {
	COL_TITLE,
	COL_LINK,
	COL_WEIGHT,
	N_COLUMNS
};

G_DEFINE_TYPE (DhBookTree, dh_book_tree, GTK_TYPE_TREE_VIEW);

#define GET_PRIVATE(instance) G_TYPE_INSTANCE_GET_PRIVATE \
  (instance, DH_TYPE_BOOK_TREE, DhBookTreePriv);

static gint signals[LAST_SIGNAL] = { 0 };

static void
book_tree_finalize (GObject *object)
{
	DhBookTreePriv *priv = GET_PRIVATE (object);

	g_object_unref (priv->store);
        g_object_unref (priv->book_manager);

        G_OBJECT_CLASS (dh_book_tree_parent_class)->finalize (object);
}

static void
dh_book_tree_class_init (DhBookTreeClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = book_tree_finalize;

        signals[LINK_SELECTED] =
                g_signal_new ("link-selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      _dh_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	g_type_class_add_private (klass, sizeof (DhBookTreePriv));
}

static void
dh_book_tree_init (DhBookTree *tree)
{
        DhBookTreePriv *priv;

        priv = GET_PRIVATE (tree);

	priv->store = gtk_tree_store_new (N_COLUMNS,
					  G_TYPE_STRING,
					  G_TYPE_POINTER,
                                          PANGO_TYPE_WEIGHT);
	priv->selected_link = NULL;
	gtk_tree_view_set_model (GTK_TREE_VIEW (tree),
				 GTK_TREE_MODEL (priv->store));

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree), FALSE);

	book_tree_add_columns (tree);

	book_tree_setup_selection (tree);
}

static void
book_tree_add_columns (DhBookTree *tree)
{
	GtkCellRenderer   *cell;
	GtkTreeViewColumn *column;

	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_attributes (column, cell,
					     "text", COL_TITLE,
                                             "weight", COL_WEIGHT,
					     NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
}

static void
book_tree_setup_selection (DhBookTree *tree)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));

	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (book_tree_selection_changed_cb),
			  tree);
}

static void
book_tree_populate_tree (DhBookTree *tree)
{
        DhBookTreePriv *priv = GET_PRIVATE (tree);
        GList          *l;

        gtk_tree_store_clear (priv->store);

        for (l = dh_book_manager_get_books (priv->book_manager);
             l;
             l = g_list_next (l)) {
                DhBook *book = DH_BOOK (l->data);
                GNode  *node;

                node = dh_book_get_tree (book);
                while(node) {
                        book_tree_insert_node (tree, node, NULL);
                        node = g_node_next_sibling (node);
                }
        }
}

static void
book_manager_disabled_book_list_changed_cb (DhBookManager *book_manager,
                                            gpointer user_data)
{
        DhBookTree *tree = user_data;
        book_tree_populate_tree (tree);
}

static void
book_tree_insert_node (DhBookTree  *tree,
		       GNode       *node,
		       GtkTreeIter *parent_iter)

{
        DhBookTreePriv *priv = GET_PRIVATE (tree);
	DhLink         *link;
	GtkTreeIter     iter;
        PangoWeight     weight;
	GNode          *child;

	link = node->data;

	gtk_tree_store_append (priv->store, &iter, parent_iter);

	if (dh_link_get_link_type (link) == DH_LINK_TYPE_BOOK) {
                weight = PANGO_WEIGHT_BOLD;
	} else {
                weight = PANGO_WEIGHT_NORMAL;
        }

        gtk_tree_store_set (priv->store, &iter,
                            COL_TITLE, dh_link_get_name (link),
                            COL_LINK, link,
                            COL_WEIGHT, weight,
                            -1);

	for (child = g_node_first_child (node);
	     child;
	     child = g_node_next_sibling (child)) {
		book_tree_insert_node (tree, child, &iter);
	}
}

static void
book_tree_selection_changed_cb (GtkTreeSelection *selection,
				DhBookTree       *tree)
{
        DhBookTreePriv *priv = GET_PRIVATE (tree);
        GtkTreeIter     iter;

	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
                DhLink *link;

		gtk_tree_model_get (GTK_TREE_MODEL (priv->store),
				    &iter,
                                    COL_LINK, &link,
                                    -1);
		if (link != priv->selected_link) {
			g_signal_emit (tree, signals[LINK_SELECTED], 0, link);
		}
		priv->selected_link = link;
	}
}

GtkWidget *
dh_book_tree_new (DhBookManager *book_manager)
{
        DhBookTree     *tree;
        DhBookTreePriv *priv;
	GtkTreeSelection *selection;
	GtkTreeIter     iter;
	DhLink *link;

	tree = g_object_new (DH_TYPE_BOOK_TREE, NULL);
        priv = GET_PRIVATE (tree);

        priv->book_manager = g_object_ref (book_manager);
        g_signal_connect (priv->book_manager,
                          "disabled-book-list-updated",
                          G_CALLBACK (book_manager_disabled_book_list_changed_cb),
                          tree);

        book_tree_populate_tree (tree);

	/* Mark the first item as selected, or it would get automatically
	 * selected when the treeview will get focus; but that's not even
	 * enough as a selection changed would still be emitted when there
	 * is no change, hence the manual tracking of selection in
	 * selected_link.
	 *   https://bugzilla.gnome.org/show_bug.cgi?id=492206
	 */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
	g_signal_handlers_block_by_func	(selection,
					 book_tree_selection_changed_cb,
					 tree);
	gtk_tree_model_get_iter_first ( GTK_TREE_MODEL (priv->store), &iter);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store),
			&iter, COL_LINK, &link, -1);
	priv->selected_link = link;
	gtk_tree_selection_select_iter (selection, &iter);
	g_signal_handlers_unblock_by_func (selection,
					   book_tree_selection_changed_cb,
					   tree);

        return GTK_WIDGET (tree);
}

static gboolean
book_tree_find_uri_foreach (GtkTreeModel *model,
			    GtkTreePath  *path,
			    GtkTreeIter  *iter,
			    FindURIData  *data)
{
	DhLink      *link;
        gchar       *link_uri;

	gtk_tree_model_get (model, iter,
			    COL_LINK, &link,
			    -1);

        link_uri = dh_link_get_uri (link);
	if (g_str_has_prefix (data->uri, link_uri)) {
		data->found = TRUE;
		data->iter = *iter;
		data->path = gtk_tree_path_copy (path);
	}
        g_free (link_uri);

	return data->found;
}

void
dh_book_tree_select_uri (DhBookTree  *tree,
			 const gchar *uri)
{
        DhBookTreePriv   *priv = GET_PRIVATE (tree);
	GtkTreeSelection *selection;
	FindURIData       data;

	data.found = FALSE;
	data.uri = uri;

	gtk_tree_model_foreach (GTK_TREE_MODEL (priv->store),
				(GtkTreeModelForeachFunc) book_tree_find_uri_foreach,
				&data);

	if (!data.found) {
		return;
	}

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));

	g_signal_handlers_block_by_func	(selection,
					 book_tree_selection_changed_cb,
					 tree);

	gtk_tree_view_expand_to_path (GTK_TREE_VIEW (tree), data.path);
	gtk_tree_selection_select_iter (selection, &data.iter);
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree), data.path, NULL, 0);

	g_signal_handlers_unblock_by_func (selection,
					   book_tree_selection_changed_cb,
					   tree);

	gtk_tree_path_free (data.path);
}

const gchar *
dh_book_tree_get_selected_book_title (DhBookTree *tree)
{
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	GtkTreePath      *path;
	DhLink           *link;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return NULL;
	}

	path = gtk_tree_model_get_path (model, &iter);

	/* Get the book node for this link. */
	while (1) {
		if (gtk_tree_path_get_depth (path) <= 1) {
			break;
		}

		gtk_tree_path_up (path);
	}

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter,
			    COL_LINK, &link,
			    -1);

	return dh_link_get_name (link);
}
