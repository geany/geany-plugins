/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2003 CodeFactory AB
 * Copyright (C) 2001-2003 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2005-2008 Imendio AB
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
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include "dh-marshal.h"
#include "dh-keyword-model.h"
#include "dh-search.h"
#include "dh-preferences.h"
#include "dh-base.h"
#include "dh-util.h"
#include "dh-book-manager.h"
#include "dh-book.h"

typedef struct {
        DhKeywordModel *model;

        DhBookManager  *book_manager;

        DhLink         *selected_link;

        GtkWidget      *book_combo;
        GtkWidget      *entry;
        GtkWidget      *hitlist;

        GCompletion    *completion;

        guint           idle_complete;
        guint           idle_filter;
} DhSearchPriv;

static void         dh_search_init                  (DhSearch         *search);
static void         dh_search_class_init            (DhSearchClass    *klass);
static void         search_grab_focus               (GtkWidget        *widget);
static void         search_selection_changed_cb     (GtkTreeSelection *selection,
                                                     DhSearch         *content);
static gboolean     search_tree_button_press_cb     (GtkTreeView      *view,
                                                     GdkEventButton   *event,
                                                     DhSearch         *search);
static gboolean     search_entry_key_press_event_cb (GtkEntry         *entry,
                                                     GdkEventKey      *event,
                                                     DhSearch         *search);
static void         search_combo_changed_cb         (GtkComboBox      *combo,
                                                     DhSearch         *search);
static void         search_entry_changed_cb         (GtkEntry         *entry,
                                                     DhSearch         *search);
static void         search_entry_activated_cb       (GtkEntry         *entry,
                                                     DhSearch         *search);
static void         search_entry_text_inserted_cb   (GtkEntry         *entry,
                                                     const gchar      *text,
                                                     gint              length,
                                                     gint             *position,
                                                     DhSearch         *search);
static gboolean     search_complete_idle            (DhSearch         *search);
static gboolean     search_filter_idle              (DhSearch         *search);
static const gchar *search_complete_func            (DhLink           *link);

enum {
        LINK_SELECTED,
        LAST_SIGNAL
};

G_DEFINE_TYPE (DhSearch, dh_search, GTK_TYPE_VBOX);

#define GET_PRIVATE(instance) G_TYPE_INSTANCE_GET_PRIVATE \
  (instance, DH_TYPE_SEARCH, DhSearchPriv);

static gint signals[LAST_SIGNAL] = { 0 };

static void
search_finalize (GObject *object)
{
        DhSearchPriv *priv;

        priv = GET_PRIVATE (object);

        g_completion_free (priv->completion);
        g_object_unref (priv->book_manager);

        G_OBJECT_CLASS (dh_search_parent_class)->finalize (object);
}

static void
dh_search_class_init (DhSearchClass *klass)
{
        GObjectClass   *object_class = (GObjectClass *) klass;;
        GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;;

        object_class->finalize = search_finalize;

        widget_class->grab_focus = search_grab_focus;

        signals[LINK_SELECTED] =
                g_signal_new ("link_selected",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (DhSearchClass, link_selected),
                              NULL, NULL,
                              _dh_marshal_VOID__POINTER,
                              G_TYPE_NONE,
                              1, G_TYPE_POINTER);

        g_type_class_add_private (klass, sizeof (DhSearchPriv));
}

static void
dh_search_init (DhSearch *search)
{
        DhSearchPriv *priv = GET_PRIVATE (search);

        priv->completion = g_completion_new (
                (GCompletionFunc) search_complete_func);

        priv->hitlist = gtk_tree_view_new ();
        priv->model = dh_keyword_model_new ();

        gtk_tree_view_set_model (GTK_TREE_VIEW (priv->hitlist),
                                 GTK_TREE_MODEL (priv->model));

        gtk_tree_view_set_enable_search (GTK_TREE_VIEW (priv->hitlist), FALSE);

        gtk_box_set_spacing (GTK_BOX (search), 4);
}

static void
search_grab_focus (GtkWidget *widget)
{
        DhSearchPriv *priv = GET_PRIVATE (widget);

        gtk_widget_grab_focus (priv->entry);
}

static void
search_selection_changed_cb (GtkTreeSelection *selection,
                             DhSearch         *search)
{
        DhSearchPriv *priv;
        GtkTreeIter   iter;

        priv = GET_PRIVATE (search);

        if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
                DhLink *link;

                gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter,
                                    DH_KEYWORD_MODEL_COL_LINK, &link,
                                    -1);

                if (link != priv->selected_link) {
                        priv->selected_link = link;
                        g_signal_emit (search, signals[LINK_SELECTED], 0, link);
                }
        }
}

/* Make it possible to jump back to the currently selected item, useful when the
 * html view has been scrolled away.
 */
