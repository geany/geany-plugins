/*
 *  
 *  GeanyGenDoc, a Geany plugin to ease generation of source code documentation
 *  Copyright (C) 2010-2011  Colomban Wendling <ban@herbesfolles.org>
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 */

#include "ggd-widget-doctype-selector.h"

#include <gtk/gtk.h>

#include "ggd-plugin.h"


#if ! GTK_CHECK_VERSION (2, 20, 0) && ! defined(gtk_widget_get_realized)
# define gtk_widget_get_realized(w) (GTK_WIDGET_REALIZED (w))
#endif


static void     ggd_doctype_selector_finalize     (GObject *object);
static void     ggd_doctype_selector_constructed  (GObject *object);
static void     doctype_column_edited_handler     (GtkCellRendererText *renderer,
                                                   gchar               *path,
                                                   gchar               *new_text,
                                                   GtkListStore        *store);
static gint     sort_language_column              (GtkTreeModel *model,
                                                   GtkTreeIter  *a,
                                                   GtkTreeIter  *b,
                                                   gpointer      user_data);
static gboolean tree_view_button_press_event      (GtkTreeView         *view,
                                                   GdkEventButton      *event,
                                                   GgdDoctypeSelector  *selector);
static gboolean tree_view_popup_menu              (GtkTreeView         *view,
                                                   GgdDoctypeSelector  *selector);
static void     tree_view_popup_clear_handler     (GtkMenuItem        *item,
                                                   GgdDoctypeSelector *selector);
static void     tree_view_popup_edit_handler      (GtkMenuItem        *item,
                                                   GgdDoctypeSelector *selector);


struct _GgdDoctypeSelectorPrivate
{
  GtkListStore   *store;
  GtkWidget      *view;
  GtkWidget      *popup_menu;
};

enum
{
  /* Sort model column IDs in the order they are shown in the view for them to
   * correspond to tree view's columns. Inivisible column at the end. */
  COLUMN_LANGUAGE,
  COLUMN_DOCTYPE,
  COLUMN_ID,
  COLUMN_TOOLTIP,
  
  N_COLUMNS
};


G_DEFINE_TYPE (GgdDoctypeSelector, ggd_doctype_selector, GTK_TYPE_SCROLLED_WINDOW)


static void
ggd_doctype_selector_finalize (GObject *object)
{
  GgdDoctypeSelector *self = GGD_DOCTYPE_SELECTOR (object);
  
  g_object_unref (self->priv->store);
  
  G_OBJECT_CLASS (ggd_doctype_selector_parent_class)->finalize (object);
}

static void
ggd_doctype_selector_class_init (GgdDoctypeSelectorClass *klass)
{
  GObjectClass       *object_class    = G_OBJECT_CLASS (klass);
  
  object_class->finalize    = ggd_doctype_selector_finalize;
  object_class->constructed = ggd_doctype_selector_constructed;
  
  g_type_class_add_private (klass, sizeof (GgdDoctypeSelectorPrivate));
}