static gboolean
search_tree_button_press_cb (GtkTreeView    *view,
                             GdkEventButton *event,
                             DhSearch       *search)
{
        GtkTreePath  *path;
        GtkTreeIter   iter;
        DhSearchPriv *priv;
        DhLink       *link;

        priv = GET_PRIVATE (search);

        gtk_tree_view_get_path_at_pos (view, event->x, event->y, &path,
                                       NULL, NULL, NULL);
        if (!path) {
                return FALSE;
        }

        gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path);
        gtk_tree_path_free (path);

        gtk_tree_model_get (GTK_TREE_MODEL (priv->model),
                            &iter,
                            DH_KEYWORD_MODEL_COL_LINK, &link,
                            -1);

        priv->selected_link = link;

        g_signal_emit (search, signals[LINK_SELECTED], 0, link);

        /* Always return FALSE so the tree view gets the event and can update
         * the selection etc.
         */
        return FALSE;
}

static gboolean
search_entry_key_press_event_cb (GtkEntry    *entry,
                                 GdkEventKey *event,
                                 DhSearch    *search)
{
        DhSearchPriv *priv = GET_PRIVATE (search);

        if (event->keyval == GDK_Tab) {
                if (event->state & GDK_CONTROL_MASK) {
                        gtk_widget_grab_focus (priv->hitlist);
                } else {
                        gtk_editable_set_position (GTK_EDITABLE (entry), -1);
                        gtk_editable_select_region (GTK_EDITABLE (entry), -1, -1);
                }
                return TRUE;
        }

        if (event->keyval == GDK_Return ||
            event->keyval == GDK_KP_Enter) {
                GtkTreeIter  iter;
                DhLink      *link;
                gchar       *name;

                /* Get the first entry found. */
                if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->model), &iter)) {
                        gtk_tree_model_get (GTK_TREE_MODEL (priv->model),
                                            &iter,
                                            DH_KEYWORD_MODEL_COL_LINK, &link,
                                            DH_KEYWORD_MODEL_COL_NAME, &name,
                                            -1);

                        gtk_entry_set_text (GTK_ENTRY (entry), name);
                        g_free (name);

                        gtk_editable_set_position (GTK_EDITABLE (entry), -1);
                        gtk_editable_select_region (GTK_EDITABLE (entry), -1, -1);

                        g_signal_emit (search, signals[LINK_SELECTED], 0, link);

                        return TRUE;
                }
        }

        return FALSE;
}

static void
search_combo_set_active_id (DhSearch    *search,
                            const gchar *book_id)
{
        DhSearchPriv *priv = GET_PRIVATE (search);
        GtkTreeIter   iter;
        GtkTreeModel *model;
        gboolean      has_next;

        g_signal_handlers_block_by_func (priv->book_combo,
                                         search_combo_changed_cb,
                                         search);

        if (book_id != NULL) {
                model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->book_combo));

                has_next = gtk_tree_model_get_iter_first (model, &iter);
                while (has_next) {
                        gchar *id;

                        gtk_tree_model_get (model, &iter,
                                            1, &id,
                                            -1);

                        if (id && strcmp (book_id, id) == 0) {
                                g_free (id);

                                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->book_combo),
                                                               &iter);
                                break;
                        }

                        g_free (id);

                        has_next = gtk_tree_model_iter_next (model, &iter);
                }
        } else {
                gtk_combo_box_set_active (GTK_COMBO_BOX (priv->book_combo), 0);
        }

        g_signal_handlers_unblock_by_func (priv->book_combo,
                                           search_combo_changed_cb,
                                           search);
}

static gchar *
search_combo_get_active_id (DhSearch *search)
{
        DhSearchPriv *priv = GET_PRIVATE (search);
        GtkTreeIter   iter;
        GtkTreeModel *model;
        gchar        *id;

        if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->book_combo),
                                            &iter)) {
                return NULL;
        }

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->book_combo));

        gtk_tree_model_get (model, &iter,
                            1, &id,
                            -1);

        return id;
}

static void
search_combo_changed_cb (GtkComboBox *combo,
                         DhSearch    *search)
{
        DhSearchPriv *priv = GET_PRIVATE (search);

        if (!priv->idle_filter) {
                priv->idle_filter =
                        g_idle_add ((GSourceFunc) search_filter_idle, search);
        }
}

static void
search_entry_changed_cb (GtkEntry *entry,
                         DhSearch *search)
{
        DhSearchPriv *priv = GET_PRIVATE (search);

        if (!priv->idle_filter) {
                priv->idle_filter =
                        g_idle_add ((GSourceFunc) search_filter_idle, search);
        }
}

static void
search_entry_activated_cb (GtkEntry *entry,
                           DhSearch *search)
{
        DhSearchPriv *priv = GET_PRIVATE (search);
        gchar        *id;
        const gchar  *str;

        id = search_combo_get_active_id (search);
        str = gtk_entry_get_text (GTK_ENTRY (priv->entry));
        dh_keyword_model_filter (priv->model, str, id);
        g_free (id);
}

static void
search_entry_text_inserted_cb (GtkEntry    *entry,
                               const gchar *text,
                               gint         length,
                               gint        *position,
                               DhSearch    *search)
{
        DhSearchPriv *priv = GET_PRIVATE (search);

        if (!priv->idle_complete) {
                priv->idle_complete =
                        g_idle_add ((GSourceFunc) search_complete_idle,
                                    search);
        }
}

static gboolean
search_complete_idle (DhSearch *search)
{
        DhSearchPriv *priv = GET_PRIVATE (search);
        const gchar  *str;
        gchar        *completed = NULL;
        gsize         length;

        str = gtk_entry_get_text (GTK_ENTRY (priv->entry));

        g_completion_complete (priv->completion, str, &completed);
        if (completed) {
                length = strlen (str);

                gtk_entry_set_text (GTK_ENTRY (priv->entry), completed);
                gtk_editable_set_position (GTK_EDITABLE (priv->entry), length);
                gtk_editable_select_region (GTK_EDITABLE (priv->entry),
                                            length, -1);
                g_free (completed);
        }

        priv->idle_complete = 0;

        return FALSE;
}

static gboolean
search_filter_idle (DhSearch *search)
{
        DhSearchPriv *priv = GET_PRIVATE (search);
        const gchar  *str;
        gchar        *id;
        DhLink       *link;

        str = gtk_entry_get_text (GTK_ENTRY (priv->entry));
        id = search_combo_get_active_id (search);
        link = dh_keyword_model_filter (priv->model, str, id);
        g_free (id);

        priv->idle_filter = 0;

        if (link) {
                g_signal_emit (search, signals[LINK_SELECTED], 0, link);
        }

        return FALSE;
}

static const gchar *
search_complete_func (DhLink *link)
{
        return dh_link_get_name (link);
}

static void
search_cell_data_func (GtkTreeViewColumn *tree_column,
                       GtkCellRenderer   *cell,
                       GtkTreeModel      *tree_model,
                       GtkTreeIter       *iter,
                       gpointer           data)
{
        DhLink       *link;
        PangoStyle    style;

        gtk_tree_model_get (tree_model, iter,
                            DH_KEYWORD_MODEL_COL_LINK, &link,
                            -1);

        style = PANGO_STYLE_NORMAL;

        if (dh_link_get_flags (link) & DH_LINK_FLAGS_DEPRECATED) {
                style |= PANGO_STYLE_ITALIC;
        }

        g_object_set (cell,
                      "text", dh_link_get_name (link),
                      "style", style,
                      NULL);
}

static gboolean
search_combo_row_separator_func (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 gpointer      data)
{
        char *label;
        char *link;
        gboolean result;

        gtk_tree_model_get (model, iter, 0, &label, 1, &link, -1);

        result = (link == NULL && label == NULL);
        g_free (label);
        g_free (link);

        return result;
}

static void
search_combo_populate (DhSearch *search)
{
        DhSearchPriv *priv;
        GtkListStore *store;
        GtkTreeIter   iter;
        GList        *l;

        priv = GET_PRIVATE (search);

        store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (priv->book_combo)));

        gtk_list_store_clear (store);

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, _("All books"),
                            1, NULL,
                            -1);

        /* Add a separator */
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, NULL,
                            1, NULL,
                            -1);

        for (l = dh_book_manager_get_books (priv->book_manager);
             l;
             l = g_list_next (l)) {
                DhBook *book = DH_BOOK (l->data);
                GNode  *node;

                node = dh_book_get_tree (book);
                if (node) {
                        DhLink *link;

                        link = node->data;

                        gtk_list_store_append (store, &iter);
                        gtk_list_store_set (store, &iter,
                                            0, dh_link_get_name (link),
                                            1, dh_link_get_book_id (link),
                                            -1);
                }
        }

        gtk_combo_box_set_active (GTK_COMBO_BOX (priv->book_combo), 0);
}


static void
search_combo_create (DhSearch *search)
{
        GtkListStore    *store;
        GtkCellRenderer *cell;
        DhSearchPriv    *priv;

        priv = GET_PRIVATE (search);

        store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
        priv->book_combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
        g_object_unref (store);

        search_combo_populate (search);

        gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (priv->book_combo),
                                              search_combo_row_separator_func,
                                              NULL, NULL);

        cell = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->book_combo),
                                    cell,
                                    TRUE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->book_combo),
                                       cell,
                                       "text", 0);
}