static void
ggd_doctype_selector_init (GgdDoctypeSelector *self)
{
  GtkCellRenderer    *renderer;
  GtkTreeViewColumn  *column;
  GtkWidget          *item;
  
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GGD_DOCTYPE_SELECTOR_TYPE,
                                            GgdDoctypeSelectorPrivate);
  /* Set scrolled window properties */
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (self),
                                       GTK_SHADOW_IN);
  
  /* Model */
  self->priv->store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING,
                                          G_TYPE_STRING, G_TYPE_UINT,
                                          G_TYPE_STRING);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (self->priv->store),
                                   COLUMN_LANGUAGE, sort_language_column,
                                   NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->priv->store),
                                        COLUMN_LANGUAGE, GTK_SORT_ASCENDING);
  
  /* View */
  self->priv->view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (self->priv->store));
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (self->priv->view), COLUMN_LANGUAGE);
  gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (self->priv->view), COLUMN_TOOLTIP);
  gtk_widget_show (self->priv->view);
  /* language column */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Language"), renderer,
                                                     "text", COLUMN_LANGUAGE,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->view), column);
  /* doctype column */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "editable", TRUE, NULL);
  g_signal_connect (renderer, "edited",
                    G_CALLBACK (doctype_column_edited_handler), self->priv->store);
  column = gtk_tree_view_column_new_with_attributes (_("Documentation type"), renderer,
                                                     "text", COLUMN_DOCTYPE,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->view), column);
  
  /* create popup menu */
  self->priv->popup_menu = gtk_menu_new ();
  g_signal_connect (self->priv->popup_menu, "deactivate",
                    G_CALLBACK (gtk_widget_hide), NULL);
  gtk_menu_attach_to_widget (GTK_MENU (self->priv->popup_menu),
                             self->priv->view, NULL);
  item = gtk_image_menu_item_new_with_mnemonic (_("_Change associated documentation type"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                 gtk_image_new_from_stock (GTK_STOCK_EDIT,
                                                           GTK_ICON_SIZE_MENU));
  g_signal_connect (item, "activate",
                    G_CALLBACK (tree_view_popup_edit_handler), self);
  gtk_menu_shell_append (GTK_MENU_SHELL (self->priv->popup_menu), item);
  gtk_widget_show (item);
  item = gtk_image_menu_item_new_with_mnemonic (_("_Disassociate documentation type"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                 gtk_image_new_from_stock (GTK_STOCK_CLEAR,
                                                           GTK_ICON_SIZE_MENU));
  g_signal_connect (item, "activate",
                    G_CALLBACK (tree_view_popup_clear_handler), self);
  gtk_menu_shell_append (GTK_MENU_SHELL (self->priv->popup_menu), item);
  gtk_widget_show (item);
  /* connect signals for popping the menu up */
  g_signal_connect (self->priv->view, "button-press-event",
                    G_CALLBACK (tree_view_button_press_event), self);
  g_signal_connect (self->priv->view, "popup-menu",
                    G_CALLBACK (tree_view_popup_menu), self);
}

static void
ggd_doctype_selector_constructed (GObject *object)
{
  GgdDoctypeSelector *self = GGD_DOCTYPE_SELECTOR (object);
  GtkTreeIter         iter;
  guint               i;
  
  /* Add the view after construction because GtkScrolledWindow needs its
   * adjustments to be non-NULL, which is done by setting a property on
   * construction. It perhaps should be fixed in GTK+? */
  gtk_container_add (GTK_CONTAINER (self), self->priv->view);
  
  /* add "All" on top of the list */
  gtk_list_store_append (self->priv->store, &iter);
  gtk_list_store_set (self->priv->store, &iter,
                      COLUMN_ID, 0,
                      COLUMN_LANGUAGE, _("All"),
                      COLUMN_TOOLTIP, _("Default documentation type for "
                                        "languages that does not have one set"),
                      COLUMN_DOCTYPE, NULL,
                      -1);
  /* i = 1: skip None */
  for (i = 1; i < GEANY_MAX_BUILT_IN_FILETYPES; i++) {
    gtk_list_store_append (self->priv->store, &iter);
    gtk_list_store_set (self->priv->store, &iter,
                        COLUMN_ID, i,
                        COLUMN_LANGUAGE, filetypes[i]->name,
                        COLUMN_TOOLTIP, filetypes[i]->title,
                        COLUMN_DOCTYPE, NULL,
                        -1);
  }
}

/* clears documentation type on the selected row */
static void
tree_view_popup_clear_handler (GtkMenuItem        *item,
                               GgdDoctypeSelector *selector)
{
  GtkTreeIter       iter;
  GtkTreeSelection *selection;
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (selector->priv->view));
  if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
    gtk_list_store_set (selector->priv->store, &iter, COLUMN_DOCTYPE, "", -1);
  }
}

/* starts editing of the documentation type on the selected row */
static void
tree_view_popup_edit_handler (GtkMenuItem        *item,
                              GgdDoctypeSelector *selector)
{
  GtkTreeIter       iter;
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeView      *view = GTK_TREE_VIEW (selector->priv->view);
  
  selection = gtk_tree_view_get_selection (view);
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    GtkTreePath        *path;
    GtkTreeViewColumn  *column;
    
    path = gtk_tree_model_get_path (model, &iter);
    column = gtk_tree_view_get_column (view, COLUMN_DOCTYPE);
    gtk_tree_view_set_cursor_on_cell (view, path, column, NULL, TRUE);
    gtk_tree_path_free (path);
  }
}