static void
completion_add_items (DhSearch *search)
{
        DhSearchPriv *priv;
        GList        *l;

        priv = GET_PRIVATE (search);

        for (l = dh_book_manager_get_books (priv->book_manager);
             l;
             l = g_list_next (l)) {
                DhBook *book = DH_BOOK (l->data);
                GList  *keywords;

                keywords = dh_book_get_keywords(book);

                if (keywords) {
                        g_completion_add_items (priv->completion,
                                                keywords);
                }
        }
}

static void
book_manager_disabled_book_list_changed_cb (DhBookManager *book_manager,
                                            gpointer user_data)
{
        DhSearch *search = user_data;
        search_combo_populate (search);
}

GtkWidget *
dh_search_new (DhBookManager *book_manager)
{
        DhSearch         *search;
        DhSearchPriv     *priv;
        GtkTreeSelection *selection;
        GtkWidget        *list_sw;
        GtkWidget        *hbox;
        GtkWidget        *book_label;
        GtkCellRenderer  *cell;

        search = g_object_new (DH_TYPE_SEARCH, NULL);

        priv = GET_PRIVATE (search);

        priv->book_manager = g_object_ref (book_manager);
        g_signal_connect (priv->book_manager,
                          "disabled-book-list-updated",
                          G_CALLBACK (book_manager_disabled_book_list_changed_cb),
                          search);

        gtk_container_set_border_width (GTK_CONTAINER (search), 2);

        search_combo_create (search);
        g_signal_connect (priv->book_combo, "changed",
                          G_CALLBACK (search_combo_changed_cb),
                          search);

        book_label = gtk_label_new_with_mnemonic (_("Search in:"));
        gtk_label_set_mnemonic_widget (GTK_LABEL (book_label), priv->book_combo);

        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start (GTK_BOX (hbox), book_label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), priv->book_combo, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (search), hbox, FALSE, FALSE, 0);

        /* Setup the keyword box. */
        priv->entry = gtk_entry_new ();
        g_signal_connect (priv->entry, "key-press-event",
                          G_CALLBACK (search_entry_key_press_event_cb),
                          search);

        g_signal_connect (priv->hitlist, "button-press-event",
                          G_CALLBACK (search_tree_button_press_cb),
                          search);

        g_signal_connect (priv->entry, "changed",
                          G_CALLBACK (search_entry_changed_cb),
                          search);

        g_signal_connect (priv->entry, "activate",
                          G_CALLBACK (search_entry_activated_cb),
                          search);

        g_signal_connect (priv->entry, "insert-text",
                          G_CALLBACK (search_entry_text_inserted_cb),
                          search);

        gtk_box_pack_start (GTK_BOX (search), priv->entry, FALSE, FALSE, 0);

        /* Setup the hitlist */
        list_sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (list_sw), GTK_SHADOW_IN);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_sw),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);

        cell = gtk_cell_renderer_text_new ();
        g_object_set (cell,
                      "ellipsize", PANGO_ELLIPSIZE_END,
                      NULL);

        gtk_tree_view_insert_column_with_data_func (
                GTK_TREE_VIEW (priv->hitlist),
                -1,
                NULL,
                cell,
                search_cell_data_func,
                search, NULL);

        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->hitlist),
                                           FALSE);
        gtk_tree_view_set_search_column (GTK_TREE_VIEW (priv->hitlist),
                                         DH_KEYWORD_MODEL_COL_NAME);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->hitlist));

        g_signal_connect (selection, "changed",
                          G_CALLBACK (search_selection_changed_cb),
                          search);

        gtk_container_add (GTK_CONTAINER (list_sw), priv->hitlist);

        gtk_box_pack_end (GTK_BOX (search), list_sw, TRUE, TRUE, 0);

        completion_add_items (search);
        dh_keyword_model_set_words (priv->model, book_manager);

        gtk_widget_show_all (GTK_WIDGET (search));

        return GTK_WIDGET (search);
}

void
dh_search_set_search_string (DhSearch    *search,
                             const gchar *str,
                             const gchar *book_id)
{
        DhSearchPriv *priv;

        g_return_if_fail (DH_IS_SEARCH (search));

        priv = GET_PRIVATE (search);

        g_signal_handlers_block_by_func (priv->entry,
                                         search_entry_changed_cb,
                                         search);

        gtk_entry_set_text (GTK_ENTRY (priv->entry), str);

        gtk_editable_set_position (GTK_EDITABLE (priv->entry), -1);
        gtk_editable_select_region (GTK_EDITABLE (priv->entry), -1, -1);

        g_signal_handlers_unblock_by_func (priv->entry,
                                           search_entry_changed_cb,
                                           search);

        search_combo_set_active_id (search, book_id);

        if (!priv->idle_filter) {
                priv->idle_filter =
                        g_idle_add ((GSourceFunc) search_filter_idle, search);
        }
}