static void
tree_view_popup_menu_position_func (GtkMenu   *menu,
                                    gint      *x,
                                    gint      *y,
                                    gboolean  *push_in,
                                    gpointer   user_data)
{
  GgdDoctypeSelector *selector = user_data;
  GtkTreeView        *view = GTK_TREE_VIEW (selector->priv->view);
  GdkScreen          *screen = gtk_widget_get_screen (selector->priv->view);
  gint                monitor_id;
  GtkTreeModel       *model;
  GtkTreeViewColumn  *column;
  GtkTreeSelection   *selection;
  GtkTreePath        *path;
  GtkTreeIter         iter;
  GdkWindow          *window;
  GdkRectangle        cell;
  GdkRectangle        monitor;
  GtkRequisition      req;
  
  g_return_if_fail (gtk_widget_get_realized (selector->priv->view));

  window = gtk_widget_get_window (GTK_WIDGET (view));
  gdk_window_get_origin (window, x, y);
  /* ensure there is a row selected */
  selection = gtk_tree_view_get_selection (view);
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    path = gtk_tree_model_get_path (model, &iter);
  } else {
    gtk_tree_view_get_cursor (view, &path, NULL);
    gtk_tree_selection_select_path (selection, path);
  }
  column = gtk_tree_view_get_column (view, COLUMN_DOCTYPE);
  gtk_tree_view_scroll_to_cell (view, path, column, FALSE, 0.0, 0.0);
  gtk_tree_view_get_cell_area (view, path, column, &cell);
  gtk_tree_path_free (path);
  gtk_tree_view_convert_bin_window_to_widget_coords (view,
                                                     *x + cell.x, *y + cell.y,
                                                     x, y);
  gtk_widget_size_request (GTK_WIDGET (menu), &req);
  monitor_id = gdk_screen_get_monitor_at_point (screen, *x, *y);
  gtk_menu_set_monitor (menu, monitor_id);
  gdk_screen_get_monitor_geometry (screen, monitor_id, &monitor);
  if (*y + cell.height + req.height <= monitor.height) {
    *y += cell.height;
  } else {
    *y -= req.height;
  }
  *x = CLAMP (*x, monitor.x, monitor.x + MAX (0, monitor.width - req.width));
  *y = CLAMP (*y, monitor.y, monitor.y + MAX (0, monitor.height - req.height));
  *push_in = FALSE;
}

static void
do_popup_menu (GgdDoctypeSelector *self,
               GdkEventButton     *event)
{
  guint               event_button;
  guint32             event_time;
  GtkMenuPositionFunc menu_position_func;
  
  if (event) {
    event_button = event->button;
    event_time = event->time;
    menu_position_func = NULL;
  } else {
    event_button = 0;
    event_time = gtk_get_current_event_time ();
    menu_position_func = tree_view_popup_menu_position_func;
  }
  gtk_menu_popup (GTK_MENU (self->priv->popup_menu), NULL, NULL,
                  menu_position_func, self, event_button, event_time);
}

static gboolean
tree_view_button_press_event (GtkTreeView        *view,
                              GdkEventButton     *event,
                              GgdDoctypeSelector *selector)
{
  gboolean handled = FALSE;
  
  /* Ignore double-clicks and triple-clicks */
  if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
    GtkTreePath *path;
    
    /* select row under the cursor before poping the menu up */
    if (gtk_tree_view_get_path_at_pos (view, (gint)event->x, (gint)event->y,
                                       &path, NULL, NULL, NULL)) {
      gtk_tree_selection_select_path (gtk_tree_view_get_selection (view), path);
      gtk_tree_view_scroll_to_cell (view, path, NULL, FALSE, 0.0, 0.0);
      gtk_tree_path_free (path);
    }
    do_popup_menu (selector, event);
    handled = TRUE;
  }

  return handled;
}

static gboolean
tree_view_popup_menu (GtkTreeView        *view,
                      GgdDoctypeSelector *selector)
{
  do_popup_menu (selector, NULL);
  
  return TRUE;
}

/* Gets the row having @language_id as language ID.
 * Returns: %TRUE if @iter was set, %FALSE otherwise */
static gboolean
get_row_iter (GgdDoctypeSelector *self,
              guint               language_id,
              GtkTreeIter        *iter)
{
  gboolean      valid;
  GtkTreeModel *model = GTK_TREE_MODEL (self->priv->store);
  gboolean      found = FALSE;
  
  valid = gtk_tree_model_get_iter_first (model, iter);
  while (valid && ! found) {
    guint id;
    
    gtk_tree_model_get (model, iter, COLUMN_ID, &id, -1);
    if (id == language_id) {
      found = TRUE;
      break;
    }
    valid = gtk_tree_model_iter_next (model, iter);
  }
  
  return found;
}

/* Updates the doctype of @language_id.
 * Returns: %TRUE if @language_id was found (and updated), %FALSE otherwise */
static gboolean
update_doctype (GgdDoctypeSelector *self,
                guint               language_id,
                const gchar        *new_doctype)
{
  GtkTreeIter iter;
  gboolean    changed = FALSE;
  
  if (get_row_iter (self, language_id, &iter)) {
    gtk_list_store_set (self->priv->store, &iter,
                        COLUMN_DOCTYPE, new_doctype,
                        -1);
    changed = TRUE;
  }
  
  return changed;
}

/* Gets the value of a column of the row containing @language_id
 * Returns: %TRUE if @result was set, %FALSE otherwise */
static gboolean
get_row_value (GgdDoctypeSelector  *self,
               guint                language_id,
               guint                column,
               void                *result)
{
  GtkTreeIter iter;
  gboolean    found = FALSE;
  
  if (get_row_iter (self, language_id, &iter)) {
    gtk_tree_model_get (GTK_TREE_MODEL (self->priv->store), &iter,
                        column, result,
                        -1);
    found = TRUE;
  }
  
  return found;
}

static void
doctype_column_edited_handler (GtkCellRendererText *renderer,
                               gchar               *path,
                               gchar               *new_text,
                               GtkListStore        *store)
{
  GtkTreeIter iter;
  
  gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (store), &iter, path);
  gtk_list_store_set (store, &iter, COLUMN_DOCTYPE, new_text, -1);
}

/* GtkTreeIterCompareFunc to sort by the language column
 * It keeps language ID 0 (All) on top. */
static gint
sort_language_column (GtkTreeModel *model,
                      GtkTreeIter  *a,
                      GtkTreeIter  *b,
                      gpointer      user_data)
{
  guint   lang_id_1;
  guint   lang_id_2;
  gchar  *lang_name_1;
  gchar  *lang_name_2;
  gint    ret = 0;
  
  gtk_tree_model_get (model, a,
                      COLUMN_ID, &lang_id_1,
                      COLUMN_LANGUAGE, &lang_name_1,
                      -1);
  gtk_tree_model_get (model, b,
                      COLUMN_ID, &lang_id_2,
                      COLUMN_LANGUAGE, &lang_name_2,
                      -1);
  if (lang_id_1 == 0 || lang_id_2 == 0) {
    /* sort All before any other */
    ret = (gint)lang_id_1 - (gint)lang_id_2;
  } else {
    /* probably no need for a better string sorting function */
    ret = g_ascii_strcasecmp (lang_name_1, lang_name_2);
  }
  g_free (lang_name_1);
  g_free (lang_name_2);
  
  return ret;
}


/*----------------------------- Begin public API -----------------------------*/

GtkWidget *
ggd_doctype_selector_new (void)
{
  return g_object_new (GGD_DOCTYPE_SELECTOR_TYPE, NULL);
}

gboolean
ggd_doctype_selector_set_doctype (GgdDoctypeSelector *self,
                                  guint               language_id,
                                  const gchar        *doctype)
{
  g_return_val_if_fail (GGD_IS_DOCTYPE_SELECTOR (self), FALSE);
  g_return_val_if_fail (language_id >= 0 &&
                        language_id < GEANY_MAX_BUILT_IN_FILETYPES,
                        FALSE);
  
  return update_doctype (self, language_id, doctype);
}

gchar *
ggd_doctype_selector_get_doctype (GgdDoctypeSelector *self,
                                  guint               language_id)
{
  gchar *doctype = NULL;
  
  g_return_val_if_fail (GGD_IS_DOCTYPE_SELECTOR (self), FALSE);
  g_return_val_if_fail (language_id >= 0 &&
                        language_id < GEANY_MAX_BUILT_IN_FILETYPES,
                        FALSE);
  
  get_row_value (self, language_id, COLUMN_DOCTYPE, &doctype);
  
  return doctype;
}
